/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
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

#ifndef _nsJavaInterfaces_h_
#define _nsJavaInterfaces_h_

#include "jni.h"
#include "nscore.h"

#define MOZILLA_NATIVE(func) Java_org_mozilla_xpcom_internal_MozillaImpl_##func
#define GRE_NATIVE(func) Java_org_mozilla_xpcom_internal_GREImpl_##func
#define XPCOM_NATIVE(func) Java_org_mozilla_xpcom_internal_XPCOMImpl_##func
#define JAVAPROXY_NATIVE(func) \
          Java_org_mozilla_xpcom_internal_XPCOMJavaProxy_##func
#define LOCKPROXY_NATIVE(func) Java_org_mozilla_xpcom_ProfileLock_##func
#define JXUTILS_NATIVE(func) \
          Java_org_mozilla_xpcom_internal_JavaXPCOMMethods_##func


extern "C" NS_EXPORT void JNICALL
MOZILLA_NATIVE(initialize) (JNIEnv* env, jobject);

extern "C" NS_EXPORT void JNICALL
GRE_NATIVE(initEmbedding) (JNIEnv* env, jobject, jobject aLibXULDirectory,
                           jobject aAppDirectory, jobject aAppDirProvider);

extern "C" NS_EXPORT void JNICALL
GRE_NATIVE(termEmbedding) (JNIEnv *env, jobject);

extern "C" NS_EXPORT jobject JNICALL
GRE_NATIVE(lockProfileDirectory) (JNIEnv *, jobject, jobject aDirectory);

extern "C" NS_EXPORT void JNICALL
GRE_NATIVE(notifyProfile) (JNIEnv *env, jobject);

extern "C" NS_EXPORT jobject JNICALL
XPCOM_NATIVE(initXPCOM) (JNIEnv* env, jobject, jobject aMozBinDirectory,
                         jobject aAppFileLocProvider);

extern "C" NS_EXPORT void JNICALL
XPCOM_NATIVE(shutdownXPCOM) (JNIEnv *env, jobject, jobject aServMgr);

extern "C" NS_EXPORT jobject JNICALL
XPCOM_NATIVE(newLocalFile) (JNIEnv *env, jobject, jstring aPath,
                            jboolean aFollowLinks);

extern "C" NS_EXPORT jobject JNICALL
XPCOM_NATIVE(getComponentManager) (JNIEnv *env, jobject);

extern "C" NS_EXPORT jobject JNICALL
XPCOM_NATIVE(getComponentRegistrar) (JNIEnv *env, jobject);

extern "C" NS_EXPORT jobject JNICALL
XPCOM_NATIVE(getServiceManager) (JNIEnv *env, jobject);

extern "C" NS_EXPORT jobject JNICALL
JAVAPROXY_NATIVE(callXPCOMMethod) (JNIEnv *env, jclass that, jobject aJavaProxy,
                                   jstring aMethodName, jobjectArray aParams);

extern "C" NS_EXPORT void JNICALL
JAVAPROXY_NATIVE(finalizeProxy) (JNIEnv *env, jclass that, jobject aJavaProxy);

extern "C" NS_EXPORT jboolean JNICALL
JAVAPROXY_NATIVE(isSameXPCOMObject) (JNIEnv *env, jclass that, jobject aProxy1,
                                     jobject aProxy2);

extern "C" NS_EXPORT void JNICALL
LOCKPROXY_NATIVE(release) (JNIEnv *env, jclass that, jlong aLockObject);

extern "C" NS_EXPORT jlong JNICALL
MOZILLA_NATIVE(getNativeHandleFromAWT) (JNIEnv* env, jobject, jobject widget);

extern "C" NS_EXPORT jlong JNICALL
JXUTILS_NATIVE(wrapJavaObject) (JNIEnv* env, jobject, jobject aJavaObject,
                                jstring aIID);

extern "C" NS_EXPORT jobject JNICALL
JXUTILS_NATIVE(wrapXPCOMObject) (JNIEnv* env, jobject, jlong aXPCOMObject,
                                 jstring aIID);

#endif // _nsJavaInterfaces_h_
