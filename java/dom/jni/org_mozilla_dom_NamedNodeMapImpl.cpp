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
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNode.h"
#include "javaDOMGlobals.h"
#include "nsDOMError.h"
#include "org_mozilla_dom_NamedNodeMapImpl.h"


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
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.getLength: NULL pointer");
    return 0;
  }

  PRUint32 length = 0;
  nsresult rv = map->GetLength(&length);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.getLength: failed", rv);
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
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.getNamedItem: NULL pointer");
    return NULL;
  }
  
  nsIDOMNode* node = nsnull;
  jboolean iscopy = JNI_FALSE;
  const char* name = env->GetStringUTFChars(jname, &iscopy);
  if (!name) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.getNamedItem: GetStringUTFChars failed");
    return NULL;
  }

  nsresult rv = map->GetNamedItem(name, &node);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, name);
  if (NS_FAILED(rv) || !node) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.getNamedItem: failed", rv);
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, node);
}

/*
 * Class:     org_mozilla_dom_NamedNodeMapImpl
 * Method:    item
 * Signature: (I)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NamedNodeMapImpl_item
  (JNIEnv *env, jobject jthis, jint jindex)
{
  if (jindex < 0 || jindex > JavaDOMGlobals::javaMaxInt) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
     ("NamedNodeMap.item: invalid index value (%d)\n", jindex));
    return NULL;
  }

  nsIDOMNamedNodeMap* map = (nsIDOMNamedNodeMap*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!map) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.item: NULL pointer");
    return NULL;
  }
  
  nsIDOMNode* node = nsnull;
  nsresult rv = map->Item((PRUint32) jindex, &node);
  if (NS_FAILED(rv) || !node) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.item: failed", rv);
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, node);
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
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.removeNamedItem: NULL pointer");
    return NULL;
  }
  
  nsIDOMNode* node = nsnull;
  jboolean iscopy = JNI_FALSE;
  const char* name = env->GetStringUTFChars(jname, &iscopy);
  if (!name) {
    JavaDOMGlobals::ThrowException(env,
      "NamedNodeMap.removeNamedItem: GetStringUTFChars failed");
    return NULL;
  }

  nsresult rv = map->RemoveNamedItem(name, &node);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, name);
  if (NS_FAILED(rv) || !node) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_NOT_FOUND_ERR) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.removeNamedItem: failed", rv, exceptionType);
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, node);
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
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.setNamedItem: NULL pointer");
    return NULL;
  }
  
  nsIDOMNode* arg = (nsIDOMNode*)
    env->GetLongField(jarg, JavaDOMGlobals::nodePtrFID);
  if (!arg) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.setNamedItem: NULL item pointer");
    return NULL;
  }

  nsIDOMNode* node = nsnull;
  nsresult rv = map->SetNamedItem(arg, &node);
  if (NS_FAILED(rv) || !node) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR ||
         NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_WRONG_DOCUMENT_ERR ||
         NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.setNamedItem: failed", rv, exceptionType);
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, node);
}

