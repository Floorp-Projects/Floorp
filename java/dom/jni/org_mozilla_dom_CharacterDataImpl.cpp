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
#include "nsIDOMCharacterData.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_CharacterDataImpl.h"

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    appendData
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_CharacterDataImpl_appendData
  (JNIEnv *env, jobject jthis, jstring jvalue)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.appendData: NULL pointer\n"));
    return;
  }
  
  jboolean iscopy = JNI_FALSE;
  const char* value = env->GetStringUTFChars(jvalue, &iscopy);
  if (!value) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.appendData: GetStringUTFChars failed\n"));
    return;
  }

  nsresult rv = data->AppendData(value);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jvalue, value);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.appendData: failed (%x)\n", rv));
    return;
  }
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    deleteData
 * Signature: (II)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_CharacterDataImpl_deleteData
  (JNIEnv *env, jobject jthis, jint offset, jint count)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.deleteData: NULL pointer\n"));
    return;
  }
  
  nsresult rv = data->DeleteData((PRUint32) offset, (PRUint32) count);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.deleteData: failed (%x)\n", rv));
    return;
  }
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_CharacterDataImpl_finalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.finalize: NULL pointer\n"));
    return;
  }

  JavaDOMGlobals::AddToGarbage(data);
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    getData
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_CharacterDataImpl_getData
  (JNIEnv *env, jobject jthis)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.getData: NULL pointer\n"));
    return NULL;
  }
  
  nsString ret;
  nsresult rv = data->GetData(ret);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.getData: failed (%x)\n", rv));
    return NULL;
  }

  char* cret = ret.ToNewCString();
  jstring jret = env->NewStringUTF(cret);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.getData: NewStringUTF failed: %s\n", cret));
  }
  delete[] cret;

  return jret;
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    getLength
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_dom_CharacterDataImpl_getLength
  (JNIEnv *env, jobject jthis)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.getLength: NULL pointer\n"));
    return 0;
  }
  
  PRUint32 len = 0;
  nsresult rv = data->GetLength(&len);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.getLength: failed (%x)\n", rv));
    return 0;
  }

  return (jint) len;
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    insertData
 * Signature: (ILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_CharacterDataImpl_insertData
  (JNIEnv *env, jobject jthis, jint offset, jstring jvalue)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.insertData: NULL pointer\n"));
    return;
  }
  
  jboolean iscopy = JNI_FALSE;
  const char* value = env->GetStringUTFChars(jvalue, &iscopy);
  if (!value) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.insertData: GetStringUTFChars failed\n"));
    return;
  }

  nsresult rv = data->InsertData((PRUint32) offset, value);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jvalue, value);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.insertData: failed (%x)\n", rv));
    return;
  }
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    replaceData
 * Signature: (IILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_CharacterDataImpl_replaceData
  (JNIEnv *env, jobject jthis, jint offset, jint count, jstring jvalue)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.replaceData: NULL pointer\n"));
    return;
  }
  
  jboolean iscopy = JNI_FALSE;
  const char* value = env->GetStringUTFChars(jvalue, &iscopy);
  if (!value) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.replaceData: GetStringUTFChars failed\n"));
    return;
  }

  nsresult rv = data->ReplaceData((PRUint32) offset, (PRUint32) count, value);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jvalue, value);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.replaceData: failed (%x)\n", rv));
    return;
  }
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    setData
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_CharacterDataImpl_setData
  (JNIEnv *env, jobject jthis, jstring jvalue)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.setData: NULL pointer\n"));
    return;
  }
  
  jboolean iscopy = JNI_FALSE;
  const char* value = env->GetStringUTFChars(jvalue, &iscopy);
  if (!value) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.setData: GetStringUTFChars failed\n"));
    return;
  }

  nsresult rv = data->SetData(value);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jvalue, value);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.setData: failed (%x)\n", rv));
    return;
  }
}

/*
 * Class:     org_mozilla_dom_CharacterDataImpl
 * Method:    substringData
 * Signature: (II)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_CharacterDataImpl_substringData
  (JNIEnv *env, jobject jthis, jint offset, jint count)
{
  nsIDOMCharacterData* data = (nsIDOMCharacterData*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!data) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.substringData: NULL pointer\n"));
    return NULL;
  }

  nsString ret;
  nsresult rv = data->SubstringData((PRUint32) offset, (PRUint32) count, ret);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.substringData: failed (%x)\n", rv));
    return NULL;
  }

  char* cret = ret.ToNewCString();
  jstring jret = env->NewStringUTF(cret);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("CharacterData.substringData: NewStringUTF failed: %s\n", cret));
  }
  delete[] cret;

  return jret;
}
