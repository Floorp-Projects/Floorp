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
#include "nsIDOMProcessingInstruction.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_ProcessingInstructionImpl.h"

/*
 * Class:     org_mozilla_dom_ProcessingInstructionImpl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_ProcessingInstructionImpl_finalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMProcessingInstruction* pi = (nsIDOMProcessingInstruction*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!pi) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("ProcessingInstruction.finalize: NULL pointer\n"));
    return;
  }

  JavaDOMGlobals::AddToGarbage(pi);
}

/*
 * Class:     org_mozilla_dom_ProcessingInstructionImpl
 * Method:    getData
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_ProcessingInstructionImpl_getData
  (JNIEnv *env, jobject jthis)
{
  nsIDOMProcessingInstruction* pi = (nsIDOMProcessingInstruction*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!pi) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("ProcessingInstruction.getData: NULL pointer\n"));
    return NULL;
  }

  nsString ret;
  nsresult rv = pi->GetData(ret);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("ProcessingInstruction.getData: failed (%x)\n", rv));
    return NULL;
  }

  char* cret = ret.ToNewCString();
  jstring jret = env->NewStringUTF(cret);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("ProcessingInstruction.getData: NewStringUTF failed (%s)\n", cret));
    return NULL;
  }
  delete[] cret;

  return jret;
}

/*
 * Class:     org_mozilla_dom_ProcessingInstructionImpl
 * Method:    getTarget
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_ProcessingInstructionImpl_getTarget
  (JNIEnv *env, jobject jthis)
{
  nsIDOMProcessingInstruction* pi = (nsIDOMProcessingInstruction*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!pi) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("ProcessingInstruction.getTarget: NULL pointer\n"));
    return NULL;
  }

  nsString ret;
  nsresult rv = pi->GetData(ret);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("ProcessingInstruction.getTarget: failed (%x)\n", rv));
    return NULL;
  }

  char* cret = ret.ToNewCString();
  jstring jret = env->NewStringUTF(cret);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("ProcessingInstruction.getTarget: NewStringUTF failed (%s)\n", cret));
    return NULL;
  }
  delete[] cret;

  return jret;
}

/*
 * Class:     org_mozilla_dom_ProcessingInstructionImpl
 * Method:    setData
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_ProcessingInstructionImpl_setData
  (JNIEnv *env, jobject jthis, jstring jdata)
{
  nsIDOMProcessingInstruction* pi = (nsIDOMProcessingInstruction*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!pi) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("ProcessingInstruction.setData: NULL pointer\n"));
    return;
  }

  jboolean iscopy = JNI_FALSE;
  const char* data = env->GetStringUTFChars(jdata, &iscopy);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("ProcessingInstruction.setData: GetStringUTFChars failed\n"));
    return;
  }
  nsresult rv = pi->SetData(data);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jdata, data);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("ProcessingInstruction.setData: failed (%x)\n", rv));
    return;
  }
}
