/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *  np2.h $Revision: 3.1 $
 *  Prototypes for functions exported by OJI based libplugin and called by the FEs or other XP libs.
 */

#ifndef _NP2_H
#define _NP2_H
#include "jni.h"
#include "prthread.h"

struct np_instance;
struct nsIPlugin;
struct nsIPluginInstance;
struct nsIPluginInstancePeer;
struct nsISupports;
PR_EXTERN(struct nsIPluginInstance*) NPL_GetOJIPluginInstance(NPEmbeddedApp *embed);
PR_EXTERN(const char *) NPL_GetText(struct nsIPluginInstance *);
PR_EXTERN(jobject) NPL_GetJavaObject(struct nsIPluginInstance *);
PR_EXTERN(void ) NPL_Release(struct nsISupports *);
PR_EXTERN(XP_Bool) NPL_IsJVMAndMochaPrefsEnabled(void);
PR_EXTERN(void)NPL_JSJInit(void);
PR_EXTERN(JNIEnv *)NPL_EnsureJNIExecEnv(PRThread* thread);

#endif /* _NP2_H */

