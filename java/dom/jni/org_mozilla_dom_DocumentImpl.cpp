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
#include "nsIDOMDocument.h"
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
#include "nsDOMError.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_DocumentImpl.h"

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    initialize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_DocumentImpl_initialize
  (JNIEnv *env, jclass)
{
  if (!JavaDOMGlobals::log) {
    JavaDOMGlobals::Initialize(env);
    PR_LOG(JavaDOMGlobals::log, PR_LOG_DEBUG, 
	   ("Document.initialize\n"));
  }
}

/*
 * Class:     org_mozilla_dom_DocumentImpl
 * Method:    createAttribute
 * Signature: (Ljava/lang/String;)Lorg/w3c/dom/Attr;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_createAttribute
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) {
    JavaDOMGlobals::ThrowException(env, 
      "Document.createAttribute: NULL pointer");
    return NULL;
  }

  const char* name = NULL;
  nsIDOMAttr* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  if (jname) {
      name = env->GetStringUTFChars(jname, &iscopy);
      if (!name) {
	  JavaDOMGlobals::ThrowException(env, 
            "Document.createAttribute: GetStringUTFChars failed");
	  return NULL;
      }
  }

  nsresult rv = doc->CreateAttribute(name, &ret);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, name);
  if (NS_FAILED(rv) || !ret) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_INVALID_CHARACTER_ERR) {
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_createCDATASection
  (JNIEnv *env, jobject jthis, jstring jdata)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createCDATASection: NULL pointer");
    return NULL;
  }

  const char* data = NULL;
  nsIDOMCDATASection* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  if (jdata) {
      data = env->GetStringUTFChars(jdata, &iscopy);
      if (!data) {
	  JavaDOMGlobals::ThrowException(env,
            "Document.createCDATASection: GetStringUTFChars failed");
	  return NULL;
      }
  }

  nsresult rv = doc->CreateCDATASection(data, &ret);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jdata, data);
  if (NS_FAILED(rv) || !ret) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_NOT_SUPPORTED_ERR) {
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_createComment
  (JNIEnv *env, jobject jthis, jstring jdata)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createComment: NULL pointer");
    return NULL;
  }

  const char* data = NULL;
  nsIDOMComment* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  if (jdata) {
      data = env->GetStringUTFChars(jdata, &iscopy);
      if (!data) {
	  JavaDOMGlobals::ThrowException(env,
            "Document.createComment: GetStringUTFChars failed");
	  return NULL;
      }
  }

  nsresult rv = doc->CreateComment(data, &ret);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jdata, data);
  if (NS_FAILED(rv) || !ret) {
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_createDocumentFragment
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
  if (NS_FAILED(rv) || !ret) {
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_createElement
  (JNIEnv *env, jobject jthis, jstring jtagName)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createElement: NULL pointer");
    return NULL;
  }

  const char* tagName = NULL;
  nsIDOMElement* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  if (jtagName) {
      tagName = env->GetStringUTFChars(jtagName, &iscopy);
      if (!tagName) {
	  JavaDOMGlobals::ThrowException(env,
            "Document.createElement: GetStringUTFChars failed");
	  return NULL;
      }
  }

  nsresult rv = doc->CreateElement(tagName, &ret);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jtagName, tagName);
  if (NS_FAILED(rv) || !ret) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_INVALID_CHARACTER_ERR) {
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_createEntityReference
  (JNIEnv *env, jobject jthis, jstring jname)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createEntityReference: NULL pointer");
    return NULL;
  }

  const char* name = NULL;
  nsIDOMEntityReference* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  if (jname) {
      name = env->GetStringUTFChars(jname, &iscopy);
      if (!name) {
	  JavaDOMGlobals::ThrowException(env,
            "Document.createEntityReference: GetStringUTFChars failed");
	  return NULL;
      }
  }

  nsresult rv = doc->CreateEntityReference(name, &ret);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, name);
  if (NS_FAILED(rv) || !ret) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_INVALID_CHARACTER_ERR ||
         NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_NOT_SUPPORTED_ERR)) {
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_createProcessingInstruction
  (JNIEnv *env, jobject jthis, jstring jtarget, jstring jdata)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createProcessingInstruction: NULL pointer");
    return NULL;
  }

  const char* target = NULL;
  const char* data = NULL;
  nsIDOMProcessingInstruction* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  jboolean iscopy2 = JNI_FALSE;
  if (jtarget) {
      target = env->GetStringUTFChars(jtarget, &iscopy);
      if (!target) {
	  JavaDOMGlobals::ThrowException(env,
            "Document.createProcessingInstruction: GetStringUTFChars target failed");
	  return NULL;
      }
  }

  if (jdata) {
      data = env->GetStringUTFChars(jdata, &iscopy2);
      if (!data) {
	  JavaDOMGlobals::ThrowException(env,
            "Document.createProcessingInstruction: GetStringUTFChars data failed");
	  return NULL;
      }
  }

  nsresult rv = doc->CreateProcessingInstruction(target, data, &ret);
  if (iscopy2 == JNI_TRUE)
    env->ReleaseStringUTFChars(jdata, data);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jtarget, target);
  if (NS_FAILED(rv) || !ret) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_NOT_SUPPORTED_ERR ||
         NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_INVALID_CHARACTER_ERR)) {
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_createTextNode
  (JNIEnv *env, jobject jthis, jstring jdata)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) {
    JavaDOMGlobals::ThrowException(env,
      "Document.createTextNode: NULL pointer");
    return NULL;
  }

  const char* data = NULL;
  nsIDOMText* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  if (jdata) {
      data = env->GetStringUTFChars(jdata, &iscopy);
      if (!data) {
	  JavaDOMGlobals::ThrowException(env,
            "Document.createAttribute: GetStringUTFChars failed");
	  return NULL;
      }
  }

  nsresult rv = doc->CreateTextNode(data, &ret);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jdata, data);
  if (NS_FAILED(rv) || !ret) {
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_getDoctype
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) 
    return NULL;

  nsIDOMDocumentType* docType = nsnull;
  nsresult rv = doc->GetDoctype(&docType);
  if (NS_FAILED(rv) || !docType) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getDoctype: failed", rv);
    return NULL;
  }

  jobject jDocType = env->AllocObject(JavaDOMGlobals::documentTypeClass);
  jobject jret = env->AllocObject(JavaDOMGlobals::attrClass);
  if (!jret) {
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_getDocumentElement
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_getElementsByTagName
  (JNIEnv *env, jobject jthis, jstring jtagName)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) {
    JavaDOMGlobals::ThrowException(env,
      "Document.getElementsByTagName: NULL pointer");
    return NULL;
  }

  const char* tagName = NULL;
  nsIDOMNodeList* elements = nsnull;
  jboolean iscopy = JNI_FALSE;
  if (jtagName) {
      tagName = env->GetStringUTFChars(jtagName, &iscopy);
      if (!tagName) {
	  JavaDOMGlobals::ThrowException(env,
            "Document.getElementsByTagName: GetStringUTFChars failed");
	  return NULL;
      }
  }

  nsresult rv = doc->GetElementsByTagName(tagName, &elements);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jtagName, tagName);
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
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentImpl_getImplementation
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
