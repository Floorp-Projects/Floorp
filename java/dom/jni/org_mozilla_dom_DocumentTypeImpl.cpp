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
#include "nsIDOMDocumentType.h"
#include "nsIDOMNamedNodeMap.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_DocumentTypeImpl.h"


/*
 * Class:     org_mozilla_dom_DocumentTypeImpl
 * Method:    getEntities
 * Signature: ()Lorg/w3c/dom/NamedNodeMap;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentTypeImpl_getEntities
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocumentType* docType = (nsIDOMDocumentType*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!docType) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DocumentType.getEntities: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNamedNodeMap* nodeMap = nsnull;
  nsresult rv = docType->GetEntities(&nodeMap);
  if (NS_FAILED(rv) || !nodeMap) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getEntities: failed (%x)\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::namedNodeMapClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getEntities: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::namedNodeMapPtrFID, (jlong) nodeMap);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getEntities: failed to set node ptr: %x\n", nodeMap));
    return NULL;
  }

  nodeMap->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentTypeImpl
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_DocumentTypeImpl_getName
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocumentType* docType = (nsIDOMDocumentType*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!docType) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DocumentType.getName: NULL pointer\n"));
    return NULL;
  }

  nsString ret;
  nsresult rv = docType->GetName(ret);
  if (NS_FAILED(rv)) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getName: failed (%x)\n", rv));
    return NULL;
  }

  jstring jret = env->NewString(ret.GetUnicode(), ret.Length());
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getName: NewString failed\n"));
  }

  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentTypeImpl
 * Method:    getNotations
 * Signature: ()Lorg/w3c/dom/NamedNodeMap;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DocumentTypeImpl_getNotations
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocumentType* docType = (nsIDOMDocumentType*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!docType) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DocumentType.getNotations: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNamedNodeMap* nodeMap = nsnull;
  nsresult rv = docType->GetNotations(&nodeMap);
  if (NS_FAILED(rv) || !nodeMap) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getNotations: failed (%x)\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::namedNodeMapClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getNotations: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::namedNodeMapPtrFID, (jlong) nodeMap);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getNotations: failed to set node ptr: %x\n", nodeMap));
    return NULL;
  }

  nodeMap->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentTypeImpl
 * Method:    getPublicId
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_DocumentTypeImpl_getPublicId
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocumentType* docType = (nsIDOMDocumentType*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!docType) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DocumentType.getPublicId: NULL pointer\n"));
    return NULL;
  }

  nsString ret;
  nsresult rv = docType->GetPublicId(ret);
  if (NS_FAILED(rv)) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getPublicId: failed (%x)\n", rv));
    return NULL;
  }

  jstring jret = env->NewString(ret.GetUnicode(), ret.Length());
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getPublicId: NewString failed\n"));
  }

  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentTypeImpl
 * Method:    getSystemId
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_DocumentTypeImpl_getSystemId
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocumentType* docType = (nsIDOMDocumentType*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!docType) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DocumentType.getSystemId: NULL pointer\n"));
    return NULL;
  }

  nsString ret;
  nsresult rv = docType->GetSystemId(ret);
  if (NS_FAILED(rv)) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getSystemId: failed (%x)\n", rv));
    return NULL;
  }

  jstring jret = env->NewString(ret.GetUnicode(), ret.Length());
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getSystemId: NewString failed\n"));
  }

  return jret;
}

/*
 * Class:     org_mozilla_dom_DocumentTypeImpl
 * Method:    getInternalSubset
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_DocumentTypeImpl_getInternalSubset
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDocumentType* docType = (nsIDOMDocumentType*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!docType) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DocumentType.getInternalSubset: NULL pointer\n"));
    return NULL;
  }

  nsString ret;
  nsresult rv = docType->GetInternalSubset(ret);
  if (NS_FAILED(rv)) {
      PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getInternalSubset: failed (%x)\n", rv));
    return NULL;
  }

  jstring jret = env->NewString(ret.GetUnicode(), ret.Length());
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DocumentType.getInternalSubset: NewString failed\n"));
  }

  return jret;
}
