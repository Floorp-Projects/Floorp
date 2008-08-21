/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Value-Profiling Utility.
 *
 * The Initial Developer of the Original Code is
 * Intel Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mohammad R. Haghighat [mohammad.r.haghighat@intel.com] 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef WIN32
#include "windows.h"
#else
#define __cdecl
#include <stdarg.h>
#include <string.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "vprof.h"

#define MIN(x,y) ((x) <= (y) ? x : y)
#define MAX(x,y) ((x) >= (y) ? x : y)

#ifndef MAXINT
#define MAXINT int(unsigned(-1)>>1)
#endif

#ifndef MAXINT64
#define MAXINT64 int64_t(uint64_t(-1)>>1)
#endif

#ifndef __STDC_WANT_SECURE_LIB__
#define sprintf_s(b,size,fmt,...) sprintf((b),(fmt),__VA_ARGS__)
#endif

#if THREADED
#define DO_LOCK(lock) Lock(lock); {
#define DO_UNLOCK(lock) }; Unlock(lock)
#else
#define DO_LOCK(lock) { (void)(lock);
#define DO_UNLOCK(lock) }
#endif

#if THREAD_SAFE  
#define LOCK(lock) DO_LOCK(lock)
#define UNLOCK(lock) DO_UNLOCK(lock)
#else
#define LOCK(lock) { (void)(lock);
#define UNLOCK(lock) }
#endif

static entry* entries = NULL;
static bool notInitialized = true;
static long glock = LOCK_IS_FREE;

#define Lock(lock) while (_InterlockedCompareExchange(lock, LOCK_IS_TAKEN, LOCK_IS_FREE) == LOCK_IS_TAKEN){};
#define Unlock(lock) _InterlockedCompareExchange(lock, LOCK_IS_FREE, LOCK_IS_TAKEN);

inline static entry* reverse (entry* s)
{
    entry_t e, n, p;

    p = NULL;
    for (e = s; e; e = n) {
        n = e->next;
        e->next = p;
        p = e;
    }
    
    return p;
}

static char* f (double d)
{
    static char s[80];
    char* p;
    sprintf_s (s, sizeof(s), "%lf", d);
    p = s+strlen(s)-1;
    while (*p == '0') {
        *p = '\0';
        p--;
        if (p == s) break;
    }
    if (*p == '.') *p = '\0';
    return s;
}

static void dumpProfile (void)
{
    entry_t e;

    entries = reverse(entries);
    printf ("event avg [min : max] total count\n");
    for (e = entries; e; e = e->next) {
        printf ("%s", e->file); 
        if (e->line >= 0) {
            printf (":%d", e->line);
        } 
        printf (" %s [%lld : %lld] %lld %lld ", 
                f(((double)e->sum)/((double)e->count)), e->min, e->max, e->sum, e->count);
        if (e->h) {
            int j = MAXINT;
            for (j = 0; j < e->h->nbins; j ++) {
                printf ("(%lld < %lld) ", e->h->count[j], e->h->lb[j]);
            }
            printf ("(%lld >= %lld) ", e->h->count[e->h->nbins], e->h->lb[e->h->nbins-1]);
        }
        if (e->func) {
            int j;
            for (j = 0; j < NUM_EVARS; j++) {
                if (e->ivar[j] != 0) {
                    printf ("IVAR%d %d ", j, e->ivar[j]);
                }
            }
            for (j = 0; j < NUM_EVARS; j++) {
                if (e->i64var[j] != 0) {
                    printf ("I64VAR%d %lld ", j, e->i64var[j]);
                }
            }
            for (j = 0; j < NUM_EVARS; j++) {
                if (e->dvar[j] != 0) {
                    printf ("DVAR%d %lf ", j, e->dvar[j]);
                }
            }
        }
        printf ("\n");
    }
    entries = reverse(entries);
}


int _profileEntryValue (void* id, int64_t value)
{
    entry_t e = (entry_t) id;
    long* lock = &(e->lock);
    LOCK (lock);
       e->value = value;
       e->sum += value;
       e->count ++;
       e->min = MIN (e->min, value);
       e->max = MAX (e->max, value);
       if (e->func) e->func (e);
    UNLOCK (lock);

    return 0;
}

inline static entry_t findEntry (char* file, int line)
{
    for (entry_t e =  entries; e; e = e->next) {
        if ((e->line == line) && (strcmp (e->file, file) == 0)) {
            return e;
        }
    }
    return NULL;
}

int profileValue(void** id, char* file, int line, int64_t value, ...)
{
    DO_LOCK (&glock);
        entry_t e = (entry_t) *id;
        if (notInitialized) {
            atexit (dumpProfile);
            notInitialized = false;
        }

        if (e == NULL) {
            e = findEntry (file, line);
            if (e) {
                *id = e;
            }
        } 

        if (e == NULL) {
            va_list va;
            e = (entry_t) malloc (sizeof(entry));
            e->lock = LOCK_IS_FREE;
            e->file = file;
            e->line = line;
            e->value = value;
            e->sum = value;
            e->count = 1;
            e->min = value;
            e->max = value;

            va_start (va, value);
            e->func = (void (__cdecl*)(void*)) va_arg (va, void*);
            va_end (va);

            e->h = NULL;

            e->genptr = NULL;

            memset (&e->ivar,   0, sizeof(e->ivar));
            memset (&e->i64var, 0, sizeof(e->i64var));
            memset (&e->dvar,   0, sizeof(e->dvar));

            e->next = entries;
            entries = e;

            if (e->func) e->func (e);

            *id = e;
        } else {
            long* lock = &(e->lock);
            LOCK (lock);
                e->value = value;
                e->sum += value;
                e->count ++;
                e->min = MIN (e->min, value);
                e->max = MAX (e->max, value);
                if (e->func) e->func (e);
            UNLOCK (lock);
        }
    DO_UNLOCK (&glock);

    return 0;
}

int _histEntryValue (void* id, int64_t value)
{
    entry_t e = (entry_t) id;
    long* lock = &(e->lock);
    hist_t h = e->h;
    int nbins = h->nbins;
    int64_t* lb = h->lb;
    int b;

    for (b = 0; b < nbins; b ++) {
        if (value < lb[b]) break;
    }

    LOCK (lock);
       e->value = value;
       e->sum += value;
       e->count ++;
       e->min = MIN (e->min, value);
       e->max = MAX (e->max, value);
       h->count[b] ++;
    UNLOCK (lock);

    return 0;
}

int histValue(void** id, char* file, int line, int64_t value, int nbins, ...)
{
    DO_LOCK (&glock);
        entry_t e = (entry_t) *id;
        if (notInitialized) {
            atexit (dumpProfile);
            notInitialized = false;
        }

        if (e == NULL) {
            e = findEntry (file, line);
            if (e) {
                *id = e;
            }
        } 

        if (e == NULL) {
            va_list va;
            hist_t h;
            int b, n, s;
            int64_t* lb;

            e = (entry_t) malloc (sizeof(entry));
            e->lock = LOCK_IS_FREE;
            e->file = file;
            e->line = line;
            e->value = value;
            e->sum = value;
            e->count = 1;
            e->min = value;
            e->max = value;
            e->func = NULL;
            e->h = h = (hist_t) malloc (sizeof(hist));
            n = 1+MAX(nbins,0);
            h->nbins = n-1;
            s = n*sizeof(int64_t);
            lb = (int64_t*) malloc (s);
            h->lb = lb;
            memset (h->lb, 0, s);
            h->count = (int64_t*) malloc (s);
            memset (h->count, 0, s);

            va_start (va, nbins);
            for (b = 0; b < nbins; b++) {
                //lb[b] = va_arg (va, int64_t);
                lb[b] = va_arg (va, int);
            }
            lb[b] = MAXINT64;
            va_end (va);

            for (b = 0; b < nbins; b ++) {
                if (value < lb[b]) break;
            }
            h->count[b] ++;

            e->genptr = NULL;

            memset (&e->ivar,   0, sizeof(e->ivar));
            memset (&e->i64var, 0, sizeof(e->i64var));
            memset (&e->dvar,   0, sizeof(e->dvar));

            e->next = entries;
            entries = e;
            *id = e;
        } else {
            int b;
            long* lock = &(e->lock);
            hist_t h=e->h;
            int64_t* lb = h->lb;
            
            LOCK (lock);
                e->value = value;
                e->sum += value;
                e->count ++;
                e->min = MIN (e->min, value);
                e->max = MAX (e->max, value);
                for (b = 0; b < nbins; b ++) {
                    if (value < lb[b]) break;
                }
                h->count[b] ++;
            UNLOCK (lock);
        }
    DO_UNLOCK (&glock);

    return 0;
}
