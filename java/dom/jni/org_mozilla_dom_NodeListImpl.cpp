/* 
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
*/

#include "prlog.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_NodeListImpl.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

/*
 * Class:     org_mozilla_dom_NodeListImpl
 * Method:    XPCOM_equals
 * Signature: (Ljava/lang/Object;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_NodeListImpl_nativeXPCOM_1equals
  (JNIEnv *env, jobject jthis, jobject jarg)
{
  jboolean b_retFlag = JNI_FALSE;

  nsIDOMNodeList* p_this = 
    (nsIDOMNodeList*) env->GetLongField(jthis, JavaDOMGlobals::nodeListPtrFID);
  if (!p_this) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeList.equals: NULL pointer\n"));
    return b_retFlag;
  }

  nsIDOMNodeList* p_arg = 
    (nsIDOMNodeList*) env->GetLongField(jarg, JavaDOMGlobals::nodeListPtrFID);
  if (!p_arg) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeList.equals: NULL arg pointer\n"));
    return b_retFlag;
  }

  nsISupports* thisSupports = nsnull;
  nsISupports* argSupports = nsnull;

  nsresult rvThis = 
    p_this->QueryInterface(kISupportsIID, (void**)(&thisSupports));
  if (NS_FAILED(rvThis) || !thisSupports) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeList.equals: this->QueryInterface failed (%x)\n", rvThis));
    return b_retFlag; 	
  }

  nsresult rvArg =
    p_arg->QueryInterface(kISupportsIID, (void**)(&argSupports));
  if (NS_FAILED(rvArg) || !argSupports) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeList.equals: arg->QueryInterface failed (%x)\n", rvArg));
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
 * Class:     org_mozilla_dom_NodeListImpl
 * Method:    XPCOM_hashCode
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_NodeListImpl_nativeXPCOM_1hashCode
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNodeList* p_this = 
    (nsIDOMNodeList*) env->GetLongField(jthis, JavaDOMGlobals::nodeListPtrFID);
  if (!p_this) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeList.hashCode: NULL pointer\n"));
    return (jint) 0;
  }

  nsISupports* thisSupports = nsnull;
  nsresult rvThis = 
    p_this->QueryInterface(kISupportsIID, (void**)(&thisSupports));
  if (NS_FAILED(rvThis) || !thisSupports) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeList.hashCode: QueryInterface failed (%x)\n", rvThis));
    return (jint) 0;
  }

  thisSupports->Release();
  return (jint) thisSupports;
}

/*
 * Class:     org_mozilla_dom_NodeListImpl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_NodeListImpl_nativeFinalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNodeList* nodes = (nsIDOMNodeList*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodeListPtrFID);
  if (!nodes) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeList.finalize: NULL pointer\n"));
    return;
  }

  JavaDOMGlobals::AddToGarbage(nodes);
}

/*
 * Class:     org_mozilla_dom_NodeListImpl
 * Method:    getLength
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_NodeListImpl_nativeGetLength
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNodeList* nodes = (nsIDOMNodeList*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodeListPtrFID);
  if (!nodes) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeList.getLength: NULL pointer\n"));
    return 0;
  }

  PRUint32 length = 0;
  nsresult rv = nodes->GetLength(&length);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeList.getLength: failed (%x)\n", rv));
    return 0;
  }

  return (jint) length;
}

/*
 * Class:     org_mozilla_dom_NodeListImpl
 * Method:    item
 * Signature: (I)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeListImpl_nativeItem
  (JNIEnv *env, jobject jthis, jint jindex)
{
  if (jindex < 0 || jindex > JavaDOMGlobals::javaMaxInt) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
     ("NodeList.item: invalid index value (%d)\n", jindex));
    return NULL;
  }

  nsIDOMNodeList* nodes = (nsIDOMNodeList*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodeListPtrFID);
  if (!nodes) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeList.item: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNode* node = NULL;
  nsresult rv = nodes->Item((PRUint32) jindex, &node);
  if (NS_FAILED(rv) || !node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeList.item: failed (%x)\n", rv));
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, node);
}

