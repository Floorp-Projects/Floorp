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

/*
 * JavaScript Debugger Native Locking and Threading support stubs
 */

#include "jsd.h"

#ifndef JSD_THREADSAFE
#error jsd_lock.c should not be compiled if JSD_THREADSAFE is not defined
#endif

/*
 * NOTE: These locks must be reentrant in the sense that they support
 * nested calls to lock and unlock.
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

void*
jsd_CurrentThread()
{
    return (void*)1;
}    
