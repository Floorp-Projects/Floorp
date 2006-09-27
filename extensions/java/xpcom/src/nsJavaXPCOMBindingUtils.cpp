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


#include "nsJavaXPCOMBindingUtils.h"
#include "nsJavaXPTCStub.h"
#include "nsJavaWrapper.h"
#include "jni.h"
#include "nsIInterfaceInfoManager.h"
#include "nsILocalFile.h"
#include "nsEventQueueUtils.h"
#include "nsProxyRelease.h"


/* Java JNI globals */
jclass stringClass = nsnull;
jclass nsISupportsClass = nsnull;
jclass xpcomExceptionClass = nsnull;

jmethodID hashCodeMID = nsnull;
jmethodID booleanValueMID = nsnull;
jmethodID charValueMID = nsnull;
jmethodID byteValueMID = nsnull;
jmethodID shortValueMID = nsnull;
jmethodID intValueMID = nsnull;
jmethodID longValueMID = nsnull;
jmethodID floatValueMID = nsnull;
jmethodID doubleValueMID = nsnull;

#ifdef DEBUG
jmethodID getNameMID = nsnull;
#endif

nsJavaXPCOMBindings* gBindings = nsnull;


/**************************************
 *  Java<->XPCOM binding stores
 **************************************/
class JavaXPCOMBindingEntry : public PLDHashEntryHdr
{
public:
  // mKey will either be a Java hash of the Java object, or the address of
  // the XPCOM object, depending on which hash table this entry is used in.
  const void*   mKey;
  jobject mJavaObject;
  void*   mXPCOMInstance;
};

PR_STATIC_CALLBACK(PRBool)
InitJavaXPCOMBindingEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                          const void *key)
{
  JavaXPCOMBindingEntry *e =
    NS_CONST_CAST(JavaXPCOMBindingEntry *,
                  NS_STATIC_CAST(const JavaXPCOMBindingEntry *, entry));

  e->mKey = key;

  return PR_TRUE;
}

NS_IMETHODIMP
nsJavaXPCOMBindings::Init()
{
  static PLDHashTableOps java_to_xpcom_hash_ops =
  {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashGetKeyStub,
    PL_DHashVoidPtrKeyStub,
    PL_DHashMatchEntryStub,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    InitJavaXPCOMBindingEntry
  };

  mJAVAtoXPCOMBindings = PL_NewDHashTable(&java_to_xpcom_hash_ops, nsnull,
                                          sizeof(JavaXPCOMBindingEntry), 16);
  if (!mJAVAtoXPCOMBindings)
    return NS_ERROR_OUT_OF_MEMORY;

  static PLDHashTableOps xpcom_to_java_hash_ops =
  {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashGetKeyStub,
    PL_DHashVoidPtrKeyStub,
    PL_DHashMatchEntryStub,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    InitJavaXPCOMBindingEntry
  };

  mXPCOMtoJAVABindings = PL_NewDHashTable(&xpcom_to_java_hash_ops, nsnull,
                                          sizeof(JavaXPCOMBindingEntry), 16);
  if (!mXPCOMtoJAVABindings)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

nsJavaXPCOMBindings::~nsJavaXPCOMBindings()
{
  if (mJAVAtoXPCOMBindings)
    PL_DHashTableDestroy(mJAVAtoXPCOMBindings);
  if (mXPCOMtoJAVABindings)
    PL_DHashTableDestroy(mXPCOMtoJAVABindings);
}

NS_IMETHODIMP
nsJavaXPCOMBindings::AddBinding(JNIEnv* env, jobject aJavaObject,
                                void* aXPCOMObject)
{
  // We use a Java hash of the Java object as a key since the JVM can return
  // different "addresses" for the same Java object, but the hash code (the
  // result of calling |hashCode()| on the Java object) will always be the same.
  jint hash = env->CallIntMethod(aJavaObject, hashCodeMID);
  jweak java_ref = env->NewWeakGlobalRef(aJavaObject);
  if (!java_ref)
    return NS_ERROR_OUT_OF_MEMORY;

  // Add a new entry into hash
  JavaXPCOMBindingEntry *entry =
    NS_STATIC_CAST(JavaXPCOMBindingEntry*,
                   PL_DHashTableOperate(mJAVAtoXPCOMBindings,
                                        NS_INT32_TO_PTR(hash),
                                        PL_DHASH_ADD));
  if (!entry)
    return NS_ERROR_FAILURE;

  // Set entry fields
  entry->mJavaObject = java_ref;
  entry->mXPCOMInstance = aXPCOMObject;

  // We want the hash key to be the actual XPCOM object
  void* xpcomObjKey = nsnull;
  if (IsXPTCStub(aXPCOMObject))
    xpcomObjKey = GetXPTCStubAddr(aXPCOMObject);
  else
    xpcomObjKey = ((JavaXPCOMInstance*) aXPCOMObject)->GetInstance();

  // Add a new entry into other hash table
  entry =
    NS_STATIC_CAST(JavaXPCOMBindingEntry*,
                   PL_DHashTableOperate(mXPCOMtoJAVABindings, xpcomObjKey,
                                        PL_DHASH_ADD));
  if (!entry)
    return NS_ERROR_FAILURE;

  entry->mJavaObject = java_ref;
  entry->mXPCOMInstance = aXPCOMObject;

  LOG(("+ Adding Java<->XPCOM binding (Java=0x%08x | XPCOM=0x%08x) weakref=0x%08x\n",
       hash, (int) xpcomObjKey, (PRUint32) java_ref));

  return NS_OK;
}

NS_IMETHODIMP
nsJavaXPCOMBindings::RemoveBinding(JNIEnv* env, jobject aJavaObject,
                                   void* aXPCOMObject)
{
  // Given a Java or XPCOM object, find the associated object.
  NS_PRECONDITION(aJavaObject != nsnull || aXPCOMObject != nsnull,
                  "Need either a Java or XPCOM object");

  jint hash = 0;
  void* xpcomObjKey = nsnull;
  JavaXPCOMBindingEntry* entry = nsnull;

  if (aJavaObject) {
    hash = env->CallIntMethod(aJavaObject, hashCodeMID);
    JavaXPCOMBindingEntry* e =
      NS_STATIC_CAST(JavaXPCOMBindingEntry*,
                     PL_DHashTableOperate(mJAVAtoXPCOMBindings,
                                          NS_INT32_TO_PTR(hash),
                                          PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(e))
      entry = e;
  } else {
    JavaXPCOMBindingEntry* e =
      NS_STATIC_CAST(JavaXPCOMBindingEntry*,
                     PL_DHashTableOperate(mXPCOMtoJAVABindings, aXPCOMObject,
                                          PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(e))
      entry = e;
  }
  if (!entry) {
    NS_WARNING("Failed to find matching entry");
    return NS_ERROR_FAILURE;
  }

  jobject jweakref = entry->mJavaObject;
  void* xpcom_obj = entry->mXPCOMInstance;

  // Get keys.  For Java, we use the hash key of the Java object.  For XPCOM,
  // we get the 'actual' object (either the nsJavaXPTCStub or the instance
  // that is wrapped by JavaXPCOMInstance).
  if (hash == 0) {
    hash = env->CallIntMethod(jweakref, hashCodeMID);
  }
  if (aXPCOMObject) {
    xpcomObjKey = aXPCOMObject;
  } else {
    if (IsXPTCStub(xpcom_obj))
      xpcomObjKey = GetXPTCStubAddr(xpcom_obj);
    else
      xpcomObjKey = ((JavaXPCOMInstance*) xpcom_obj)->GetInstance();
  }

  NS_ASSERTION(env->IsSameObject(jweakref, aJavaObject),
               "Weakref does not point to expected Java object");
  NS_ASSERTION(!env->IsSameObject(jweakref, NULL),
               "Weakref refers to garbage collected Java object. No longer valid");

  // Remove both instances from stores
  PL_DHashTableOperate(mJAVAtoXPCOMBindings, NS_INT32_TO_PTR(hash),
                       PL_DHASH_REMOVE);
  PL_DHashTableOperate(mXPCOMtoJAVABindings, xpcomObjKey, PL_DHASH_REMOVE);
  LOG(("- Removing Java<->XPCOM binding (Java=0x%08x | XPCOM=0x%08x) weakref=0x%08x\n",
       hash, (int) xpcomObjKey, (PRUint32) jweakref));

  env->DeleteWeakGlobalRef(NS_STATIC_CAST(jweak, jweakref));
  return NS_OK;
}

void*
nsJavaXPCOMBindings::GetXPCOMObject(JNIEnv* env, jobject aJavaObject)
{
  jint hash = env->CallIntMethod(aJavaObject, hashCodeMID);

  JavaXPCOMBindingEntry *entry =
    NS_STATIC_CAST(JavaXPCOMBindingEntry*,
                   PL_DHashTableOperate(mJAVAtoXPCOMBindings,
                                        NS_INT32_TO_PTR(hash),
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
#ifdef DEBUG_pedemonte
    void* xpcomObjKey = nsnull;
    if (IsXPTCStub(entry->mXPCOMInstance))
      xpcomObjKey = GetXPTCStubAddr(entry->mXPCOMInstance);
    else
      xpcomObjKey = ((JavaXPCOMInstance*) entry->mXPCOMInstance)->GetInstance();
    LOG(("< Get Java<->XPCOM binding (Java=0x%08x | XPCOM=0x%08x)\n",
         hash, (int) xpcomObjKey));
#endif
    return entry->mXPCOMInstance;
  }

  return nsnull;
}

NS_IMETHODIMP
nsJavaXPCOMBindings::GetJavaObject(JNIEnv* env, void* aXPCOMObject,
                                   const nsIID& aIID, PRBool aDoReleaseObject,
                                   jobject* aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  *aResult = nsnull;
  nsISupports* xpcom_obj = NS_STATIC_CAST(nsISupports*, aXPCOMObject);
  nsresult rv;

  nsJavaXPTCStub* stub = nsnull;
  xpcom_obj->QueryInterface(NS_GET_IID(nsJavaXPTCStub), (void**) &stub);
  if (stub) {
    // Get associated Java object directly from nsJavaXPTCStub
    *aResult = stub->GetJavaObject();
    NS_ASSERTION(*aResult != nsnull, "nsJavaXPTCStub w/o matching Java object");
    NS_RELEASE(stub);
    rv = NS_OK;
  } else {
    // Get associated Java object from hash table
    JavaXPCOMBindingEntry *entry =
      NS_STATIC_CAST(JavaXPCOMBindingEntry*,
                     PL_DHashTableOperate(mXPCOMtoJAVABindings, aXPCOMObject,
                                          PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      *aResult = entry->mJavaObject;
      rv = NS_OK;
    }
  }

  if (*aResult == nsnull) {
    // No Java object is associated with the given XPCOM object, so we create
    // a Java proxy.
    rv = CreateJavaProxy(env, xpcom_obj, aIID, aResult);
    if (NS_SUCCEEDED(rv) && aDoReleaseObject)
      NS_RELEASE(xpcom_obj);   // Owning ref passed on
  }

#ifdef DEBUG_pedemonte
  if (*aResult) {
    LOG(("< Get Java<->XPCOM binding (Java=0x%08x | XPCOM=0x%08x)\n",
         env->CallIntMethod(*aResult, hashCodeMID), (int) aXPCOMObject));
  }
#endif

  return rv;
}


/******************************
 *  InitializeJavaGlobals
 ******************************/
PRBool gInitialized = PR_FALSE;

PRBool
InitializeJavaGlobals(JNIEnv *env)
{
  if (gInitialized)
    return PR_TRUE;

  jclass clazz;
  if (!(clazz = env->FindClass("java/lang/Object")) ||
      !(hashCodeMID = env->GetMethodID(clazz, "hashCode","()I")))
  {
    goto init_error;
  }

  if (!(clazz = env->FindClass("java/lang/Boolean")) ||
      !(booleanValueMID = env->GetMethodID(clazz,"booleanValue","()Z")))
  {
    goto init_error;
  }

  if (!(clazz = env->FindClass("java/lang/Character")) ||
      !(charValueMID = env->GetMethodID(clazz,"charValue","()C")))
  {
    goto init_error;
  }

  if (!(clazz = env->FindClass("java/lang/Byte")) ||
      !(byteValueMID = env->GetMethodID(clazz,"byteValue","()B")))
  {
    goto init_error;
  }

  if (!(clazz = env->FindClass("java/lang/Short")) ||
      !(shortValueMID = env->GetMethodID(clazz,"shortValue","()S")))
  {
    goto init_error;
  }

  if (!(clazz = env->FindClass("java/lang/Integer")) ||
      !(intValueMID = env->GetMethodID(clazz,"intValue","()I")))
  {
    goto init_error;
  }

  if (!(clazz = env->FindClass("java/lang/Long")) ||
      !(longValueMID = env->GetMethodID(clazz,"longValue","()J")))
  {
    goto init_error;
  }

  if (!(clazz = env->FindClass("java/lang/Float")) ||
      !(floatValueMID = env->GetMethodID(clazz,"floatValue","()F")))
  {
    goto init_error;
  }

  if (!(clazz = env->FindClass("java/lang/Double")) ||
      !(doubleValueMID = env->GetMethodID(clazz,"doubleValue","()D")))
  {
    goto init_error;
  }

  if (!(clazz = env->FindClass("java/lang/String")) ||
      !(stringClass = (jclass) env->NewGlobalRef(clazz)))
  {
    goto init_error;
  }

  if (!(clazz = env->FindClass("org/mozilla/xpcom/nsISupports")) ||
      !(nsISupportsClass = (jclass) env->NewGlobalRef(clazz)))
  {
    goto init_error;
  }

  if (!(clazz = env->FindClass("org/mozilla/xpcom/XPCOMException")) ||
      !(xpcomExceptionClass = (jclass) env->NewGlobalRef(clazz)))
  {
    goto init_error;
  }

#ifdef DEBUG
  if (!(clazz = env->FindClass("java/lang/Class")) ||
      !(getNameMID = env->GetMethodID(clazz, "getName","()Ljava/lang/String;")))
  {
    goto init_error;
  }
#endif

  gBindings = new nsJavaXPCOMBindings();
  if (NS_FAILED(gBindings->Init())) {
    goto init_error;
  }

  gInitialized = PR_TRUE;
  return PR_TRUE;

init_error:
  // If we encounter an error during initialization, then free any globals that
  // were allocated, and return false.
  FreeJavaGlobals(env);
  return PR_FALSE;
}

/*************************
 *    FreeJavaGlobals
 *************************/
void
FreeJavaGlobals(JNIEnv* env)
{
  if (stringClass) {
    env->DeleteGlobalRef(stringClass);
    stringClass = nsnull;
  }
  if (nsISupportsClass) {
    env->DeleteGlobalRef(nsISupportsClass);
    nsISupportsClass = nsnull;
  }
  if (xpcomExceptionClass) {
    env->DeleteGlobalRef(xpcomExceptionClass);
    xpcomExceptionClass = nsnull;
  }

  if (gBindings) {
    delete gBindings;
    gBindings = nsnull;
  }

  gInitialized = PR_FALSE;
}


/**********************************************************
 *    JavaXPCOMInstance
 *********************************************************/
JavaXPCOMInstance::JavaXPCOMInstance(nsISupports* aInstance,
                                     nsIInterfaceInfo* aIInfo)
    : mInstance(aInstance),
      mIInfo(aIInfo)
{
  NS_ADDREF(mInstance);
}

JavaXPCOMInstance::~JavaXPCOMInstance()
{
  nsCOMPtr<nsIEventQueue> eventQ;
  nsresult rv = NS_GetMainEventQ(getter_AddRefs(eventQ));
  if (NS_SUCCEEDED(rv))
    rv = NS_ProxyRelease(eventQ, mInstance);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to release using NS_ProxyRelease");
}


nsresult
GetIIDForMethodParam(nsIInterfaceInfo *iinfo, const nsXPTMethodInfo *methodInfo,
                     const nsXPTParamInfo &paramInfo, PRUint16 methodIndex,
                     nsXPTCMiniVariant *dispatchParams,
                     PRBool isFullVariantArray, nsID &result)
{
  nsresult rv;

  switch (paramInfo.GetType().TagPart())
  {
    case nsXPTType::T_INTERFACE:
      rv = iinfo->GetIIDForParamNoAlloc(methodIndex, &paramInfo, &result);
      break;

    case nsXPTType::T_INTERFACE_IS:
    {
      PRUint8 argnum;
      rv = iinfo->GetInterfaceIsArgNumberForParam(methodIndex, &paramInfo, &argnum);
      if (NS_FAILED(rv))
        break;

      const nsXPTParamInfo& arg_param = methodInfo->GetParam(argnum);
      const nsXPTType& arg_type = arg_param.GetType();

      // The xpidl compiler ensures this. We reaffirm it for safety.
      if (!arg_type.IsPointer() || arg_type.TagPart() != nsXPTType::T_IID) {
        rv = NS_ERROR_UNEXPECTED;
        break;
      }

      nsID *p = nsnull;
      if (isFullVariantArray) {
        p = (nsID *) ((nsXPTCVariant*) dispatchParams)[argnum].val.p;
      } else {
        p = (nsID *) dispatchParams[argnum].val.p;
      }
      if (!p)
        return NS_ERROR_UNEXPECTED;

      result = *p;
      break;
    }

    default:
      rv = NS_ERROR_UNEXPECTED;
  }
  return rv;
}


/*******************************
 *  JNI helper functions
 *******************************/

void
ThrowException(JNIEnv* env, const nsresult aErrorCode, const char* aMessage)
{
  // Only throw this exception if one hasn't already been thrown, so we don't
  // mask a previous exception/error.
  if (env->ExceptionCheck())
    return;

  // If the error code we get is for an Out Of Memory error, try to throw an
  // OutOfMemoryError.  The JVM may have enough memory to create this error.
  if (aErrorCode == NS_ERROR_OUT_OF_MEMORY) {
    jclass clazz = env->FindClass("java/lang/OutOfMemoryError");
    if (clazz) {
      env->ThrowNew(clazz, aMessage);
    }
    env->DeleteLocalRef(clazz);
    return;
  }

  // If the error was not handled above, then create an XPCOMException with the
  // given error code and message.

  // Create parameters and method signature. Max of 2 params.  The error code
  // comes before the message string.
  PRUint32 index = 0;
  jvalue* args = new jvalue[2];
  if (!args) {
    ThrowException(env, NS_ERROR_OUT_OF_MEMORY,
                   "Out of memory while throwing another exception");
    return;
  }

  nsCAutoString methodSig("(");
  if (aErrorCode) {
    args[index++].j = aErrorCode;
    methodSig.Append('J');
  }
  if (aMessage) {
    args[index].l = env->NewStringUTF(aMessage);
    if (args[index].l == nsnull) {
      delete[] args;
      return;
    }
    methodSig.AppendLiteral("Ljava/lang/String;");
  }
  methodSig.AppendLiteral(")V");

  // create exception object
  jthrowable throwObj = nsnull;
  jmethodID mid = env->GetMethodID(xpcomExceptionClass, "<init>",
                                   methodSig.get());
  if (mid) {
    throwObj = (jthrowable) env->NewObjectA(xpcomExceptionClass, mid, args);
  }
  NS_ASSERTION(throwObj, "Failed to create XPCOMException object");

  // throw exception
  if (throwObj) {
    env->Throw(throwObj);
  }

  // cleanup
  delete[] args;
}

nsAString*
jstring_to_nsAString(JNIEnv* env, jstring aString)
{
  const PRUnichar* buf = nsnull;
  if (aString) {
    buf = env->GetStringChars(aString, nsnull);
    if (!buf)
      return nsnull;  // exception already thrown
  }

  nsString* str = new nsString(buf);
  env->ReleaseStringChars(aString, buf);

  // returns string, or nsnull if 'new' failed
  return str;
}

nsACString*
jstring_to_nsACString(JNIEnv* env, jstring aString)
{
  const char* buf = nsnull;
  if (aString) {
    buf = env->GetStringUTFChars(aString, nsnull);
    if (!buf)
      return nsnull;  // exception already thrown
  }

  nsCString* str = new nsCString(buf);
  env->ReleaseStringUTFChars(aString, buf);

  // returns string, or nsnull if 'new' failed
  return str;
}

nsresult
File_to_nsILocalFile(JNIEnv* env, jobject aFile, nsILocalFile** aLocalFile)
{
  nsresult rv = NS_ERROR_FAILURE;
  jstring pathName = nsnull;
  jclass clazz = env->FindClass("java/io/File");
  if (clazz) {
    jmethodID pathMID = env->GetMethodID(clazz, "getCanonicalPath",
                                         "()Ljava/lang/String;");
    if (pathMID) {
      pathName = (jstring) env->CallObjectMethod(aFile, pathMID);
      if (pathName != nsnull && !env->ExceptionCheck())
        rv = NS_OK;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    nsAString* path = jstring_to_nsAString(env, pathName);
    if (!path)
      rv = NS_ERROR_OUT_OF_MEMORY;

    if (NS_SUCCEEDED(rv)) {
      rv = NS_NewLocalFile(*path, false, aLocalFile);
      delete path;
    }
  }

  return rv;
}

