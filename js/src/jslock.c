/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include "jslock.h"

#ifdef JS_THREADSAFE
#include "prlog.h"
#include "prlock.h"
#include "prthread.h"
#include "prlong.h"

PRLock* js_owner_lock = 0;
PRThread  *js_owner_thread = 0;
unsigned  js_owner_count = 0;

void
js_lock_task(JSTaskState *task)
{
    PRThread *me = PR_GetCurrentThread();

    if (! js_owner_lock) {
      js_owner_lock = PR_NewLock();
    }

    if ( js_owner_thread == me) {
        PR_ASSERT(js_owner_count > 0);
        js_owner_count++;
    }
    else {
        PR_Lock(js_owner_lock);
        PR_ASSERT(js_owner_count == 0);
        js_owner_count = 1;
        js_owner_thread = me;
    }

}

void
js_unlock_task(JSTaskState *task)
{
    if (js_owner_thread != PR_GetCurrentThread())
        return;

    if ( --js_owner_count == 0) {
        js_owner_thread = 0;
        PR_Unlock(js_owner_lock);
    }
    PR_ASSERT(js_owner_count >= 0);
}


/*
PRMonitor *js_owner_monitor;
PRThread  *js_owner_thread;
unsigned  js_owner_count;

void
js_lock_task(JSTaskState *task)
{
    PRThread *t = PR_CurrentThread();

    if (!js_owner_monitor)
        js_owner_monitor = PR_NewNamedMonitor("js-owner-monitor");
    PR_EnterMonitor(js_owner_monitor);

    if (js_owner_thread == t) {
        js_owner_count++;
    } else {
        do {
            if (!js_owner_thread) {
                js_owner_thread = t;
                break;
            }
            PR_Wait(js_owner_monitor, PR_INTERVAL_MAX);
        } while (js_owner_thread != t);
    }
    PR_ExitMonitor(js_owner_monitor);
}

void
js_unlock_task(JSTaskState *task)
{
    PR_ASSERT(PR_CurrentThread() == js_owner_thread);
    PR_EnterMonitor(js_owner_monitor);
    if (js_owner_count) {
        js_owner_count--;
    } else {
        js_owner_thread = NULL;
        PR_Notify(js_owner_monitor);
    }
    PR_ExitMonitor(js_owner_monitor);
}
*/

#ifdef DEBUG
int
js_is_task_locked(JSTaskState *task)
{
    return (PR_CurrentThread() == js_owner_thread);
}
#endif

#endif /* JS_THREADSAFE */
