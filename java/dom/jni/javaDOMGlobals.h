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

#ifndef __JavaDOMGlobals_h__
#define __JavaDOMGlobals_h__

#include "jni.h"
#include "prclist.h"

class nsISupports;
class nsIDOMNode;
class PRLogModuleInfo;
class PRLock;

class JavaDOMGlobals {

 public:
  static jclass attrClass;
  static jclass cDataSectionClass;
  static jclass commentClass;
  static jclass documentClass;
  static jclass documentFragmentClass;
  static jclass documentTypeClass;
  static jclass domImplementationClass;
  static jclass elementClass;
  static jclass entityClass;
  static jclass entityReferenceClass;
  static jclass namedNodeMapClass;
  static jclass nodeClass;
  static jclass nodeListClass;
#if 0
  static jclass notationClass;
#endif
  static jclass processingInstructionClass;
  static jclass textClass;

  static jfieldID nodePtrFID;
  static jfieldID nodeListPtrFID;
  static jfieldID domImplementationPtrFID;

  static jfieldID nodeTypeAttributeFID;
  static jfieldID nodeTypeCDataSectionFID;
  static jfieldID nodeTypeCommentFID;
  static jfieldID nodeTypeDocumentFragmentFID;
  static jfieldID nodeTypeDocumentFID;
  static jfieldID nodeTypeDocumentTypeFID;
  static jfieldID nodeTypeElementFID;
  static jfieldID nodeTypeEntityFID;
  static jfieldID nodeTypeEntityReferenceFID;
  static jfieldID nodeTypeNotationFID;
  static jfieldID nodeTypeProcessingInstructionFID;
  static jfieldID nodeTypeTextFID;

  static PRLogModuleInfo* log;
  static PRCList garbage;
  static PRLock* garbageLock;

  static void Initialize(JNIEnv *env);
  static void Destroy(JNIEnv *env);
  static jobject CreateNodeSubtype(JNIEnv *env, 
				   nsIDOMNode *node);

  static void AddToGarbage(nsISupports* domObject);
  static void TakeOutGarbage();
};

#endif /* __JavaDOMGlobals_h__ */
