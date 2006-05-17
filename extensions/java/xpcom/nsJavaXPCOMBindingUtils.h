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
 * Portions created by the Initial Developer are Copyright (C) 2005
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
#include "nsJavaXPTCStub.h"
#include "nsAutoLock.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

//#define DEBUG_JAVAXPCOM
//#define DEBUG_JAVAXPCOM_REFCNT

#ifdef DEBUG_JAVAXPCOM
#define LOG(x)  printf x
#else
#define LOG(x)  /* nothing */
#endif


/*********************
 * Java JNI globals
 *********************/

extern jclass booleanClass;
extern jclass charClass;
extern jclass byteClass;
extern jclass shortClass;
extern jclass intClass;
extern jclass longClass;
extern jclass floatClass;
extern jclass doubleClass;
extern jclass stringClass;
extern jclass nsISupportsClass;
extern jclass xpcomExceptionClass;
extern jclass xpcomJavaProxyClass;

extern jmethodID hashCodeMID;
extern jmethodID booleanValueMID;
extern jmethodID booleanInitMID;
extern jmethodID charValueMID;
extern jmethodID charInitMID;
extern jmethodID byteValueMID;
extern jmethodID byteInitMID;
extern jmethodID shortValueMID;
extern jmethodID shortInitMID;
extern jmethodID intValueMID;
extern jmethodID intInitMID;
extern jmethodID longValueMID;
extern jmethodID longInitMID;
extern jmethodID floatValueMID;
extern jmethodID floatInitMID;
extern jmethodID doubleValueMID;
extern jmethodID doubleInitMID;
extern jmethodID createProxyMID;
extern jmethodID isXPCOMJavaProxyMID;
extern jmethodID getNativeXPCOMInstMID;

#ifdef DEBUG_JAVAXPCOM
extern jmethodID getNameMID;
extern jmethodID proxyToStringMID;
#endif

class NativeToJavaProxyMap;
extern NativeToJavaProxyMap* gNativeToJavaProxyMap;
class JavaToXPTCStubMap;
extern JavaToXPTCStubMap* gJavaToXPTCStubMap;

extern nsTHashtable<nsDepCharHashKey>* gJavaKeywords;

// The Java garbage collector runs in a separate thread.  Since it calls the
// finalizeProxy() function in nsJavaWrapper.cpp, we need to make sure that
// all the structures touched by finalizeProxy() are multithread aware.
extern PRLock* gJavaXPCOMLock;

extern PRBool gJavaXPCOMInitialized;

/**
 * Initialize global structures used by JavaXPCOM.
 * @param env   Java environment pointer
 * @return PR_TRUE if JavaXPCOM is initialized; PR_FALSE if an error occurred
 */
PRBool InitializeJavaGlobals(JNIEnv *env);

/**
 * Frees global structures that were allocated by InitializeJavaGlobals().
 * @param env   Java environment pointer
 */
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
  nsISupports*        mInstance;
  nsIInterfaceInfo*   mIInfo;
};


/**************************************
 *  Java<->XPCOM object mappings
 **************************************/

/**
 * Maps native XPCOM objects to their associated Java proxy object.
 */
class NativeToJavaProxyMap
{
  friend PLDHashOperator DestroyJavaProxyMappingEnum(PLDHashTable* aTable,
                                                     PLDHashEntryHdr* aHeader,
                                                     PRUint32 aNumber,
                                                     void* aData);

protected:
  struct ProxyList
  {
    ProxyList(const jobject aRef, const nsIID& aIID, ProxyList* aList)
      : javaObject(aRef)
      , iid(aIID)
      , next(aList)
    { }

    const jobject   javaObject;
    const nsIID     iid;
    ProxyList*      next;
  };

  struct Entry : public PLDHashEntryHdr
  {
    nsISupports*  key;
    ProxyList*    list;
  };

public:
  NativeToJavaProxyMap()
    : mHashTable(nsnull)
  { }

  ~NativeToJavaProxyMap()
  {
    NS_ASSERTION(mHashTable == nsnull,
                 "MUST call Destroy() before deleting object");
  }

  nsresult Init();

  nsresult Destroy(JNIEnv* env);

  nsresult Add(JNIEnv* env, nsISupports* aXPCOMObject, const nsIID& aIID,
               jobject aProxy);

  nsresult Find(JNIEnv* env, nsISupports* aNativeObject, const nsIID& aIID,
                jobject* aResult);

  nsresult Remove(JNIEnv* env, nsISupports* aNativeObject, const nsIID& aIID);

protected:
  PLDHashTable* mHashTable;
};

/**
 * Maps Java objects to their associated nsJavaXPTCStub.
 */
class JavaToXPTCStubMap
{
  friend PLDHashOperator DestroyXPTCMappingEnum(PLDHashTable* aTable,
                                                PLDHashEntryHdr* aHeader,
                                                PRUint32 aNumber, void* aData);

protected:
  struct Entry : public PLDHashEntryHdr
  {
    jint              key;
    nsJavaXPTCStub*   xptcstub;
  };

public:
  JavaToXPTCStubMap()
    : mHashTable(nsnull)
  { }

  ~JavaToXPTCStubMap()
  {
    NS_ASSERTION(mHashTable == nsnull,
                 "MUST call Destroy() before deleting object");
  }

  nsresult Init();

  nsresult Destroy();

  nsresult Add(JNIEnv* env, jobject aJavaObject, nsJavaXPTCStub* aProxy);

  nsresult Find(JNIEnv* env, jobject aJavaObject, const nsIID& aIID,
                nsJavaXPTCStub** aResult);

  nsresult Remove(JNIEnv* env, jobject aJavaObject);

protected:
  PLDHashTable* mHashTable;
};


/*******************************
 *  Helper functions
 *******************************/

/**
 * Finds the associated Java object for the given XPCOM object and IID.  If no
 * such Java object exists, then it creates one.
 *
 * @param env           Java environment pointer
 * @param aXPCOMObject  XPCOM object for which to find/create Java object
 * @param aIID          desired interface IID for Java object
 * @param aResult       on success, holds reference to Java object
 *
 * @return  NS_OK if succeeded; all other return values are error codes.
 */
nsresult GetNewOrUsedJavaObject(JNIEnv* env, nsISupports* aXPCOMObject,
                                const nsIID& aIID, jobject* aResult);

/**
 * Finds the associated XPCOM object for the given Java object and IID.  If no
 * such XPCOM object exists, then it creates one.
 *
 * @param env           Java environment pointer
 * @param aJavaObject   Java object for which to find/create XPCOM object
 * @param aIID          desired interface IID for XPCOM object
 * @param aResult       on success, holds AddRef'd reference to XPCOM object
 * @param aIsXPTCStub   on success, holds PR_TRUE if aResult points to XPTCStub;
 *                      PR_FALSE if aResult points to native XPCOM object
 *
 * @return  NS_OK if succeeded; all other return values are error codes.
 */
nsresult GetNewOrUsedXPCOMObject(JNIEnv* env, jobject aJavaObject,
                                 const nsIID& aIID, nsISupports** aResult,
                                 PRBool* aIsXPTCStub);

nsresult GetIIDForMethodParam(nsIInterfaceInfo *iinfo,
                              const nsXPTMethodInfo *methodInfo,
                              const nsXPTParamInfo &paramInfo,
                              PRUint8 paramType, PRUint16 methodIndex,
                              nsXPTCMiniVariant *dispatchParams,
                              PRBool isFullVariantArray,
                              nsID &result);


/*******************************
 *  JNI helper functions
 *******************************/

/**
 * Returns a pointer to the appropriate JNIEnv structure.  This function is
 * useful in callbacks or other functions that are not called directly from
 * Java and therefore do not have the JNIEnv structure passed in.
 *
 * @return  pointer to JNIEnv structure for current thread
 */
JNIEnv* GetJNIEnv();

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
 * nsAString/nsACstring.  Caller must delete nsAString/nsACString.
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
