/* 
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
the License for the specific language governing rights and limitations
under the License.

The Initial Developer of the Original Code is Sun Microsystems,
Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
Inc. All Rights Reserved. 
*/

#include "prlog.h"
#include "nsIDOMDOMImplementation.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_DOMImplementationImpl.h"

/*
 * Class:     org_mozilla_dom_DOMImplementationImpl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_DOMImplementationImpl_finalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDOMImplementation* dom = (nsIDOMDOMImplementation*) 
    env->GetLongField(jthis, JavaDOMGlobals::domImplementationPtrFID);
  if (!dom) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DOMImplementation.finalize: NULL pointer\n"));
    return;
  }

  JavaDOMGlobals::AddToGarbage(dom);
}

/*
 * Class:     org_mozilla_dom_DOMImplementationImpl
 * Method:    hasFeature
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_DOMImplementationImpl_hasFeature
  (JNIEnv *env, jobject jthis, jstring jfeature, jstring jversion)
{
  nsIDOMDOMImplementation* dom = (nsIDOMDOMImplementation*) 
    env->GetLongField(jthis, JavaDOMGlobals::domImplementationPtrFID);
  if (!dom) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DOMImplementation.hasFeature: NULL pointer\n"));
    return JNI_FALSE;
  }

  jboolean iscopy = JNI_FALSE;
  const char* feature = env->GetStringUTFChars(jfeature, &iscopy);
  if (!feature) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMImplementation.hasFeature: GetStringUTFChars feature failed\n"));
    return JNI_FALSE;
  }

  jboolean iscopy2 = JNI_FALSE;
  const char* version = env->GetStringUTFChars(jversion, &iscopy2);
  if (!version) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMImplementation.hasFeature: GetStringUTFChars version failed\n"));
    return JNI_FALSE;
  }

  PRBool ret = PR_FALSE;
  nsresult rv = dom->HasFeature(feature, version, &ret);
  if (iscopy2 == JNI_TRUE)
    env->ReleaseStringUTFChars(jversion, version);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jfeature, feature);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMImplementation.hasFeature: failed (%x)\n", rv));
  }

  return ret == PR_TRUE ? JNI_TRUE : JNI_FALSE;
}
