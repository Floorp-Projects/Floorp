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
#include "nsCOMPtr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIBoxObject.h"
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNodeList.h"
#include "nsDOMError.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_ElementImpl.h"

static PRBool isInterceptableAttr(nsString *attrName);
static jstring handleInterceptableAttr(nsIDOMElement *element,
				       nsString *attrName,
				       JNIEnv *env,
				       jobject jthis);

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    getAttribute
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_ElementImpl_getAttribute
  (JNIEnv *env, jobject jthis, jstring jname)
{
  jstring jattr = nsnull;
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element || !jname) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getAttribute: NULL pointer");
    return NULL;
  }

  nsString* cname = JavaDOMGlobals::GetUnicode(env, jname);
  if (!cname)
    return NULL;

  // Special case intercepts
  if (isInterceptableAttr(cname)) {
      jattr = handleInterceptableAttr(element, cname, env, jthis);
  }
  else {
      nsString attr;
      nsresult rv = element->GetAttribute(*cname, attr);  
      nsMemory::Free(cname);
      
      if (NS_FAILED(rv)) {
	  JavaDOMGlobals::ThrowException(env,
					 "Element.getAttribute: failed", rv);
	  jattr = NULL;
      }
      else {
	  jattr = env->NewString((jchar*) attr.get(), attr.Length());
	  if (!jattr) {
	      JavaDOMGlobals::ThrowException(env,
                "Element.getAttribute: NewString failed");
	      jattr = NULL;
	  }
      }
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
  if (!element || !jname) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getAttributeNode: NULL pointer");
    return NULL;
  }

  nsString* cname = JavaDOMGlobals::GetUnicode(env, jname);
  if (!cname)
    return NULL;

  nsIDOMAttr* attr = nsnull;
  nsresult rv = element->GetAttributeNode(*cname, &attr);  
  nsMemory::Free(cname);

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
  if (!element || !jname) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getElementsByTagName: NULL pointer");
    return NULL;
  }

  nsString* cname = JavaDOMGlobals::GetUnicode(env, jname);
  if (!cname)
    return NULL;

  nsIDOMNodeList* nodes = nsnull;
  nsresult rv = element->GetElementsByTagName(*cname, &nodes);
  nsMemory::Free(cname);

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

  jstring jTagName = env->NewString((jchar*) tagName.get(), tagName.Length());
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
  if (!element || !jname) {
    JavaDOMGlobals::ThrowException(env,
      "Element.removeAttribute: NULL pointer");
    return;
  }

  nsString* name = JavaDOMGlobals::GetUnicode(env, jname);
  if (!name)
    return;

  nsresult rv = element->RemoveAttribute(*name);  
  nsMemory::Free(name);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR) {
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
  if (!element || !joldAttr) {
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
        (rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR ||
         rv == NS_ERROR_DOM_NOT_FOUND_ERR)) {
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
  if (!element || !jname || !jvalue) {
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttribute: NULL pointer");
    return;
  }

  nsString* name = JavaDOMGlobals::GetUnicode(env, jname);
  if (!name)
      return;

  nsString* value = JavaDOMGlobals::GetUnicode(env, jvalue);
  if (!value) {
    nsMemory::Free(name);
    return;
  }

  nsresult rv = element->SetAttribute(*name, *value);
  nsMemory::Free(value);
  nsMemory::Free(name);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (rv == NS_ERROR_DOM_INVALID_CHARACTER_ERR ||
         rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR)) {
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
  if (!element || !jnewAttr) {
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
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR ||
         rv == NS_ERROR_DOM_WRONG_DOCUMENT_ERR ||
         rv == NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttributeNode: failed", rv, exceptionType);
    return NULL;
  }
  if (!ret)
    return NULL;

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

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    getAttributeNS
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_ElementImpl_getAttributeNS
  (JNIEnv *env, jobject jthis, jstring jnamespaceURI, jstring jlocalName)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element || !jnamespaceURI || !jlocalName) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getAttributeNS: NULL pointer");
    return NULL;
  }

  nsString* namespaceURI = JavaDOMGlobals::GetUnicode(env, jnamespaceURI);
  if (!namespaceURI)
      return NULL;

  nsString* localName = JavaDOMGlobals::GetUnicode(env, jlocalName);
  if (!localName) {
      nsMemory::Free(namespaceURI);
      return NULL;
  }

  nsString ret;
  nsresult rv = element->GetAttributeNS(*namespaceURI, *localName, ret);
  nsMemory::Free(namespaceURI);
  nsMemory::Free(localName);

  if (NS_FAILED(rv)) {
      JavaDOMGlobals::ThrowException(env,
	  "Element.getAttributeNS: failed");
      return NULL;
  }

  jstring jret = env->NewString((jchar*) ret.get(), ret.Length());
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getAttributeNS: NewString failed\n"));
  }

  return jret;
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    setAttributeNS
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_ElementImpl_setAttributeNS
  (JNIEnv *env, jobject jthis, jstring jnamespaceURI, jstring jqualifiedName, jstring jvalue)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element || !jnamespaceURI || !jqualifiedName || !jvalue) {
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttributeNS: NULL pointer");
    return;
  }

  nsString* namespaceURI = JavaDOMGlobals::GetUnicode(env, jnamespaceURI);
  if (!namespaceURI)
      return;

  nsString* qualifiedName = JavaDOMGlobals::GetUnicode(env, jqualifiedName);
  if (!qualifiedName) {
      nsMemory::Free(namespaceURI);
      return;
  }

  nsString* value = JavaDOMGlobals::GetUnicode(env, jvalue);
  if (!value) {
      nsMemory::Free(namespaceURI);
      nsMemory::Free(qualifiedName);
      return;
  }

  nsresult rv = element->SetAttributeNS(*namespaceURI, *qualifiedName, *value);
  nsMemory::Free(namespaceURI);
  nsMemory::Free(qualifiedName);
  nsMemory::Free(value);

  if (NS_FAILED(rv)) {
      JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
      if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
	  (rv == NS_ERROR_DOM_INVALID_CHARACTER_ERR ||
	   rv == NS_ERROR_DOM_NAMESPACE_ERR ||
	   rv == NS_ERROR_DOM_WRONG_DOCUMENT_ERR)) {
	  exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
      }
      JavaDOMGlobals::ThrowException(env,
	  "ElementImpl.setAttributeNS: failed", rv, exceptionType);
  }
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    removeAttributeNS
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_ElementImpl_removeAttributeNS
  (JNIEnv *env, jobject jthis, jstring jnamespaceURI, jstring jlocalName)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element || !jnamespaceURI || !jlocalName) {
    JavaDOMGlobals::ThrowException(env,
      "Element.removeAttributeNS: NULL pointer");
    return;
  }

  nsString* namespaceURI = JavaDOMGlobals::GetUnicode(env, jnamespaceURI);
  if (!namespaceURI)
      return;

  nsString* localName = JavaDOMGlobals::GetUnicode(env, jlocalName);
  if (!localName) {
      nsMemory::Free(namespaceURI);
      return;
  }

  nsresult rv = element->RemoveAttributeNS(*namespaceURI, *localName);
  nsMemory::Free(namespaceURI);
  nsMemory::Free(localName);

  if (NS_FAILED(rv)) {
      JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
      if (rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR) {
	  exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
      }
      JavaDOMGlobals::ThrowException(env,
	  "Element.removeAttributeNS: failed", rv, exceptionType);
  }
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    getAttributeNodeNS
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Lorg/w3c/dom/Attr;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_ElementImpl_getAttributeNodeNS
  (JNIEnv *env, jobject jthis, jstring jnamespaceURI, jstring jlocalName)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element || !jnamespaceURI || !jlocalName) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getAttributeNodeNS: NULL pointer");
    return NULL;
  }

  nsString* namespaceURI = JavaDOMGlobals::GetUnicode(env, jnamespaceURI);
  if (!namespaceURI)
      return NULL;

  nsString* localName = JavaDOMGlobals::GetUnicode(env, jlocalName);
  if (!localName) {
      nsMemory::Free(namespaceURI);
      return NULL;
  }

  nsIDOMAttr* attr = nsnull;
  nsresult rv = element->GetAttributeNodeNS(*namespaceURI, *localName, &attr);
  nsMemory::Free(namespaceURI);
  nsMemory::Free(localName);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getAttributeNodeNS: failed", rv);
    return NULL;
  }
  if (!attr)
    return NULL;

  return JavaDOMGlobals::CreateNodeSubtype(env, (nsIDOMNode*)attr);
}


/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    setAttributeNodeNS
 * Signature: (Lorg/w3c/dom/Attr;)Lorg/w3c/dom/Attr;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_ElementImpl_setAttributeNodeNS
  (JNIEnv *env, jobject jthis, jobject jnewAttr)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element || !jnewAttr) {
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttributeNodeNS: NULL pointer");
    return NULL;
  }

  nsIDOMAttr* newAttr = (nsIDOMAttr*) 
    env->GetLongField(jnewAttr, JavaDOMGlobals::nodePtrFID);
  if (!newAttr) {
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttributeNodeNS: NULL arg pointer");
    return NULL;
  }

  nsIDOMAttr* ret = nsnull;
  nsresult rv = element->SetAttributeNodeNS(newAttr, &ret);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR ||
         rv == NS_ERROR_DOM_WRONG_DOCUMENT_ERR ||
         rv == NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "Element.setAttributeNodeNS: failed", rv, exceptionType);
    return NULL;
  }
  if (!ret)
    return NULL;

  return JavaDOMGlobals::CreateNodeSubtype(env, (nsIDOMNode*)ret);
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    getElementsByTagNameNS
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Lorg/w3c/dom/NodeList;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_ElementImpl_getElementsByTagNameNS
  (JNIEnv *env, jobject jthis, jstring jnamespaceURI, jstring jlocalName)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element || !jnamespaceURI || !jlocalName) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getElementsByTagNameNS: NULL pointer");
    return NULL;
  }

  nsString* namespaceURI = JavaDOMGlobals::GetUnicode(env, jnamespaceURI);
  if (!namespaceURI)
      return NULL;

  nsString* localName = JavaDOMGlobals::GetUnicode(env, jlocalName);
  if (!localName) {
      nsMemory::Free(namespaceURI);
      return NULL;
  }

  nsIDOMNodeList* nodes = nsnull;
  nsresult rv = element->GetElementsByTagNameNS(*namespaceURI, *localName, &nodes);
  nsMemory::Free(namespaceURI);
  nsMemory::Free(localName);

  if (NS_FAILED(rv) || !nodes) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getElementsByTagNameNS: failed", rv);
    return NULL;
  }

  jobject jnodes = env->AllocObject(JavaDOMGlobals::nodeListClass);
  if (!jnodes) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getElementsByTagNameNS: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jnodes, JavaDOMGlobals::nodeListPtrFID, (jlong) nodes);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Element.getElementsByTagNameNS: failed to set node ptr");
    return NULL;
  }

  nodes->AddRef();
  return jnodes;
}

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    hasAttribute
 * Signature: (Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_ElementImpl_hasAttribute
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element || !jname) {
    JavaDOMGlobals::ThrowException(env,
      "Element.hasAttribute: NULL pointer");
    return JNI_FALSE;
  }

  nsString* name = JavaDOMGlobals::GetUnicode(env, jname);
  if (!name)
      return JNI_FALSE;

  PRBool hasAttr = PR_FALSE;
  nsresult rv = element->HasAttribute(*name, &hasAttr);
  nsMemory::Free(name);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "Element.hasAttribute: failed", rv);
    return JNI_FALSE;
  }

  return (hasAttr == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}


/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    hasAttributeNS
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_ElementImpl_hasAttributeNS
  (JNIEnv *env, jobject jthis, jstring jnamespaceURI, jstring jlocalName)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element || !jnamespaceURI || !jlocalName) {
    JavaDOMGlobals::ThrowException(env,
      "Element.hasAttributeNS: NULL pointer");
    return JNI_FALSE;
  }

  nsString* namespaceURI = JavaDOMGlobals::GetUnicode(env, jnamespaceURI);
  if (!namespaceURI)
      return JNI_FALSE;

  nsString* localName = JavaDOMGlobals::GetUnicode(env, jlocalName);
  if (!localName) {
      nsMemory::Free(namespaceURI);
      return JNI_FALSE;
  }

  PRBool hasAttr = PR_FALSE;
  nsresult rv = element->HasAttributeNS(*namespaceURI, *localName, &hasAttr);
  nsMemory::Free(namespaceURI);
  nsMemory::Free(localName);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "Element.hasAttributeNS: failed", rv);
    return JNI_FALSE;
  }

  return (hasAttr == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}

static PRBool isInterceptableAttr(nsString *attrName)
{
    PRBool result = PR_FALSE;			
    if (attrName->Equals(NS_LITERAL_STRING("clientX")) ||
	attrName->Equals(NS_LITERAL_STRING("clientY")) ||
	attrName->Equals(NS_LITERAL_STRING("screenX")) ||
	attrName->Equals(NS_LITERAL_STRING("screenY"))
	) {
	result = PR_TRUE;
    }
    return result;
}

static jstring handleInterceptableAttr(nsIDOMElement *element,
				       nsString *attrName,
				       JNIEnv *env,
				       jobject jthis)
{
    if (nsnull == element || 
	nsnull == attrName ||
	nsnull == env ||
	nsnull == jthis) {
	return nsnull;
    }

    jstring result = nsnull;
    nsCOMPtr<nsIDOMDocument> ownerDocument = nsnull;
    nsCOMPtr<nsIDOMNSDocument> nsDocument = nsnull;
    nsCOMPtr<nsIBoxObject> boxObject = nsnull;
    nsresult rv = NS_OK;
    PRInt32 coord = 0;
    PRBool hasValue = PR_FALSE;
    const PRInt32 bufLen = 20;
    char buf[bufLen];
    memset(buf, 0, bufLen);

    rv = element->GetOwnerDocument(getter_AddRefs(ownerDocument));
    if (NS_SUCCEEDED(rv)){
	nsDocument = do_QueryInterface(ownerDocument, &rv);
	if (NS_SUCCEEDED(rv)) {
	    rv = nsDocument->GetBoxObjectFor(element, 
					     getter_AddRefs(boxObject));
	    if (NS_SUCCEEDED(rv)) {
		if (attrName->Equals(NS_LITERAL_STRING("clientX"))) {
		    rv = boxObject->GetX(&coord);
		    if (NS_SUCCEEDED(rv)) {
			hasValue = PR_TRUE;
		    }
		}
		else if (attrName->Equals(NS_LITERAL_STRING("clientY"))) {
		    rv = boxObject->GetY(&coord);
		    if (NS_SUCCEEDED(rv)) {
			hasValue = PR_TRUE;
		    }
		}
		else if (attrName->Equals(NS_LITERAL_STRING("screenX"))) {
		    rv = boxObject->GetScreenX(&coord);
		    if (NS_SUCCEEDED(rv)) {
			hasValue = PR_TRUE;
		    }
		}
		else if (attrName->Equals(NS_LITERAL_STRING("screenY"))) {
		    rv = boxObject->GetScreenY(&coord);
		    if (NS_SUCCEEDED(rv)) {
			hasValue = PR_TRUE;
		    }
		}
	    }
	}
    }
    if (hasValue) {
	DOM_ITOA(coord, buf, 10);
	result = env->NewStringUTF(buf);
    }

    return result;
}
