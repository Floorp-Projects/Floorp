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
#include "xptcall.h"
#include "xptinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Initialize caches for often-used Java classes (all in java.lang).
     * Will return JNI_FALSE if those classes, or their fields 
     * and methods, aren't found.
     * @deprecated
     */
    /* XXX: should be hidden in first call to xpjd_InvokeMethod */
jboolean xpjd_InitJavaCaches(JNIEnv *env);

    /**
     * Uncaches often-used Java classes (e.g. during unload).
     */
    /* XXX: should be part of general unloading mechanism */
void xpjd_FlushJavaCaches(JNIEnv *env);

    /**
     * Convenience method to get the nsIInterfaceInfo for an 
     * interface denoted by the Java IID object `iid'.
     * Will return JNI_FALSE if `iid' isn't a org.mozilla.xpcom.nsID
     * or no info object exists.
     */
jboolean xpjd_GetInterfaceInfo(JNIEnv *env, 
			       jobject iid,
			       nsIInterfaceInfo **info);

    /**
     * Convenience method to get the nsIInterfaceInfo for an 
     * interface denoted by the C++ struct `iid'.
     * Will return JNI_FALSE if `iid' isn't a org.mozilla.xpcom.nsID
     * or no info object exists.
     */
jboolean xpjd_GetInterfaceInfoNative(REFNSIID iid,
				     nsIInterfaceInfo **info);

    /**
     * Invoke a method on 'target' indicated by the 'methodIndex'-th 
     * method in `info', with arguments `args'.
     * Throws a Java exception if `target' can't receive the method, 
     * `methodIndex' is out of bounds, `args' contains too few arguments,
     * or any of the pointer arguments is NULL.
     */
void xpjd_InvokeMethod(JNIEnv *env, 
		       nsISupports *target,
		       nsIInterfaceInfo *info,
		       jint methodIndex,
		       jobjectArray args);

#if 0 /* not implemented yet */

    /**
     * Optimized version of `xpjd_InvokeMethod' to get attributes, or 
     * methods with only one in parameter and no outs or inouts.
     */
jobject xpjd_GetAttribute(JNIEnv *env, 
			  nsISupports *target,
			  nsIInterfaceInfo *info,
			  jint getMethodIndex);

    /**
     * Optimized version of `xpjd_InvokeMethod' to set attributes, or 
     * methods with only one out parameter and no ins or inouts.
     */
void xpjd_SetAttribute(JNIEnv *env, 
		       nsISupports *target,
		       nsIInterfaceInfo *info,
		       jint setMethodIndex,
		       jobject value);

#endif

#ifdef __cplusplus
}
#endif
