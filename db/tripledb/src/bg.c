/* -*- Mode: C; indent-tabs-mode: nil; -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the TripleDB database library.
 *
 * The Initial Developer of the Original Code is
 * Geocast Network Systems.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terry Weissman <terry@mozilla.org>
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

#include "tdbapi.h"
#include "tdbbg.h"

#ifdef TDB_USE_NSPR

#include "tdbtypes.h"

#include "prtypes.h"

#include "prcvar.h"
#include "prlock.h"
#include "prmem.h"
#include "prinrval.h"
#include "prthread.h"

#include "plstr.h"



typedef struct _TDBBGFunc {
    struct _TDBBGFunc* next;
    char* name;
    TDBBG_Function func;
    void* closure;
    TDBInt32 priority;
    PRIntervalTime when;
} TDBBGFunc;

struct _TDBBG {
    PRThread* bgthread;         /* Background, low-priority thread. */
    PRLock* mutex;
    PRCondVar* cvar;            /* Condition variable to wake up the
                                   background thread.  */
    TDBBool killbgthread;        /* If TRUE, then we are waiting for the
                                   background thread to die. */
    TDBBool bgthreadidle;
    TDBBGFunc* firstfunc;
};


static void
backgroundThread(void* closure)
{
    TDBBG* bg = (TDBBG*) closure;
    TDBBGFunc* func;
    TDBBGFunc** ptr;
    TDBBGFunc** which;
    PRIntervalTime now;
    PRIntervalTime delay;
    PR_Lock(bg->mutex);
    while (1) {
        if (bg->killbgthread) break;
        if (bg->firstfunc) {
            now = PR_IntervalNow();
            delay = (bg->firstfunc->when > now) ? 
                (bg->firstfunc->when - now) : 0;
        } else {
            delay = PR_INTERVAL_NO_TIMEOUT;
        }
        if (delay > 0) {
            bg->bgthreadidle = TDB_TRUE;
            PR_NotifyAllCondVar(bg->cvar);
            PR_WaitCondVar(bg->cvar, delay);
            bg->bgthreadidle = TDB_FALSE;
        }
        if (bg->killbgthread) break;
        now = PR_IntervalNow();
        if (bg->firstfunc == NULL || bg->firstfunc->when > now) continue;
        /* Search through anything whose time has come, and find the one with
           the highest priority. */
        which = &(bg->firstfunc);
        for (ptr = which ; *ptr ; ptr = &((*ptr)->next)) {
            if ((*ptr)->when > now) break;
            if ((*ptr)->priority > (*which)->priority) {
                which = ptr;
            }
        }
        func = *which;
        *which = func->next;
        PR_Unlock(bg->mutex);
        (*func->func)(func->closure);
        if (func->name) PR_Free(func->name);
        PR_Free(func);
        PR_Lock(bg->mutex);
    }
    PR_Unlock(bg->mutex);
}


TDBBG*
TDBBG_Open()
{
    TDBBG* bg;
    bg = PR_NEWZAP(TDBBG);
    if (!bg) return NULL;
    
    bg->mutex = PR_NewLock();
    if (!bg->mutex) goto FAIL;

    bg->cvar = PR_NewCondVar(bg->mutex);
    if (!bg->cvar) goto FAIL;

    bg->bgthread =
        PR_CreateThread(PR_SYSTEM_THREAD, backgroundThread, bg,
                        PR_PRIORITY_NORMAL, PR_LOCAL_THREAD,
                        PR_JOINABLE_THREAD, 0);
    if (bg->bgthread == NULL) goto FAIL;
    PR_Lock(bg->mutex);
    while (!bg->bgthreadidle) {
        PR_WaitCondVar(bg->cvar, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(bg->mutex);

    return bg;
 FAIL:
    TDBBG_Close(bg);
    return NULL;
}


TDBStatus
TDBBG_Close(TDBBG* bg)
{
    TDBBGFunc* func;
    tdb_ASSERT(bg != NULL);
    if (!bg) return TDB_FAILURE;
    if (bg->bgthread) {
        PR_Lock(bg->mutex);
        bg->killbgthread = TDB_TRUE;
        PR_NotifyAllCondVar(bg->cvar);
        PR_Unlock(bg->mutex);
        PR_JoinThread(bg->bgthread);
    }
    if (bg->cvar) PR_DestroyCondVar(bg->cvar);
    if (bg->mutex) PR_DestroyLock(bg->mutex);
    while (bg->firstfunc) {
        func = bg->firstfunc;
        bg->firstfunc = func->next;
        PR_Free(func->name);
        PR_Free(func);
    }
    PR_Free(bg);
    return TDB_SUCCESS;
}


TDBStatus
TDBBG_AddFunction(TDBBG* bg, const char* name,
               TDBInt32 secondsToWait, TDBInt32 priority,
               TDBBG_Function f, void* closure)
{
    TDBBGFunc** ptr;
    TDBBGFunc* func;
    if (bg == NULL|| name == NULL || f == NULL) {
        tdb_ASSERT(0);
        return PR_FAILURE;
    }
    func = PR_NEWZAP(TDBBGFunc);
    if (!func) return PR_FAILURE;
    func->name = PL_strdup(name);
    if (!func->name) {
        PR_Free(func->name);
        PR_Free(func);
        return PR_FAILURE;
    }
    func->func = f;
    func->closure = closure;
    func->priority = priority;
    func->when = PR_IntervalNow() + PR_SecondsToInterval(secondsToWait);
    PR_Lock(bg->mutex);
    for (ptr = &(bg->firstfunc) ; *ptr ; ptr = &((*ptr)->next)) {
        if (func->when <= (*ptr)->when) break;
    }
    func->next = *ptr;
    *ptr = func;
    if (bg->firstfunc == func) {
        /* We have a new head function, which probably means that the
           background thread has to sleep less long than it was before,
           so wake it up. */
        PR_NotifyAllCondVar(bg->cvar);
    }
    PR_Unlock(bg->mutex);
    return PR_SUCCESS;
}


PRStatus
TDBBG_RescheduleFunction(TDBBG* bg, const char* name,
                      PRInt32 secondsToWait,
                      PRInt32 priority,
                      TDBBG_Function func, void* closure)
{
    TDBBG_RemoveFunction(bg, name, func, closure);
    return TDBBG_AddFunction(bg, name, secondsToWait, priority, func, closure);
}


PRStatus
TDBBG_RemoveFunction(TDBBG* bg, const char* name, TDBBG_Function f, void* closure)
{
    PRStatus status = PR_FAILURE;
    TDBBGFunc** ptr;
    TDBBGFunc* func;
    if (bg == NULL|| name == NULL || f == NULL) {
        tdb_ASSERT(0);
        return PR_FAILURE;
    }
    PR_Lock(bg->mutex);
    ptr = &(bg->firstfunc);
    while (*ptr) {
        if ((*ptr)->func == f && (*ptr)->closure == closure &&
            strcmp((*ptr)->name, name) == 0) {
            func = *ptr;
            *ptr = func->next;
            PR_Free(func->name);
            PR_Free(func);
            status = PR_SUCCESS;
        } else {
            ptr = &((*ptr)->next);
        }
    }
    PR_Unlock(bg->mutex);
    return status;
}


PRStatus
TDBBG_WaitUntilIdle(TDBBG* bg)
{
    if (bg == NULL) {
        tdb_ASSERT(0);
        return PR_FAILURE;
    }
    PR_Lock(bg->mutex);
    while (!bg->bgthreadidle) {
        PR_WaitCondVar(bg->cvar, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(bg->mutex);
    return PR_SUCCESS;
}
#endif /* TDB_USE_NSPR */
