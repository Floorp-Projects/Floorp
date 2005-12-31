/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
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
 * Portable JRE support functions - pared this down to minimal set I need
 */

#include <stdio.h>
#include <stdlib.h>
#include <jni.h>

#include "jre_md.h"

/*
 * Java runtime settings.
 */
typedef struct JRESettings {
    char *javaHome;	    /* Java home directory */
    char *runtimeLib;	    /* Runtime shared library or DLL */
    char *classPath;	    /* Default class path */
    char *compiler;	    /* Just-in-time (JIT) compiler */
    char *majorVersion;	    /* Major version of runtime */
    char *minorVersion;	    /* Minor version of runtime */
    char *microVersion;	    /* Micro version of runtime */
} JRESettings;

/*
 * JRE functions.
 */
void *JRE_LoadLibrary(const char *path);
void  JRE_UnloadLibrary(void *handle);
jint JRE_GetDefaultJavaVMInitArgs(void *handle, void *vmargsp);
jint JRE_CreateJavaVM(void *handle, JavaVM **vmp, JNIEnv **envp,
		      void *vmargsp);
jint JRE_GetCurrentSettings(JRESettings *set);
jint JRE_GetSettings(JRESettings *set, const char *ver);
jint JRE_GetDefaultSettings(JRESettings *set);
jint JRE_ParseVersion(const char *version,
		      char **majorp, char **minorp, char **microp);
char *JRE_MakeVersion(const char *major, const char *minor, const char *micro);
void *JRE_Malloc(size_t size);
void JRE_FatalError(JNIEnv *env, const char *msg);
char *JRE_GetDefaultRuntimeLib(const char *dir);
char *JRE_GetDefaultClassPath(const char *dir);
