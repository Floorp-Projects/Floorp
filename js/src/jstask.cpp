/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9.1 code, released
 * June 30, 2009.
 *
 * The Initial Developer of the Original Code is
 *   Andreas Gal <gal@mozilla.com>
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "jstask.h"

#ifdef JS_THREADSAFE
static void start(void* arg) {
    ((JSBackgroundThread*)arg)->work();
}

JSBackgroundThread::JSBackgroundThread()
  : thread(NULL), stack(NULL), lock(NULL), wakeup(NULL), shutdown(false)
{
}

JSBackgroundThread::~JSBackgroundThread()
{
    if (wakeup)
        PR_DestroyCondVar(wakeup);
    if (lock)
        PR_DestroyLock(lock);
    /* PR_DestroyThread is not necessary. */
}

bool
JSBackgroundThread::init()
{
    if (!(lock = PR_NewLock()))
        return false;
    if (!(wakeup = PR_NewCondVar(lock)))
        return false;
    thread = PR_CreateThread(PR_USER_THREAD, start, this, PR_PRIORITY_LOW,
                             PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    return !!thread;
}

void
JSBackgroundThread::cancel()
{
    PR_Lock(lock);
    if (shutdown) {
        PR_Unlock(lock);
        return;
    }
    shutdown = true;
    PR_NotifyCondVar(wakeup);
    PR_Unlock(lock);
    PR_JoinThread(thread);
}

void
JSBackgroundThread::work()
{
    PR_Lock(lock);
    while (!shutdown) {
        PR_WaitCondVar(wakeup, PR_INTERVAL_NO_TIMEOUT);
        JSBackgroundTask* t;
        while ((t = stack) != NULL) {
            stack = t->next;
            PR_Unlock(lock);
            t->run();
            delete t;
            PR_Lock(lock);
        }
    }
    PR_Unlock(lock);
}

bool
JSBackgroundThread::busy()
{
    return !!stack; // we tolerate some racing here
}

void
JSBackgroundThread::schedule(JSBackgroundTask* task)
{
    PR_Lock(lock);
    if (shutdown) {
        PR_Unlock(lock);
        task->run();
        delete task;
        return;
    }
    task->next = stack;
    stack = task;
    PR_NotifyCondVar(wakeup);
    PR_Unlock(lock);
}

#endif
