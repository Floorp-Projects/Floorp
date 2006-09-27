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
#include "nsString.h"

#ifdef DEBUG_pedemonte
#define LOG(x)  printf x
#else
#define LOG(x)  /* nothing */
#endif


/*********************
 * Java JNI globals
 *********************/
extern jclass intClass;
extern jclass intArrayClass;
extern jclass stringClass;
extern jclass nsISupportsClass;
extern jclass xpcomExceptionClass;

extern jmethodID hashCodeMID;
extern jmethodID booleanValueMID;
extern jmethodID charValueMID;
extern jmethodID byteValueMID;
extern jmethodID shortValueMID;
extern jmethodID intValueMID;
extern jmethodID longValueMID;
extern jmethodID floatValueMID;
extern jmethodID doubleValueMID;

#ifdef DEBUG
extern jmethodID getNameMID;
#endif

PRBool InitializeJavaGlobals(JNIEnv *env);
void FreeJavaGlobals(JNIEnv* env);


/*************************
 *  JavaXPCOMInstance
 *************************/
class JavaXPCOMInstance
{
public:
  JavaXPCOMInstance(nsISupports* aInstance, nsIInterfaceInfo* aIInfo);
  ~JavaXPCOMInstance();

	nsISupports* GetInstance()	{ return mInstance; }
	nsIInterfaceInfo* InterfaceInfo() { return mIInfo; }

private:
	nsISupports*               mInstance;
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

/*******************************
 *  JNI helper functions
 *******************************/
// java.lang.String to nsAString/nsACString
nsAString* jstring_to_nsAString(JNIEnv* env, jstring aString);
nsACString* jstring_to_nsACString(JNIEnv* env, jstring aString);

// java.io.File to nsILocalFile
nsresult File_to_nsILocalFile(JNIEnv* env, jobject aFile,
                              nsILocalFile** aLocalFile);

#endif // _nsJavaXPCOMBindingUtils_h_
