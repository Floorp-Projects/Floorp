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
#include "nsIDOMText.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_TextImpl.h"

/*
 * Class:     org_mozilla_dom_TextImpl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_TextImpl_finalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMText* text = (nsIDOMText*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!text) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Text.finalize: NULL pointer\n"));
    return;
  }

  JavaDOMGlobals::AddToGarbage(text);
}

/*
 * Class:     org_mozilla_dom_TextImpl
 * Method:    splitText
 * Signature: (I)Lorg/w3c/dom/Text;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_TextImpl_splitText
  (JNIEnv *env, jobject jthis, jint joffset)
{
  nsIDOMText* text = (nsIDOMText*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!text) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Text.splitText: NULL pointer\n"));
    return NULL;
  }

  nsIDOMText* ret = nsnull;
  nsresult rv = text->SplitText((PRUint32) joffset, &ret);
  if (NS_FAILED(rv) || !ret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Text.splitText: failed (%x)\n", rv));
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::textClass);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Text.splitText: failed to allocate object\n"));
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Text.splitText: failed to set node ptr: %x\n", ret));
    return NULL;
  }

  ret->AddRef();
  return jret;
}
