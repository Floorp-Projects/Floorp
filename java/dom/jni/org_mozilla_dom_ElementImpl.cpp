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
#include "nsIDOMElement.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNodeList.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_ElementImpl.h"

/*
 * Class:     org_mozilla_dom_ElementImpl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_ElementImpl_finalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMElement* element = (nsIDOMElement*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!element) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Element.finalize: NULL pointer\n"));
    return;
  }

  JavaDOMGlobals::AddToGarbage(element);
}

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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Element.getAttribute: NULL pointer\n"));
    return NULL;
  }

  jboolean iscopy = JNI_FALSE;
  const char* cname = env->GetStringUTFChars(jname, &iscopy);
  if (!cname) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getAttribute: GetStringUTFChars failed\n"));
    return NULL;
  }

  nsString attr;
  nsresult rv = element->GetAttribute(cname, attr);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, cname);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getAttribute: failed (%x)\n", rv));
    return NULL;
  }

  char* cattr = attr.ToNewCString();
  jstring jattr = env->NewStringUTF(cattr);
  if (!jattr) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getAttribute: NewStringUTF failed (%s)\n", cattr));
    return NULL;
  }
  delete[] cattr;

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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Element.getAttributeNode: NULL pointer\n"));
    return NULL;
  }

  jboolean iscopy = JNI_FALSE;
  const char* cname = env->GetStringUTFChars(jname, &iscopy);
  if (!cname) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getAttributeNode: GetStringUTFChars failed\n"));
    return NULL;
  }

  nsIDOMAttr* attr = nsnull;
  nsresult rv = element->GetAttributeNode(cname, &attr);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, cname);
  if (NS_FAILED(rv) || !attr) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getAttributeNode: failed (%x)\n", rv));
    return NULL;
  }

  jobject jattr = env->AllocObject(JavaDOMGlobals::attrClass);
  if (!jattr) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getAttributeNode: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jattr, JavaDOMGlobals::nodePtrFID, (jlong) attr);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getAttributeNode: failed to set node ptr: %x\n", attr));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Element.getElementsByTagName: NULL pointer\n"));
    return NULL;
  }

  jboolean iscopy = JNI_FALSE;
  const char* cname = env->GetStringUTFChars(jname, &iscopy);
  if (!cname) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getElementsByTagName: GetStringUTFChars failed\n"));
    return NULL;
  }

  nsIDOMNodeList* nodes = nsnull;
  nsresult rv = element->GetElementsByTagName(cname, &nodes);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, cname);
  if (NS_FAILED(rv) || !nodes) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getElementsByTagName: failed (%x)\n", rv));
    return NULL;
  }

  jobject jnodes = env->AllocObject(JavaDOMGlobals::nodeListClass);
  if (!jnodes) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getElementsByTagName: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jnodes, JavaDOMGlobals::nodeListPtrFID, (jlong) nodes);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getElementsByTagName: failed to set node ptr: %x\n", nodes));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Element.getTagName: NULL pointer\n"));
    return NULL;
  }

  nsString tagName;
  nsresult rv = element->GetTagName(tagName);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getTagName: failed (%x)\n", rv));
    return NULL;
  }

  char* cTagName = tagName.ToNewCString();
  jstring jTagName = env->NewStringUTF(cTagName);
  if (!jTagName) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.getTagName: NewStringUTF failed (%s)\n", cTagName));
    return NULL;
  }
  delete[] cTagName;

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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Element.normalize: NULL pointer\n"));
    return;
  }

  nsresult rv = element->Normalize();
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.normalize: failed (%x)\n", rv));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Element.removeAttribute: NULL pointer\n"));
    return;
  }

  jboolean iscopy = JNI_FALSE;
  const char* name = env->GetStringUTFChars(jname, &iscopy);
  if (!name) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.removeAttribute: GetStringUTFChars failed\n"));
    return;
  }

  nsresult rv = element->RemoveAttribute(name);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, name);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.removeAttribute: failed (%x)\n", rv));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Element.removeAttributeNode: NULL pointer\n"));
    return NULL;
  }

  nsIDOMAttr* oldAttr = (nsIDOMAttr*) 
    env->GetLongField(joldAttr, JavaDOMGlobals::nodePtrFID);
  if (!oldAttr) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Element.removeAttributeNode: NULL arg pointer\n"));
    return NULL;
  }

  nsIDOMAttr* ret = nsnull;
  nsresult rv = element->RemoveAttributeNode(oldAttr, &ret);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.removeAttributeNode: failed (%x)\n", rv));
    return NULL;
  }

  jobject jattr = env->AllocObject(JavaDOMGlobals::attrClass);
  if (!jattr) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.removeAttributeNode: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jattr, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.removeAttributeNode: failed to set node ptr: %x\n", ret));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Element.setAttribute: NULL pointer\n"));
    return;
  }

  jboolean iscopy = JNI_FALSE;
  const char* name = env->GetStringUTFChars(jname, &iscopy);
  if (!name) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.setAttribute: GetStringUTFChars name failed\n"));
    return;
  }

  jboolean iscopy2 = JNI_FALSE;
  const char* value = env->GetStringUTFChars(jvalue, &iscopy2);
  if (!name) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.setAttribute: GetStringUTFChars value failed\n"));
    return;
  }

  nsresult rv = element->SetAttribute(name, value);
  if (iscopy2 == JNI_TRUE)
    env->ReleaseStringUTFChars(jvalue, value);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jname, name);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.setAttribute: failed (%x)\n", rv));
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
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Element.setAttributeNode: NULL pointer\n"));
    return NULL;
  }

  nsIDOMAttr* newAttr = (nsIDOMAttr*) 
    env->GetLongField(jnewAttr, JavaDOMGlobals::nodePtrFID);
  if (!newAttr) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Element.setAttributeNode: NULL arg pointer\n"));
    return NULL;
  }

  nsIDOMAttr* ret = nsnull;
  nsresult rv = element->SetAttributeNode(newAttr, &ret);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.setAttributeNode: failed (%x)\n", rv));
    return NULL;
  }

  jobject jattr = env->AllocObject(JavaDOMGlobals::attrClass);
  if (!jattr) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.setAttributeNode: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jattr, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Element.setAttributeNode: failed to set node ptr: %x\n", ret));
    return NULL;
  }

  ret->AddRef();
  return jattr;
}

