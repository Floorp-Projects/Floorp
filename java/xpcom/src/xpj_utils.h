/*
 * The contents of this file are subject to the Mozilla Public License 
 * Version 1.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for 
 * the specific language governing rights and limitations under the License.
 *
 * Contributors:
 *    Frank Mitchell (frank.mitchell@sun.com)
 */
#include <jni.h>

#include "nsISupports.h"
#include "xptinfo.h"

/*
 * Macros defined in some, but not all, jni_md.h, for 
 * compilers that don't have 64 bit ints.
 */
#ifndef jlong_L2I
#  define jlong_L2I(_i, _l) ((_i) = (_l))
#  define jlong_I2L(_l, _i) ((_l) = (_i))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Start up XPCOM
     * @deprecated
     */
    /* XXX: should move into test directory */
nsresult xpj_InitXPCOM();

    /**
     * Find the method info for method or attribute `methodname'.
     * If an attribute, do not prefix with "get" or "set"; set 
     * `wantsSetter' to select the getter or setter.
     * The contents of `indexPtr' are set to the method's index.
     */
nsresult xpj_GetMethodInfoByName(const nsID *iid, 
				 const char *methodname,
				 PRBool wantSetter, 
				 const nsXPTMethodInfo **miptr,
				 int *indexPtr);

/* Defined in xpj_nsID.cpp */

    /** Create a new Java wrapper for `id' */
jobject  ID_NewJavaID(JNIEnv *env, const nsID* id);

    /** Unwrap an nsID; returns the internal struct, *not a copy* */
nsID* ID_GetNative(JNIEnv *env, jobject self);

    /** Copies `id' to a Java wrapper */
void ID_SetNative(JNIEnv *env, jobject self, nsID* id);

    /** Compares two Java wrappers for an nsID */
jboolean ID_IsEqual(JNIEnv *env, jobject self, jobject other);

#ifdef __cplusplus
}
#endif

