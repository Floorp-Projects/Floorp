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

#ifndef __JavaDOMGlobals_h__
#define __JavaDOMGlobals_h__

#include "jni.h"
#include "prclist.h"
#include "nsError.h"
#include "nsString.h"

// workaround for bug 30927
#ifdef ERROR
#undef ERROR
#endif

class nsISupports;
class nsIDOMNode;
struct PRLogModuleInfo;
struct PRLock;

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
  static jclass notationClass;
  static jclass processingInstructionClass;
  static jclass textClass;

  static jfieldID nodePtrFID;
  static jfieldID nodeListPtrFID;
  static jfieldID domImplementationPtrFID;
  static jfieldID namedNodeMapPtrFID;

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

  static jclass domExceptionClass;
  static jmethodID domExceptionInitMID;
  static jclass runtimeExceptionClass;
  static jmethodID runtimeExceptionInitMID;

  static const char* const DOM_EXCEPTION_MESSAGE[];

  typedef enum ExceptionType { EXCEPTION_RUNTIME, 
			       EXCEPTION_DOM } ExceptionType;
  
  static PRLogModuleInfo* log;
  static PRCList garbage;
  static PRLock* garbageLock;

  static PRInt32 javaMaxInt;

  static void Initialize(JNIEnv *env);
  static void Destroy(JNIEnv *env);
  static jobject CreateNodeSubtype(JNIEnv *env, 
				   nsIDOMNode *node);

  static void AddToGarbage(nsISupports* domObject);
  static void TakeOutGarbage();

  static void ThrowException(JNIEnv *env,
                             const char * message = NULL,
                             nsresult rv = NS_OK,
                             ExceptionType exceptionType = EXCEPTION_RUNTIME);

  static nsString* GetUnicode(JNIEnv *env,
			      jstring str);
};

#if defined(XP_UNIX) || defined(XP_MAC) || defined(XP_BEOS)

#define DOM_ITOA(intVal, buf, radix) sprintf(buf, "%d", intVal)
#else
#include <stdlib.h>
#define DOM_ITOA(intVal, buf, radix) itoa(intVal, buf, radix)
#endif
  
#endif /* __JavaDOMGlobals_h__ */
