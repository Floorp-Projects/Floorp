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
#include "nsIDOMText.h"
#include "nsDOMError.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_TextImpl.h"


/*
 * Class:     org_mozilla_dom_TextImpl
 * Method:    splitText
 * Signature: (I)Lorg/w3c/dom/Text;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_dom_TextImpl_splitText
  (JNIEnv *env, jobject jthis, jint joffset)
{
  if (joffset < 0 || joffset > JavaDOMGlobals::javaMaxInt) {
    JavaDOMGlobals::ThrowException(env, "",
                 NS_ERROR_DOM_INDEX_SIZE_ERR,
                 JavaDOMGlobals::EXCEPTION_DOM);
    return NULL;
  }

  nsIDOMText* text = (nsIDOMText*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!text) {
    JavaDOMGlobals::ThrowException(env,
      "Text.splitText: NULL pointer");
    return NULL;
  }

  nsIDOMText* ret = nsnull;
  nsresult rv = text->SplitText((PRUint32) joffset, &ret);
  if (NS_FAILED(rv) || !ret) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR ||
         NS_ERROR_GET_CODE(rv) == NS_ERROR_DOM_INDEX_SIZE_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "Text.splitText: failed", rv, exceptionType);
    return NULL;
  }

  jobject jret = env->AllocObject(JavaDOMGlobals::textClass);
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,
      "Text.splitText: failed to allocate object");
    return NULL;
  }

  env->SetLongField(jret, JavaDOMGlobals::nodePtrFID, (jlong) ret);
  if (env->ExceptionOccurred()) {
    JavaDOMGlobals::ThrowException(env,
      "Text.splitText: failed to set node ptr");
    return NULL;
  }

  ret->AddRef();
  return jret;
}
