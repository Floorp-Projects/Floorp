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
#include "nsIDOMCharacterData.h"
#include "nsDOMError.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_CharacterDataImpl.h"

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    appendData
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_CharacterDataImpl_nativeAppendData
  (JNIEnv *env, jobject jthis, jstring jvalue)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data || !jvalue) {
    JavaDOMGlobals::ThrowException(env, 
      "CharacterData.appendData: NULL pointer");
    return;
  }
  
  nsString* value = JavaDOMGlobals::GetUnicode(env, jvalue);
  if (!value)
    return;

  nsresult rv = data->AppendData(*value);
  nsMemory::Free(value);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,  
      "CharacterData.appendData: failed", rv, exceptionType);
    return;
  }
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    deleteData
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_CharacterDataImpl_nativeDeleteData
  (JNIEnv *env, jobject jthis, jint offset, jint count)
{
  if (offset < 0 || offset > JavaDOMGlobals::javaMaxInt || 
      count < 0 || count > JavaDOMGlobals::javaMaxInt) {
    JavaDOMGlobals::ThrowException(env, "CharacterData.deleteData: failed",
                 NS_ERROR_DOM_INDEX_SIZE_ERR,
                 JavaDOMGlobals::EXCEPTION_DOM);
    return;
  }

  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    JavaDOMGlobals::ThrowException(env,  
      "CharacterData.deleteData: NULL pointer");
    return;
  }
  
  nsresult rv = data->DeleteData((PRUint32) offset, (PRUint32) count);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (rv == NS_ERROR_DOM_INDEX_SIZE_ERR ||
         rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "CharacterData.deleteData: failed", rv, exceptionType);
    return;
  }
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    getData
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_CharacterDataImpl_nativeGetData
  (JNIEnv *env, jobject jthis)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    JavaDOMGlobals::ThrowException(env,  
      "CharacterData.getData: NULL pointer");
    return NULL;
  }
  
  nsString ret;
  nsresult rv = data->GetData(ret);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (rv == NS_ERROR_DOM_DOMSTRING_SIZE_ERR) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "CharacterData.getData: failed", rv, exceptionType);
    return NULL;
  }

  jstring jret = env->NewString((jchar*) ret.get(), ret.Length());
  if (!jret) {
    JavaDOMGlobals::ThrowException(env,  
      "CharacterData.getData: NewString failed");
  }

  return jret;
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    getLength
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_CharacterDataImpl_nativeGetLength
  (JNIEnv *env, jobject jthis)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    JavaDOMGlobals::ThrowException(env,  
      "CharacterData.getLength: NULL pointer");
    return 0;
  }
  
  PRUint32 len = 0;
  nsresult rv = data->GetLength(&len);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ThrowException(env,  
      "CharacterData.getLength: failed", rv);
    return 0;
  }

  return (jint) len;
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    insertData
 * Signature: (ILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_CharacterDataImpl_nativeInsertData
  (JNIEnv *env, jobject jthis, jint offset, jstring jvalue)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data || !jvalue) {
    JavaDOMGlobals::ThrowException(env,  
      "CharacterData.insertData: NULL pointer");
    return;
  }
  
  nsString* value = JavaDOMGlobals::GetUnicode(env, jvalue);
  if (!value)
    return;

  nsresult rv = data->InsertData((PRUint32) offset, *value);
  nsMemory::Free(value);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (rv == NS_ERROR_DOM_INDEX_SIZE_ERR ||
         rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "CharacterData.insertData: failed", rv, exceptionType);
    return;
  }
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    replaceData
 * Signature: (IILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_CharacterDataImpl_nativeReplaceData
  (JNIEnv *env, jobject jthis, jint offset, jint count, jstring jvalue)
{
  if (offset < 0 || offset > JavaDOMGlobals::javaMaxInt || 
      count < 0 || count > JavaDOMGlobals::javaMaxInt) {
    JavaDOMGlobals::ThrowException(env, "CharacterData.replaceData: failed",
                 NS_ERROR_DOM_INDEX_SIZE_ERR,
                 JavaDOMGlobals::EXCEPTION_DOM);
    return;
  }

  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data || !jvalue) {
    JavaDOMGlobals::ThrowException(env,  
      "CharacterData.replaceData: NULL pointer");
    return;
  }
  
  nsString* value = JavaDOMGlobals::GetUnicode(env, jvalue);
  if (!value)
    return;

  nsresult rv = data->ReplaceData((PRUint32) offset, (PRUint32) count, *value);
  nsMemory::Free(value);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (rv == NS_ERROR_DOM_INDEX_SIZE_ERR ||
         rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "CharacterData.replaceData: failed", rv, exceptionType);
    return;
  }
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    setData
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_CharacterDataImpl_nativeSetData
  (JNIEnv *env, jobject jthis, jstring jvalue)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data || !jvalue) {
    JavaDOMGlobals::ThrowException(env,  
      "CharacterData.setData: NULL pointer");
    return;
  }
  
  nsString* value = JavaDOMGlobals::GetUnicode(env, jvalue);
  if (!value)
    return;

  nsresult rv = data->SetData(*value);
  nsMemory::Free(value);

  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (rv == NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "CharacterData.setData: failed", rv, exceptionType);
    return;
  }
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    substringData
 * Signature: (II)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_CharacterDataImpl_nativeSubstringData
  (JNIEnv *env, jobject jthis, jint offset, jint count)
{
  if (offset < 0 || offset > JavaDOMGlobals::javaMaxInt || 
      count < 0 || count > JavaDOMGlobals::javaMaxInt) {
    JavaDOMGlobals::ThrowException(env, "CharacterData.substringData: failed",
                 NS_ERROR_DOM_INDEX_SIZE_ERR,
                 JavaDOMGlobals::EXCEPTION_DOM);
    return NULL;
  }

  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    JavaDOMGlobals::ThrowException(env,  
      "CharacterData.substringData: NULL pointer");
    return NULL;
  }

  nsString ret;
  nsresult rv = data->SubstringData((PRUint32) offset, (PRUint32) count, ret);
  if (NS_FAILED(rv)) {
    JavaDOMGlobals::ExceptionType exceptionType = JavaDOMGlobals::EXCEPTION_RUNTIME;
    if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM &&
        (rv == NS_ERROR_DOM_INDEX_SIZE_ERR ||
         rv == NS_ERROR_DOM_DOMSTRING_SIZE_ERR)) {
      exceptionType = JavaDOMGlobals::EXCEPTION_DOM;
    }
    JavaDOMGlobals::ThrowException(env,
      "CharacterData.substringData: failed", rv, exceptionType);
    return NULL;
  }

  jstring jret = env->NewString((jchar*) ret.get(), ret.Length());
  if (!jret) {
    JavaDOMGlobals::ThrowException(env, 
      "CharacterData.substringData: NewString failed");
  }

  return jret;
}
