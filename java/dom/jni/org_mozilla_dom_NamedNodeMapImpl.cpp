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
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNode.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_NamedNodeMapImpl.h"

/*
 * Class:     org_mozilla_dom_NamedNodeMapImpl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_NamedNodeMapImpl_finalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNamedNodeMap* map = (nsIDOMNamedNodeMap*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!map) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeMap.finalize: NULL pointer\n"));
    return;
  }

  JavaDOMGlobals::AddToGarbage(map);
}

/*
 * Class:     org_mozilla_dom_NamedNodeMapImpl
 * Method:    getLength
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_NamedNodeMapImpl_getLength
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNamedNodeMap* map = (nsIDOMNamedNodeMap*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!map) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeMap.getLength: NULL pointer\n"));
    return 0;
  }

  PRUint32 length = 0;
  nsresult rv = map->GetLength(&length);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeMap.getLength: failed (%x)\n", rv));
    return 0;
  }

  return (jint) length;
}

/*
 * Class:     org_mozilla_dom_NamedNodeMapImpl
 * Method:    getNamedItem
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NamedNodeMapImpl_getNamedItem
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMNamedNodeMap* map = (nsIDOMNamedNodeMap*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!map) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeMap.getNamedItem: NULL pointer\n"));
    return NULL;
  }
  
  nsIDOMNode* node = nsnull;
  jboolean iscopy = JNI_FALSE;
  const char* name = env->GetStringUTFChars(jname, &iscopy);
  if (!name) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeMap.getNamedItem: GetStringUTFChars failed\n"));
    return NULL;
  }

  nsresult rv = map->GetNamedItem(name, &node);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, name);
  if (NS_FAILED(rv) || !node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeMap.getNamedItem: failed (%x)\n", rv));
    return NULL;
  }

  jobject jnode = env->AllocObject(JavaDOMGlobals::nodeClass);
  if (!jnode) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NamedNodeMap.getNamedItem: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jnode, JavaDOMGlobals::nodePtrFID, (jlong) node);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NamedNodeMap.getNamedItem: failed to set node ptr: %x\n", node));
    return NULL;
  }

  node->AddRef();
  return jnode;
}

/*
 * Class:     org_mozilla_dom_NamedNodeMapImpl
 * Method:    item
 * Signature: (I)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NamedNodeMapImpl_item
  (JNIEnv *env, jobject jthis, jint jindex)
{
  nsIDOMNamedNodeMap* map = (nsIDOMNamedNodeMap*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!map) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeMap.item: NULL pointer\n"));
    return NULL;
  }
  
  nsIDOMNode* node = nsnull;
  nsresult rv = map->Item((PRUint32) jindex, &node);
  if (NS_FAILED(rv) || !node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeMap.item: failed (%x)\n", rv));
    return NULL;
  }

  jobject jnode = env->AllocObject(JavaDOMGlobals::nodeClass);
  if (!jnode) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeMap.item: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jnode, JavaDOMGlobals::nodePtrFID, (jlong) node);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NamedNodeMap.item: failed to set node ptr: %x\n", node));
    return NULL;
  }

  node->AddRef();
  return jnode;
}

/*
 * Class:     org_mozilla_dom_NamedNodeMapImpl
 * Method:    removeNamedItem
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NamedNodeMapImpl_removeNamedItem
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMNamedNodeMap* map = (nsIDOMNamedNodeMap*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!map) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeMap.removeNamedItem: NULL pointer\n"));
    return NULL;
  }
  
  nsIDOMNode* node = nsnull;
  jboolean iscopy = JNI_FALSE;
  const char* name = env->GetStringUTFChars(jname, &iscopy);
  if (!name) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NamedNodeMap.removeNamedItem: GetStringUTFChars failed\n"));
    return NULL;
  }

  nsresult rv = map->RemoveNamedItem(name, &node);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, name);
  if (NS_FAILED(rv) || !node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeMap.removeNamedItem: failed (%x)\n", rv));
    return NULL;
  }

  jobject jnode = env->AllocObject(JavaDOMGlobals::nodeClass);
  if (!jnode) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NamedNodeMap.removeNamedItem: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jnode, JavaDOMGlobals::nodePtrFID, (jlong) node);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NamedNodeMap.removeNamedItem: failed to set node ptr: %x\n", node));
    return NULL;
  }

  node->AddRef();
  return jnode;
}

/*
 * Class:     org_mozilla_dom_NamedNodeMapImpl
 * Method:    setNamedItem
 * Signature: (Lorg/w3c/dom/Node;)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NamedNodeMapImpl_setNamedItem
  (JNIEnv *env, jobject jthis, jobject jarg)
{
  nsIDOMNamedNodeMap* map = (nsIDOMNamedNodeMap*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!map) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeMap.setNamedItem: NULL pointer\n"));
    return NULL;
  }
  
  nsIDOMNode* arg = (nsIDOMNode*)
    env->GetLongField(jarg, JavaDOMGlobals::nodePtrFID);
  if (!arg) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("NodeMap.setNamedItem: NULL item pointer\n"));
    return NULL;
  }

  nsIDOMNode* node = nsnull;
  nsresult rv = map->SetNamedItem(arg, &node);
  if (NS_FAILED(rv) || !node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NodeMap.setNamedItem: failed (%x)\n", rv));
    return NULL;
  }

  jobject jnode = env->AllocObject(JavaDOMGlobals::nodeClass);
  if (!jnode) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NamedNodeMap.setNamedItem: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jnode, JavaDOMGlobals::nodePtrFID, (jlong) node);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("NamedNodeMap.setNamedItem: failed to set node ptr: %x\n", node));
    return NULL;
  }

  node->AddRef();
  return jnode;
}

