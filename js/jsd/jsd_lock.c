/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * JavaScript Debugging support - Locking and threading support
 */

/*                                                                           
* ifdef JSD_USE_NSPR_LOCKS then you musat build and run against NSPR2.       
* Otherwise, there are stubs that can be filled in with your own locking     
* code. Also, note that these stubs include a jsd_CurrentThread()            
* implementation that only works on Win32 - this is needed for the inprocess 
* Java-based debugger.                                                       
*/                                                                           

#include "jsd.h"

#ifdef JSD_THREADSAFE

#ifdef JSD_USE_NSPR_LOCKS

#include "prlock.h"
#include "prthread.h"

#ifdef JSD_ATTACH_THREAD_HACK
#include "pprthred.h"   /* need this as long as JS_AttachThread is needed */
#endif

struct JSDStaticLock
{
    void*     owner;
    PRLock*   lock;
    int       count;
#ifdef DEBUG
    uint16    sig;
#endif
};

/* 
 * This exists to wrap non-NSPR theads (e.g. Java threads) in NSPR wrappers.
 * XXX We ignore the memory leak issue.
 * It is claimed that future versions of NSPR will automatically wrap on 
 * the call to PR_GetCurrentThread.
 *
 * XXX We ignore the memory leak issue - i.e. we never call PR_DetachThread.
 *
 */
#undef _CURRENT_THREAD
#ifdef JSD_ATTACH_THREAD_HACK
#define _CURRENT_THREAD(out)                                                  \
JS_BEGIN_MACRO                                                                \
    out = (void*) PR_GetCurrentThread();                                      \
    if(!out)                                                                  \
        out = (void*) JS_AttachThread(PR_USER_THREAD,PR_PRIORITY_NORMAL,NULL);\
    JS_ASSERT(out);                                                           \
JS_END_MACRO
#else
#define _CURRENT_THREAD(out)             \
JS_BEGIN_MACRO                           \
    out = (void*) PR_GetCurrentThread(); \
    JS_ASSERT(out);                      \
JS_END_MACRO
#endif

#ifdef DEBUG
#define JSD_LOCK_SIG 0x10CC10CC
void ASSERT_VALID_LOCK(JSDStaticLock* lock)
{
    JS_ASSERT(lock);
    JS_ASSERT(lock->lock);
    JS_ASSERT(lock->count >= 0);
    JS_ASSERT((! lock->count && ! lock->owner) || (lock->count && lock->owner));
    JS_ASSERT(lock->sig == (uint16) JSD_LOCK_SIG);
}    
#else
#define ASSERT_VALID_LOCK(x) ((void)0)
#endif

void*
jsd_CreateLock()
{
    JSDStaticLock* lock;

    if( ! (lock = calloc(1, sizeof(JSDStaticLock))) || 
        ! (lock->lock = PR_NewLock()) )
    {
        if(lock)
        {
            free(lock);
            lock = NULL;
        }
    }
#ifdef DEBUG
    if(lock) lock->sig = (uint16) JSD_LOCK_SIG;
#endif
    return lock;
}    

void
jsd_Lock(JSDStaticLock* lock)
{
    void* me;

    _CURRENT_THREAD(me);

    ASSERT_VALID_LOCK(lock);

    if(lock->owner == me)
        lock->count++;
    else
    {
        PR_Lock(lock->lock);            /* this can block... */
        JS_ASSERT(lock->owner == 0);
        lock->count = 1;
        lock->owner = me;
    }
    ASSERT_VALID_LOCK(lock);
}    

void
jsd_Unlock(JSDStaticLock* lock)
{
    void* me;

    ASSERT_VALID_LOCK(lock);
    _CURRENT_THREAD(me);

    if(lock->owner != me)
    {
        JS_ASSERT(0);   /* it's an error to unlock a lock you don't own */
        return;
    }
    if(--lock->count == 0)
    {
        lock->owner = NULL;
        PR_Unlock(lock->lock);
    }
    ASSERT_VALID_LOCK(lock);
}    

#ifdef DEBUG
JSBool
jsd_IsLocked(JSDStaticLock* lock)
{
    void* me;
    ASSERT_VALID_LOCK(lock);
    _CURRENT_THREAD(me);
    return lock->owner == me ? JS_TRUE : JS_FALSE;
}    
#endif /* DEBUG */

void*
jsd_CurrentThread()
{
    void* me;
    _CURRENT_THREAD(me);
    return me;
}    


#else  /* ! JSD_USE_NSPR_LOCKS */

#ifdef WIN32    
#pragma message("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
#pragma message("!! you are compiling the stubbed version of jsd_lock.c !!")
#pragma message("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
#endif

/*
 * NOTE: 'Real' versions of these locks must be reentrant in the sense that 
 * they support nested calls to lock and unlock. 
 */

void*
jsd_CreateLock()
{
    return (void*)1;
}    

void
jsd_Lock(void* lock)
{
}    

void
jsd_Unlock(void* lock)
{
}    

#ifdef DEBUG
JSBool
jsd_IsLocked(void* lock)
{
    return JS_TRUE;
}    
#endif /* DEBUG */

/* 
 * This Windows only thread id code is here to allow the Java-based 
 * JSDebugger to work with the single threaded js.c shell (even without 
 * real locking and threading support).
 */

#ifdef WIN32    
/* bogus (but good enough) declaration*/
extern void* __stdcall GetCurrentThreadId(void);
#endif

void*
jsd_CurrentThread()
{
#ifdef WIN32    
    return GetCurrentThreadId();
#else
    return (void*)1;
#endif
}    

#endif /* JSD_USE_NSPR_LOCKS */

#endif /* JSD_THREADSAFE */
