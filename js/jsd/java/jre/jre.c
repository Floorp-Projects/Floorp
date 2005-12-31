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
