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

#include "nsJavaXPTCStub.h"
#include "nsJavaWrapper.h"
#include "nsJavaXPCOMBindingUtils.h"
#include "prmem.h"
#include "nsIInterfaceInfoManager.h"
#include "nsString.h"
#include "nsString.h"
#include "nsCRT.h"


nsJavaXPTCStub::nsJavaXPTCStub(JNIEnv* aJavaEnv, jobject aJavaObject,
                               nsIInterfaceInfo *aIInfo)
  : mJavaEnv(aJavaEnv)
  , mIInfo(aIInfo)
  , mMaster(nsnull)
{
  mJavaObject = aJavaEnv->NewGlobalRef(aJavaObject);

#ifdef DEBUG
  jboolean isCopy;
  nsID* iid = nsnull;
  char* iid_str = nsnull;
  jclass clazz = mJavaEnv->GetObjectClass(mJavaObject);
  jstring name = (jstring) mJavaEnv->CallObjectMethod(clazz, getNameMID);
  const char* javaObjectName = mJavaEnv->GetStringUTFChars(name, &isCopy);
  if (mIInfo) {
    mIInfo->GetInterfaceIID(&iid);
    if (iid) {
      iid_str = iid->ToString();
      nsMemory::Free(iid);
    }
  }
  LOG("+++ nsJavaXPTCStub(this=0x%08x java_obj=0x%08x %s iid=%s)\n", (int) this,
      mJavaEnv->CallIntMethod(mJavaObject, hashCodeMID), javaObjectName,
      iid_str ? iid_str : "NULL");
  if (isCopy)
    mJavaEnv->ReleaseStringUTFChars(name, javaObjectName);
  if (iid_str)
    nsMemory::Free(iid_str);
#endif
}

nsJavaXPTCStub::~nsJavaXPTCStub()
{
#ifdef DEBUG
  jboolean isCopy;
  jclass clazz = mJavaEnv->GetObjectClass(mJavaObject);
  jstring name = (jstring) mJavaEnv->CallObjectMethod(clazz, getNameMID);
  const char* javaObjectName = mJavaEnv->GetStringUTFChars(name, &isCopy);
  LOG("--- ~nsJavaXPTCStub(this=0x%08x java_obj=0x%08x %s)\n", (int) this,
      mJavaEnv->CallIntMethod(mJavaObject, hashCodeMID), javaObjectName);
  if (isCopy)
    mJavaEnv->ReleaseStringUTFChars(name, javaObjectName);
#endif

  if (!mMaster) {
    // delete each child stub
    for (PRInt32 i = 0; i < mChildren.Count(); i++) {
      delete (nsJavaXPTCStub*) mChildren[i];
    }

    RemoveJavaXPCOMBinding(mJavaEnv, mJavaObject, this);
  }

  mJavaEnv->DeleteGlobalRef(mJavaObject);
}

NS_IMETHODIMP_(nsrefcnt)
nsJavaXPTCStub::AddRef()
{
  // if this is a child
  if (mMaster)
    return mMaster->AddRef();

  // if this is the master interface
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");
  NS_ASSERT_OWNINGTHREAD(nsJavaXPTCStub);
  ++mRefCnt;
  NS_LOG_ADDREF(this, mRefCnt, "nsJavaXPTCStub", sizeof(*this));
  return mRefCnt;
}

NS_IMETHODIMP_(nsrefcnt)
nsJavaXPTCStub::Release()
{
  // if this is a child
  if (mMaster)
    return mMaster->Release();

  NS_PRECONDITION(0 != mRefCnt, "dup release");
  NS_ASSERT_OWNINGTHREAD(nsJavaXPTCStub);
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "nsJavaXPTCStub");
  if (mRefCnt == 0) {
    mRefCnt = 1; /* stabilize */
    delete this;
    return 0;
  }
  return mRefCnt;
}

NS_IMETHODIMP
nsJavaXPTCStub::QueryInterface(const nsID &aIID, void **aInstancePtr)
{
  LOG("JavaStub::QueryInterface()\n");
  nsJavaXPTCStub *master = mMaster ? mMaster : this;

  // This helps us differentiate between the help classes.
  if (aIID.Equals(NS_GET_IID(nsJavaXPTCStub)))
  {
    *aInstancePtr = master;
    NS_ADDREF(this);
    return NS_OK;
  }

  // always return the master stub for nsISupports
  if (aIID.Equals(NS_GET_IID(nsISupports)))
  {
    *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                   NS_STATIC_CAST(nsXPTCStubBase*, master));
    NS_ADDREF(master);
    return NS_OK;
  }

  // All Java objects support weak references
  if (aIID.Equals(NS_GET_IID(nsISupportsWeakReference)))
  {
    *aInstancePtr = NS_STATIC_CAST(nsISupportsWeakReference*, master);
    NS_ADDREF(master);
    return NS_OK;
  }

  // does any existing stub support the requested IID?
  nsJavaXPTCStub *stub = master->FindStubSupportingIID(aIID);
  if (stub)
  {
    *aInstancePtr = stub;
    NS_ADDREF(stub);
    return NS_OK;
  }

  // Query Java object
  LOG("\tCalling Java object queryInterface\n");
  jclass clazz = mJavaEnv->GetObjectClass(mJavaObject);
  char* sig = "(Ljava/lang/String;)Lorg/mozilla/xpcom/nsISupports;";
  jmethodID qiMID = mJavaEnv->GetMethodID(clazz, "queryInterface", sig);
  NS_ASSERTION(qiMID, "Failed to get queryInterface method ID");

  char* iid_str = aIID.ToString();
  jstring iid_jstr = mJavaEnv->NewStringUTF(iid_str);
  PR_Free(iid_str);
  jobject obj = mJavaEnv->CallObjectMethod(mJavaObject, qiMID, iid_jstr);

  if (obj) {
    // Get interface info for new java object
    nsCOMPtr<nsIInterfaceInfoManager> iim = XPTI_GetInterfaceInfoManager();
    nsCOMPtr<nsIInterfaceInfo> iinfo;
    iim->GetInfoForIID(&aIID, getter_AddRefs(iinfo));

    nsJavaXPTCStub* stub = new nsJavaXPTCStub(mJavaEnv, obj, iinfo);

    // add stub to the master's list of children, so we can preserve
    // symmetry in future QI calls.  the master will delete each child
    // when it is destroyed.  the refcount of each child is bound to
    // the refcount of the master.  this is done to deal with code
    // like this:
    //
    //   nsCOMPtr<nsIBar> bar = ...;
    //   nsIFoo *foo;
    //   {
    //     nsCOMPtr<nsIFoo> temp = do_QueryInterface(bar);
    //     foo = temp;
    //   }
    //   foo->DoStuff();
    //
    // while this code is not valid XPCOM (since it is using |foo|
    // after having called Release on it), such code is unfortunately
    // very common in the mozilla codebase.  the assumption this code
    // is making is that so long as |bar| is alive, it should be valid
    // to access |foo| even if the code doesn't own a strong reference
    // to |foo|!  clearly wrong, but we need to support it anyways.

    stub->mMaster = master;
    master->mChildren.AppendElement(stub);

    *aInstancePtr = stub;
    NS_ADDREF(stub);
    return NS_OK;
  }

  *aInstancePtr = nsnull;
  return NS_NOINTERFACE;
}

PRBool
nsJavaXPTCStub::SupportsIID(const nsID &iid)
{
  PRBool match;
  nsCOMPtr<nsIInterfaceInfo> iter = mIInfo;
  do
  {
    if (NS_SUCCEEDED(iter->IsIID(&iid, &match)) && match)
      return PR_TRUE;

    nsCOMPtr<nsIInterfaceInfo> parent;
    iter->GetParent(getter_AddRefs(parent));
    iter = parent;
  }
  while (iter != nsnull);

  return PR_FALSE;
}

nsJavaXPTCStub *
nsJavaXPTCStub::FindStubSupportingIID(const nsID &iid)
{
  NS_ASSERTION(mMaster == nsnull, "this is not a master stub");

  if (SupportsIID(iid))
    return this;

  for (PRInt32 i = 0; i < mChildren.Count(); i++)
  {
    nsJavaXPTCStub *child = (nsJavaXPTCStub *) mChildren[i];
    if (child->SupportsIID(iid))
      return child;
  }
  return nsnull;
}

NS_IMETHODIMP
nsJavaXPTCStub::GetInterfaceInfo(nsIInterfaceInfo **aInfo)
{
  NS_ADDREF(*aInfo = mIInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsJavaXPTCStub::CallMethod(PRUint16 aMethodIndex,
                         const nsXPTMethodInfo *aMethodInfo,
                         nsXPTCMiniVariant *aParams)
{
#ifdef DEBUG
  const char* ifaceName;
  mIInfo->GetNameShared(&ifaceName);
  LOG("nsJavaXPTCStub::CallMethod [%s::%s]\n", ifaceName, aMethodInfo->GetName());
#endif

  nsresult rv = NS_OK;

  nsCAutoString methodSig("(");

  // Create jvalue array to hold Java params
  PRUint8 paramCount = aMethodInfo->GetParamCount();
  jvalue* java_params = nsnull;
  const nsXPTParamInfo* retvalInfo = nsnull;
  if (paramCount) {
    java_params = new jvalue[paramCount];

    for (PRUint8 i = 0; i < paramCount && NS_SUCCEEDED(rv); i++)
    {
      const nsXPTParamInfo &paramInfo = aMethodInfo->GetParam(i);
      if (!paramInfo.IsRetval()) {
        rv = SetupJavaParams(paramInfo, aMethodInfo, aMethodIndex, aParams,
                             aParams[i], java_params[i], methodSig);
      } else {
        retvalInfo = &paramInfo;
      }
    }
    NS_ASSERTION(NS_SUCCEEDED(rv), "SetupJavaParams failed");
  }

  // Finish method signature
  if (NS_SUCCEEDED(rv)) {
    methodSig.Append(")");
    if (retvalInfo) {
      nsCAutoString retvalSig;
      rv = GetRetvalSig(retvalInfo, retvalSig);
      methodSig.Append(retvalSig);
    } else {
      methodSig.Append("V");
    }
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetRetvalSig failed");
  }

  // Get Java method to call
  jmethodID mid = nsnull;
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString methodName;
    if (aMethodInfo->IsGetter() || aMethodInfo->IsSetter()) {
      if (aMethodInfo->IsGetter())
        methodName.Append("get");
      else
        methodName.Append("set");
      methodName.Append(aMethodInfo->GetName());
      methodName.SetCharAt(toupper(methodName[3]), 3);
    } else {
      methodName.Append(aMethodInfo->GetName());
      methodName.SetCharAt(tolower(methodName[0]), 0);
    }

    jclass clazz = mJavaEnv->GetObjectClass(mJavaObject);
    if (clazz)
      mid = mJavaEnv->GetMethodID(clazz, methodName.get(), methodSig.get());
    NS_ASSERTION(mid, "Failed to get requested method for Java object");
  }

  // Call method
  jvalue retval;
  if (mid) {
    if (!retvalInfo) {
      mJavaEnv->CallVoidMethodA(mJavaObject, mid, java_params);
    } else {
      switch (retvalInfo->GetType().TagPart())
      {
        case nsXPTType::T_I8:
        case nsXPTType::T_U8:
          retval.b = mJavaEnv->CallByteMethodA(mJavaObject, mid,
                                               java_params);
          break;

        case nsXPTType::T_I16:
        case nsXPTType::T_U16:
          retval.s = mJavaEnv->CallShortMethodA(mJavaObject, mid,
                                                java_params);
          break;

        case nsXPTType::T_I32:
        case nsXPTType::T_U32:
          retval.i = mJavaEnv->CallIntMethodA(mJavaObject, mid,
                                              java_params);
          break;

        case nsXPTType::T_FLOAT:
          retval.f = mJavaEnv->CallFloatMethodA(mJavaObject, mid,
                                                java_params);
          break;

        case nsXPTType::T_DOUBLE:
          retval.d = mJavaEnv->CallDoubleMethodA(mJavaObject, mid,
                                                 java_params);
          break;

        case nsXPTType::T_BOOL:
          retval.z = mJavaEnv->CallBooleanMethodA(mJavaObject, mid,
                                                  java_params);
          break;

        case nsXPTType::T_CHAR:
        case nsXPTType::T_WCHAR:
          retval.c = mJavaEnv->CallCharMethodA(mJavaObject, mid,
                                               java_params);
          break;

        case nsXPTType::T_CHAR_STR:
        case nsXPTType::T_WCHAR_STR:
        case nsXPTType::T_IID:
        case nsXPTType::T_ASTRING:
        case nsXPTType::T_DOMSTRING:
        case nsXPTType::T_UTF8STRING:
        case nsXPTType::T_CSTRING:
        case nsXPTType::T_INTERFACE:
        case nsXPTType::T_INTERFACE_IS:
          retval.l = mJavaEnv->CallObjectMethodA(mJavaObject, mid,
                                                 java_params);
          break;

        case nsXPTType::T_VOID:
          retval.i = mJavaEnv->CallIntMethodA(mJavaObject, mid,
                                              java_params);
          break;

        default:
          NS_WARNING("Unhandled retval type");
          break;
      }
    }

    // Check for exception from called Java function
    jthrowable exp = mJavaEnv->ExceptionOccurred();
    if (exp) {
#ifdef DEBUG
      mJavaEnv->ExceptionDescribe();
#endif
      mJavaEnv->ExceptionClear();
      rv = NS_ERROR_FAILURE;
    }
  }

  // Handle any inout params
  if (NS_SUCCEEDED(rv)) {
    for (PRUint8 i = 0; i < paramCount; i++)
    {
      const nsXPTParamInfo &paramInfo = aMethodInfo->GetParam(i);
      if (!paramInfo.IsRetval()) {
        rv = FinalizeJavaParams(paramInfo, aMethodInfo, aMethodIndex, aParams,
                                aParams[i], java_params[i]);
      } else {
        rv = FinalizeJavaParams(paramInfo, aMethodInfo, aMethodIndex, aParams,
                                aParams[i], retval);
      }
    }
    NS_ASSERTION(NS_SUCCEEDED(rv), "FinalizeJavaParams/SetXPCOMRetval failed");
  }

  if (java_params)
    delete [] java_params;

  return rv;
}

nsresult
nsJavaXPTCStub::SetupJavaParams(const nsXPTParamInfo &aParamInfo,
                const nsXPTMethodInfo* aMethodInfo,
                PRUint16 aMethodIndex,
                nsXPTCMiniVariant* aDispatchParams,
                nsXPTCMiniVariant &aVariant, jvalue &aJValue,
                nsACString &aMethodSig)
{
  nsresult rv;
  const nsXPTType &type = aParamInfo.GetType();
  PRUint8 tag = type.TagPart();
  switch (tag)
  {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
    {
      if (!aParamInfo.IsOut()) {
        aJValue.b = aVariant.val.u8;
        aMethodSig.Append("B");
      } else {
        jbyteArray array = mJavaEnv->NewByteArray(1);
        mJavaEnv->SetByteArrayRegion(array, 0, 1, (jbyte*) &(aVariant.val.u8));
        aJValue.l = array;
        aMethodSig.Append("[B");
      }
    }
    break;

    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
    {
      if (!aParamInfo.IsOut()) {
        aJValue.s = aVariant.val.u16;
        aMethodSig.Append("S");
      } else {
        jshortArray array = mJavaEnv->NewShortArray(1);
        mJavaEnv->SetShortArrayRegion(array, 0, 1,
                                      (jshort*) &(aVariant.val.u16));
        aJValue.l = array;
        aMethodSig.Append("[S");
      }
    }
    break;

    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
    {
      if (!aParamInfo.IsOut()) {
        aJValue.i = aVariant.val.u32;
        aMethodSig.Append("I");
      } else {
        jintArray array = mJavaEnv->NewIntArray(1);
        mJavaEnv->SetIntArrayRegion(array, 0, 1, (jint*) &(aVariant.val.u32));
        aJValue.l = array;
        aMethodSig.Append("[I");
      }
    }
    break;

    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
    {
      if (!aParamInfo.IsOut()) {
        aJValue.j = aVariant.val.u64;
        aMethodSig.Append("J");
      } else {
        jlongArray array = mJavaEnv->NewLongArray(1);
        mJavaEnv->SetLongArrayRegion(array, 0, 1, (jlong*) &(aVariant.val.u64));
        aJValue.l = array;
        aMethodSig.Append("[J");
      }
    }
    break;

    case nsXPTType::T_FLOAT:
    {
      if (!aParamInfo.IsOut()) {
        aJValue.f = aVariant.val.f;
        aMethodSig.Append("F");
      } else {
        jfloatArray array = mJavaEnv->NewFloatArray(1);
        mJavaEnv->SetFloatArrayRegion(array, 0, 1, (jfloat*) &(aVariant.val.f));
        aJValue.l = array;
        aMethodSig.Append("[F");
      }
    }
    break;

    case nsXPTType::T_DOUBLE:
    {
      if (!aParamInfo.IsOut()) {
        aJValue.d = aVariant.val.d;
        aMethodSig.Append("D");
      } else {
        jdoubleArray array = mJavaEnv->NewDoubleArray(1);
        mJavaEnv->SetDoubleArrayRegion(array, 0, 1,
                                       (jdouble*) &(aVariant.val.d));
        aJValue.l = array;
        aMethodSig.Append("[D");
      }
    }
    break;

    case nsXPTType::T_BOOL:
    {
      if (!aParamInfo.IsOut()) {
        aJValue.z = aVariant.val.b;
        aMethodSig.Append("Z");
      } else {
        jbooleanArray array = mJavaEnv->NewBooleanArray(1);
        mJavaEnv->SetBooleanArrayRegion(array, 0, 1,
                                        (jboolean*) &(aVariant.val.b));
        aJValue.l = array;
        aMethodSig.Append("[Z");
      }
    }
    break;

    case nsXPTType::T_CHAR:
    case nsXPTType::T_WCHAR:
    {
      if (!aParamInfo.IsOut()) {
        aJValue.c = aVariant.val.wc;
        aMethodSig.Append("C");
      } else {
        jcharArray array = mJavaEnv->NewCharArray(1);
        mJavaEnv->SetCharArrayRegion(array, 0, 1, (jchar*) &(aVariant.val.wc));
        aJValue.l = array;
        aMethodSig.Append("[C");
      }
    }
    break;

    case nsXPTType::T_CHAR_STR:
    case nsXPTType::T_WCHAR_STR:
    {
      jobject str;
      if (aVariant.val.p) {
        if (tag == nsXPTType::T_CHAR_STR) {
          str = mJavaEnv->NewStringUTF((const char*) aVariant.val.p);
        } else {
          const PRUnichar* buf = (const PRUnichar*) aVariant.val.p;
          str = mJavaEnv->NewString(buf, nsCRT::strlen(buf));
        }
      } else {
        str = nsnull;
      }

      if (!aParamInfo.IsOut()) {
        aJValue.l = str;
        aMethodSig.Append("Ljava/lang/String;");
      } else {
        aJValue.l = mJavaEnv->NewObjectArray(1, stringClass, str);
        aMethodSig.Append("[Ljava/lang/String;");
      }
    }
    break;

    case nsXPTType::T_IID:
    {
      jstring str = nsnull;
      nsID* iid = (nsID*) aVariant.val.p;
      if (iid) {
        char* iid_str = iid->ToString();
        str = mJavaEnv->NewStringUTF(iid_str);
        PR_Free(iid_str);
      }

      if (!aParamInfo.IsOut()) {
        aJValue.l = str;
        aMethodSig.Append("Ljava/lang/String;");
      } else {
        aJValue.l = mJavaEnv->NewObjectArray(1, stringClass, str);
        aMethodSig.Append("[Ljava/lang/String;");
      }
    }
    break;

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
      jobject java_stub = nsnull;
      if (aVariant.val.p) {
        java_stub = GetMatchingJavaObject(mJavaEnv, aVariant.val.p);

        if (java_stub == nsnull) {
          // wrap xpcom instance
          nsID iid;
          JavaXPCOMInstance* inst;
          rv = GetIIDForMethodParam(mIInfo, aMethodInfo, aParamInfo,
                                    aMethodIndex, aDispatchParams,
                                    PR_FALSE, iid);
          if (NS_FAILED(rv))
            return rv;

          inst = CreateJavaXPCOMInstance((nsISupports*) aVariant.val.p, &iid);

          if (inst) {
            // create java stub
            char* iface_name;
            inst->InterfaceInfo()->GetName(&iface_name);
            java_stub = CreateJavaWrapper(mJavaEnv, iface_name);

            if (java_stub) {
              // Associate XPCOM object w/ Java stub
              AddJavaXPCOMBinding(mJavaEnv, java_stub, inst);
            }
          }
        }
      }

      if (!aParamInfo.IsOut()) {
        aJValue.l = java_stub;
        aMethodSig.Append("Lorg/mozilla/xpcom/nsISupports;");
      } else {
        aJValue.l = mJavaEnv->NewObjectArray(1, nsISupportsClass, java_stub);
        aMethodSig.Append("[Lorg/mozilla/xpcom/nsISupports;");
      }
    }
    break;

    case nsXPTType::T_ASTRING:
    case nsXPTType::T_DOMSTRING:
    {
      jstring jstr = nsnull;
      nsString* str = (nsString*) aVariant.val.p;
      if (str) {
        jstr = mJavaEnv->NewString(str->get(), str->Length());
      }

      if (!aParamInfo.IsOut()) {
        aJValue.l = jstr;
        aMethodSig.Append("Ljava/lang/String;");
      } else {
        aJValue.l = mJavaEnv->NewObjectArray(1, stringClass, jstr);
        aMethodSig.Append("[Ljava/lang/String;");
      }
    }
    break;

    case nsXPTType::T_UTF8STRING:
    case nsXPTType::T_CSTRING:
    {
      jstring jstr = nsnull;
      nsCString* str = (nsCString*) aVariant.val.p;
      if (str) {
        jstr = mJavaEnv->NewStringUTF(str->get());
      }

      if (!aParamInfo.IsOut()) {
        aJValue.l = jstr;
        aMethodSig.Append("Ljava/lang/String;");
      } else {
        aJValue.l = mJavaEnv->NewObjectArray(1, stringClass, jstr);
        aMethodSig.Append("[Ljava/lang/String;");
      }
    }
    break;

    // Pass the 'void*' address as an integer
    case nsXPTType::T_VOID:
    {
      if (!aParamInfo.IsOut()) {
        aJValue.i = (jint) aVariant.val.p;
        aMethodSig.Append("I");
      } else {
        jintArray array = mJavaEnv->NewIntArray(1);
        mJavaEnv->SetIntArrayRegion(array, 0, 1, (jint*) &(aVariant.val.p));
        aJValue.l = array;
        aMethodSig.Append("[I");
      }
    }
    break;

    case nsXPTType::T_ARRAY:
      NS_WARNING("array types are not yet supported");
      return NS_ERROR_NOT_IMPLEMENTED;
      break;

    case nsXPTType::T_PSTRING_SIZE_IS:
    case nsXPTType::T_PWSTRING_SIZE_IS:
    default:
      NS_WARNING("unexpected parameter type");
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

nsresult
nsJavaXPTCStub::GetRetvalSig(const nsXPTParamInfo* aParamInfo,
                           nsACString &aRetvalSig)
{
  const nsXPTType &type = aParamInfo->GetType();
  PRUint8 tag = type.TagPart();
  switch (tag)
  {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
      aRetvalSig.Append("B");
      break;

    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
      aRetvalSig.Append("S");
      break;

    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
      aRetvalSig.Append("I");
      break;

    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
      aRetvalSig.Append("J");
      break;

    case nsXPTType::T_FLOAT:
      aRetvalSig.Append("F");
      break;

    case nsXPTType::T_DOUBLE:
      aRetvalSig.Append("D");
      break;

    case nsXPTType::T_BOOL:
      aRetvalSig.Append("Z");
      break;

    case nsXPTType::T_CHAR:
    case nsXPTType::T_WCHAR:
      aRetvalSig.Append("C");
      break;

    case nsXPTType::T_CHAR_STR:
    case nsXPTType::T_WCHAR_STR:
    case nsXPTType::T_IID:
    case nsXPTType::T_ASTRING:
    case nsXPTType::T_DOMSTRING:
    case nsXPTType::T_UTF8STRING:
    case nsXPTType::T_CSTRING:
      aRetvalSig.Append("Ljava/lang/String;");
      break;

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
      aRetvalSig.Append("Lorg/mozilla/xpcom/nsISupports;");
      break;

    // XXX Probably won't work for 64 bit addr
    case nsXPTType::T_VOID:
      aRetvalSig.Append("I");
      break;

    case nsXPTType::T_ARRAY:
      NS_WARNING("array types are not yet supported");
      return NS_ERROR_NOT_IMPLEMENTED;
      break;

    case nsXPTType::T_PSTRING_SIZE_IS:
    case nsXPTType::T_PWSTRING_SIZE_IS:
    default:
      NS_WARNING("unexpected parameter type");
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

nsresult
nsJavaXPTCStub::FinalizeJavaParams(const nsXPTParamInfo &aParamInfo,
                                 const nsXPTMethodInfo* aMethodInfo,
                                 PRUint16 aMethodIndex,
                                 nsXPTCMiniVariant* aDispatchParams,
                                 nsXPTCMiniVariant &aVariant, jvalue &aJValue)
{
  nsresult rv;
  const nsXPTType &type = aParamInfo.GetType();

  switch (type.TagPart())
  {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
    {
      if (aParamInfo.IsOut()) {
        if (aParamInfo.IsRetval()) {
          *((PRUint8 *) aVariant.val.p) = aJValue.b;
        } else {
          if (aJValue.l) {
            jboolean isCopy = PR_FALSE;
            jbyte* array = mJavaEnv->GetByteArrayElements((jbyteArray) aJValue.l,
                                                          &isCopy);
            *((PRUint8 *) aVariant.val.p) = array[0];
            if (isCopy) {
              mJavaEnv->ReleaseByteArrayElements((jbyteArray) aJValue.l, array,
                                                 JNI_ABORT);
            }
          }
        }
      }
    }
    break;

    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
    {
      if (aParamInfo.IsOut()) {
        if (aParamInfo.IsRetval()) {
          *((PRUint16 *) aVariant.val.p) = aJValue.s;
        } else {
          if (aJValue.l) {
            jboolean isCopy = PR_FALSE;
            jshort* array =
                        mJavaEnv->GetShortArrayElements((jshortArray) aJValue.l,
                                                        &isCopy);
            *((PRUint16 *) aVariant.val.p) = array[0];
            if (isCopy) {
              mJavaEnv->ReleaseShortArrayElements((jshortArray) aJValue.l, array,
                                                  JNI_ABORT);
            }
          }
        }
      }
    }
    break;

    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
    {
      if (aParamInfo.IsOut()) {
        if (aParamInfo.IsRetval()) {
          *((PRUint32 *) aVariant.val.p) = aJValue.i;
        } else {
          if (aJValue.l) {
            jboolean isCopy = PR_FALSE;
            jint* array = mJavaEnv->GetIntArrayElements((jintArray) aJValue.l,
                                                         &isCopy);
            *((PRUint32 *) aVariant.val.p) = array[0];
            if (isCopy) {
              mJavaEnv->ReleaseIntArrayElements((jintArray) aJValue.l, array,
                                                JNI_ABORT);
            }
          }
        }
      }
    }
    break;

    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
    {
      if (aParamInfo.IsOut()) {
        if (aParamInfo.IsRetval()) {
          *((PRUint64 *) aVariant.val.p) = aJValue.j;
        } else {
          if (aJValue.l) {
            jboolean isCopy = PR_FALSE;
            jlong* array = mJavaEnv->GetLongArrayElements((jlongArray) aJValue.l,
                                                          &isCopy);
            *((PRUint64 *) aVariant.val.p) = array[0];
            if (isCopy) {
              mJavaEnv->ReleaseLongArrayElements((jlongArray) aJValue.l, array,
                                                 JNI_ABORT);
            }
          }
        }
      }
    }
    break;

    case nsXPTType::T_FLOAT:
    {
      if (aParamInfo.IsOut()) {
        if (aParamInfo.IsRetval()) {
          *((float *) aVariant.val.p) = aJValue.f;
        } else {
          if (aJValue.l) {
            jboolean isCopy = PR_FALSE;
            jfloat* array =
                        mJavaEnv->GetFloatArrayElements((jfloatArray) aJValue.l,
                                                        &isCopy);
            *((float *) aVariant.val.p) = array[0];
            if (isCopy) {
              mJavaEnv->ReleaseFloatArrayElements((jfloatArray) aJValue.l, array,
                                                  JNI_ABORT);
            }
          }
        }
      }
    }
    break;

    case nsXPTType::T_DOUBLE:
    {
      if (aParamInfo.IsOut()) {
        if (aParamInfo.IsRetval()) {
          *((double *) aVariant.val.p) = aJValue.d;
        } else {
          if (aJValue.l) {
            jboolean isCopy = PR_FALSE;
            jdouble* array =
                      mJavaEnv->GetDoubleArrayElements((jdoubleArray) aJValue.l,
                                                       &isCopy);
            *((double *) aVariant.val.p) = array[0];
            if (isCopy) {
              mJavaEnv->ReleaseDoubleArrayElements((jdoubleArray) aJValue.l,
                                                   array, JNI_ABORT);
            }
          }
        }
      }
    }
    break;

    case nsXPTType::T_BOOL:
    {
      if (aParamInfo.IsOut()) {
        if (aParamInfo.IsRetval()) {
          *((PRBool *) aVariant.val.p) = aJValue.z;
        } else {
          if (aJValue.l) {
            jboolean isCopy = PR_FALSE;
            jboolean* array =
                    mJavaEnv->GetBooleanArrayElements((jbooleanArray) aJValue.l,
                                                      &isCopy);
            *((PRBool *) aVariant.val.p) = array[0];
            if (isCopy) {
              mJavaEnv->ReleaseBooleanArrayElements((jbooleanArray) aJValue.l,
                                                    array, JNI_ABORT);
            }
          }
        }
      }
    }
    break;

    case nsXPTType::T_CHAR:
    case nsXPTType::T_WCHAR:
    {
      if (aParamInfo.IsOut()) {
        if (aParamInfo.IsRetval()) {
          if (type.TagPart() == nsXPTType::T_CHAR)
            *((char *) aVariant.val.p) = aJValue.c;
          else
            *((PRUnichar *) aVariant.val.p) = aJValue.c;
        } else {
          if (aJValue.l) {
            jboolean isCopy = PR_FALSE;
            jchar* array = mJavaEnv->GetCharArrayElements((jcharArray) aJValue.l,
                                                          &isCopy);
            if (type.TagPart() == nsXPTType::T_CHAR)
              *((char *) aVariant.val.p) = array[0];
            else
              *((PRUnichar *) aVariant.val.p) = array[0];
            if (isCopy) {
              mJavaEnv->ReleaseCharArrayElements((jcharArray) aJValue.l, array,
                                                 JNI_ABORT);
            }
          }
        }
      }
    }
    break;

    case nsXPTType::T_CHAR_STR:
    {
      // XXX not sure how this should be handled
      if (aParamInfo.IsOut()) {
        jstring str = nsnull;
        if (aParamInfo.IsRetval()) {
          str = (jstring) aJValue.l;
        } else if (aParamInfo.IsOut()) {
          str = (jstring)
                    mJavaEnv->GetObjectArrayElement((jobjectArray) aJValue.l, 0);
        }

        if (str) {
          jboolean isCopy;
          const char* char_ptr = mJavaEnv->GetStringUTFChars(str, &isCopy);
          // XXX is this strdup right?
          *((char **) aVariant.val.p) = strdup(char_ptr);
          if (isCopy) {
            mJavaEnv->ReleaseStringUTFChars(str, char_ptr);
          }
        } else {
          *((char **) aVariant.val.p) = nsnull;
        }
      }
    }
    break;

    case nsXPTType::T_WCHAR_STR:
    {
      // XXX not sure how this should be handled
      if (aParamInfo.IsOut()) {
        jstring str = nsnull;
        if (aParamInfo.IsRetval()) {
          str = (jstring) aJValue.l;
        } else if (aParamInfo.IsOut()) {
          str = (jstring)
                    mJavaEnv->GetObjectArrayElement((jobjectArray) aJValue.l, 0);
        }

        if (str) {
          jboolean isCopy;
          const jchar* wchar_ptr = mJavaEnv->GetStringChars(str, &isCopy);

          PRUint32 length = nsCRT::strlen(wchar_ptr);
          PRUnichar* string = (PRUnichar*) malloc(length + 1); // add one for null
          memcpy(string, wchar_ptr, length * sizeof(PRUnichar));
          string[length] = 0;
          *((PRUnichar **) aVariant.val.p) = string;

          if (isCopy) {
            mJavaEnv->ReleaseStringChars(str, wchar_ptr);
          }
        } else {
          *((PRUnichar **) aVariant.val.p) = nsnull;
        }
      }
    }
    break;

    case nsXPTType::T_IID:
    {
      if (aParamInfo.IsOut()) {
        jstring str = nsnull;
        if (aParamInfo.IsRetval()) {
          str = (jstring) aJValue.l;
        } else {
          str = (jstring)
                    mJavaEnv->GetObjectArrayElement((jobjectArray) aJValue.l, 0);
        }

        if (str) {
          jboolean isCopy;
          const char* char_ptr = mJavaEnv->GetStringUTFChars(str, &isCopy);

          if (aVariant.val.p) {
            // If not null, then we just put in the new value.
            nsID* iid = *((nsID**) aVariant.val.p);
            iid->Parse(char_ptr);
          } else {
            // If the argument that was passed in was null, then we need to
            // create a new nsID.
            nsID* iid = new nsID;
            iid->Parse(char_ptr);
            *((nsID **) aVariant.val.p) = iid;
          }

          if (isCopy) {
            mJavaEnv->ReleaseStringUTFChars(str, char_ptr);
          }
        } else {
          *((nsID **) aVariant.val.p) = nsnull;
        }
      }
    }
    break;

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
      if (aParamInfo.IsOut()) {
        jobject java_obj = nsnull;
        if (aParamInfo.IsRetval()) {
          java_obj = aJValue.l;
        } else {
          java_obj = mJavaEnv->GetObjectArrayElement((jobjectArray) aJValue.l, 0);
        }

        if (java_obj) {
          // Check if we already have a corresponding XPCOM object
          void* inst = GetMatchingXPCOMObject(mJavaEnv, java_obj);

          // Get IID for this param
          nsID iid;
          rv = GetIIDForMethodParam(mIInfo, aMethodInfo, aParamInfo,
                                    aMethodIndex, aDispatchParams,
                                    PR_FALSE, iid);
          if (NS_FAILED(rv))
            return rv;

          PRBool isWeakRef = iid.Equals(NS_GET_IID(nsIWeakReference));

          if (inst == nsnull && !isWeakRef) {
            // Get interface info for class
            nsCOMPtr<nsIInterfaceInfoManager> iim = XPTI_GetInterfaceInfoManager();
            nsCOMPtr<nsIInterfaceInfo> iinfo;
            iim->GetInfoForIID(&iid, getter_AddRefs(iinfo));

            // Create XPCOM stub
            nsJavaXPTCStub* xpcomStub = new nsJavaXPTCStub(mJavaEnv, java_obj,
                                                           iinfo);
            inst = SetAsXPTCStub(xpcomStub);
            AddJavaXPCOMBinding(mJavaEnv, java_obj, inst);
          }

          if (isWeakRef) {
            // If the function expects an weak reference, then we need to
            // create it here.
            nsJavaXPTCStubWeakRef* weakref =
                                  new nsJavaXPTCStubWeakRef(mJavaEnv, java_obj);
            NS_ADDREF(weakref);
            *((void **) aVariant.val.p) = (void*) weakref;
          } else if (IsXPTCStub(inst)) {
            nsJavaXPTCStub* xpcomStub = GetXPTCStubAddr(inst);
            NS_ADDREF(xpcomStub);
            *((void **) aVariant.val.p) = (void*) xpcomStub;
          } else {
            JavaXPCOMInstance* xpcomInst = (JavaXPCOMInstance*) inst;
            *((void **) aVariant.val.p) = (void*) xpcomInst->GetInstance();
          }
        } else {
          *((void **) aVariant.val.p) = nsnull;
        }
      }
    }
    break;

    case nsXPTType::T_ASTRING:
    case nsXPTType::T_DOMSTRING:
    {
      if (aJValue.l && aParamInfo.IsOut()) {
        jstring str = nsnull;
        if (aParamInfo.IsRetval()) {
          str = (jstring) aJValue.l;
        } else {
          str = (jstring)
                    mJavaEnv->GetObjectArrayElement((jobjectArray) aJValue.l, 0);
        }

        if (str) {
          jboolean isCopy;
          const jchar* wchar_ptr = mJavaEnv->GetStringChars(str, &isCopy);

          if (aVariant.val.p) {
            ((nsString*) aVariant.val.p)->Assign(wchar_ptr);
          } else {
            nsString* embedStr = new nsString(wchar_ptr);
            aVariant.val.p = embedStr;
          }

          if (isCopy) {
            mJavaEnv->ReleaseStringChars(str, wchar_ptr);
          }
        }
      }
    }
    break;

    case nsXPTType::T_UTF8STRING:
    case nsXPTType::T_CSTRING:
    {
      if (aJValue.l && aParamInfo.IsOut()) {
        jstring str = nsnull;
        if (aParamInfo.IsRetval()) {
          str = (jstring) aJValue.l;
        } else {
          str = (jstring)
                    mJavaEnv->GetObjectArrayElement((jobjectArray) aJValue.l, 0);
        }

        if (str) {
          jboolean isCopy;
          const char* char_ptr = mJavaEnv->GetStringUTFChars(str, &isCopy);

          if (aVariant.val.p) {
            ((nsCString*) aVariant.val.p)->Assign(char_ptr);
          } else {
            nsCString* embedStr = new nsCString(char_ptr);
            aVariant.val.p = embedStr;
          }

          if (isCopy) {
            mJavaEnv->ReleaseStringUTFChars(str, char_ptr);
          }
        }
      }
    }
    break;

    case nsXPTType::T_VOID:
    {
      if (aParamInfo.IsOut()) {
        if (aParamInfo.IsRetval()) {
          *((PRUint32 *) aVariant.val.p) = aJValue.i;
        } else {
          if (aJValue.l) {
            jboolean isCopy = PR_FALSE;
            jint* array = mJavaEnv->GetIntArrayElements((jintArray) aJValue.l,
                                                         &isCopy);
            *((PRUint32 *) aVariant.val.p) = array[0];
            if (isCopy) {
              mJavaEnv->ReleaseIntArrayElements((jintArray) aJValue.l, array,
                                                JNI_ABORT);
            }
          }
        }
      }
    }
    break;

    default:
      NS_WARNING("unexpected parameter type");
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsJavaXPTCStub::GetWeakReference(nsIWeakReference** aInstancePtr)
{
  if (mMaster)
    return mMaster->GetWeakReference(aInstancePtr);

  LOG("==> nsJavaXPTCStub::GetWeakReference()\n");

  if (!aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  nsJavaXPTCStubWeakRef* weakref =
                       new nsJavaXPTCStubWeakRef(mJavaEnv, mJavaObject);
  *aInstancePtr = weakref;
  NS_ADDREF(*aInstancePtr);

  return NS_OK;
}

jobject
nsJavaXPTCStub::GetJavaObject()
{
  return mJavaObject;
}
