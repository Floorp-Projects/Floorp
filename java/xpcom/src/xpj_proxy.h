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
     * Returns a jobject implementing the interface indicated by `iid',
     * or NULL if `ptr' does not implement that interface.
     * The function wraps the XPCOM object in a Java proxy, unless it 
     * is itself a proxy to a Java object; in that case, it unwraps the 
     * proxy before testing.
     * The refcount of `ptr' will be incremented if it is wrapped.
     */
    /* XXX: should we skip actual QueryInterface if not a proxy? */
jobject xpjp_QueryInterfaceToJava(JNIEnv *env,
				  nsISupports *ptr,
				  REFNSIID iid);

    /**
     * Sets `instance' to an instance of the interface indicated by `iid'.
     * If the Java object is a proxy, the function unwraps it and performs 
     * QueryInterface(); otherwise, it performs QueryInterface() on 
     * `object' and, if that succeeds, wraps it in a C++ proxy.
     * The refcount of `object' will be incremented if it is unwrapped.
     */
jboolean xpjp_QueryInterfaceToXPCOM(JNIEnv *env, 
				    jobject object, 
				    REFNSIID iid, 
				    void **instance);

    /**
     * If `proxy' is a proxy to a nsISupports, returns the 
     * contained pointer and returns true; otherwise, return NULL.
     * Used mainly for proxy implementation; others should 
     * use QueryInterfaceToXPCOM().
     * The refcount of the return value is unchanged.
     */
nsISupports* xpjp_UnwrapProxy(JNIEnv *env, jobject proxy);

    /**
     * Perform ptr->AddRef() in a thread-safe manner.
     */
void xpjp_SafeAddRef(nsISupports *ptr);

    /**
     * Perform ptr->Release() in a thread-safe manner.
     */
void xpjp_SafeRelease(nsISupports *ptr);

    /**
     * Perform AddRef() on `proxy''s underlying object in a 
     * thread-safe manner.  
     * Has no effect if proxy is not a proxy to a ref-counted object.
     */
void xpjp_SafeAddRefProxy(JNIEnv *env, jobject proxy);

    /**
     * Perform Release() on `proxy''s underlying object in a 
     * thread-safe manner.  
     * Has no effect if proxy is not a proxy to a ref-counted object.
     */
void xpjp_SafeReleaseProxy(JNIEnv *env, jobject proxy);


    /* deprecated API */

jboolean xpjp_QueryInterfaceForProxyID(jlong ref, 
				       REFNSIID iid,
				       void **instance);

void xpjp_SafeAddRefProxyID(jlong ref);

void xpjp_SafeReleaseProxyID(jlong ref);

#ifdef __cplusplus
}
#endif
