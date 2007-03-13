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
#include "nsIDOM3Document.h"
#include "nsIDOMAttr.h"
#include "nsIDOMComment.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMEntityReference.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMDocumentEvent.h"
#include "nsDOMError.h"
#include "javaDOMGlobals.h"
#include "javaDOMEventsGlobals.h"
#include "org_mozilla_dom_DocumentImpl.h"

static NS_DEFINE_IID(kIDOMDocumentEventIID, NS_IDOMDOCUMENTEVENT_IID);

// undefine macros from WINBASE.H
#ifdef CreateEvent
#undef CreateEvent
#endif

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    createAttribute
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/Attr;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeCreateAttribute
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc || !jname) {
    JavaDOMGlobals::ThrowException(env, 
      "Document.createAttribute: NULL pointer");
    return NULL;
  }

  nsIDOMAttr* ret = nsnull;
  nsString* name = JavaDOMGlobals::GetUnicode(env, jname);
  if (!name)
    return NULL;

  nsresult rv = doc->CreateAttribute(*name, &ret);  
  nsMemory::Free(name);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (rv == NS_ERROR_DOM_INVALID_CHARACTER_ERR) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "Document.createAttribute: failed", rv, exceptionType);

    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::attrClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createAttribute: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createAttribute: failed to set node ptr");
    return NULL;
  }

  ret->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    createCDATASection
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/CDATASection;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeCreateCDATASection
  (JNIEnv *env, jobject jthis, jstring jdata)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc || !jdata) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createCDATASection: NULL pointer");
    return NULL;
  }

  nsIDOMCDATASection* ret = nsnull;
  nsString* data = JavaDOMGlobals::GetUnicode(env, jdata);
  if (!data)
    return NULL;

  nsresult rv = doc->CreateCDATASection(*data, &ret);
  nsMemory::Free(data);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (rv == NS_ERROR_DOM_NOT_SUPPORTED_ERR) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "Document.createCDATASection: failed", rv, exceptionType);
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::cDataSectionClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createCDATASection: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createCDATASection: failed to set node ptr");
    return NULL;
  }

  ret->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    createComment
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/Comment;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeCreateComment
  (JNIEnv *env, jobject jthis, jstring jdata)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc || !jdata) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createComment: NULL pointer");
    return NULL;
  }

  nsIDOMComment* ret = nsnull;
  nsString* data = JavaDOMGlobals::GetUnicode(env, jdata);
  if (!data)
    return NULL;

  nsresult rv = doc->CreateComment(*data, &ret);  
  nsMemory::Free(data);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createComment: failed", rv);
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::commentClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createComment: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createComment: failed to set node ptr");
    return NULL;
  }

  ret->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    createDocumentFragment
 * Signature: ()Lorg/w3c/dom/DocumentFragment;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeCreateDocumentFragment
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createDocumentFragment: NULL pointer");
    return NULL;
  }

  nsIDOMDocumentFragment* ret = nsnull;
  nsresult rv = doc->CreateDocumentFragment(&ret);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createDocumentFragment: failed", rv);
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::documentFragmentClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createDocumentFragment: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createDocumentFragment: failed to set node ptr");
    return NULL;
  }

  ret->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    createElement
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/Element;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeCreateElement
  (JNIEnv *env, jobject jthis, jstring jtagName)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc || !jtagName) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createElement: NULL pointer");
    return NULL;
  }

  nsIDOMElement* ret = nsnull;
  nsString* tagName = JavaDOMGlobals::GetUnicode(env, jtagName);
  if (!tagName)
      return NULL;
  nsresult rv = doc->CreateElement(*tagName, &ret); 
  nsMemory::Free(tagName);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (rv == NS_ERROR_DOM_INVALID_CHARACTER_ERR) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "Document.createElement: failed", rv, exceptionType);

    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::elementClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createElement: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createElement: failed to set node ptr");
    return NULL;
  }

  ret->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    createEntityReference
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/EntityReference;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeCreateEntityReference
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc || !jname) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createEntityReference: NULL pointer");
    return NULL;
  }

  nsIDOMEntityReference* ret = nsnull;
  nsString* name = JavaDOMGlobals::GetUnicode(env, jname);
  if (!name)
    return NULL;

  nsresult rv = doc->CreateEntityReference(*name, &ret);  
  nsMemory::Free(name);

  if (NS_FAILED(rv) || ret == nsnull) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (rv == NS_ERROR_DOM_INVALID_CHARACTER_ERR ||
         rv == NS_ERROR_DOM_NOT_SUPPORTED_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "Document.createEntityReference: failed", rv, exceptionType);
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::entityReferenceClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createEntityReference: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createEntityReference: failed to set node ptr");
    return NULL;
  }

  ret->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    createProcessingInstruction
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Lorg/w3c/dom/ProcessingInstruction;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeCreateProcessingInstruction
  (JNIEnv *env, jobject jthis, jstring jtarget, jstring jdata)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc || !jtarget || !jdata) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createProcessingInstruction: NULL pointer");
    return NULL;
  }

  nsIDOMProcessingInstruction* ret = nsnull;
  
  nsString* target = JavaDOMGlobals::GetUnicode(env, jtarget);
  if (!target)
    return NULL;

  nsString* data = JavaDOMGlobals::GetUnicode(env, jdata);
  if (!data) {
    nsMemory::Free(target);
    return NULL;
  }

  nsresult rv = doc->CreateProcessingInstruction(*target, *data, &ret);
  nsMemory::Free(target);
  nsMemory::Free(data);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (rv == NS_ERROR_DOM_NOT_SUPPORTED_ERR ||
         rv == NS_ERROR_DOM_INVALID_CHARACTER_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "Document.createProcessingInstruction failed", rv, exceptionType);
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::processingInstructionClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createProcessingInstruction: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createProcessingInstruction: failed to set node ptr");
    return NULL;
  }

  ret->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    createTextNode
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/Text;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeCreateTextNode
  (JNIEnv *env, jobject jthis, jstring jdata)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc || !jdata) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createTextNode: NULL pointer");
    return NULL;
  }

  nsIDOMText* ret = nsnull;
  nsString* data = JavaDOMGlobals::GetUnicode(env, jdata);
  if (!data)
    return NULL;

  nsresult rv = doc->CreateTextNode(*data, &ret);  
  nsMemory::Free(data);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createTextNode failed", rv);
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::textClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createTextNode: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createTextNode: failed to set node ptr");
    return NULL;
  }

  ret->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    getDoctype
 * Signature: ()Lorg/w3c/dom/DocumentType;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeGetDoctype
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) 
    return NULL;

  nsIDOMDocumentType* docType = nsnull;
  nsresult rv = doc->GetDoctype(&docType);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getDoctype: failed", rv);
    return NULL;
  }
  if (!docType)
    return NULL;

  jobject jDocType = env->AllocObject(JavaDOMGlobals::documentTypeClass);
  if (!jDocType) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getDoctype: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jDocType, JavaDOMGlobals::nodePtrFID, (jlong) docType);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getDoctype: failed to set node ptr");
    return NULL;
  }

  docType->AddRef();
  return jDocType;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    getDocumentElement
 * Signature: ()Lorg/w3c/dom/Element;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeGetDocumentElement
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getDocumentElement: NULL pointer");
    return NULL;
  }

  nsIDOMElement* element = nsnull;
  nsresult rv = doc->GetDocumentElement(&element);
  if (NS_FAILED(rv) || !element) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getDocumentElement: failed", rv);
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::elementClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getDocumentElement: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) element);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getDocumentElement: failed to set node ptr");
    return NULL;
  }

  element->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    getElementsByTagName
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/NodeList;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeGetElementsByTagName
  (JNIEnv *env, jobject jthis, jstring jtagName)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc || !jtagName) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getElementsByTagName: NULL pointer");
    return NULL;
  }

  nsIDOMNodeList* elements = nsnull;
  nsString* tagName = JavaDOMGlobals::GetUnicode(env, jtagName);
  if (!tagName)
    return NULL;

  nsresult rv = doc->GetElementsByTagName(*tagName, &elements);  
  nsMemory::Free(tagName);

  if (NS_FAILED(rv) || !elements) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getElementsByTagName: failed", rv);
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::nodeListClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getElementsByTagName: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) elements);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getElementsByTagName: failed to set node ptr");
    return NULL;
  }

  elements->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    getImplementation
 * Signature: ()Lorg/w3c/dom/DOMImplementation;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeGetImplementation
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getImplementation: NULL pointer");
    return NULL;
  }

  nsIDOMDOMImplementation* impl = nsnull;
  nsresult rv = doc->GetImplementation(&impl);
  if (NS_FAILED(rv) || !impl) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getImplementation: failed", rv);
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::domImplementationClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getImplementation: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) impl);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getImplementation: failed to set node ptr");
    return NULL;
  }

  impl->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    createEvent
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/events/Event;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeCreateEvent
  (JNIEnv *env, jobject jthis, jstring jtype)
{
  nsISupports *doc = (nsISupports*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc || !jtype) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createEvent: NULL pointer");
    return NULL;
  }

  nsString* type = JavaDOMGlobals::GetUnicode(env, jtype);
  if (!type)
    return NULL;

  nsIDOMDocumentEvent* docEvent = nsnull;
  doc->QueryInterface(kIDOMDocumentEventIID, (void **) &docEvent);
  if(!docEvent) {
      JavaDOMGlobals::ThrowException(env,
	  "Document.createEvent: NULL DOMDocumentEvent pointer");
      return NULL;
  }

  nsIDOMEvent* event = nsnull;
  nsresult rv = docEvent->CreateEvent(*type, &event);
  nsMemory::Free(type);

  if (NS_FAILED(rv) || !event) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createEvent: failed", rv);
    return NULL;
  }

  return JavaDOMEventsGlobals::CreateEventSubtype(env, event);
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    getElementsByTagNameNS
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Lorg/w3c/dom/NodeList;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeGetElementsByTagNameNS
  (JNIEnv *env, jobject jthis, jstring jnamespaceURI, jstring jlocalName)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc || !jnamespaceURI || !jlocalName) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getElementsByTagNameNS: NULL pointer");
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

  nsIDOMNodeList* elements = nsnull;

  nsresult rv = doc->GetElementsByTagNameNS(*namespaceURI, *localName, &elements);  
  nsMemory::Free(namespaceURI);
  nsMemory::Free(localName);

  if (NS_FAILED(rv) || !elements) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getElementsByTagNameNS: failed", rv);
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::nodeListClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getElementsByTagNameNS: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) elements);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getElementsByTagNameNS: failed to set node ptr");
    return NULL;
  }

  elements->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    getElementById
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/Element;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_nativeGetElementById
  (JNIEnv *env, jobject jthis, jstring jelementId)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc || !jelementId) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getElementById: NULL pointer");
    return NULL;
  }

  nsString* elementId = JavaDOMGlobals::GetUnicode(env, jelementId);
  if (!elementId)
    return NULL;

  nsIDOMElement* element = nsnull;
  nsresult rv = doc->GetElementById(*elementId, &element);  
  nsMemory::Free(elementId);

  if (NS_FAILED(rv) || !element) {
    JavaDOMGlobals::ThrowException(env,
				   "Document.getElementById: failed", rv, 
				   JavaDOMGlobals::EXCEPTION_DOM);
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, (nsIDOMNode*)element);
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    getDocumentURI
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_DocumentImpl_nativeGetDocumentURI
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  jstring result = nsnull;
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOM3Document> dom3 = do_QueryInterface(doc, &rv);
  if (NS_FAILED(rv) || !dom3) {
      JavaDOMGlobals::ThrowException(env,
      "Document.getDocumentURI: failed", rv);
      return result;
  }
  nsString ret;
  rv = dom3->GetDocumentURI(ret);
  if (NS_FAILED(rv)) {
      JavaDOMGlobals::ThrowException(env,
      "Document.getDocumentURI: failed", rv);
      return result;
  }
  result = env->NewString((jchar*) ret.get(), ret.Length());
  if (!result) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getDocumentURI: NewString failed\n"));
  }

  return result;
  
  
}
