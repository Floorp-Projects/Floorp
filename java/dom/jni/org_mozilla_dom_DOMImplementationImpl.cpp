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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

/*
 * Class:     org_mozilla_dom_DOMImplementationImpl
 * Method:    XPCOM_equals
 * Signature: (Ljava/lang/Object;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_DOMImplementationImpl_XPCOM_1equals
  (JNIEnv *env, jobject jthis, jobject jarg)
{
  jboolean b_retFlag = JNI_FALSE;

  nsIDOMDOMImplementation* p_this = (nsIDOMDOMImplementation*) 
    env->GetLongField(jthis, JavaDOMGlobals::domImplementationPtrFID);
  if (!p_this) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DOMImplementation.equals: NULL pointer\n"));
    return b_retFlag;
  }

  nsIDOMDOMImplementation* p_arg = (nsIDOMDOMImplementation*) 
    env->GetLongField(jarg, JavaDOMGlobals::domImplementationPtrFID);
  if (!p_arg) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DOMImplementation.equals: NULL arg pointer\n"));
    return b_retFlag;
  }

  nsISupports* thisSupports = nsnull;
  nsISupports* argSupports = nsnull;

  nsresult rvThis = 
    p_this->QueryInterface(kISupportsIID, (void**)(&thisSupports));
  if (NS_FAILED(rvThis) || !thisSupports) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMImplementation.equals: this->QueryInterface failed (%x)\n", 
	    rvThis));
    return b_retFlag; 	
  }

  nsresult rvArg =
    p_arg->QueryInterface(kISupportsIID, (void**)(&argSupports));
  if (NS_FAILED(rvArg) || !argSupports) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMImplementation.equals: arg->QueryInterface failed (%x)\n", 
	    rvArg));
    thisSupports->Release();
    return b_retFlag;
  }

  if (thisSupports == argSupports)
    b_retFlag = JNI_TRUE;
  
  thisSupports->Release();
  argSupports->Release();

  return b_retFlag;
}

/*
 * Class:     org_mozilla_dom_DOMImplementationImpl
 * Method:    XPCOM_hashCode
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_DOMImplementationImpl_XPCOM_1hashCode
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDOMImplementation* p_this = (nsIDOMDOMImplementation*) 
    env->GetLongField(jthis, JavaDOMGlobals::domImplementationPtrFID);
  if (!p_this) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DOMImplementation.hashCode: NULL pointer\n"));
    return (jint) 0;
  }

  nsISupports* thisSupports = nsnull;
  nsresult rvThis = 
    p_this->QueryInterface(kISupportsIID, (void**)(&thisSupports));
  if (NS_FAILED(rvThis) || !thisSupports) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMImplementation.hashCode: QueryInterface failed (%x)\n", 
	    rvThis));
    return (jint) 0;
  }

  thisSupports->Release();
  return (jint) thisSupports;
}

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
