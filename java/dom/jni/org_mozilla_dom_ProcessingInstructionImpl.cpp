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
#include "nsIDOMProcessingInstruction.h"
#include "nsDOMError.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_ProcessingInstructionImpl.h"


/*
 * Class:     org_mozilla_dom_ProcessingInstructionImpl
 * Method:    getData
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_ProcessingInstructionImpl_nativeGetData
  (JNIEnv *env, jobject jthis)
{
  nsIDOMProcessingInstruction* pi = (nsIDOMProcessingInstruction*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!pi) {
    JavaDOMGlobals::ThrowException(env,
      "ProcessingInstruction.getData: NULL pointer");
    return NULL;
  }

  nsString ret;
  nsresult rv = pi->GetData(ret);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "ProcessingInstruction.getData: failed", rv);
    return NULL;
  }

  jstring jret = env->NewString((jchar*) ret.get(), ret.Length());
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "ProcessingInstruction.getData: NewString failed");
    return NULL;
  }

  return jret;
}

/*
 * Class:     org_mozilla_dom_ProcessingInstructionImpl
 * Method:    getTarget
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_ProcessingInstructionImpl_nativeGetTarget
  (JNIEnv *env, jobject jthis)
{
  nsIDOMProcessingInstruction* pi = (nsIDOMProcessingInstruction*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!pi) {
    JavaDOMGlobals::ThrowException(env,
      "ProcessingInstruction.getTarget: NULL pointer");
    return NULL;
  }

  nsString ret;
  nsresult rv = pi->GetTarget(ret);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,
      "ProcessingInstruction.getTarget: failed", rv);
    return NULL;
  }

  jstring jret = env->NewString((jchar*) ret.get(), ret.Length());
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "ProcessingInstruction.getTarget: NewString failed");
    return NULL;
  }

  return jret;
}

/*
 * Class:     org_mozilla_dom_ProcessingInstructionImpl
 * Method:    setData
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_ProcessingInstructionImpl_nativeSetData
  (JNIEnv *env, jobject jthis, jstring jdata)
{
  nsIDOMProcessingInstruction* pi = (nsIDOMProcessingInstruction*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!pi || !jdata) {
    JavaDOMGlobals::ThrowException(env,
      "ProcessingInstruction.setData: NULL pointer");
    return;
  }

  nsString* data = JavaDOMGlobals::GetUnicode(env, jdata);
  if (!data)
    return;

  nsresult rv = pi->SetData(*data);
  nsMemory::Free(data);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "ProcessingInstruction.setData: failed", rv, exceptionType);
    return;
  }
}
