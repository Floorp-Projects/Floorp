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
#include "pldhash.h"

#ifdef DEBUG_pedemonte
#define LOG(x)  printf x
#else
#define LOG(x)  /* nothing */
#endif

#if defined(XP_WIN) || defined(XP_MAC)
#define JX_EXPORT   JNIEXPORT
#else
#define JX_EXPORT   JNIEXPORT NS_EXPORT
#endif


/*********************
 * Java JNI globals
 *********************/
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

class nsJavaXPCOMBindings;
extern nsJavaXPCOMBindings* gBindings;

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

  nsISupports* GetInstance()  { return mInstance; }
  nsIInterfaceInfo* InterfaceInfo() { return mIInfo; }

private:
  nsISupports*               mInstance;
  nsCOMPtr<nsIInterfaceInfo> mIInfo;
};

/**
 * Creates a new JavaXPCOMInstance which holds the xpcom object and its
 * interface info based on the given IID.
 *
 * @param aXPCOMObject  xpcom object to wrap
 * @param aIID          interface IID for aXPCOMObject
 * @param aResult       pointer that will hold new JavaXPCOMInstance
 *
 * @return NS_ERROR_OUT_OF_MEMORY if out of memory;
 *         NS_ERROR_FAILURE for any other error condition
 */
nsresult CreateJavaXPCOMInstance(nsISupports* aXPCOMObject, const nsIID* aIID,
                                 JavaXPCOMInstance** aResult);


/**************************************
 *  Java<->XPCOM binding stores
 **************************************/
// This class is used to store the associations between existing Java object
// and XPCOM objects.
class nsJavaXPCOMBindings
{
public:
  nsJavaXPCOMBindings()
    : mJAVAtoXPCOMBindings(nsnull)
    , mXPCOMtoJAVABindings(nsnull)
  { }
  ~nsJavaXPCOMBindings();

  // Initializes internal structures.
  NS_IMETHOD Init();

  // Associates the given Java object with the given XPCOM object
  NS_IMETHOD AddBinding(JNIEnv* env, jobject aJavaStub, void* aXPCOMObject);

  // Given either a Java object or XPCOM object, removes the association
  // between the two.
  NS_IMETHOD RemoveBinding(JNIEnv* env, jobject aJavaObject, void* aXPCOMObject);

  // Given a Java object, returns the associated XPCOM object.
  void* GetXPCOMObject(JNIEnv* env, jobject aJavaObject);

  // Given an XPCOM object, returns the associated Java Object.
  jobject GetJavaObject(JNIEnv* env, void* aXPCOMObject);

private:
  PLDHashTable* mJAVAtoXPCOMBindings;
  PLDHashTable* mXPCOMtoJAVABindings;
};



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

/**
 * Constructs and throws an exception.  Some error codes (such as
 * NS_ERROR_OUT_OF_MEMORY) are handled by the appropriate Java exception/error.
 * Otherwise, an instance of XPCOMException is created with the given error
 * code and message.
 *
 * @param env         Java environment pointer
 * @param aErrorCode  The error code returned by an XPCOM/Gecko function. Pass
 *                    zero for the default behaviour.
 * @param aMessage    A string that provides details for throwing this
 *                    exception. Pass in <code>nsnull</code> for the default
 *                    behaviour.
 *
 * @throws OutOfMemoryError if aErrorCode == NS_ERROR_OUT_OF_MEMORY
 *         XPCOMException   for all other error codes
 */
void ThrowException(JNIEnv* env, const nsresult aErrorCode,
                    const char* aMessage);

/**
 * Helper functions for converting from java.lang.String to
 * nsAString/nsACstring.
 *
 * @param env       Java environment pointer
 * @param aString   Java string to convert
 *
 * @return  nsAString/nsACString with same content as given Java string; or
 *          <code>nsnull</code> if out of memory
 */
nsAString* jstring_to_nsAString(JNIEnv* env, jstring aString);
nsACString* jstring_to_nsACString(JNIEnv* env, jstring aString);

/**
 * Helper function for converting from java.io.File to nsILocalFile.
 *
 * @param env         Java environment pointer
 * @param aFile       Java File to convert
 * @param aLocalFile  returns the converted nsILocalFile
 *
 * @return  NS_OK for success; other values indicate error in conversion
 */
nsresult File_to_nsILocalFile(JNIEnv* env, jobject aFile,
                              nsILocalFile** aLocalFile);

#endif // _nsJavaXPCOMBindingUtils_h_
