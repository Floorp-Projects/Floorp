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
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMProcessingInstruction.h"
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
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_DocumentImpl_finalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocument* doc = (nsIDOMDocument*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!doc) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Document.finalize: NULL pointer\n"));
    return;
  }

  JavaDOMGlobals::AddToGarbage(doc);
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Document.createAttribute: NULL pointer\n"));
    return NULL;
  }

  nsIDOMAttr* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  const char* name = env->GetStringUTFChars(jname, &iscopy);
  if (!name) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createAttribute: GetStringUTFChars failed\n"));
    return NULL;
  }

  nsresult rv = doc->CreateAttribute(name, &ret);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, name);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createAttribute: error %x\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::attrClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createAttribute: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createAttribute: failed to set node ptr: %x\n", ret));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Document.createCDATASection: NULL pointer\n"));
    return NULL;
  }

  nsIDOMCDATASection* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  const char* data = env->GetStringUTFChars(jdata, &iscopy);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createCDATASection: GetStringUTFChars failed\n"));
    return NULL;
  }

  nsresult rv = doc->CreateCDATASection(data, &ret);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jdata, data);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createCDATASection: error %x\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::cDataSectionClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createCDATASection: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createCDATASection: failed to set node ptr: %x\n", ret));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Document.createComment: NULL pointer\n"));
    return NULL;
  }

  nsIDOMComment* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  const char* data = env->GetStringUTFChars(jdata, &iscopy);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createComment: GetStringUTFChars failed\n"));
    return NULL;
  }

  nsresult rv = doc->CreateComment(data, &ret);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jdata, data);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createComment: error %x\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::commentClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createComment: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createComment: failed to set node ptr: %x\n", ret));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Document.createDocumentFragment: NULL pointer\n"));
    return NULL;
  }

  nsIDOMDocumentFragment* ret = nsnull;
  nsresult rv = doc->CreateDocumentFragment(&ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createDocumentFragment: error %x\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::documentFragmentClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createDocumentFragment: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createDocumentFragment: failed to set node ptr: %x\n", ret));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Document.createElement: NULL pointer\n"));
    return NULL;
  }

  nsIDOMElement* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  const char* tagName = env->GetStringUTFChars(jtagName, &iscopy);
  if (!tagName) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createElement: GetStringUTFChars failed\n"));
    return NULL;
  }

  nsresult rv = doc->CreateElement(tagName, &ret);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jtagName, tagName);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createElement: error %x\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::elementClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createElement: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createElement: failed to set node ptr: %x\n", ret));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Document.createEntityReference: NULL pointer\n"));
    return NULL;
  }

  nsIDOMCDATASection* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  const char* name = env->GetStringUTFChars(jname, &iscopy);
  if (!name) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createEntityReference: GetStringUTFChars failed\n"));
    return NULL;
  }


  nsresult rv = doc->CreateCDATASection(name, &ret);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, name);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createEntityReference: error %x\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::entityReferenceClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createEntityReference: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createEntityReference: failed to set node ptr: %x\n", ret));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Document.createProcessingInstruction: NULL pointer\n"));
    return NULL;
  }

  nsIDOMProcessingInstruction* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  jboolean iscopy2 = JNI_FALSE;
  const char* target = env->GetStringUTFChars(jtarget, &iscopy);
  if (!target) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createProcessingInstruction: GetStringUTFChars target failed\n"));
    return NULL;
  }

  const char* data = env->GetStringUTFChars(jdata, &iscopy2);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createProcessingInstruction: GetStringUTFChars data failed\n"));
    return NULL;
  }

  nsresult rv = doc->CreateProcessingInstruction(target, data, &ret);
  if (iscopy2 == JNI_TRUE)
    env->ReleaseStringUTFChars(jdata, data);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jtarget, target);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createProcessingInstruction: error %x\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::processingInstructionClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createProcessingInstruction: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createProcessingInstruction: failed to set node ptr: %x\n", ret));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Document.createTextNode: NULL pointer\n"));
    return NULL;
  }

  nsIDOMText* ret = nsnull;
  jboolean iscopy = JNI_FALSE;
  const char* data = env->GetStringUTFChars(jdata, &iscopy);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createAttribute: GetStringUTFChars failed\n"));
    return NULL;
  }

  nsresult rv = doc->CreateTextNode(data, &ret);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jdata, data);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createTextNode: error %x\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::textClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createTextNode: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.createTextNode: failed to set node ptr: %x\n", ret));
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
  if (!doc) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getDoctype: NULL pointer\n"));
    return NULL;
  }

  nsIDOMDocumentType* docType = nsnull;
  nsresult rv = doc->GetDoctype(&docType);
  if (NS_FAILED(rv) || !docType) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getDoctype: error %x\n", rv));
    return NULL;
  }

  jobject jDocType = env->AllocObject(JavaDOMGlobals::documentTypeClass);
  jobject jret = env->AllocObject(JavaDOMGlobals::attrClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getDoctype: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jDocType, JavaDOMGlobals::nodePtrFID, (jlong) docType);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getDoctype: failed to set node ptr: %x\n", docType));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getDocumentElement: NULL pointer\n"));
    return NULL;
  }

  nsIDOMElement* element = nsnull;
  nsresult rv = doc->GetDocumentElement(&element);
  if (NS_FAILED(rv) || !element) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getDocumentElement: error %x\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::elementClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getDocumentElement: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) element);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getDocumentElement: failed to set node ptr: %x\n", element));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getElementsByTagName: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNodeList* elements = nsnull;
  jboolean iscopy = JNI_FALSE;
  const char* tagName = env->GetStringUTFChars(jtagName, &iscopy);
  if (!tagName) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getElementsByTagName: GetStringUTFChars failed\n"));
    return NULL;
  }

  nsresult rv = doc->GetElementsByTagName(tagName, &elements);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jtagName, tagName);
  if (NS_FAILED(rv) || !elements) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getElementsByTagName: error %x\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::nodeListClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getElementsByTagName: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) elements);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getElementsByTagName: failed to set node ptr: %x\n", elements));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getImplementation: NULL pointer\n"));
    return NULL;
  }

  nsIDOMDOMImplementation* impl = nsnull;
  nsresult rv = doc->GetImplementation(&impl);
  if (NS_FAILED(rv) || !impl) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getImplementation: error %x\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::domImplementationClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getImplementation: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) impl);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Document.getImplementation: failed to set node ptr: %x\n", impl));
    return NULL;
  }

  impl->AddRef();
  return jret;
}
