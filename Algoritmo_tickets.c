#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

#define MAX_INSTR_LENGTH 1000
#define MAX_STR_LEN 100
#define MAX_NUM_REGS 10

int NUM_NODOS = 0;
int NUM_PROCESOS = 0;

struct mensaje
{
    long tipo;
    int ticket;
    int id_nodo;
    int puerto;
    long prioridad;
};

struct info_a_mandar_al_hilo{
    int diferencia;
    int puerto;
    int id_nodos[10];
    int prioridad;
};

struct info_hilo_proceso{
    long prio;
    float time;
};

// VARIABLES UTILES
int mi_ticket = 0;
int mi_id = 0;
int mi_buzon_interno = 0;
int id_nodos_pendientes[100] = {0};
int puertos_pendientes[100] = {0};
int num_pend = 0;
int quiero = 0;
int esperando = 0;
int max_ticket = 0;
int primero = 1;
int repetir = 0;
int esperar = 0;
int semaforo_de_esperar = 0;
int primer_proceso_general = 1;
int consultas_esperando = 0;
int anulaciones_esperando = 0;
int pagos_esperando = 0;
int admin_reser_esperando = 0;
int consultas_dentro = 0;
int antes_de_entrar = 0;
int hizo_requests_consultas = 0;

int permitiendo_adelantamiento_de_ronda = 0;
int no_volver_a_permitir_adelantamientos = 0;
int anulaciones_esperando_a_adelantar = 0;
int pagos_esperando_a_adelantar = 0;
int ultimo_adelantando = 0;
int alguien_dentro = 0;

int consultas_esperando_la_seccion_critica = 0;

int peticiones_pendientes_de_consultas[100] = {0};
int oks_a_consultas_ya_mandados = 0;
// VARIABLES UTILES

//CAMBIO DE VARIABLES
int id_nodos[200];
int diferencia;
int puerto = 1;
//CAMBIO DE VARIABLES

// VARIABLES PARA HACER ADELANTAMIENTOS
int proceso_prioritario_echa_a_consultas = 0;
// VARIABLES PARA HACER ADELANTAMIENTOS

// EL SEMAFORO
sem_t semaforo_seccion_critica;
// EL SEMAFORO

// SINCRONIZACION
sem_t semaforo_quiero;
sem_t semaforo_ticket;
sem_t semaforo_mi_id;
sem_t semaforo_nodos_pendientes;
sem_t semaforo_num_pendientes;
sem_t semaforo_max_ticket;
sem_t semaforo_primero_dentro;
sem_t semaforo_primero_variable;
sem_t semaforo_no_hacer_requests;
sem_t semaforo_semaforo_de_esperar;
sem_t semaforo_primer_proceso_general;
sem_t semaforo_consultas_dentro;
sem_t semaforo_antes_de_entrar;
sem_t semaforo_primero;
sem_t semaforo_repetir;
sem_t semaforo_esperar;
sem_t semaforo_consultas_esperando;
sem_t semaforo_hizo_requests_consultas;
sem_t semaforo_permitiendo_adelantamiento_de_ronda;
sem_t semaforo_no_volver_a_permitir_adelantamientos;
sem_t semaforo_anulaciones_esperando_a_adelantar;
sem_t semaforo_pagos_esperando_a_adelantar;
sem_t semaforo_ultimo_adelantando;
sem_t semaforo_alguien_dentro;
sem_t semaforo_oks_a_consultas_ya_mandados;
sem_t semaforo_esperando;
sem_t semaforo_consultas_esperando_la_seccion_critica;

sem_t semaforo_anulaciones_adelantamiento_de_ronda;
sem_t semaforo_pagos_adelantamiento_de_ronda;

sem_t semaforo_coordinador_main_hilos;
// SINCRONIZACION

sem_t semaforo_finalizador;

// SEMAFOROS DE PRIORIDADES
sem_t semaforo_anulaciones;
sem_t semaforo_pagos;
sem_t semaforo_administracion_reservas;
sem_t semaforo_consultas;
// SEMAFOROS DE PRIORIDADES

///////
char* registros[10000000];
long posicion_en_registro = 0;
///////

char* generar_registro(char* pid, int tipo_proceso, struct timeval tiempo1, struct timeval tiempo2) {
    // Crear un array para almacenar la cadena de registro
    char registro_str[MAX_STR_LEN * 4];

    // Convertir los valores de tiempo a estructuras de tiempo
    struct tm tiempo1_tm = *localtime(&tiempo1.tv_sec);
    struct tm tiempo2_tm = *localtime(&tiempo2.tv_sec);

    // Crear cadenas de caracteres formateadas para las horas
    char hora1_str[MAX_STR_LEN];
    char hora2_str[MAX_STR_LEN];
    strftime(hora1_str, MAX_STR_LEN, "%H:%M:%S", &tiempo1_tm);
    strftime(hora2_str, MAX_STR_LEN, "%H:%M:%S", &tiempo2_tm);

    // Crear cadena de registro con formato "tipo de proceso;pid;hora1;hora2;diferencia"
    char tipo_proceso_str[MAX_STR_LEN];
    switch (tipo_proceso) {
        case 1:
            strcpy(tipo_proceso_str, "ANULACION");
            break;
        case 2:
            strcpy(tipo_proceso_str, "PAGO");
            break;
        case 3:
            strcpy(tipo_proceso_str, "RESERVA/ADMINISTRACION");
            break;
        case 4:
            strcpy(tipo_proceso_str, "CONSULTA");
            break;
        default:
            sprintf(tipo_proceso_str, "%d", tipo_proceso);
            break;
    }

    sprintf(registro_str, "%d;%d;%s;%s;%s.%06ld;%s.%06ld;%ld",
        NUM_NODOS, NUM_PROCESOS,
        tipo_proceso_str, pid,
        hora1_str, (long) tiempo1.tv_usec,
        hora2_str, (long) tiempo2.tv_usec,
        (long)(tiempo2.tv_sec - tiempo1.tv_sec)*1000000L + (long)(tiempo2.tv_usec - tiempo1.tv_usec));

    // Asignar memoria para la cadena de registro y copiar la cadena generada
    char* registro = malloc(strlen(registro_str) + 1);
    strcpy(registro, registro_str);

    // Devolver la cadena de registro
    return registro;
}

void *proceso( void *arg)
{

    struct info_hilo_proceso* info_pasada_param = (struct info_hilo_proceso*) arg; 
    int id_aux = 0;
    int i = 0;
    int mi_puerto;
    long mi_prioridad;
    struct mensaje peticion;
    struct mensaje respuesta;
    struct timeval tiempo1;
    struct timeval tiempo2;
    int hilo_num = (int)pthread_self();
    char hilo_str[20];
    snprintf(hilo_str, sizeof(hilo_str), "%d", hilo_num);
    int longitud_peticion;
    int longitud_respuesta;
    int max_ticket_empece = 0;
    int mi_ticket_pedi = 0;
    int puedo_adelantar = 0;

    float SC_TIME = info_pasada_param->time;
    //printf("+++++++SC_TIME: %d+++++++\n",SC_TIME);

    // INICIAR LONGITUD
    longitud_peticion = sizeof(peticion) - sizeof(peticion.tipo);
    longitud_respuesta = sizeof(respuesta) - sizeof(respuesta.tipo);
    // INICAR LONGITUD

    // INICIAR LA INFORMACION DADA POR EL MAIN
    mi_prioridad = info_pasada_param->prio;
    //printf("+++++++PRIO: %lu+++++++\n",mi_prioridad);
    mi_puerto = puerto;
    //printf("1mi_nodo: %i,mi_puerto: %i\n",mi_id,mi_puerto);
    sem_post(&semaforo_coordinador_main_hilos);

    //printf("mi_prioridad: %li\n",mi_prioridad);
        sem_wait(&semaforo_quiero);
        quiero++;
        sem_post(&semaforo_quiero);
        gettimeofday(&tiempo1,NULL);
        //idea para adelantamiento de ronda
        sem_wait(&semaforo_permitiendo_adelantamiento_de_ronda);
        if(permitiendo_adelantamiento_de_ronda==1 && (mi_prioridad==1 || mi_prioridad==2)){
            sem_post(&semaforo_permitiendo_adelantamiento_de_ronda);
            if(mi_prioridad==1){
                puedo_adelantar=1;
                sem_wait(&semaforo_anulaciones_esperando_a_adelantar);
                anulaciones_esperando_a_adelantar++;
                sem_post(&semaforo_anulaciones_esperando_a_adelantar);
                sem_wait(&semaforo_anulaciones_adelantamiento_de_ronda);
                sem_wait(&semaforo_anulaciones_esperando_a_adelantar);
                anulaciones_esperando_a_adelantar--;
                sem_post(&semaforo_anulaciones_esperando_a_adelantar);
                sem_wait(&semaforo_anulaciones_esperando_a_adelantar);
                sem_wait(&semaforo_pagos_esperando_a_adelantar);
                if(anulaciones_esperando_a_adelantar!=0){
                    sem_post(&semaforo_anulaciones_adelantamiento_de_ronda);
                    //printf("salio anu y dejo a otra anu\n");
                }
                else if(pagos_esperando_a_adelantar!=0){
                    sem_post(&semaforo_pagos_adelantamiento_de_ronda);
                    //printf("salio anu y dejo a pago\n");
                }
                else{
                    sem_wait(&semaforo_ultimo_adelantando);
                    ultimo_adelantando = 1;
                    sem_post(&semaforo_ultimo_adelantando);
                    //printf("ultimo set por anu\n");
                }
                sem_post(&semaforo_pagos_esperando_a_adelantar);
                sem_post(&semaforo_anulaciones_esperando_a_adelantar);
            }
            if(mi_prioridad==2){
                puedo_adelantar=1;
                sem_wait(&semaforo_pagos_esperando_a_adelantar);
                pagos_esperando_a_adelantar++;
                sem_post(&semaforo_pagos_esperando_a_adelantar);
                sem_wait(&semaforo_pagos_adelantamiento_de_ronda);
                sem_wait(&semaforo_pagos_esperando_a_adelantar);
                pagos_esperando_a_adelantar--;
                sem_post(&semaforo_pagos_esperando_a_adelantar);
                sem_wait(&semaforo_pagos_esperando_a_adelantar);
                if(pagos_esperando_a_adelantar!=0){
                    sem_post(&semaforo_pagos_adelantamiento_de_ronda);
                    //printf("salio pago y dejo a otra pago\n");
                }
                else{
                    sem_wait(&semaforo_ultimo_adelantando);
                    ultimo_adelantando = 1;
                    sem_post(&semaforo_ultimo_adelantando);
                    //printf("ultimo set por pago\n");
                }
                sem_post(&semaforo_pagos_esperando_a_adelantar);
            }
        }
        else{
            sem_post(&semaforo_permitiendo_adelantamiento_de_ronda);
        }
        //idea para adelantamiento de ronda
        if(puedo_adelantar==0){

        sem_wait(&semaforo_primero_variable);

        sem_wait(&semaforo_hizo_requests_consultas);
        if(hizo_requests_consultas==0 || mi_prioridad!=4){
        sem_post(&semaforo_hizo_requests_consultas);

        sem_wait(&semaforo_primero);
        sem_wait(&semaforo_consultas_dentro);//dudoso
        if(primero || (consultas_dentro!=0 && mi_prioridad!=4)){
            if(mi_prioridad==4){
                sem_wait(&semaforo_consultas_esperando_la_seccion_critica);
                consultas_esperando_la_seccion_critica=1;
                sem_post(&semaforo_consultas_esperando_la_seccion_critica);
            }
            sem_post(&semaforo_consultas_dentro);
            sem_post(&semaforo_primero);
            sem_wait(&semaforo_consultas_dentro);
            if(consultas_dentro!=0 && mi_prioridad!=4){//puede dar fallo
                sem_post(&semaforo_consultas_dentro);
                sem_wait(&semaforo_esperar);
                esperar=1;
                sem_post(&semaforo_esperar);
                sem_wait(&semaforo_repetir);
                repetir=1;
                sem_post(&semaforo_repetir);
            }
            else{
                sem_post(&semaforo_consultas_dentro);
            }
            sem_wait(&semaforo_esperar);
            if(esperar){//si el primero no mando los mensajes antes de que le llegue una peticion mamamos (a lo mejor hay que cambiarlo de sitio)
                sem_post(&semaforo_esperar);

                sem_wait(&semaforo_semaforo_de_esperar);
                semaforo_de_esperar++;
                sem_post(&semaforo_semaforo_de_esperar);

                sem_wait(&semaforo_no_hacer_requests);
                sem_wait(&semaforo_semaforo_de_esperar);
                semaforo_de_esperar--;
                if(semaforo_de_esperar!=0){//esto hace que salgan todos los que esten aqui y vayan al de la seccion critica (creo que no afecta porque solo hay uno)
                    sem_post(&semaforo_no_hacer_requests);
                }
                sem_post(&semaforo_semaforo_de_esperar);
                sem_wait(&semaforo_esperar);
                esperar=0;
                sem_post(&semaforo_esperar);
            }
            else{
                sem_post(&semaforo_esperar);
            }
            sem_wait(&semaforo_max_ticket);
            sem_wait(&semaforo_ticket);
            max_ticket_empece = max_ticket;
            switch(mi_prioridad){
                case 1:
                    mi_ticket = max_ticket+1;
                    break;
                case 2:
                    mi_ticket = max_ticket+2;
                    break;
                case 3:
                    mi_ticket = max_ticket+3;
                    break;
                case 4:
                    mi_ticket = max_ticket+4;
                    break;
            }
            mi_ticket_pedi = mi_ticket;
            sem_post(&semaforo_ticket);
            sem_post(&semaforo_max_ticket);


            //printf("Mi ticket: %i , Max ticket %i, Mi puerto: %d\n", mi_ticket, max_ticket,mi_puerto);
            sem_wait(&semaforo_ticket);
            sem_wait(&semaforo_max_ticket);
            // INICAR STRUCT PETICION
            peticion.tipo = 1;
            peticion.ticket = mi_ticket;
            peticion.id_nodo = mi_id;
            peticion.puerto = mi_puerto;
            peticion.prioridad = mi_prioridad;
            // INICIAR STRUCT PETICION
            sem_post(&semaforo_max_ticket);
            sem_post(&semaforo_ticket);
            for (i = 0; i < diferencia; i++)
            {
            if (msgsnd(id_nodos[i], &peticion, longitud_peticion, 0) == -1) // 1 es no bloqueante
                exit(-1);
            }
            //printf("2mi_nodo: %i,mi_puerto: %i\n",mi_id,mi_puerto);
            primero=0;
            if(mi_prioridad==4){
                sem_wait(&semaforo_hizo_requests_consultas);
                hizo_requests_consultas=1;
                sem_post(&semaforo_hizo_requests_consultas);
            }
            sem_wait(&semaforo_primero_dentro);
        }
        else{
            sem_post(&semaforo_consultas_dentro);
            sem_post(&semaforo_primero);
        }
        
        sem_wait(&semaforo_ultimo_adelantando);
        if(ultimo_adelantando==1){/////////////////////
            sem_post(&semaforo_ultimo_adelantando);
            sem_wait(&semaforo_primero);
            primero = 1;
            sem_post(&semaforo_primero);
            sem_wait(&semaforo_ultimo_adelantando);
            ultimo_adelantando = 0;
            sem_post(&semaforo_ultimo_adelantando);
        }
        else{
            sem_post(&semaforo_ultimo_adelantando);
        }
        }
        else{
            sem_post(&semaforo_hizo_requests_consultas);
        }
        if(mi_prioridad==4){
            sem_wait(&semaforo_consultas_esperando_la_seccion_critica);
            consultas_esperando_la_seccion_critica=0;
            sem_post(&semaforo_consultas_esperando_la_seccion_critica);
        }
        sem_post(&semaforo_primero_variable);
        }
        //printf("3mi_nodo: %i,mi_puerto: %i\n",mi_id,mi_puerto);
        puedo_adelantar=0;
        sem_wait(&semaforo_esperando);
        esperando++;
        sem_post(&semaforo_esperando);
        sem_wait(&semaforo_antes_de_entrar);
        antes_de_entrar++;
        sem_post(&semaforo_antes_de_entrar);

        //PRIORIDADES IDEA 1
        sem_wait(&semaforo_primer_proceso_general);
        if(primer_proceso_general==0 || (proceso_prioritario_echa_a_consultas==1 && mi_prioridad==4)){//el primer proceso se salta esto
            sem_post(&semaforo_primer_proceso_general);
            switch(mi_prioridad){
            case 1:
                anulaciones_esperando++;
                sem_wait(&semaforo_consultas_dentro);
                if(consultas_dentro!=0){
                    proceso_prioritario_echa_a_consultas=1;
                }
                sem_post(&semaforo_consultas_dentro);
                sem_wait(&semaforo_anulaciones);
                if(proceso_prioritario_echa_a_consultas==1){
                    proceso_prioritario_echa_a_consultas=0;
                }
                anulaciones_esperando--;
                break;
            case 2:
                pagos_esperando++;
                sem_wait(&semaforo_consultas_dentro);
                if(consultas_dentro!=0){
                    proceso_prioritario_echa_a_consultas=1;
                }
                sem_post(&semaforo_consultas_dentro);
                sem_wait(&semaforo_pagos);
                if(proceso_prioritario_echa_a_consultas==1){
                    proceso_prioritario_echa_a_consultas=0;
                }
                pagos_esperando--;
                break;
            case 3:
                admin_reser_esperando++;
                sem_wait(&semaforo_consultas_dentro);
                if(consultas_dentro!=0){
                    proceso_prioritario_echa_a_consultas=1;
                }
                sem_post(&semaforo_consultas_dentro);
                sem_wait(&semaforo_administracion_reservas);
                if(proceso_prioritario_echa_a_consultas==1){
                    proceso_prioritario_echa_a_consultas=0;
                }
                admin_reser_esperando--;
                break;
            case 4:
              sem_wait(&semaforo_consultas_dentro);
              if(consultas_dentro==0 || proceso_prioritario_echa_a_consultas==1){
                sem_post(&semaforo_consultas_dentro);
                sem_wait(&semaforo_consultas_esperando);
                consultas_esperando++;
                sem_post(&semaforo_consultas_esperando);
                sem_wait(&semaforo_consultas);
                sem_wait(&semaforo_consultas_esperando);
                consultas_esperando--;
                sem_post(&semaforo_consultas_esperando);
                sem_wait(&semaforo_consultas_esperando);
                if(consultas_esperando!=0){
                    sem_post(&semaforo_consultas_esperando);
                    sem_post(&semaforo_consultas);//cada consulta saca a la siguiente
                }
                else{
                    sem_post(&semaforo_consultas_esperando);
                }
              }
              else{
                sem_post(&semaforo_consultas_dentro);
              }
                break;
            default:
                break;
            }
        }
        else{
            sem_post(&semaforo_primer_proceso_general);
        }
        sem_wait(&semaforo_antes_de_entrar);
        antes_de_entrar--;
        sem_post(&semaforo_antes_de_entrar);
        sem_wait(&semaforo_primer_proceso_general);
        primer_proceso_general = 0;
        sem_post(&semaforo_primer_proceso_general);
        //PRIORIDADES IDEA 1
        if(mi_prioridad!=4){//consultas
            sem_wait(&semaforo_seccion_critica);
        }
        else{
            sem_wait(&semaforo_consultas_dentro);
            consultas_dentro++;
            sem_post(&semaforo_consultas_dentro);
            sem_wait(&semaforo_oks_a_consultas_ya_mandados);
            if(oks_a_consultas_ya_mandados==0){
                oks_a_consultas_ya_mandados=1;
                sem_post(&semaforo_oks_a_consultas_ya_mandados);
                respuesta.tipo = 2;
                respuesta.id_nodo = mi_id;
                respuesta.ticket = 0;
                for (i = 0; i < num_pend; i++)
                {               
                    respuesta.puerto = puertos_pendientes[i];
                    if(peticiones_pendientes_de_consultas[i]==1){
                        if (msgsnd(id_nodos_pendientes[i], &respuesta, longitud_respuesta, 1) == -1){ // 1 es no bloqueante
                        exit(-1);
                        }
                        peticiones_pendientes_de_consultas[i]=2;
                    }
                }
            }
            else{
                sem_post(&semaforo_oks_a_consultas_ya_mandados);
            }
        }
        sem_wait(&semaforo_permitiendo_adelantamiento_de_ronda);
        sem_wait(&semaforo_no_volver_a_permitir_adelantamientos);
        if(no_volver_a_permitir_adelantamientos==0 && permitiendo_adelantamiento_de_ronda==1){
            permitiendo_adelantamiento_de_ronda = 0;
            no_volver_a_permitir_adelantamientos = 1;
        }
        sem_post(&semaforo_no_volver_a_permitir_adelantamientos);
        sem_post(&semaforo_permitiendo_adelantamiento_de_ronda);

        sem_wait(&semaforo_alguien_dentro);
        alguien_dentro++;
        sem_post(&semaforo_alguien_dentro);
        // SECCION CRITICA
        gettimeofday(&tiempo2,NULL);
        printf("*****\n");
        printf("[%i,%i,%li] Dentro de la seccion critica en ronda [%d,%d]\n",mi_id,mi_puerto,mi_prioridad,mi_ticket_pedi,max_ticket_empece);
        printf("*****\n");
        usleep(SC_TIME*1000000);
        printf("*****\n");
        printf("[%i,%i,%li] Termine\n",mi_id,mi_puerto,mi_prioridad);
        printf("*****\n\n\n");
        // SECCION CRITICA
        sem_wait(&semaforo_alguien_dentro);
        alguien_dentro--;
        sem_post(&semaforo_alguien_dentro);

        sem_wait(&semaforo_ticket);
        mi_ticket=999999;
        sem_post(&semaforo_ticket);

        if(mi_prioridad!=4){//consultas
            sem_post(&semaforo_seccion_critica);
        }
        else{
            sem_wait(&semaforo_consultas_dentro);
            consultas_dentro--;
            sem_post(&semaforo_consultas_dentro);
        }
        sem_wait(&semaforo_esperando);
        esperando--;
        sem_post(&semaforo_esperando);
        sem_wait(&semaforo_quiero);
        quiero--;
        sem_post(&semaforo_quiero);

        if(mi_prioridad==4){
            sem_wait(&semaforo_hizo_requests_consultas);
            hizo_requests_consultas=0;
            sem_post(&semaforo_hizo_requests_consultas);
        }
        sem_wait(&semaforo_anulaciones_esperando_a_adelantar);
        sem_wait(&semaforo_pagos_esperando_a_adelantar);
        if(anulaciones_esperando_a_adelantar!=0 || pagos_esperando_a_adelantar!=0){
            sem_post(&semaforo_pagos_esperando_a_adelantar);
            sem_post(&semaforo_anulaciones_esperando_a_adelantar);
            sem_wait(&semaforo_primero);
            primero = 0;
            sem_post(&semaforo_primero);
            sem_wait(&semaforo_primer_proceso_general);
            primer_proceso_general = 1;
            sem_post(&semaforo_primer_proceso_general);
            if(anulaciones_esperando_a_adelantar!=0){
                sem_post(&semaforo_anulaciones_adelantamiento_de_ronda);
            }
            else{
                sem_post(&semaforo_pagos_adelantamiento_de_ronda);
            }
        }
        else{
        sem_post(&semaforo_pagos_esperando_a_adelantar);
        sem_post(&semaforo_anulaciones_esperando_a_adelantar);
        //PRIORIDADES IDEA 1
        sem_wait(&semaforo_consultas_dentro);
        sem_wait(&semaforo_esperando);
        if(esperando!=0 && consultas_dentro==0){
            if(anulaciones_esperando!=0){
                sem_post(&semaforo_anulaciones);
            }
            else if(pagos_esperando!=0)
            {
                sem_post(&semaforo_pagos);
            }
            else if(admin_reser_esperando!=0)
            {
                sem_post(&semaforo_administracion_reservas);
            }
            else    
                sem_post(&semaforo_consultas);
        }
        sem_post(&semaforo_esperando);
        sem_post(&semaforo_consultas_dentro);
        //PRIORIDADES IDEA 1
        sem_wait(&semaforo_repetir);
        sem_wait(&semaforo_esperando);
        if(esperando==0 && repetir==1){
            sem_post(&semaforo_esperando);
            sem_post(&semaforo_repetir);
                respuesta.tipo = 2;
                respuesta.id_nodo = mi_id;
                respuesta.ticket = 0;
                sem_wait(&semaforo_primer_proceso_general);
                primer_proceso_general=1;
                sem_post(&semaforo_primer_proceso_general);
                sem_wait(&semaforo_nodos_pendientes);
                for (i = 0; i < num_pend; i++)
                {               
                    respuesta.puerto = puertos_pendientes[i];
                    if(peticiones_pendientes_de_consultas[i]!=2){
                        if (msgsnd(id_nodos_pendientes[i], &respuesta, longitud_respuesta, 1) == -1){ // 1 es no bloqueante
                        exit(-1);
                        }
                        //printf("proceso %i en nodo %i mande ok a %i\n",mi_puerto,mi_id,id_nodos_pendientes[i]);
                    }
                    //printf("nodo: %i mando oks\n",mi_id);
                    peticiones_pendientes_de_consultas[i]=0;
                }
                sem_post(&semaforo_nodos_pendientes);
                sem_wait(&semaforo_oks_a_consultas_ya_mandados);
                oks_a_consultas_ya_mandados=0;
                sem_post(&semaforo_oks_a_consultas_ya_mandados);
                //sleep(1);//>>para poder observar las prioridades
                sem_wait(&semaforo_repetir);
                repetir=0;
                sem_post(&semaforo_repetir);
                sem_wait(&semaforo_num_pendientes);
                num_pend = 0;
                sem_post(&semaforo_num_pendientes);

                sem_wait(&semaforo_no_volver_a_permitir_adelantamientos);
                no_volver_a_permitir_adelantamientos=0;//////
                sem_post(&semaforo_no_volver_a_permitir_adelantamientos);
                sem_wait(&semaforo_permitiendo_adelantamiento_de_ronda);
                permitiendo_adelantamiento_de_ronda=0;//////
                sem_post(&semaforo_permitiendo_adelantamiento_de_ronda);
                //printf("acabamos\n");
                sem_post(&semaforo_no_hacer_requests);
        
        }
        else{
            sem_post(&semaforo_esperando);
            sem_post(&semaforo_repetir);
        }
        }

        sem_wait(&semaforo_quiero);
        if(quiero==0){
            sem_post(&semaforo_quiero);
            sem_wait(&semaforo_primer_proceso_general);
            primer_proceso_general=1;
            sem_post(&semaforo_primer_proceso_general);
            sem_wait(&semaforo_primero);
            primero=1;
            sem_post(&semaforo_primero);
        }
        else{
            sem_post(&semaforo_quiero);
        }
        //////registro
        char* registro = generar_registro(hilo_str,mi_prioridad,tiempo1,tiempo2);
        registros[posicion_en_registro] = registro;
        posicion_en_registro++;
        sem_post(&semaforo_finalizador);
        pthread_exit(NULL);
}

void *receptor(void *args){
int id_nodo_origen = 0;
    int ticket_origen = 0;
    int tipo_origen = 0;
    int prioridad_origen = 0;
    int puerto_origen = 0;
    int longitud_peticion;
    int longitud_respuesta;
    int longitud_mensajito = 0;
    int cuenta = 0;
    struct mensaje mensaje_recibido;
    struct mensaje respuesta;
    struct info_a_mandar_al_hilo *aux;
    longitud_peticion = sizeof(mensaje_recibido) - sizeof(mensaje_recibido.tipo);
    longitud_respuesta = sizeof(respuesta) - sizeof(respuesta.tipo);

    float DELAY_TIME = *(float *)args;
    //printf("+++++++++++++++TIEMPO DE DELAY : %d+++++++++++++++\n", DELAY_TIME);

    //printf("diferencia en receptor: %d\n",diferencia);

    while (1)
    {
        // RECIBIR MENSAJE AJENA

        if (msgrcv(mi_id, &mensaje_recibido, longitud_peticion, 0, 0) == -1) // RECIBE TODOS LOS MENSAJES QUE LE LLEGUEN
        {                           
            exit(-1);
        }
        srand(mi_id);
        usleep(DELAY_TIME*1000000);
        tipo_origen = mensaje_recibido.tipo;
        ticket_origen = mensaje_recibido.ticket;
        id_nodo_origen = mensaje_recibido.id_nodo;
        puerto_origen = mensaje_recibido.puerto;
        prioridad_origen = mensaje_recibido.prioridad;

        // RECIBIR PETICION AJENA
        if (tipo_origen == 1)
        { // ESTO ES QUE RECIBIO UNA REQUEST

            sem_wait(&semaforo_nodos_pendientes);
            sem_wait(&semaforo_quiero);
            sem_wait(&semaforo_ticket);
            sem_wait(&semaforo_mi_id);//////////////////////////////////
            sem_wait(&semaforo_num_pendientes);
            //sem_wait(&semaforo_nodos_pendientes);

            sem_wait(&semaforo_max_ticket);
            if (max_ticket < ticket_origen)
            {
                max_ticket = ticket_origen;
            }

            sem_post(&semaforo_max_ticket);

            /*printf("Quiero: %i\n",quiero);
            printf("Mi ticket: %i, ticket origen: %i\n",mi_ticket,ticket_origen);
            printf("Mi id: %i, id origen: %i\n",mi_id,id_nodo_origen);*/
            sem_wait(&semaforo_antes_de_entrar);
            if (quiero==0 || (ticket_origen < mi_ticket) || (ticket_origen == mi_ticket && (id_nodo_origen < mi_id)) || (prioridad_origen==4 && consultas_dentro!=0 && num_pend==0 && antes_de_entrar==0))//esto pa varias consultas en diferentes nodos
            {
                //printf("nodo: %i mande oks\n",mi_id);
                sem_post(&semaforo_num_pendientes);
                sem_post(&semaforo_mi_id);
                sem_post(&semaforo_ticket);
                sem_post(&semaforo_quiero);
                sem_post(&semaforo_nodos_pendientes);

                sem_post(&semaforo_antes_de_entrar);
                sem_wait(&semaforo_no_volver_a_permitir_adelantamientos);
                sem_wait(&semaforo_alguien_dentro);
                sem_wait(&semaforo_quiero);
                sem_wait(&semaforo_consultas_esperando_la_seccion_critica);
                if(no_volver_a_permitir_adelantamientos==0 && alguien_dentro==0 && quiero!=0 && consultas_esperando_la_seccion_critica==0){
                    sem_post(&semaforo_consultas_esperando_la_seccion_critica);
                    sem_post(&semaforo_quiero);
                    sem_post(&semaforo_alguien_dentro);
                    sem_post(&semaforo_no_volver_a_permitir_adelantamientos);
                    sem_wait(&semaforo_permitiendo_adelantamiento_de_ronda);
                    permitiendo_adelantamiento_de_ronda = 1;
                    sem_post(&semaforo_permitiendo_adelantamiento_de_ronda);
                }
                else{
                    sem_post(&semaforo_consultas_esperando_la_seccion_critica);
                    sem_post(&semaforo_quiero);
                    sem_post(&semaforo_alguien_dentro);
                    sem_post(&semaforo_no_volver_a_permitir_adelantamientos);
                }
                sem_wait(&semaforo_mi_id);
                respuesta.id_nodo = mi_id;
                sem_post(&semaforo_mi_id);
                respuesta.tipo = 2;
                respuesta.puerto = puerto_origen;
                sem_wait(&semaforo_ticket);
                respuesta.ticket = mi_ticket;
                sem_post(&semaforo_ticket);
                if (msgsnd(id_nodo_origen, &respuesta, longitud_respuesta, 1) == -1) // 1 es no bloqueante
                    exit(-1);
            }
            else
            {   
                //printf("nodo: %i no mande oks\n",mi_id);
                sem_post(&semaforo_num_pendientes);
                sem_post(&semaforo_mi_id);
                sem_post(&semaforo_ticket);
                sem_post(&semaforo_quiero);
                sem_post(&semaforo_nodos_pendientes);

                sem_post(&semaforo_antes_de_entrar);
                sem_wait(&semaforo_primero);
                primero = 1;///
                sem_post(&semaforo_primero);
                sem_wait(&semaforo_repetir);
                repetir = 1;///
                sem_post(&semaforo_repetir);
                sem_wait(&semaforo_esperar);
                esperar = 1;///
                sem_post(&semaforo_esperar);
                sem_wait(&semaforo_no_volver_a_permitir_adelantamientos);
                sem_wait(&semaforo_alguien_dentro);
                if(no_volver_a_permitir_adelantamientos==0 && alguien_dentro==0){
                    sem_post(&semaforo_alguien_dentro);
                    sem_post(&semaforo_no_volver_a_permitir_adelantamientos);
                    sem_wait(&semaforo_permitiendo_adelantamiento_de_ronda);
                    permitiendo_adelantamiento_de_ronda = 1;
                    sem_post(&semaforo_permitiendo_adelantamiento_de_ronda);
                }
                else{
                    sem_post(&semaforo_alguien_dentro);
                    sem_post(&semaforo_no_volver_a_permitir_adelantamientos);
                }
                sem_wait(&semaforo_num_pendientes);
                id_nodos_pendientes[num_pend] = id_nodo_origen;
                puertos_pendientes[num_pend] = puerto_origen;//////se puede quitar
                sem_post(&semaforo_num_pendientes);
                if(prioridad_origen==4){
                    sem_wait(&semaforo_num_pendientes);
                    peticiones_pendientes_de_consultas[num_pend] = 1;
                    sem_post(&semaforo_num_pendientes);
                    sem_wait(&semaforo_oks_a_consultas_ya_mandados);
                    oks_a_consultas_ya_mandados=0;
                    sem_post(&semaforo_oks_a_consultas_ya_mandados);
                }
                sem_wait(&semaforo_num_pendientes);
                num_pend++;
                sem_post(&semaforo_num_pendientes);
            }
        }
        else{
            cuenta++;
            if(cuenta==(diferencia)){//ya recibio todos los oks
                cuenta=0;
                sem_post(&semaforo_primero_dentro);
            }
        }
    }
}

void main(int argc, char *argv[])
{
    struct info_a_mandar_al_hilo info;
    struct info_hilo_proceso info_proceso;
    int i = 0;
    int numero_procesos = 0;
    long prioridad_del_proceso = 0;
    pthread_t thread_receptor;

    // INICIAR SEMAFOROS
    sem_init(&semaforo_quiero, 0, 1);
    sem_init(&semaforo_ticket, 0, 1);
    sem_init(&semaforo_mi_id, 0, 1);
    sem_init(&semaforo_nodos_pendientes, 0, 1);
    sem_init(&semaforo_num_pendientes, 0, 1);
    sem_init(&semaforo_max_ticket, 0, 1);
    sem_init(&semaforo_seccion_critica, 0, 1);
    sem_init(&semaforo_primero_variable, 0, 1);
    sem_init(&semaforo_primero_dentro, 0, 0);
    sem_init(&semaforo_no_hacer_requests, 0, 0);
    sem_init(&semaforo_anulaciones, 0, 0);
    sem_init(&semaforo_administracion_reservas, 0, 0);
    sem_init(&semaforo_pagos, 0, 0);
    sem_init(&semaforo_consultas, 0, 0);
    sem_init(&semaforo_semaforo_de_esperar, 0, 1);
    sem_init(&semaforo_anulaciones_adelantamiento_de_ronda, 0, 0);
    sem_init(&semaforo_pagos_adelantamiento_de_ronda, 0, 0);

    sem_init(&semaforo_primero,0,1);
    sem_init(&semaforo_repetir,0,1);
    sem_init(&semaforo_esperar,0,1);
    sem_init(&semaforo_primer_proceso_general,0,1);
    sem_init(&semaforo_consultas_esperando,0,1);
    sem_init(&semaforo_consultas_dentro,0,1);
    sem_init(&semaforo_antes_de_entrar,0,1);
    sem_init(&semaforo_hizo_requests_consultas,0,1);
    sem_init(&semaforo_permitiendo_adelantamiento_de_ronda,0,1);
    sem_init(&semaforo_no_volver_a_permitir_adelantamientos,0,1);
    sem_init(&semaforo_anulaciones_esperando_a_adelantar,0,1);
    sem_init(&semaforo_pagos_esperando_a_adelantar,0,1);
    sem_init(&semaforo_ultimo_adelantando,0,1);
    sem_init(&semaforo_alguien_dentro,0,1);
    sem_init(&semaforo_oks_a_consultas_ya_mandados,0,1);
    sem_init(&semaforo_esperando,0,1);
    sem_init(&semaforo_consultas_esperando_la_seccion_critica,0,1);
    sem_init(&semaforo_coordinador_main_hilos,0,0);
    sem_init(&semaforo_finalizador,0,0);
    // INICIAR SEMAFOROS

    if (argc != 9) {
        printf("Usage: %s [nodo_minimo] [nodo_maximo] [node_id] [instrucciones] [tiempos] [SC_TIME] [DELAY_TIME] [putput]\n", argv[0]);
        return;
    }

    int nodo_minimo = atoi(argv[1]);
    int nodo_maximo = atoi(argv[2]);
    mi_id = atoi(argv[3]);
    char* instrucciones = argv[4];
    char* tiempos = argv[5];
    float SC_TIME = atof(argv[6]);
    float DELAY_TIME = atof(argv[7]);
    char *output_filename = argv[8];

    /*printf("EN EL MAIN SC TIME: %d\n",SC_TIME );
    printf("EN EL MAIN DELAY TIME: %d\n",DELAY_TIME );*/
    info_proceso.time= SC_TIME;

    int longitud_instrucciones = 0;
    // Array para almacenar las instrucciones como enteros.
    int instr_arr[MAX_INSTR_LENGTH];
    int instr_count = 0;

    // Dividir la cadena de instrucciones en subcadenas separadas por comas y convertirlas en enteros.
    char* token = strtok(instrucciones, ",");
    while (token != NULL && instr_count < MAX_INSTR_LENGTH) {
        // Omitir los espacios en blanco antes del número.
        while (*token== ' ') {
            token++;
        }
        int instr = atoi(token);
        longitud_instrucciones++;
        instr_arr[instr_count++] = instr;
        token = strtok(NULL, ",");
    }

    // Array para almacenar los tiempos como enteros.
    int tiempos_arr[MAX_INSTR_LENGTH];
    int tiempos_count = 0;

    // Dividir la cadena de tiempos en subcadenas separadas por comas y convertirlas en enteros.
    char* token_tiempo = strtok(tiempos, ",");
    while (token_tiempo != NULL && tiempos_count < MAX_INSTR_LENGTH) {
        // Omitir los espacios en blanco antes del número.
        while (*token_tiempo == ' ') {
            token_tiempo++;
        }
        int tiempo = atoi(token_tiempo);
        tiempos_arr[tiempos_count++] = tiempo;
        token_tiempo = strtok(NULL, ",");
    }

    // INICIAR ID_NODOS
    int a = 0;
    for (i = nodo_minimo; i <= nodo_maximo; i++)
    {
        if (i != mi_id)
        {
            id_nodos[a] = i;
            a++;
        }
    }
    diferencia = (nodo_maximo - nodo_minimo); //buzon 0 es de nodo 0 buzon 1 es el buzon interno del nodo 0
    NUM_NODOS = diferencia+1;
    // INICIAR ID_NODOS

    // INICIAR THREADS
    pthread_create(&thread_receptor, NULL, receptor, (void *)&DELAY_TIME);//esto crea el receptor y le pasa la struct con la diferencia
    
    int contador = 0;

    //INICIAR HILOS
    for (int j = 0; j < longitud_instrucciones; j += 4) {
        
        //printf("j: %d\n", j);

        // Extraer cuatro elementos del array de instrucciones.
        int instr1 = instr_arr[j];
        int instr2 = instr_arr[j+1];
        int instr3 = instr_arr[j+2];
        int instr4 = instr_arr[j+3];

        // Extraer un elemento del array de tiempos.
        int tiempo = tiempos_arr[j/4];

        // Mostrar los resultados.
        //printf("Instrucciones: %d %d %d %d, Tiempo: %d\n", instr1, instr2, instr3, instr4, tiempo);
        sleep(tiempo);
        prioridad_del_proceso=1;
        info_proceso.prio=prioridad_del_proceso;
        for(a=0; a<instr1; a++){
            //printf("prioridad: %li\n",prioridad_del_proceso);
            contador++;
            pthread_create(&thread_receptor, NULL, proceso, (void *)&info_proceso);
            puerto=contador;
            sem_wait(&semaforo_coordinador_main_hilos);
        }
        prioridad_del_proceso=2;
        info_proceso.prio=prioridad_del_proceso;
        for(a=0; a<instr2; a++){
            //printf("prioridad: %li\n",prioridad_del_proceso);
            contador++;
            pthread_create(&thread_receptor, NULL, proceso, (void *)&info_proceso);
            puerto=contador;
            sem_wait(&semaforo_coordinador_main_hilos);
        }
        prioridad_del_proceso=3;
        info_proceso.prio=prioridad_del_proceso;
        for(a=0; a<instr3; a++){
            //printf("prioridad: %li\n",prioridad_del_proceso);
            contador++;
            pthread_create(&thread_receptor, NULL, proceso, (void *)&info_proceso);
            puerto=contador;
            sem_wait(&semaforo_coordinador_main_hilos);
        }
        prioridad_del_proceso=4;
        info_proceso.prio=prioridad_del_proceso;
        for(a=0; a<instr4; a++){
            //printf("prioridad: %li\n",prioridad_del_proceso);
            contador++;
            pthread_create(&thread_receptor, NULL, proceso, (void *)&info_proceso);
            puerto=contador;
            sem_wait(&semaforo_coordinador_main_hilos);
        }

    }
    NUM_PROCESOS = contador;
    for(a=0;a<(contador);a++){
        sem_wait(&semaforo_finalizador);
    }
    sleep(0.2*mi_id);
    FILE* log_file = fopen(output_filename,"a");
    if (log_file == NULL) {
        printf("Error al abrir archivo de log\n");
        exit(1);
    }

    for (int i = 0; i < (posicion_en_registro); i++) {
        fprintf(log_file, "%s\n", registros[i]);
    }

    fclose(log_file);
    /*for(a = 0; a<numero_procesos; a++){
        info.puerto = a+1;
        if(a<4){
            info.prioridad = 1;
        }
        else if(a<8 && a>=4){
            info.prioridad = 2;
        }
        else if(a<12 && a>=8){
            info.prioridad = 3;
        }
        else{
            info.prioridad = 4;
        }
        pthread_create(&thread_receptor, NULL, proceso, (void *)&info);
        sleep(1);
    }*/
    printf("Termine\n");
    sleep(400);
    return;
}