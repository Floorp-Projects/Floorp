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
#include "nsIDOMDOMImplementation.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_DOMImplementationImpl.h"
#include "nsDOMError.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

/*
 * Class:     org_mozilla_dom_DOMImplementationImpl
 * Method:    XPCOM_equals
 * Signature: (Ljava/lang/Object;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_DOMImplementationImpl_nativeXPCOM_1equals
  (JNIEnv *env, jobject jthis, jobject jarg)
{
  jboolean b_retFlag = JNI_FALSE;

  nsIDOMDOMImplementation* p_this = (nsIDOMDOMImplementation*) 
    env->GetLongField(jthis, JavaDOMGlobals::domImplementationPtrFID);
  if (!p_this) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DOMImplementation.equals: NULL pointer\n"));
    return b_retFlag;
  }

  nsIDOMDOMImplementation* p_arg = (nsIDOMDOMImplementation*) 
    env->GetLongField(jarg, JavaDOMGlobals::domImplementationPtrFID);
  if (!p_arg) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DOMImplementation.equals: NULL arg pointer\n"));
    return b_retFlag;
  }

  nsISupports* thisSupports = nsnull;
  nsISupports* argSupports = nsnull;

  nsresult rvThis = 
    p_this->QueryInterface(kISupportsIID, (void**)(&thisSupports));
  if (NS_FAILED(rvThis) || !thisSupports) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMImplementation.equals: this->QueryInterface failed (%x)\n", 
	    rvThis));
    return b_retFlag; 	
  }

  nsresult rvArg =
    p_arg->QueryInterface(kISupportsIID, (void**)(&argSupports));
  if (NS_FAILED(rvArg) || !argSupports) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMImplementation.equals: arg->QueryInterface failed (%x)\n", 
	    rvArg));
    thisSupports->Release();
    return b_retFlag;
  }

  if (thisSupports == argSupports)
    b_retFlag = JNI_TRUE;
  
  thisSupports->Release();
  argSupports->Release();

  return b_retFlag;
}

/*
 * Class:     org_mozilla_dom_DOMImplementationImpl
 * Method:    XPCOM_hashCode
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_DOMImplementationImpl_nativeXPCOM_1hashCode
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDOMImplementation* p_this = (nsIDOMDOMImplementation*) 
    env->GetLongField(jthis, JavaDOMGlobals::domImplementationPtrFID);
  if (!p_this) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DOMImplementation.hashCode: NULL pointer\n"));
    return (jint) 0;
  }

  nsISupports* thisSupports = nsnull;
  nsresult rvThis = 
    p_this->QueryInterface(kISupportsIID, (void**)(&thisSupports));
  if (NS_FAILED(rvThis) || !thisSupports) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMImplementation.hashCode: QueryInterface failed (%x)\n", 
	    rvThis));
    return (jint) 0;
  }

  thisSupports->Release();
  return (jint) thisSupports;
}

/*
 * Class:     org_mozilla_dom_DOMImplementationImpl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_DOMImplementationImpl_nativeFinalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMDOMImplementation* dom = (nsIDOMDOMImplementation*) 
    env->GetLongField(jthis, JavaDOMGlobals::domImplementationPtrFID);
  if (!dom) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DOMImplementation.finalize: NULL pointer\n"));
    return;
  }

  JavaDOMGlobals::AddToGarbage(dom);
}

/*
 * Class:     org_mozilla_dom_DOMImplementationImpl
 * Method:    hasFeature
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_DOMImplementationImpl_nativeHasFeature
  (JNIEnv *env, jobject jthis, jstring jfeature, jstring jversion)
{
  nsIDOMDOMImplementation* dom = (nsIDOMDOMImplementation*) 
    env->GetLongField(jthis, JavaDOMGlobals::domImplementationPtrFID);
  if (!dom || !jfeature) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("DOMImplementation.hasFeature: NULL pointer\n"));
    return JNI_FALSE;
  }

  nsString* feature = JavaDOMGlobals::GetUnicode(env, jfeature);
  if (!feature)
      return JNI_FALSE;

  nsString* version;
  if (jversion) {
      version = JavaDOMGlobals::GetUnicode(env, jversion);
      if (!version) {
	  nsMemory::Free(feature);
	  return JNI_FALSE;
      }
  } else {
      version = new nsString();
  }

  PRBool ret = PR_FALSE;
  nsresult rv = dom->HasFeature(*feature, *version, &ret);
  nsMemory::Free(feature);
  nsMemory::Free(version);

  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("DOMImplementation.hasFeature: failed (%x)\n", rv));
  }

  return ret == PR_TRUE ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_mozilla_dom_DOMImplementationImpl
 * Method:    createDocumentType
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Lorg/w3c/dom/DocumentType;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DOMImplementationImpl_nativeCreateDocumentType
  (JNIEnv *env, jobject jthis, jstring jqualifiedName, jstring jpublicID, jstring jsystemID)
{
  nsIDOMDOMImplementation* dom = (nsIDOMDOMImplementation*) 
    env->GetLongField(jthis, JavaDOMGlobals::domImplementationPtrFID);
  if (!dom || !jqualifiedName || !jpublicID || !jsystemID) {
    JavaDOMGlobals::ThrowException(env,
	"DOMImplementation.createDocumentType: NULL pointer");
    return NULL;
  }

  nsString* qualifiedName = JavaDOMGlobals::GetUnicode(env, jqualifiedName);
  if (!qualifiedName)
      return NULL;

  nsString* publicID = JavaDOMGlobals::GetUnicode(env, jpublicID);
  if (!publicID) {
      nsMemory::Free(qualifiedName);
      return NULL;
  }

  nsString* systemID = JavaDOMGlobals::GetUnicode(env, jsystemID);
  if (!systemID) {
      nsMemory::Free(qualifiedName);
      nsMemory::Free(publicID);
      return NULL;
  }

  nsIDOMDocumentType* docType = nsnull;
  nsresult rv = dom->CreateDocumentType(*qualifiedName, *publicID, *systemID, &docType);
  nsMemory::Free(qualifiedName);
  nsMemory::Free(publicID);
  nsMemory::Free(systemID);

  if (NS_FAILED(rv)) {
      JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
      if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
	  (rv == NS_ERROR_DOM_INVALID_CHARACTER_ERR ||
	   rv == NS_ERROR_DOM_NAMESPACE_ERR)) {
	  exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
      }
      JavaDOMGlobals::ThrowException(env,
	  "DOMImplementation.createDocumentType: failed", rv, exceptionType);
      return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, (nsIDOMNode*)docType);
}

/*
 * Class:     org_mozilla_dom_DOMImplementationImpl
 * Method:    createDocument
 * Signature: (Ljava/lang/String;Ljava/lang/String;Lorg/w3c/dom/DocumentType;)Lorg/w3c/dom/Document;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_DOMImplementationImpl_nativeCreateDocument
  (JNIEnv *env, jobject jthis, jstring jnamespaceURI, jstring jqualifiedName, jobject jdoctype)
{
  nsIDOMDOMImplementation* dom = (nsIDOMDOMImplementation*) 
    env->GetLongField(jthis, JavaDOMGlobals::domImplementationPtrFID);
  if (!dom || !jnamespaceURI || !jqualifiedName) {
    JavaDOMGlobals::ThrowException(env,
	"DOMImplementation.createDocument: NULL pointer");
    return NULL;
  }

  nsString* namespaceURI = JavaDOMGlobals::GetUnicode(env, jnamespaceURI);
  if (!namespaceURI)
      return NULL;

  nsString* qualifiedName = JavaDOMGlobals::GetUnicode(env, jqualifiedName);
  if (!qualifiedName) {
      nsMemory::Free(namespaceURI);
      return NULL;
  }

  nsIDOMDocumentType* docType = nsnull;
  if (jdoctype) {
      docType = (nsIDOMDocumentType*)
	env->GetLongField(jdoctype, JavaDOMGlobals::nodeListPtrFID);
      if (!docType) {
	JavaDOMGlobals::ThrowException(env,
	    "DOMImplementation.createDocument: NULL arg pointer");
	return NULL;
      }
  }

  nsIDOMDocument* doc = nsnull;
  nsresult rv = dom->CreateDocument(*namespaceURI, *qualifiedName, docType, &doc);
  nsMemory::Free(namespaceURI);
  nsMemory::Free(qualifiedName);

  if (NS_FAILED(rv)) {
      JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
      if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
	  (rv == NS_ERROR_DOM_INVALID_CHARACTER_ERR ||
	   rv == NS_ERROR_DOM_NAMESPACE_ERR ||
	   rv == NS_ERROR_DOM_WRONG_DOCUMENT_ERR)) {
	  exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
      }
      JavaDOMGlobals::ThrowException(env,
	  "DOMImplementation.createDocument: failed", rv, exceptionType);
      return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, (nsIDOMNode*)doc);    
}
