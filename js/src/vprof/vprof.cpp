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

#include "VMPI.h"

// Note, this is not supported in configurations with more than one AvmCore running
// in the same process.

#ifdef WIN32
#include "windows.h"
#else
#define __cdecl
#include <stdarg.h>
#include <string.h>
#endif

#include "vprof.h"

#ifndef MIN
#define MIN(x,y) ((x) <= (y) ? x : y)
#endif
#ifndef MAX
#define MAX(x,y) ((x) >= (y) ? x : y)
#endif

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

#if defined(WIN32) && !defined(UNDER_CE)
	static void vprof_printf(const char* format, ...)
	{
		va_list args;
		va_start(args, format);

		char buf[1024];
		vsnprintf(buf, sizeof(buf), format, args);
		
		va_end(args);

		printf(buf);
		::OutputDebugStringA(buf);
	}
#else
	#define vprof_printf printf
#endif

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
    p = s+VMPI_strlen(s)-1;
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
    vprof_printf ("event avg [min : max] total count\n");
    for (e = entries; e; e = e->next) {
        if (e->count == 0) continue;  // ignore entries with zero count.
        vprof_printf ("%s", e->file); 
        if (e->line >= 0) {
            vprof_printf (":%d", e->line);
        } 
        vprof_printf (" %s [%lld : %lld] %lld %lld ", 
                f(((double)e->sum)/((double)e->count)), (long long int)e->min, (long long int)e->max, (long long int)e->sum, (long long int)e->count);
        if (e->h) {
            int j = MAXINT;
            for (j = 0; j < e->h->nbins; j ++) {
                vprof_printf ("(%lld < %lld) ", (long long int)e->h->count[j], (long long int)e->h->lb[j]);
            }
            vprof_printf ("(%lld >= %lld) ", (long long int)e->h->count[e->h->nbins], (long long int)e->h->lb[e->h->nbins-1]);
        }
        if (e->func) {
            int j;
            for (j = 0; j < NUM_EVARS; j++) {
                if (e->ivar[j] != 0) {
                    vprof_printf ("IVAR%d %d ", j, e->ivar[j]);
                }
            }
            for (j = 0; j < NUM_EVARS; j++) {
                if (e->i64var[j] != 0) {
                    vprof_printf ("I64VAR%d %lld ", j, (long long int)e->i64var[j]);
                }
            }
            for (j = 0; j < NUM_EVARS; j++) {
                if (e->dvar[j] != 0) {
                    vprof_printf ("DVAR%d %lf ", j, e->dvar[j]);
                }
            }
        }
        vprof_printf ("\n");
    }
    entries = reverse(entries);
}

inline static entry_t findEntry (char* file, int line)
{
    for (entry_t e =  entries; e; e = e->next) {
        if ((e->line == line) && (VMPI_strcmp (e->file, file) == 0)) {
            return e;
        }
    }
    return NULL;
}

// Initialize the location pointed to by 'id' to a new value profile entry
// associated with 'file' and 'line', or do nothing if already initialized.
// An optional final argument provides a user-defined probe function.

int initValueProfile(void** id, char* file, int line, ...)
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
            e->value = 0;
            e->sum = 0;
            e->count = 0;
            e->min = 0;
            e->max = 0;
            // optional probe function argument
            va_start (va, line);
            e->func = (void (__cdecl*)(void*)) va_arg (va, void*);
            va_end (va);
            e->h = NULL;
            e->genptr = NULL;
            VMPI_memset (&e->ivar,   0, sizeof(e->ivar));
            VMPI_memset (&e->i64var, 0, sizeof(e->i64var));
            VMPI_memset (&e->dvar,   0, sizeof(e->dvar));
            e->next = entries;
            entries = e;
            *id = e;
        }
    DO_UNLOCK (&glock);

    return 0;
}

// Record a value profile event.

int profileValue(void* id, int64_t value)
{
    entry_t e = (entry_t) id;
    long* lock = &(e->lock);
    LOCK (lock);
        e->value = value;
        if (e->count == 0) {
            e->sum = value;
            e->count = 1;
            e->min = value;
            e->max = value;
        } else {
            e->sum += value;
            e->count ++;
            e->min = MIN (e->min, value);
            e->max = MAX (e->max, value);
        }
        if (e->func) e->func (e);
    UNLOCK (lock);

    return 0;
}

// Initialize the location pointed to by 'id' to a new histogram profile entry
// associated with 'file' and 'line', or do nothing if already initialized.

int initHistProfile(void** id, char* file, int line, int nbins, ...)
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
            e->value = 0;
            e->sum = 0;
            e->count = 0;
            e->min = 0;
            e->max = 0;
            e->func = NULL;
            e->h = h = (hist_t) malloc (sizeof(hist));
            n = 1+MAX(nbins,0);
            h->nbins = n-1;
            s = n*sizeof(int64_t);
            lb = (int64_t*) malloc (s);
            h->lb = lb;
            VMPI_memset (h->lb, 0, s);
            h->count = (int64_t*) malloc (s);
            VMPI_memset (h->count, 0, s);

            va_start (va, nbins);
            for (b = 0; b < nbins; b++) {
                //lb[b] = va_arg (va, int64_t);
                lb[b] = va_arg (va, int);
            }
            lb[b] = MAXINT64;
            va_end (va);

            e->genptr = NULL;
            VMPI_memset (&e->ivar,   0, sizeof(e->ivar));
            VMPI_memset (&e->i64var, 0, sizeof(e->i64var));
            VMPI_memset (&e->dvar,   0, sizeof(e->dvar));
            e->next = entries;
            entries = e;
            *id = e;
        }
    DO_UNLOCK (&glock);

    return 0;
}

// Record a histogram profile event.

int histValue(void* id, int64_t value)
{
    entry_t e = (entry_t) id;
    long* lock = &(e->lock);
    hist_t h = e->h;
    int nbins = h->nbins;
    int64_t* lb = h->lb;
    int b;

    LOCK (lock);
        e->value = value;
        if (e->count == 0) {
            e->sum = value;
            e->count = 1;
            e->min = value;
            e->max = value;
        } else {
            e->sum += value;
            e->count ++;
            e->min = MIN (e->min, value);
            e->max = MAX (e->max, value);
        }
        for (b = 0; b < nbins; b ++) {
            if (value < lb[b]) break;
        }
        h->count[b] ++;
    UNLOCK (lock);

    return 0;
}

#if defined(_MSC_VER) && defined(_M_IX86)
uint64_t readTimestampCounter()
{
	// read the cpu cycle counter.  1 tick = 1 cycle on IA32
	_asm rdtsc;
}
#elif defined(__GNUC__) && (__i386__ || __x86_64__)
uint64_t readTimestampCounter()
{
   uint32_t lo, hi;
   __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
   return (uint64_t(hi) << 32) | lo;
}
#else
// add stub for platforms without it, so fat builds don't fail
uint64_t readTimestampCounter() { return 0; }
#endif

