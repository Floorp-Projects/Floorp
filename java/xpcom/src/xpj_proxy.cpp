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
#include "xpjava.h"

#define JAVA_XPCOBJECT_CLASS "org/mozilla/xpcom/ComObject"

static jclass classComObject = NULL;
static jfieldID ComObject_objptr_ID = NULL;

/* --------- SUPPORT FUNCTIONS ------------- */

/* Because not all platforms convert jlong to "long long" 
 *
 * NOTE: this code was cut&pasted from xpj_XPCMethod.cpp, with tweaks.
 *   Normally I wouldn't do this, but my reasons are:
 *
 *   1. My alternatives were to put it in xpjava.h or xpjava.cpp
 *      I'd like to take stuff *out* of xpjava.h, and putting it 
 *      in xpjava.cpp would preclude inlining.
 *
 *   2. How we map proxies to XPCOM objects is an implementation 
 *      detail, which may change in the future (e.g. an index 
 *      into a proxy table).  Thus ToPtr/ToJLong is only of 
 *      interest to those objects that stuff pointers into jlongs.
 *
 *   3. This allows adaptations to each implementation, for 
 *      type safety (e.g. taking and returning nsISupports*).
 *
 *   -- frankm, 99.09.09
 */
static inline jlong ToJLong(nsISupports *p) {
    jlong result;
    jlong_I2L(result, (int)p);
    return result;
}

static inline nsISupports* ToPtr(jlong l) {
    int result;
    jlong_L2I(result, l);
    return (nsISupports *)result;
}

static inline jboolean xpjp_QueryInterface(nsISupports *object, 
					   REFNSIID iid, 
					   void **instance) {
    assert(object != NULL && instance != NULL);

    if (NS_SUCCEEDED(object->QueryInterface(iid, instance))) {
	return JNI_TRUE;
    }
    return JNI_FALSE;
}

static jboolean xpjp_InitJavaCaches(JNIEnv *env) {
    if (ComObject_objptr_ID == NULL) {
	classComObject = env->FindClass(JAVA_XPCOBJECT_CLASS);
	if (classComObject == NULL) return JNI_FALSE;

	classComObject = (jclass)env->NewGlobalRef(classComObject);
	if (classComObject == NULL) return JNI_FALSE;

	ComObject_objptr_ID = env->GetFieldID(classComObject, "objptr", "J");
	if (ComObject_objptr_ID == NULL) return JNI_FALSE;
    }
    return JNI_TRUE;
}


static jclass xpjp_ClassForInterface(JNIEnv *env, REFNSIID iid) {
#if 0
    // Get info
    jclass result = classComObject; // null?
    nsIInterfaceInfo *info = nsnull;
    char *interface_name;
    char *name_space;
    char *full_classname;
    char *ch;
    nsresult res;

    res = interfaceInfoManager->GetInfoForIID(&iid, &info);

    if (NS_FAILED(res)) {
        cerr << "Failed to find info for " << iid.ToString() << endl;
        goto end;
    }

    // XXX: PENDING: find name_space somehow
    name_space = "org.mozilla.xpcom";
    
    res = info->GetName(&interface_name);

    if (NS_FAILED(res)) {
        cerr << "Failed to find name for " << iid.ToString() << endl;
        goto end;
    }

    // Construct class name
    full_classname = 
	(char*)nsAllocator::Alloc(strlen(interface_name) + strlen(name_space) + 2);

    strcpy(full_classname, name_space);
    ch = full_classname;
    while (ch != '\0') {
	if (*ch == '.') {
	    *ch = '/';
	}
	ch++;
    }
    strcat(full_classname, "/");
    strcat(full_classname, interface_name);

    cerr << "Looking for " << full_classname << endl;

    result = env->FindClass(full_classname);
    // XXX: PENDING: If no such class found, make it

    // XXX: PENDING: Cache result
 end:
    // Cleanup
    nsAllocator::Free(interface_name);
    nsAllocator::Free(full_classname);
    //nsAllocator::Free(name_space);

    return result;
#else
    return classComObject;
#endif
}

/* --------- PROXY API FUNCTIONS ------------- */

jobject xpjp_QueryInterfaceToJava(JNIEnv *env,
				  nsISupports *obj,
				  REFNSIID iid) {
    if (!xpjp_InitJavaCaches(env)) {
	return NULL;
    }

    // XXX: Bad implementation; returns a new proxy every time
    jobject result = 0;
    jmethodID initID = 0;
    jclass proxyClass = xpjp_ClassForInterface(env, iid);
    jobject jiid = ID_NewJavaID(env, &iid);

    assert(proxyClass != 0);

    initID = env->GetMethodID(proxyClass, "<init>", "(J)V");

    if (initID != NULL) {
        result = env->NewObject(proxyClass, initID, ToJLong(obj), jiid);
    }
    else {
        initID = env->GetMethodID(proxyClass, "<init>", "()V");

        result = env->NewObject(proxyClass, initID);
        env->SetLongField(result, ComObject_objptr_ID, ToJLong(obj));
    }

    return result; 
}

jboolean xpjp_QueryInterfaceToXPCOM(JNIEnv *env, 
				    jobject proxy, 
				    REFNSIID iid, 
				    void **instance) {
    nsISupports* xpcobj = xpjp_UnwrapProxy(env, proxy);
    if (xpcobj != NULL) {
	return xpjp_QueryInterface(xpcobj, iid, instance);
    }
    return PR_FALSE;
}

nsISupports* xpjp_UnwrapProxy(JNIEnv *env, jobject proxy) {
    if (!xpjp_InitJavaCaches(env)) {
	return PR_FALSE;
    }
    return ToPtr(env->GetLongField(proxy, ComObject_objptr_ID));
}

void xpjp_SafeAddRef(nsISupports *object) {
    /* XXX: NOT "SAFE" */
    NS_ADDREF(object);
}

void xpjp_SafeRelease(nsISupports *object) {
    /* XXX: NOT "SAFE" */
    NS_RELEASE(object);
}

void xpjp_SafeAddRefProxy(JNIEnv *env, jobject proxy) {
    nsISupports* xpcobj = xpjp_UnwrapProxy(env, proxy);
    if (xpcobj != NULL) {
	xpjp_SafeAddRef(xpcobj);
    }
}

void xpjp_SafeReleaseProxy(JNIEnv *env, jobject proxy) {
    nsISupports* xpcobj = xpjp_UnwrapProxy(env, proxy);
    if (xpcobj != NULL) {
	xpjp_SafeRelease(xpcobj);
    }
}

/* deprecated */
jboolean xpjp_QueryInterfaceForProxyID(jlong ref, 
				       REFNSIID iid, 
				       void **instance) {
    nsISupports *xpcobj = ToPtr(ref);

    if (xpcobj != NULL) {
	return xpjp_QueryInterface(xpcobj, iid, instance);
    }
    return PR_FALSE;
}

/* deprecated */
void xpjp_SafeReleaseProxyID(jlong ref) {
    xpjp_SafeRelease(ToPtr(ref));
}

/* deprecated */
void xpjp_SafeAddRefProxyID(jlong ref) {
    xpjp_SafeAddRef(ToPtr(ref));
}
