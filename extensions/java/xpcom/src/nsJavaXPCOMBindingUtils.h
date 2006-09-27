/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsJavaXPCOMBindingUtils_h_
#define _nsJavaXPCOMBindingUtils_h_

#include "jni.h"
#include "xptcall.h"
#include "nsCOMPtr.h"

#ifdef DEBUG_pedemonte
#define LOG(...)  printf(__VA_ARGS__)
#else
#define LOG(...)  /* nothing */
#endif


/*********************
 * Java JNI globals
 *********************/
extern jclass classClass;
extern jmethodID getNameMID;

extern jclass objectClass;
extern jmethodID hashCodeMID;

extern jclass booleanClass;
extern jclass booleanArrayClass;
extern jmethodID booleanInitMID;
extern jmethodID booleanValueMID;

extern jclass charClass;
extern jclass charArrayClass;
extern jmethodID charInitMID;
extern jmethodID charValueMID;

extern jclass byteClass;
extern jclass byteArrayClass;
extern jmethodID byteInitMID;
extern jmethodID byteValueMID;

extern jclass shortClass;
extern jclass shortArrayClass;
extern jmethodID shortInitMID;
extern jmethodID shortValueMID;

extern jclass intClass;
extern jclass intArrayClass;
extern jmethodID intInitMID;
extern jmethodID intValueMID;

extern jclass longClass;
extern jclass longArrayClass;
extern jmethodID longInitMID;
extern jmethodID longValueMID;

extern jclass floatClass;
extern jclass floatArrayClass;
extern jmethodID floatInitMID;
extern jmethodID floatValueMID;

extern jclass doubleClass;
extern jclass doubleArrayClass;
extern jmethodID doubleInitMID;
extern jmethodID doubleValueMID;

extern jclass stringClass;
extern jclass stringArrayClass;

extern jclass nsISupportsClass;

extern jclass exceptionClass;

PRBool InitializeJavaGlobals(JNIEnv *env);
void FreeJavaGlobals(JNIEnv* env);


/*************************
 *  JavaXPCOMInstance
 *************************/
class JavaXPCOMInstance
{
public:
	JavaXPCOMInstance(nsISupports* aInstance, nsIInterfaceInfo* aIInfo)
		: mInstance(aInstance),
		  mIInfo(aIInfo)
	{}

	nsISupports* GetInstance()	{ return mInstance; }
	nsIInterfaceInfo* InterfaceInfo();

private:
	nsCOMPtr<nsISupports>      mInstance;
	nsCOMPtr<nsIInterfaceInfo> mIInfo;
};

JavaXPCOMInstance* CreateJavaXPCOMInstance(nsISupports* aXPCOMObject,
                                           const nsIID* aIID);


/**************************************
 *  Java<->XPCOM binding stores
 **************************************/
void          AddJavaXPCOMBinding(JNIEnv* env, jobject aJavaStub,
                                  void* aXPCOMObject);
void          RemoveJavaXPCOMBinding(JNIEnv* env, jobject aJavaObject,
                                     void* aXPCOMObject);
void*         GetMatchingXPCOMObject(JNIEnv* env, jobject aJavaObject);
jobject       GetMatchingJavaObject(JNIEnv* env, void* aXPCOMObject);


void ThrowXPCOMException(JNIEnv* env, int aFailureCode);

nsresult GetIIDForMethodParam(nsIInterfaceInfo *iinfo,
                              const nsXPTMethodInfo *methodInfo,
                              const nsXPTParamInfo &paramInfo,
                              PRUint16 methodIndex,
                              nsXPTCMiniVariant *dispatchParams,
                              PRBool isFullVariantArray,
                              nsID &result);

#endif // _nsJavaXPCOMBindingUtils_h_
