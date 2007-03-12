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
JNIEXPORT jint JNICALL Java_org_mozilla_dom_NamedNodeMapImpl_nativeGetLength
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNamedNodeMap* map = (nsIDOMNamedNodeMap*) 
    env->GetLongField(jthis, JavaDOMGlobals::namedNodeMapPtrFID);
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NamedNodeMapImpl_nativeGetNamedItem
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMNamedNodeMap* map = (nsIDOMNamedNodeMap*) 
    env->GetLongField(jthis, JavaDOMGlobals::namedNodeMapPtrFID);
  if (!map || !jname) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.getNamedItem: NULL pointer");
    return NULL;
  }
  
  nsIDOMNode* node = nsnull;
  nsString* name = JavaDOMGlobals::GetUnicode(env, jname);
  if (!name)
    return NULL;

  nsresult rv = map->GetNamedItem(*name, &node);
  nsMemory::Free(name);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.getNamedItem: failed", rv);
    return NULL;
  }
  if (!node)
    return NULL;

  return JavaDOMGlobals::CreateNodeSubtype(env, node);
}

/*
 * Class:     org_mozilla_dom_NamedNodeMapImpl
 * Method:    item
 * Signature: (I)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NamedNodeMapImpl_nativeItem
  (JNIEnv *env, jobject jthis, jint jindex)
{
  if (jindex < 0 || jindex > JavaDOMGlobals::javaMaxInt) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
     ("NamedNodeMap.item: invalid index value (%d)\n", jindex));
    return NULL;
  }

  nsIDOMNamedNodeMap* map = (nsIDOMNamedNodeMap*) 
    env->GetLongField(jthis, JavaDOMGlobals::namedNodeMapPtrFID);
  if (!map) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.item: NULL pointer");
    return NULL;
  }
  
  nsIDOMNode* node = nsnull;
  nsresult rv = map->Item((PRUint32) jindex, &node);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.item: failed", rv);
    return NULL;
  }
  if (!node)
    return NULL;

  return JavaDOMGlobals::CreateNodeSubtype(env, node);
}

/*
 * Class:     org_mozilla_dom_NamedNodeMapImpl
 * Method:    removeNamedItem
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NamedNodeMapImpl_nativeRemoveNamedItem
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMNamedNodeMap* map = (nsIDOMNamedNodeMap*) 
    env->GetLongField(jthis, JavaDOMGlobals::namedNodeMapPtrFID);
  if (!map || !jname) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.removeNamedItem: NULL pointer");
    return NULL;
  }
  
  nsIDOMNode* node = nsnull;
  nsString* name = JavaDOMGlobals::GetUnicode(env, jname);
  if (!name)
    return NULL;

  nsresult rv = map->RemoveNamedItem(*name, &node);
  nsMemory::Free(name);

  if (NS_FAILED(rv) || !node) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (rv == NS_ERROR_DOM_NOT_FOUND_ERR) {
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NamedNodeMapImpl_nativeSetNamedItem
  (JNIEnv *env, jobject jthis, jobject jarg)
{
  nsIDOMNamedNodeMap* map = (nsIDOMNamedNodeMap*) 
    env->GetLongField(jthis, JavaDOMGlobals::namedNodeMapPtrFID);
  if (!map || !jarg) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.setNamedItem: NULL pointer");
    return NULL;
  }
  
  nsIDOMNode* arg = (nsIDOMNode*)
    env->GetLongField(jarg, JavaDOMGlobals::namedNodeMapPtrFID);
  if (!arg) {
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.setNamedItem: NULL item pointer");
    return NULL;
  }

  nsIDOMNode* node = nsnull;
  nsresult rv = map->SetNamedItem(arg, &node);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR ||
         rv == NS_ERROR_DOM_WRONG_DOCUMENT_ERR ||
         rv == NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "NodeMap.setNamedItem: failed", rv, exceptionType);
    return NULL;
  }
  if (!node)
    return NULL;

  return JavaDOMGlobals::CreateNodeSubtype(env, node);
}

