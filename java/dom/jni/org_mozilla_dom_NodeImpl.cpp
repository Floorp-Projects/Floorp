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
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMDocument.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_NodeImpl.h"

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_NodeImpl_finalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.finalize: NULL pointer\n"));
    return;
  }

  JavaDOMGlobals::AddToGarbage(node);
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    appendChild
 * Signature: (Lorg/w3c/dom/Node;)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_appendChild
  (JNIEnv *env, jobject jthis, jobject jchild)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.appendChild: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNode* child = (nsIDOMNode*) 
    env->GetLongField(jchild, JavaDOMGlobals::nodePtrFID);
  if (!child) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.appendChild: NULL child pointer\n"));
    return NULL;
  }

  nsIDOMNode* ret = nsnull;
  nsresult rv = node->AppendChild(child, &ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.appendChild: failed (%x)\n", rv));
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    cloneNode
 * Signature: (Z)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_cloneNode
  (JNIEnv *env, jobject jthis, jboolean jdeep)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.cloneNode: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNode* ret = nsnull;
  PRBool deep = jdeep == JNI_TRUE ? PR_TRUE : PR_FALSE;
  nsresult rv = node->CloneNode(deep, &ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.cloneNode: failed (%x)\n", rv));
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    getAttributes
 * Signature: ()Lorg/w3c/dom/NamedNodeMap;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_getAttributes
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.getAttributes: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNamedNodeMap* nodeMap = nsnull;
  nsresult rv = node->GetAttributes(&nodeMap);
  if (NS_FAILED(rv) || !nodeMap) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getAttributes: failed (%x)\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::namedNodeMapClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getAttributes: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) nodeMap);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getAttributes: failed to set node ptr: %x\n", nodeMap));
    return NULL;
  }

  nodeMap->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    getChildNodes
 * Signature: ()Lorg/w3c/dom/NodeList;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_getChildNodes
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.getChildNodes: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNodeList* nodeList = nsnull;
  nsresult rv = node->GetChildNodes(&nodeList);
  if (NS_FAILED(rv) || !nodeList) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getChildNodes: failed (%x)\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::nodeListClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getChildNodes: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodeListPtrFID, (jlong) nodeList);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getChildNodes: failed to set node ptr: %x\n", nodeList));
    return NULL;
  }

  nodeList->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    getFirstChild
 * Signature: ()Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_getFirstChild
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.getFirstChild: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNode* ret = nsnull;
  nsresult rv = node->GetFirstChild(&ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getFirstChild: failed (%x)\n", rv));
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    getLastChild
 * Signature: ()Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_getLastChild
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.getLastChild: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNode* ret = nsnull;
  nsresult rv = node->GetLastChild(&ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getLastChild: failed (%x)\n", rv));
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    getNextSibling
 * Signature: ()Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_getNextSibling
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.getNextSibling: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNode* ret = nsnull;
  nsresult rv = node->GetNextSibling(&ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getNextSibling: failed (%x)\n", rv));
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    getNodeName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_NodeImpl_getNodeName
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.getNodeName: NULL pointer\n"));
    return NULL;
  }

  nsString ret;
  nsresult rv = node->GetNodeName(ret);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getNodeName: failed (%x)\n", rv));
    return NULL;
  }

  char* cret = ret.ToNewCString();
  jstring jret = env->NewStringUTF(cret);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getNodeName: NewStringUTF failed (%s)\n", cret));
    return NULL;
  }
  delete[] cret;

  return jret;
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    getNodeType
 * Signature: ()S
 */
JNIEXPORT jshort JNICALL Java_org_mozilla_dom_NodeImpl_getNodeType
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.getNodeType: NULL pointer\n"));
    return (jshort) NULL;
  }

  PRUint16 type;
  nsresult rv = node->GetNodeType(&type);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getNodeType: failed (%x)\n", rv));
    return (jshort) NULL;
  }

  jfieldID typeFID = NULL;
  switch (type) {
  case nsIDOMNode::ATTRIBUTE_NODE:
      typeFID = JavaDOMGlobals::nodeTypeAttributeFID;
      break;

  case nsIDOMNode::CDATA_SECTION_NODE:
      typeFID = JavaDOMGlobals::nodeTypeCDataSectionFID;
      break;

  case nsIDOMNode::COMMENT_NODE:
      typeFID = JavaDOMGlobals::nodeTypeCommentFID;
      break;

  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
      typeFID = JavaDOMGlobals::nodeTypeDocumentFragmentFID;
      break;

  case nsIDOMNode::DOCUMENT_NODE:
      typeFID = JavaDOMGlobals::nodeTypeDocumentFID;
      break;

  case nsIDOMNode::DOCUMENT_TYPE_NODE:
      typeFID = JavaDOMGlobals::nodeTypeDocumentTypeFID;
      break;

  case nsIDOMNode::ELEMENT_NODE:
      typeFID = JavaDOMGlobals::nodeTypeElementFID;
      break;

  case nsIDOMNode::ENTITY_NODE:
      typeFID = JavaDOMGlobals::nodeTypeEntityFID;
      break;

  case nsIDOMNode::ENTITY_REFERENCE_NODE:
      typeFID = JavaDOMGlobals::nodeTypeEntityReferenceFID;
      break;

  case nsIDOMNode::NOTATION_NODE:
      typeFID = JavaDOMGlobals::nodeTypeNotationFID;
      break;

  case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
      typeFID = JavaDOMGlobals::nodeTypeProcessingInstructionFID;
      break;

  case nsIDOMNode::TEXT_NODE:
      typeFID = JavaDOMGlobals::nodeTypeTextFID;
      break;
  }

  jshort ret = 0;
  if (typeFID) {
      jclass nodeClass = env->GetObjectClass(jthis);
      if (!nodeClass) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	       ("Node.getNodeType: GetObjectClass failed (%x)\n", jthis));
	return ret;
      }

      ret = env->GetStaticShortField(nodeClass, typeFID);
      if (env->ExceptionOccurred()) {
	PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	       ("Node.getNodeType: typeFID failed\n"));
	return ret;
      }
  } else {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getNodeType: illegal type %d\n", type));
  }

  return ret;
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    getNodeValue
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_NodeImpl_getNodeValue
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.getNodeValue: NULL pointer\n"));
    return NULL;
  }

  nsString ret;
  nsresult rv = node->GetNodeValue(ret);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getNodeValue: failed (%x)\n", rv));
    return NULL;
  }

  char* cret = ret.ToNewCString();
  jstring jret = env->NewStringUTF(cret);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getNodeValue: NewStringUTF failed (%s)\n", cret));
    return NULL;
  }
  delete[] cret;

  return jret;
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    getOwnerDocument
 * Signature: ()Lorg/w3c/dom/Document;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_getOwnerDocument
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.getOwnerDocument: NULL pointer\n"));
    return NULL;
  }

  nsIDOMDocument* ret = nsnull;
  nsresult rv = node->GetOwnerDocument(&ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getOwnerDocument: failed (%x)\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::documentClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getOwnerDocument: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getOwnerDocument: failed to set node ptr: %x\n", ret));
    return NULL;
  }

  ret->AddRef();
  return jret;
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    getParentNode
 * Signature: ()Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_getParentNode
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.getParentNode: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNode* ret = nsnull;
  nsresult rv = node->GetParentNode(&ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getParentNode: failed (%x)\n", rv));
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    getPreviousSibling
 * Signature: ()Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_getPreviousSibling
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.getPreviousSibling: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNode* ret = nsnull;
  nsresult rv = node->GetPreviousSibling(&ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.getPreviousSibling: failed (%x)\n", rv));
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    hasChildNodes
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_NodeImpl_hasChildNodes
  (JNIEnv *env, jobject jthis)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.hasChildNodes: NULL pointer\n"));
    return (jboolean) NULL;
  }

  PRBool ret = PR_FALSE;
  nsresult rv = node->HasChildNodes(&ret);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.hasChildNodes: failed (%x)\n", rv));
    return (jboolean) NULL;
  }

  return ret == PR_TRUE ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    insertBefore
 * Signature: (Lorg/w3c/dom/Node;Lorg/w3c/dom/Node;)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_insertBefore
  (JNIEnv *env, jobject jthis, jobject jnewChild, jobject jrefChild)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.insertBefore: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNode* newChild = (nsIDOMNode*) 
    env->GetLongField(jnewChild, JavaDOMGlobals::nodePtrFID);
  if (!newChild) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.insertBefore: NULL newChild pointer\n"));
    return NULL;
  }

  nsIDOMNode* refChild = (nsIDOMNode*) 
    env->GetLongField(jrefChild, JavaDOMGlobals::nodePtrFID);
  if (!refChild) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.insertBefore: NULL refChild pointer\n"));
    return NULL;
  }

  nsIDOMNode* ret = nsnull;
  nsresult rv = node->InsertBefore(newChild, refChild, &ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.insertBefore: failed (%x)\n", rv));
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    removeChild
 * Signature: (Lorg/w3c/dom/Node;)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_removeChild
  (JNIEnv *env, jobject jthis, jobject joldChild)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.removeChild: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNode* oldChild = (nsIDOMNode*) 
    env->GetLongField(joldChild, JavaDOMGlobals::nodePtrFID);
  if (!oldChild) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.removeChild: NULL oldChild pointer\n"));
    return NULL;
  }

  nsIDOMNode* ret = nsnull;
  nsresult rv = node->RemoveChild(oldChild, &ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.removeChild: failed (%x)\n", rv));
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    replaceChild
 * Signature: (Lorg/w3c/dom/Node;Lorg/w3c/dom/Node;)Lorg/w3c/dom/Node;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_NodeImpl_replaceChild
  (JNIEnv *env, jobject jthis, jobject jnewChild, jobject joldChild)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.replaceChild: NULL pointer\n"));
    return NULL;
  }

  nsIDOMNode* newChild = (nsIDOMNode*) 
    env->GetLongField(jnewChild, JavaDOMGlobals::nodePtrFID);
  if (!newChild) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.replaceChild: NULL newChild pointer\n"));
    return NULL;
  }

  nsIDOMNode* oldChild = (nsIDOMNode*) 
    env->GetLongField(joldChild, JavaDOMGlobals::nodePtrFID);
  if (!oldChild) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.replaceChild: NULL oldChild pointer\n"));
    return NULL;
  }

  nsIDOMNode* ret = nsnull;
  nsresult rv = node->ReplaceChild(newChild, oldChild, &ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.replaceChild: failed (%x)\n", rv));
    return NULL;
  }

  return JavaDOMGlobals::CreateNodeSubtype(env, ret);
}

/*
 * Class:     org_mozilla_dom_NodeImpl
 * Method:    setNodeValue
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_NodeImpl_setNodeValue
  (JNIEnv *env, jobject jthis, jstring jvalue)
{
  nsIDOMNode* node = (nsIDOMNode*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!node) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Node.setNodeValue: NULL pointer\n"));
    return;
  }

  jboolean iscopy = JNI_FALSE;
  const char* value = env->GetStringUTFChars(jvalue, &iscopy);
  if (!value) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.setNodeValue: GetStringUTFChars failed\n"));
    return;
  }

  nsresult rv = node->SetNodeValue(value);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jvalue, value);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Node.setNodeValue: failed (%x)\n", rv));
    return;
  }
}
