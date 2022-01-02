/*
 *
 *    nrstats.h
 *
 *    $Source: /Users/ekr/tmp/nrappkit-dump/nrappkit/src/stats/nrstats.h,v $
 *    $Revision: 1.4 $
 *    $Date: 2007/06/26 22:37:55 $
 *
 *    API for keeping and sharing statistics
 *
 *
 *    Copyright (C) 2003-2005, Network Resonance, Inc.
 *    Copyright (C) 2006, Network Resonance, Inc.
 *    All Rights Reserved
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions
 *    are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of Network Resonance, Inc. nor the name of any
 *       contributors to this software may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#ifndef __NRSTATS_H__
#define __NRSTATS_H__

#include <sys/types.h>
#ifdef WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif
#include <r_types.h>

#ifndef CAPTURE_USER
#define CAPTURE_USER "pcecap"
#endif

#define NR_MAX_STATS_TYPES         256   /* max number of stats objects */
#define NR_MAX_STATS_TYPE_NAME     26

typedef struct NR_stats_type_ {
      char name[NR_MAX_STATS_TYPE_NAME];
      int (*reset)(void *stats);
      int (*print)(void *stats, char *stat_namespace, void (*output)(void *handle, const char *fmt, ...), void *handle);
      int  (*get_lib_name)(char **libname);
      unsigned int size;
} NR_stats_type;

typedef struct NR_stats_app_ {
        time_t               last_counter_reset;
        time_t               last_restart;
        UINT8                total_restarts;
        char                 version[64];
} NR_stats_app;

extern NR_stats_type *NR_stats_type_app;

/* everything measured in bytes */
typedef struct NR_stats_memory_ {
        UINT8                current_size;
        UINT8                max_size;
        UINT8                in_use;
        UINT8                in_use_max;
} NR_stats_memory;

extern NR_stats_type *NR_stats_type_memory;

/*  all functions below return 0 on success, else they return an
 *  error code */

/* if errprintf is null, warnings and errors will be sent to /dev/null */
extern int NR_stats_startup(char *app_name, char *user_name, void (*errprintf)(void *handle, const char *fmt, ...), void *errhandle);
extern int NR_stats_shutdown(void);
#define NR_STATS_CREATE  (1<<0)
extern int NR_stats_get(char *module_name, NR_stats_type *type, int flag, void **stats);
extern int NR_stats_clear(void *stats);   /* zeroizes */
extern int NR_stats_reset(void *stats);   /* zeros "speedometers" */
extern int NR_stats_register(NR_stats_type *type);
extern int NR_stats_acquire_mutex(void *stats);
extern int NR_stats_release_mutex(void *stats);
// TODO: should _get_names take an app_name argument (0==all)????
extern int NR_stats_get_names(unsigned int *nnames, char ***names);
extern int NR_stats_get_by_name(char *name, NR_stats_type **type, void **stats);
extern int NR_stats_get_lib_name(void *stats, char **lib_name);
extern int NR_stats_rmids(void);

extern char *NR_prefix_to_stats_module(char *prefix);

#define NR_INCREMENT_STAT(stat) do { \
       stat++; if(stat>stat##_max) stat##_max=stat; \
     } while (0)
#define NR_UPDATE_STAT(stat,newval) do { \
       stat=newval; if(stat>stat##_max) stat##_max=stat; \
     } while (0)

#endif
