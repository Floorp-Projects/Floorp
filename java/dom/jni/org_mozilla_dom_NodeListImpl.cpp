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
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_NodeListImpl.h"

/*
 * Class:     org_mozilla_dom_NodeListImpl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_NodeListImpl_finalize
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
JNIEXPORT jint JNICALL Java_org_mozilla_dom_NodeListImpl_getLength
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeListImpl_item
  (JNIEnv *env, jobject jthis, jint jindex)
{
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

  jobject jnode = env->AllocObject(JavaDOMGlobals::nodeClass);
  if (!jnode) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeList.item: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jnode, JavaDOMGlobals::nodePtrFID, (jlong) node);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeList.item: failed to set node ptr: %x\n", node));
    return NULL;
  }

  node->AddRef();
  return jnode;
}

