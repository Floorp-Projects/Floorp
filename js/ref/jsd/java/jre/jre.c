/*
 * @(#)jre.c	1.4 97/05/19 David Connelly
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
 * Portable JRE Support functions.
 */

#include <string.h>
#include <stdlib.h>
#include <jni.h>
#include "jre.h"

/*
 * Exits the runtime with the specified error message.
 */
void
JRE_FatalError(JNIEnv *env, const char *msg)
{
    if ((*env)->ExceptionOccurred(env)) {
	(*env)->ExceptionDescribe(env);
    }
    (*env)->FatalError(env, msg);
}

/*
 * Parses a runtime version string. Returns 0 if the successful, otherwise
 * returns -1 if the format of the version string was invalid.
 */
jint
JRE_ParseVersion(const char *ver, char **majorp, char **minorp, char **microp)
{
    int n1 = 0, n2 = 0, n3 = 0;

    sscanf(ver, "%*[0-9]%n.%*[0-9]%n.%*[0-9a-zA-Z]%n", &n1, &n2, &n3);
    if (n1 == 0 || n2 == 0) {
	return -1;
    }
    if (n3 != 0) {
	if (n3 != (int)strlen(ver)) {
	    return -1;
	}
    } else if (n2 != (int)strlen(ver)) {
	return -1;
    }
    *majorp = JRE_Malloc(n1 + 1);
    strncpy(*majorp, ver, n1);
    (*majorp)[n1] = 0;
    *minorp = JRE_Malloc(n2 - n1);
    strncpy(*minorp, ver + n1 + 1, n2 - n1 - 1);
    (*minorp)[n2 - n1 - 1] = 0;
    if (n3 != 0) {
	*microp = JRE_Malloc(n3 - n2);
	strncpy(*microp, ver + n2 + 1, n3 - n2 - 1);
	(*microp)[n3 - n2 - 1] = 0;
    }
    return 0;
}

/*
 * Creates a version number string from the specified major, minor, and
 * micro version numbers.
 */
char *
JRE_MakeVersion(const char *major, const char *minor, const char *micro)
{
    char *ver = 0;

    if (major != 0 && minor != 0) {
	int len = strlen(major) + strlen(minor);
	if (micro != 0) {
	    ver = JRE_Malloc(len + strlen(micro) + 3);
	    sprintf(ver, "%s.%s.%s", major, minor, micro);
	} else {
	    ver = JRE_Malloc(len + 2);
	    sprintf(ver, "%s.%s", major, minor);
	}
    }
    return ver;
}

/*
 * Allocate memory or die.
 */
void *
JRE_Malloc(size_t size)
{
    void *p = malloc(size);
    if (p == 0) {
	perror("malloc");
	exit(1);
    }
    return p;
}
