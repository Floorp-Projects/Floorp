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
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNodeList.h"
#include "nsDOMError.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_ElementImpl.h"


/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    getAttribute
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_ElementImpl_getAttribute
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getAttribute: NULL pointer");
    return NULL;
  }

  const char* cname = NULL;
  jboolean iscopy = JNI_FALSE;
  if (jname) {
    cname = env->GetStringUTFChars(jname, &iscopy);
    if (!cname) {
      JavaDOMGlobals::ThrowException(env,
        "Element.getAttribute: GetStringUTFChars failed");
      return NULL;
    }
  }

  nsString attr;
  nsresult rv = element->GetAttribute(cname, attr);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, cname);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getAttribute: failed", rv);
    return NULL;
  }

  jstring jattr = env->NewString(attr.GetUnicode(), attr.Length());
  if (!jattr) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getAttribute: NewString failed");
    return NULL;
  }

  return jattr;
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    getAttributeNode
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/Attr;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_ElementImpl_getAttributeNode
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getAttributeNode: NULL pointer");
    return NULL;
  }

  const char* cname = NULL;
  jboolean iscopy = JNI_FALSE;
  if (jname) {
    cname = env->GetStringUTFChars(jname, &iscopy);
    if (!cname) {
      JavaDOMGlobals::ThrowException(env,
        "Element.getAttributeNode: GetStringUTFChars failed");
      return NULL;
    }
  }

  nsIDOMAttr* attr = nsnull;
  nsresult rv = element->GetAttributeNode(cname, &attr);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, cname);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getAttributeNode: failed", rv);
    return NULL;
  }
  if (!attr)
    return NULL;

  jobject jattr = env->AllocObject(JavaDOMGlobals::attrClass);
  if (!jattr) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getAttributeNode: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jattr, JavaDOMGlobals::nodePtrFID, (jlong) attr);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getAttributeNode: failed to set node ptr");
    return NULL;
  }

  attr->AddRef();
  return jattr;
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    getElementsByTagName
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/NodeList;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_ElementImpl_getElementsByTagName
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getElementsByTagName: NULL pointer");
    return NULL;
  }

  const char* cname = NULL;
  jboolean iscopy = JNI_FALSE;
  if (jname) {
    cname = env->GetStringUTFChars(jname, &iscopy);
    if (!cname) {
      JavaDOMGlobals::ThrowException(env,
        "Element.getElementsByTagName: GetStringUTFChars failed");
      return NULL;
    }
  }

  nsIDOMNodeList* nodes = nsnull;
  nsresult rv = element->GetElementsByTagName(cname, &nodes);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, cname);
  if (NS_FAILED(rv) || !nodes) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getElementsByTagName: failed", rv);
    return NULL;
  }

  jobject jnodes = env->AllocObject(JavaDOMGlobals::nodeListClass);
  if (!jnodes) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getElementsByTagName: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jnodes, JavaDOMGlobals::nodeListPtrFID, (jlong) nodes);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getElementsByTagName: failed to set node ptr");
    return NULL;
  }

  nodes->AddRef();
  return jnodes;
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    getTagName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_ElementImpl_getTagName
  (JNIEnv *env, jobject jthis)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getTagName: NULL pointer");
    return NULL;
  }

  nsString tagName;
  nsresult rv = element->GetTagName(tagName);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getTagName: failed", rv);
    return NULL;
  }

  jstring jTagName = env->NewString(tagName.GetUnicode(), tagName.Length());
  if (!jTagName) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getTagName: NewString failed");
    return NULL;
  }

  return jTagName;
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    normalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_ElementImpl_normalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element) {
    JavaDOMGlobals::ThrowException(env,
      "Element.normalize: NULL pointer");
    return;
  }

  nsresult rv = element->Normalize();
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "Element.normalize: failed", rv);
    return;
  }
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    removeAttribute
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_ElementImpl_removeAttribute
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element) {
    JavaDOMGlobals::ThrowException(env,
      "Element.removeAttribute: NULL pointer");
    return;
  }

  jboolean iscopy = JNI_FALSE;
  const char* name = env->GetStringUTFChars(jname, &iscopy);
  if (!name) {
    JavaDOMGlobals::ThrowException(env,
      "Element.removeAttribute: GetStringUTFChars failed");
    return;
  }

  nsresult rv = element->RemoveAttribute(name);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, name);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "Element.removeAttribute: failed", rv, exceptionType);
    return;
  }
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    removeAttributeNode
 * Signature: (Lorg/w3c/dom/Attr;)Lorg/w3c/dom/Attr;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_ElementImpl_removeAttributeNode
  (JNIEnv *env, jobject jthis, jobject joldAttr)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element) {
    JavaDOMGlobals::ThrowException(env,
      "Element.removeAttributeNode: NULL pointer");
    return NULL;
  }

  nsIDOMAttr* oldAttr = (nsIDOMAttr*) 
    env->GetLongField(joldAttr, JavaDOMGlobals::nodePtrFID);
  if (!oldAttr) {
    JavaDOMGlobals::ThrowException(env,
      "Element.removeAttributeNode: NULL arg pointer");
    return NULL;
  }

  nsIDOMAttr* ret = nsnull;
  nsresult rv = element->RemoveAttributeNode(oldAttr, &ret);
  if (NS_FAILED(rv) || !ret) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR ||
         NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_NOT_FOUND_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "Element.removeAttributeNode: failed", rv, exceptionType);
    return NULL;
  }

  jobject jattr = env->AllocObject(JavaDOMGlobals::attrClass);
  if (!jattr) {
    JavaDOMGlobals::ThrowException(env,
      "Element.removeAttributeNode: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jattr, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Element.removeAttributeNode: failed to set node ptr");
    return NULL;
  }

  ret->AddRef();
  return jattr;
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    setAttribute
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_ElementImpl_setAttribute
  (JNIEnv *env, jobject jthis, jstring jname, jstring jvalue)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element) {
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttribute: NULL pointer");
    return;
  }

  const char* name = NULL;
  const char* value = NULL;
  jboolean iscopy = JNI_FALSE;
  if (jname) {
    name = env->GetStringUTFChars(jname, &iscopy);
    if (!name) {
      JavaDOMGlobals::ThrowException(env,
        "Element.setAttribute: GetStringUTFChars name failed");
      return;
    }
  }

  jboolean iscopy2 = JNI_FALSE;
  if (jvalue) {
    value = env->GetStringUTFChars(jvalue, &iscopy2);
    if (!value) {
      JavaDOMGlobals::ThrowException(env,
        "Element.setAttribute: GetStringUTFChars name failed");
      return;
    }
  }

  nsresult rv = element->SetAttribute(name, value);
  if (iscopy2 == JNI_TRUE)
    env->ReleaseStringUTFChars(jvalue, value);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, name);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_INVALID_CHARACTER_ERR ||
         NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttribute: failed", rv, exceptionType);
    return;
  }
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    setAttributeNode
 * Signature: (Lorg/w3c/dom/Attr;)Lorg/w3c/dom/Attr;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_ElementImpl_setAttributeNode
  (JNIEnv *env, jobject jthis, jobject jnewAttr)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element) {
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttributeNode: NULL pointer");
    return NULL;
  }

  nsIDOMAttr* newAttr = (nsIDOMAttr*) 
    env->GetLongField(jnewAttr, JavaDOMGlobals::nodePtrFID);
  if (!newAttr) {
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttributeNode: NULL arg pointer");
    return NULL;
  }

  nsIDOMAttr* ret = nsnull;
  nsresult rv = element->SetAttributeNode(newAttr, &ret);
  if (NS_FAILED(rv) || !ret) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR ||
         NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_WRONG_DOCUMENT_ERR ||
         NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttributeNode: failed", rv, exceptionType);
    return NULL;
  }

  jobject jattr = env->AllocObject(JavaDOMGlobals::attrClass);
  if (!jattr) {
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttributeNode: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jattr, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttributeNode: failed to set node ptr");
    return NULL;
  }

  ret->AddRef();
  return jattr;
}

