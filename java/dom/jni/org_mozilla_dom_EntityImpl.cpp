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
#include "nsIDOMEntity.h"
#include "javaDOMGlobals.h"
#include "org_mozilla_dom_EntityImpl.h"

/*
 * Class:     org_mozilla_dom_EntityImpl
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_dom_EntityImpl_finalize
  (JNIEnv *env, jobject jthis)
{
  nsIDOMEntity* entity = (nsIDOMEntity*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!entity) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Entity.finalize: NULL pointer\n"));
    return;
  }

  JavaDOMGlobals::AddToGarbage(entity);
}

/*
 * Class:     org_mozilla_dom_EntityImpl
 * Method:    getNotationName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_EntityImpl_getNotationName
  (JNIEnv *env, jobject jthis)
{
  nsIDOMEntity* entity = (nsIDOMEntity*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!entity) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Entity.getNotationName: NULL pointer\n"));
    return NULL;
  }

  nsString ret;
  nsresult rv = entity->GetNotationName(ret);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Entity.getNotationName: failed (%x)\n", rv));
    return NULL;
  }

  char* cret = ret.ToNewCString();
  jstring jret = env->NewStringUTF(cret);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Entity.getNotationName: NewStringUTF failed (%s)\n", cret));
    return NULL;
  }
  delete[] cret;

  return jret;
}

/*
 * Class:     org_mozilla_dom_EntityImpl
 * Method:    getPublicId
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_EntityImpl_getPublicId
  (JNIEnv *env, jobject jthis)
{
  nsIDOMEntity* entity = (nsIDOMEntity*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!entity) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Entity.getPublicId: NULL pointer\n"));
    return NULL;
  }

  nsString ret;
  nsresult rv = entity->GetPublicId(ret);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Entity.getPublicId: failed (%x)\n", rv));
    return NULL;
  }

  char* cret = ret.ToNewCString();
  jstring jret = env->NewStringUTF(cret);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Entity.getPublicId: NewStringUTF failed (%s)\n", cret));
    return NULL;
  }
  delete[] cret;

  return jret;
}

/*
 * Class:     org_mozilla_dom_EntityImpl
 * Method:    getSystemId
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_dom_EntityImpl_getSystemId
  (JNIEnv *env, jobject jthis)
{
  nsIDOMEntity* entity = (nsIDOMEntity*) 
    env->GetLongField(jthis, JavaDOMGlobals::nodePtrFID);
  if (!entity) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_WARNING, 
	   ("Entity.getSystemId: NULL pointer\n"));
    return NULL;
  }

  nsString ret;
  nsresult rv = entity->GetSystemId(ret);
  if (NS_FAILED(rv)) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Entity.getSystemId: failed (%x)\n", rv));
    return NULL;
  }

  char* cret = ret.ToNewCString();
  jstring jret = env->NewStringUTF(cret);
  if (!jret) {
    PR_LOG(JavaDOMGlobals::log, PR_LOG_ERROR, 
	   ("Entity.getSystemId: NewStringUTF failed (%s)\n", cret));
    return NULL;
  }
  delete[] cret;

  return jret;
}
