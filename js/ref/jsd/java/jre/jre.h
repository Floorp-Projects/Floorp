/*
 * @(#)jre.h	1.7 97/05/19 David Connelly
 *
 * Copyright (c) 1997 Sun Microsystems, Inc. All Rights Reserved.
 *
 * Sun grants you ("Licensee") a non-exclusive, royalty free, license to use,
 * modify and redistribute this software in source and binary code form,
 * provided that i) this copyright notice and license appear on all copies of
 * the software; and ii) Licensee does not utilize the software in a manner
 * which is disparaging to Sun.
 *
 * This software is provided "AS IS," without a warranty of any kind. ALL
 * EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN AND ITS LICENSORS SHALL NOT BE
 * LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THE SOFTWARE OR ITS DERIVATIVES. IN NO EVENT WILL SUN OR ITS
 * LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA, OR FOR DIRECT,
 * INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER
 * CAUSED AND REGARDLESS OF THE THEORY OF LIABILITY, ARISING OUT OF THE USE OF
 * OR INABILITY TO USE SOFTWARE, EVEN IF SUN HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * This software is not designed or intended for use in on-line control of
 * aircraft, air traffic, aircraft navigation or aircraft communications; or in
 * the design, construction, operation or maintenance of any nuclear
 * facility. Licensee represents and warrants that it will not use or
 * redistribute the Software for such purposes.
 */

/*
 * Portable JRE support functions.
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
