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
#include "prmon.h"
#include "nsAutoLock.h"
#include "nsIDOMNode.h"
#include "javaDOMGlobals.h"
#include "javaDOMEventsGlobals.h"

jclass JavaDOMGlobals::attrClass = NULL;
jclass JavaDOMGlobals::cDataSectionClass = NULL;
jclass JavaDOMGlobals::commentClass = NULL;
jclass JavaDOMGlobals::documentClass = NULL;
jclass JavaDOMGlobals::documentFragmentClass = NULL;
jclass JavaDOMGlobals::documentTypeClass = NULL;
jclass JavaDOMGlobals::domImplementationClass = NULL;
jclass JavaDOMGlobals::elementClass = NULL;
jclass JavaDOMGlobals::entityClass = NULL;
jclass JavaDOMGlobals::entityReferenceClass = NULL;
jclass JavaDOMGlobals::namedNodeMapClass = NULL;
jclass JavaDOMGlobals::nodeClass = NULL;
jclass JavaDOMGlobals::nodeListClass = NULL;
jclass JavaDOMGlobals::notationClass = NULL;
jclass JavaDOMGlobals::processingInstructionClass = NULL;
jclass JavaDOMGlobals::textClass = NULL;

jfieldID JavaDOMGlobals::nodePtrFID = NULL;
jfieldID JavaDOMGlobals::nodeListPtrFID = NULL;
jfieldID JavaDOMGlobals::domImplementationPtrFID = NULL;
jfieldID JavaDOMGlobals::namedNodeMapPtrFID = NULL;

jfieldID JavaDOMGlobals::nodeTypeAttributeFID = NULL;
jfieldID JavaDOMGlobals::nodeTypeCDataSectionFID = NULL;
jfieldID JavaDOMGlobals::nodeTypeCommentFID = NULL;
jfieldID JavaDOMGlobals::nodeTypeDocumentFragmentFID = NULL;
jfieldID JavaDOMGlobals::nodeTypeDocumentFID = NULL;
jfieldID JavaDOMGlobals::nodeTypeDocumentTypeFID = NULL;
jfieldID JavaDOMGlobals::nodeTypeElementFID = NULL;
jfieldID JavaDOMGlobals::nodeTypeEntityFID = NULL;
jfieldID JavaDOMGlobals::nodeTypeEntityReferenceFID = NULL;
jfieldID JavaDOMGlobals::nodeTypeNotationFID = NULL;
jfieldID JavaDOMGlobals::nodeTypeProcessingInstructionFID = NULL;
jfieldID JavaDOMGlobals::nodeTypeTextFID = NULL;

jclass JavaDOMGlobals::domExceptionClass = NULL;
jmethodID JavaDOMGlobals::domExceptionInitMID = NULL;
jclass JavaDOMGlobals::runtimeExceptionClass = NULL;
jmethodID JavaDOMGlobals::runtimeExceptionInitMID = NULL;

const char* const JavaDOMGlobals::DOM_EXCEPTION_MESSAGE[] = 
  {"Invalid DOM error code",
   "Index is out of bounds",
   "Could not fit text",
   "Wrong hierarchy request",
   "Wrong document usage",
   "Invalid character",
   "Data is unsupported",
   "Modification disallowed",
   "Node not found",
   "Type is unsupported",
   "Attribute is alreay in use", 
   "Invalid state",
   "Syntax error",
   "Invalid modification",
   "Namespace error",
   "Invalid access"};

PRLogModuleInfo* JavaDOMGlobals::log = NULL;

PRCList JavaDOMGlobals::garbage = PR_INIT_STATIC_CLIST(&garbage);
PRLock* JavaDOMGlobals::garbageLock = NULL;

PRInt32 JavaDOMGlobals::javaMaxInt = 0;

class jniDOMGarbage : public PRCList {
public:
  jniDOMGarbage(nsISupports* p) { domObject = p; }
  nsISupports* domObject;
};

void JavaDOMGlobals::Initialize(JNIEnv *env) 
{
  garbageLock = PR_NewLock();
  log = PR_NewLogModule("javadom");
  PR_INIT_CLIST(&garbage);

  /* Node is loaded first because it is the base class of a lot of the
     others */
  nodeClass = env->FindClass("org/mozilla/dom/NodeImpl");
  if (!nodeClass) return;
  nodeClass = (jclass) env->NewGlobalRef(nodeClass);
  if (!nodeClass) return;
  nodePtrFID = 
    env->GetFieldID(nodeClass, "p_nsIDOMNode", "J");
  if (!nodePtrFID) return;

  attrClass = env->FindClass("org/mozilla/dom/AttrImpl");
  if (!attrClass) return;
  attrClass = (jclass) env->NewGlobalRef(attrClass);
  if (!attrClass) return;

  cDataSectionClass = env->FindClass("org/mozilla/dom/CDATASectionImpl");
  if (!cDataSectionClass) return;
  cDataSectionClass = (jclass) env->NewGlobalRef(cDataSectionClass);
  if (!cDataSectionClass) return;

  commentClass = env->FindClass("org/mozilla/dom/CommentImpl");
  if (!commentClass) return;
  commentClass = (jclass) env->NewGlobalRef(commentClass);
  if (!commentClass) return;

  documentClass = env->FindClass("org/mozilla/dom/DocumentImpl");
  if (!documentClass) return;
  documentClass = (jclass) env->NewGlobalRef(documentClass);
  if (!documentClass) return;

  documentFragmentClass = env->FindClass("org/mozilla/dom/DocumentFragmentImpl");
  if (!documentFragmentClass) return;
  documentFragmentClass = (jclass) env->NewGlobalRef(documentFragmentClass);
  if (!documentFragmentClass) return;

  documentTypeClass = env->FindClass("org/mozilla/dom/DocumentTypeImpl");
  if (!documentTypeClass) return;
  documentTypeClass = (jclass) env->NewGlobalRef(documentTypeClass);
  if (!documentTypeClass) return;

  domImplementationClass = env->FindClass("org/mozilla/dom/DOMImplementationImpl");
  if (!domImplementationClass) return;
  domImplementationClass = (jclass) env->NewGlobalRef(domImplementationClass);
  if (!domImplementationClass) return;
  domImplementationPtrFID = 
    env->GetFieldID(domImplementationClass, "p_nsIDOMDOMImplementation", "J");
  if (!domImplementationPtrFID) return;

  elementClass = env->FindClass("org/mozilla/dom/ElementImpl");
  if (!elementClass) return;
  elementClass = (jclass) env->NewGlobalRef(elementClass);
  if (!elementClass) return;

  entityClass = env->FindClass("org/mozilla/dom/EntityImpl");
  if (!entityClass) return;
  entityClass = (jclass) env->NewGlobalRef(entityClass);
  if (!entityClass) return;

  entityReferenceClass = env->FindClass("org/mozilla/dom/EntityReferenceImpl");
  if (!entityReferenceClass) return;
  entityReferenceClass = (jclass) env->NewGlobalRef(entityReferenceClass);
  if (!entityReferenceClass) return;

  namedNodeMapClass = env->FindClass("org/mozilla/dom/NamedNodeMapImpl");
  if (!namedNodeMapClass) return;
  namedNodeMapPtrFID = 
    env->GetFieldID(namedNodeMapClass, "p_nsIDOMNamedNodeMap", "J");
  if (!namedNodeMapPtrFID) return;
  namedNodeMapClass = (jclass) env->NewGlobalRef(namedNodeMapClass);
  if (!namedNodeMapClass) return;

  nodeListClass = env->FindClass("org/mozilla/dom/NodeListImpl");
  if (!nodeListClass) return;
  nodeListClass = (jclass) env->NewGlobalRef(nodeListClass);
  if (!nodeListClass) return;
  nodeListPtrFID = 
    env->GetFieldID(nodeListClass, "p_nsIDOMNodeList", "J");
  if (!nodeListPtrFID) return;

  nodeTypeAttributeFID = 
    env->GetStaticFieldID(nodeClass, "ATTRIBUTE_NODE", "S");
  if (!nodeTypeAttributeFID) return;
  nodeTypeCDataSectionFID = 
    env->GetStaticFieldID(nodeClass, "CDATA_SECTION_NODE", "S");
  if (!nodeTypeCDataSectionFID) return;
  nodeTypeCommentFID = 
    env->GetStaticFieldID(nodeClass, "COMMENT_NODE", "S");
  if (!nodeTypeCommentFID) return;
  nodeTypeDocumentFragmentFID = 
    env->GetStaticFieldID(nodeClass, "DOCUMENT_FRAGMENT_NODE", "S");
  if (!nodeTypeDocumentFragmentFID) return;
  nodeTypeDocumentFID = 
    env->GetStaticFieldID(nodeClass, "DOCUMENT_NODE", "S");
  if (!nodeTypeDocumentFID) return;
  nodeTypeDocumentTypeFID = 
    env->GetStaticFieldID(nodeClass, "DOCUMENT_TYPE_NODE", "S");
  if (!nodeTypeDocumentTypeFID) return;
  nodeTypeElementFID = 
    env->GetStaticFieldID(nodeClass, "ELEMENT_NODE", "S");
  if (!nodeTypeElementFID) return;
  nodeTypeEntityFID = 
    env->GetStaticFieldID(nodeClass, "ENTITY_NODE", "S");
  if (!nodeTypeEntityFID) return;
  nodeTypeEntityReferenceFID = 
    env->GetStaticFieldID(nodeClass, "ENTITY_REFERENCE_NODE", "S");
  if (!nodeTypeEntityReferenceFID) return;
  nodeTypeNotationFID = 
    env->GetStaticFieldID(nodeClass, "NOTATION_NODE", "S");
  if (!nodeTypeNotationFID) return;
  nodeTypeProcessingInstructionFID = 
    env->GetStaticFieldID(nodeClass, "PROCESSING_INSTRUCTION_NODE", "S");
  if (!nodeTypeProcessingInstructionFID) return;
  nodeTypeTextFID = 
    env->GetStaticFieldID(nodeClass, "TEXT_NODE", "S");
  if (!nodeTypeTextFID) return;

  notationClass = env->FindClass("org/mozilla/dom/NotationImpl");
  if (!notationClass) return;
  notationClass = (jclass) env->NewGlobalRef(notationClass);
  if (!notationClass) return;

  processingInstructionClass = env->FindClass("org/mozilla/dom/ProcessingInstructionImpl");
  if (!processingInstructionClass) return;
  processingInstructionClass = (jclass) env->NewGlobalRef(processingInstructionClass);
  if (!processingInstructionClass) return;

  textClass = env->FindClass("org/mozilla/dom/TextImpl");
  if (!textClass) return;
  textClass = (jclass) env->NewGlobalRef(textClass);
  if (!textClass) return;

  domExceptionClass = env->FindClass("org/mozilla/dom/DOMExceptionImpl");
  if (!domExceptionClass) return;
  domExceptionClass = (jclass) env->NewGlobalRef(domExceptionClass);
  if (!domExceptionClass) return;

  domExceptionInitMID = 
    env->GetMethodID(domExceptionClass, "<init>", "(SLjava/lang/String;)V");
  if (!domExceptionInitMID) return;

  runtimeExceptionClass = env->FindClass("java/lang/RuntimeException");
  if (!runtimeExceptionClass) return;
  runtimeExceptionClass = (jclass) env->NewGlobalRef(runtimeExceptionClass);
  if (!runtimeExceptionClass) return;

  runtimeExceptionInitMID = 
    env->GetMethodID(runtimeExceptionClass, "<init>", "(Ljava/lang/String;)V");
  if (!runtimeExceptionInitMID) return;


  jclass integerClass = env->FindClass("java/lang/Integer");
  jfieldID javaMaxIntFID = 
    env->GetStaticFieldID(integerClass, "MAX_VALUE", "I");
  javaMaxInt = env->GetStaticIntField(integerClass, javaMaxIntFID);

  //init events globals
  JavaDOMEventsGlobals::Initialize(env);
}

void JavaDOMGlobals::Destroy(JNIEnv *env) 
{
  //destroy events stuff
  JavaDOMEventsGlobals::Destroy(env);

  env->DeleteGlobalRef(attrClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete Attr global ref %x\n", 
	    attrClass));
    return;
  }
  attrClass = NULL;

  env->DeleteGlobalRef(cDataSectionClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete CDATASection global ref %x\n", 
	    cDataSectionClass));
    return;
  }
  cDataSectionClass = NULL;

  env->DeleteGlobalRef(commentClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete Comment global ref %x\n", 
	    commentClass));
    return;
  }
  commentClass = NULL;

  env->DeleteGlobalRef(documentClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete Document global ref %x\n", 
	    documentClass));
    return;
  }
  documentClass = NULL;

  env->DeleteGlobalRef(documentFragmentClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete DocumentFragment global ref %x\n", 
	    documentFragmentClass));
    return;
  }
  documentFragmentClass = NULL;

  env->DeleteGlobalRef(documentTypeClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete DocumentType global ref %x\n", 
	    documentTypeClass));
    return;
  }
  documentTypeClass = NULL;

  env->DeleteGlobalRef(domImplementationClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete DOMImplementation global ref %x\n", 
	    domImplementationClass));
    return;
  }
  domImplementationClass = NULL;

  env->DeleteGlobalRef(elementClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete Element global ref %x\n", 
	    elementClass));
    return;
  }
  elementClass = NULL;

  env->DeleteGlobalRef(entityClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete Entity global ref %x\n", 
	    entityClass));
    return;
  }
  entityClass = NULL;

  env->DeleteGlobalRef(entityReferenceClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete EntityReference global ref %x\n", 
	    entityReferenceClass));
    return;
  }
  entityReferenceClass = NULL;

  env->DeleteGlobalRef(namedNodeMapClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete NamedNodeMap global ref %x\n", 
	    namedNodeMapClass));
    return;
  }
  namedNodeMapClass = NULL;

  env->DeleteGlobalRef(nodeListClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete NodeList global ref %x\n", 
	    nodeListClass));
    return;
  }
  nodeListClass = NULL;

  env->DeleteGlobalRef(nodeClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete Node global ref %x\n", 
	    nodeClass));
    return;
  }
  nodeClass = NULL;

  env->DeleteGlobalRef(notationClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete Notation global ref %x\n", 
	    notationClass));
    return;
  }
  notationClass = NULL;

  env->DeleteGlobalRef(processingInstructionClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete ProcessingInstruction global ref %x\n", 
	    processingInstructionClass));
    return;
  }
  processingInstructionClass = NULL;

  env->DeleteGlobalRef(textClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete Text global ref %x\n", 
	    textClass));
    return;
  }
  textClass = NULL;

  env->DeleteGlobalRef(domExceptionClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete DOM Exception global ref %x\n", 
	    domExceptionClass));
    return;
  }
  domExceptionClass = NULL;

  env->DeleteGlobalRef(runtimeExceptionClass);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::Destroy: failed to delete DOM Exception global ref %x\n", 
	    runtimeExceptionClass));
    return;
  }
  runtimeExceptionClass = NULL;

  TakeOutGarbage();
  PR_DestroyLock(garbageLock);
}

jobject JavaDOMGlobals::CreateNodeSubtype(JNIEnv *env, 
					  nsIDOMNode *node) 
{
  if (!node)
    return NULL;

  PRUint16 nodeType = 0;
  (void) node->GetNodeType(&nodeType);

  jclass clazz = nodeClass;
  switch (nodeType) {
  case nsIDOMNode::ATTRIBUTE_NODE:
    clazz = attrClass;
    break;

  case nsIDOMNode::CDATA_SECTION_NODE:
    clazz = cDataSectionClass;
    break;

  case nsIDOMNode::COMMENT_NODE:
    clazz = commentClass;
    break;

  case nsIDOMNode::DOCUMENT_NODE:
    clazz = documentClass;
    break;

  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    clazz = documentFragmentClass;
    break;

  case nsIDOMNode::DOCUMENT_TYPE_NODE:
    clazz = documentTypeClass;
    break;

  case nsIDOMNode::ELEMENT_NODE:
    clazz = elementClass;
    break;

  case nsIDOMNode::ENTITY_NODE:
    clazz = entityClass;
    break;

  case nsIDOMNode::ENTITY_REFERENCE_NODE:
    clazz = entityReferenceClass;
    break;

  case nsIDOMNode::NOTATION_NODE:
    clazz = notationClass;
    break;

  case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
    clazz = processingInstructionClass;
    break;

  case nsIDOMNode::TEXT_NODE:
    clazz = textClass;
    break;
  }

  PR_LOG(log, PR_LOG_ERROR, 
	 ("JavaDOMGlobals::CreateNodeSubtype: allocating Node of type %d, clazz=%x\n", 
	  nodeType, clazz));
  jobject jnode = env->AllocObject(clazz);
  if (!jnode) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::CreateNodeSubtype: failed to allocate Node of type %d\n", 
	    nodeType));
    return NULL;
  }

  env->SetLongField(jnode, nodePtrFID, (jlong) node);
  if (env->ExceptionOccurred()) {
    PR_LOG(log, PR_LOG_ERROR, 
	   ("JavaDOMGlobals::CreateNodeSubtype: failed to set native ptr %x\n", 
	    (jlong) node));
    return NULL;
  }
  node->AddRef();

  return jnode;
}

void JavaDOMGlobals::AddToGarbage(nsISupports* domObject)
{
  nsAutoLock lock(garbageLock);
  jniDOMGarbage* elem = new jniDOMGarbage(domObject);
  PR_INSERT_BEFORE(elem, &garbage);
  PR_LOG(log, PR_LOG_DEBUG, 
	 ("JavaDOMGlobals::AddToGarbage: Scheduling %x\n", domObject));
}

void JavaDOMGlobals::TakeOutGarbage()
{
  nsAutoLock lock(garbageLock);

  PRUint32 count = 0;
  nsISupports* domo = NULL;

  PRCList* chain = NULL;
  PRCList* elem = NULL;
  for (chain = PR_LIST_HEAD(&garbage);
       chain != &garbage; 
       chain = PR_NEXT_LINK(chain)) {

    delete elem;
    elem = chain;
    domo = ((jniDOMGarbage*) elem)->domObject;
    PR_LOG(log, PR_LOG_DEBUG, 
	   ("JavaDOMGlobals::TakeOutGarbage: Releasing %x\n", domo));
    domo->Release();
    domo = NULL;

    count++;
  }
  delete elem;
  PR_INIT_CLIST(&garbage);

  if (count)
    PR_LOG(log, PR_LOG_DEBUG, 
	   ("JavaDOMGlobals::TakeOutGarbage: Released %d objects\n", count));
}

// caller must return from JNI immediately after calling this function
void JavaDOMGlobals::ThrowException(JNIEnv *env,
                                    const char * message,
                                    nsresult rv,
                                    ExceptionType exceptionType) {

  jthrowable newException = NULL;

  if (exceptionType == EXCEPTION_DOM) {
    PR_LOG(log, PR_LOG_ERROR, 
           ("JavaDOMGlobals::ThrowException: (%x) %s: %s\n", 
            NS_ERROR_GET_CODE(rv), 
            message ? message : "", 
            DOM_EXCEPTION_MESSAGE[NS_ERROR_GET_CODE(rv)]));

    jstring jmessage = 
      env->NewStringUTF(DOM_EXCEPTION_MESSAGE[NS_ERROR_GET_CODE(rv)]);

    newException = 
      (jthrowable)env->NewObject(domExceptionClass, 
                                 domExceptionInitMID,
                                 NS_ERROR_GET_CODE(rv),
                                 jmessage);
  } else {
    char buffer[256];
    const char* msg = message;
    if (rv != NS_OK) {
      sprintf(buffer, 
              "(%x.%x) %s",
              NS_ERROR_GET_MODULE(rv),
              NS_ERROR_GET_CODE(rv),
              message ? message : "");
      msg = buffer;
    }

    PR_LOG(log, PR_LOG_ERROR, 
           ("JavaDOMGlobals::ThrowException: %s\n", msg));

    jstring jmessage = env->NewStringUTF(msg);

    newException = 
      (jthrowable)env->NewObject(runtimeExceptionClass, 
                                 runtimeExceptionInitMID,
                                 jmessage);

  }

  if (newException != NULL) {
    env->Throw(newException);
  } 

  // an exception is thrown in any case
}

nsString* JavaDOMGlobals::GetUnicode(JNIEnv *env,
				     jstring jstr)
{
  jboolean iscopy = JNI_FALSE;
  const jchar* name = env->GetStringChars(jstr, &iscopy);
  if (!name) {
    ThrowException(env, "GetStringChars failed");
    return NULL;
  }
  nsString* ustr = new nsString((PRUnichar*)name, 
				env->GetStringLength(jstr));
  env->ReleaseStringChars(jstr, name);
  return ustr;
}
