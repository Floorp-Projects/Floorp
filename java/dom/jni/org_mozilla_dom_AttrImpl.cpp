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
#include "nsIDOMAttr.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_AttrImpl.h"

/*
 * Class:     org_mozilla_dom_AttrImpl
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_AttrImpl_getName
  (JNIEnv *env, jobject jthis)
{
  nsIDOMAttr* attr = (nsIDOMAttr*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!attr) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Attr.getName: NULL pointer\n"));
    return NULL;
  }

  nsString name;
  nsresult rv = attr->GetName(name);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Attr.getName: failed (%x)\n", rv));
    return NULL;
  }

  char* cname = name.ToNewCString();
  jstring jname = env->NewStringUTF(cname);
  if (!jname) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Attr.getName: NewStringUTF failed: %s\n", cname));
  }
  delete[] cname;

  return jname;
}

/*
 * Class:     org_mozilla_dom_AttrImpl
 * Method:    getSpecified
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_dom_AttrImpl_getSpecified
  (JNIEnv *env, jobject jthis)
{
  nsIDOMAttr* attr = (nsIDOMAttr*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!attr) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Attr.getSpecified: NULL pointer\n"));
    return JNI_FALSE;
  }

  PRBool specified = PR_FALSE;
  nsresult rv = attr->GetSpecified(&specified);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Attr.getSpecified: failed (%x)\n", rv));
    return JNI_FALSE;
  }

  return (specified == PR_TRUE) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     org_mozilla_dom_AttrImpl
 * Method:    getValue
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_AttrImpl_getValue
  (JNIEnv *env, jobject jthis)
{
  nsIDOMAttr* attr = (nsIDOMAttr*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!attr) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Attr.getValue: NULL pointer\n"));
    return NULL;
  }

  nsString value;
  nsresult rv = attr->GetValue(value);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Attr.getValue: failed (%x)\n", rv));
    return NULL;
  }

  char* cvalue = value.ToNewCString();
  jstring jval = env->NewStringUTF(cvalue);
  if (!jval) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Attr.getValue: NewStringUTF failed: %s\n", cvalue));
  }
  delete[] cvalue;

  return jval;
}

/*
 * Class:     org_mozilla_dom_AttrImpl
 * Method:    setValue
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_AttrImpl_setValue
  (JNIEnv *env, jobject jthis, jstring jval)
{
  nsIDOMAttr* attr = (nsIDOMAttr*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!attr) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Attr.setValue: NULL pointer\n"));
    return;
  }

  jboolean iscopy = JNI_FALSE;
  const char* cvalue = env->GetStringUTFChars(jval, &iscopy);
  if (!cvalue) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Attr.setValue: GetStringUTFChars failed\n"));
    return;
  }

  nsresult rv = attr->SetValue(cvalue);
  if (iscopy == JNI_TRUE)
    env->ReleaseStringUTFChars(jval, cvalue);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Attr.setValue: failed (%x)\n", rv));
    return;
  }
}

/*
 * Class:     org_mozilla_dom_AttrImpl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_AttrImpl_finalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMAttr* attr = (nsIDOMAttr*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!attr) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Attr.finalize: NULL pointer\n"));
    return;
  }

  JavaDOMGlobals::AddToGarbage(attr);
}

