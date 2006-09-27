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

#include "nsJavaWrapper.h"
#include "nsJavaXPTCStub.h"
#include "nsJavaXPCOMBindingUtils.h"
#include "jni.h"
#include "xptcall.h"
#include "nsIInterfaceInfoManager.h"
#include "nsString.h"
#include "nsString.h"
#include "nsCRT.h"
#include "prmem.h"

static nsID nullID = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};


nsresult
SetupParams(JNIEnv *env, const jobject aParam, const nsXPTParamInfo &aParamInfo,
            const nsXPTMethodInfo* aMethodInfo, nsIInterfaceInfo* aIInfo,
            PRUint16 aMethodIndex, nsXPTCVariant* aDispatchParams,
            nsXPTCVariant &aVariant)
{
  nsresult rv = NS_OK;
  const nsXPTType &type = aParamInfo.GetType();

  // defaults
  aVariant.ptr = nsnull;
  aVariant.type = type;
  aVariant.flags = 0;

  PRUint8 tag = type.TagPart();
  switch (tag)
  {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
    {
      LOG("byte\n");
      if (!aParamInfo.IsOut()) {
        aVariant.val.u8 = env->CallByteMethod(aParam, byteValueMID);
      } else {
        jboolean isCopy = JNI_FALSE;
        jbyte* buf = nsnull;
        if (aParam) {
          buf = env->GetByteArrayElements((jbyteArray) aParam, &isCopy);
        }

        aVariant.ptr = buf;
        aVariant.flags = nsXPTCVariant::PTR_IS_DATA;
        if (isCopy) {
          aVariant.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
        }
      }
    }
    break;

    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
    {
      LOG("short\n");
      if (!aParamInfo.IsOut()) {
        aVariant.val.u16 = env->CallShortMethod(aParam, shortValueMID);
      } else {
        jboolean isCopy = JNI_FALSE;
        jshort* buf = nsnull;
        if (aParam) {
          buf = env->GetShortArrayElements((jshortArray) aParam, &isCopy);
        }

        aVariant.ptr = buf;
        aVariant.flags = nsXPTCVariant::PTR_IS_DATA;
        if (isCopy) {
          aVariant.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
        }
      }
    }
    break;

    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
    {
      LOG("int\n");
      if (!aParamInfo.IsOut()) {
        aVariant.val.u32 = env->CallIntMethod(aParam, intValueMID);
      } else {
        jboolean isCopy = JNI_FALSE;
        jint* buf = nsnull;
        if (aParam) {
          buf = env->GetIntArrayElements((jintArray) aParam, &isCopy);
        }

        aVariant.ptr = buf;
        aVariant.flags = nsXPTCVariant::PTR_IS_DATA;
        if (isCopy) {
          aVariant.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
        }
      }
    }
    break;

    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
    {
      LOG("long\n");
      if (!aParamInfo.IsOut()) {
        aVariant.val.u64 = env->CallLongMethod(aParam, longValueMID);
      } else {
        jboolean isCopy = JNI_FALSE;
        jlong* buf = nsnull;
        if (aParam) {
          buf = env->GetLongArrayElements((jlongArray) aParam, &isCopy);
        }

        aVariant.ptr = buf;
        aVariant.flags = nsXPTCVariant::PTR_IS_DATA;
        if (isCopy) {
          aVariant.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
        }
      }
    }
    break;

    case nsXPTType::T_FLOAT:
    {
      LOG("float\n");
      if (!aParamInfo.IsOut()) {
        aVariant.val.f = env->CallFloatMethod(aParam, floatValueMID);
      } else {
        jboolean isCopy = JNI_FALSE;
        jfloat* buf = nsnull;
        if (aParam) {
          buf = env->GetFloatArrayElements((jfloatArray) aParam, &isCopy);
        }

        aVariant.ptr = buf;
        aVariant.flags = nsXPTCVariant::PTR_IS_DATA;
        if (isCopy) {
          aVariant.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
        }
      }
    }
    break;

    case nsXPTType::T_DOUBLE:
    {
      LOG("double\n");
      if (!aParamInfo.IsOut()) {
        aVariant.val.d = env->CallDoubleMethod(aParam, doubleValueMID);
      } else {
        jboolean isCopy = JNI_FALSE;
        jdouble* buf = nsnull;
        if (aParam) {
          buf = env->GetDoubleArrayElements((jdoubleArray) aParam, &isCopy);
        }

        aVariant.ptr = buf;
        aVariant.flags = nsXPTCVariant::PTR_IS_DATA;
        if (isCopy) {
          aVariant.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
        }
      }
    }
    break;

    case nsXPTType::T_BOOL:
    {
      LOG("boolean\n");
      if (!aParamInfo.IsOut()) {
        aVariant.val.b = env->CallBooleanMethod(aParam, booleanValueMID);
      } else {
        jboolean isCopy = JNI_FALSE;
        jboolean* buf = nsnull;
        if (aParam) {
          buf = env->GetBooleanArrayElements((jbooleanArray) aParam, &isCopy);
        }

        aVariant.ptr = buf;
        aVariant.flags = nsXPTCVariant::PTR_IS_DATA;
        if (isCopy) {
          aVariant.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
        }
      }
    }
    break;

    case nsXPTType::T_CHAR:
    case nsXPTType::T_WCHAR:
    {
      LOG("char\n");
      if (!aParamInfo.IsOut()) {
        if (tag == nsXPTType::T_CHAR)
          aVariant.val.c = env->CallCharMethod(aParam, charValueMID);
        else
          aVariant.val.wc = env->CallCharMethod(aParam, charValueMID);
      } else {
        jboolean isCopy = JNI_FALSE;
        jchar* buf = nsnull;
        if (aParam) {
          buf = env->GetCharArrayElements((jcharArray) aParam, &isCopy);
        }

        aVariant.ptr = buf;
        aVariant.flags = nsXPTCVariant::PTR_IS_DATA;
        if (isCopy) {
          aVariant.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
        }
      }
    }
    break;

    case nsXPTType::T_CHAR_STR:
    case nsXPTType::T_WCHAR_STR:
    {
      LOG("String\n");
      jstring data = nsnull;
      if (!aParamInfo.IsOut()) {
        data = (jstring) aParam;
      } else {
        if (aParam)
          data = (jstring) env->GetObjectArrayElement((jobjectArray) aParam, 0);
      }

      jboolean isCopy = JNI_FALSE;
      const void* buf = nsnull;
      if (data) {
        if (tag == nsXPTType::T_CHAR_STR) {
          buf = env->GetStringUTFChars(data, &isCopy);
        } else {
          buf = env->GetStringChars(data, &isCopy);
        }
      }

      aVariant.val.p = aVariant.ptr = NS_CONST_CAST(void*, buf);
      aVariant.flags = nsXPTCVariant::PTR_IS_DATA;
      if (isCopy) {
        aVariant.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
      }
    }
    break;

    case nsXPTType::T_IID:
    {
      LOG("String(IID)\n");
      jstring data = nsnull;
      if (!aParamInfo.IsOut()) {
        data = (jstring) aParam;
      } else {
        if (aParam)
          data = (jstring) env->GetObjectArrayElement((jobjectArray) aParam, 0);
      }

      nsID* iid = new nsID;
      if (data) {
        jboolean isCopy;
        const char* str = nsnull;
        str = env->GetStringUTFChars(data, &isCopy);
        iid->Parse(str);
        if (isCopy) {
          env->ReleaseStringUTFChars(data, str);
        }
      } else {
        *iid = nullID;
      }

      aVariant.val.p = aVariant.ptr = iid;
      aVariant.flags = nsXPTCVariant::PTR_IS_DATA;
    }
    break;

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
      LOG("nsISupports\n");
      jobject java_obj = nsnull;
      if (!aParamInfo.IsOut()) {
        java_obj = (jobject) aParam;
      } else {
        if (aParam)
          java_obj = (jobject) env->GetObjectArrayElement((jobjectArray) aParam, 0);
      }

      if (java_obj) {
        // Check if we already have a corresponding XPCOM object
        void* inst = GetMatchingXPCOMObject(env, java_obj);

        // Get IID for this param
        nsID iid;
        rv = GetIIDForMethodParam(aIInfo, aMethodInfo, aParamInfo,
                                  aMethodIndex, aDispatchParams, PR_TRUE,
                                  iid);
        if (NS_FAILED(rv))
          return rv;

        PRBool isWeakRef = iid.Equals(NS_GET_IID(nsIWeakReference));

        if (inst == nsnull && !isWeakRef) {
          // If there is not corresponding XPCOM object, then that means that the
          // parameter is non-generated class (that is, it is not one of our
          // Java stubs that represent an exising XPCOM object).  So we need to
          // create an XPCOM stub, that can route any method calls to the class.

          // Get interface info for class
          nsCOMPtr<nsIInterfaceInfoManager> iim = XPTI_GetInterfaceInfoManager();
          nsCOMPtr<nsIInterfaceInfo> iinfo;
          iim->GetInfoForIID(&iid, getter_AddRefs(iinfo));

          // Create XPCOM stub
          nsJavaXPTCStub* xpcomStub = new nsJavaXPTCStub(env, java_obj, iinfo);
          inst = SetAsXPTCStub(xpcomStub);
          AddJavaXPCOMBinding(env, java_obj, inst);
        }

        if (isWeakRef) {
          // If the function expects an weak reference, then we need to
          // create it here.
          nsJavaXPTCStubWeakRef* weakref =
                                      new nsJavaXPTCStubWeakRef(env, java_obj);
          NS_ADDREF(weakref);
          aVariant.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
          aVariant.val.p = aVariant.ptr = (void*) weakref;
        } else if (IsXPTCStub(inst)) {
          nsJavaXPTCStub* xpcomStub = GetXPTCStubAddr(inst);
          NS_ADDREF(xpcomStub);
          aVariant.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
          aVariant.val.p = aVariant.ptr = (void*) xpcomStub;
        } else {
          JavaXPCOMInstance* xpcomInst = (JavaXPCOMInstance*) inst;
          aVariant.val.p = aVariant.ptr = (void*) xpcomInst->GetInstance();
        }
      } else {
        aVariant.val.p = aVariant.ptr = nsnull;
      }

      aVariant.flags |= nsXPTCVariant::PTR_IS_DATA;
      aVariant.SetValIsInterface();
    }
    break;

    case nsXPTType::T_ASTRING:
    case nsXPTType::T_DOMSTRING:
    {
      LOG("String\n");
      jstring data = nsnull;
      if (!aParamInfo.IsOut()) {
        data = (jstring) aParam;
      } else {
        if (aParam)
          data = (jstring) env->GetObjectArrayElement((jobjectArray) aParam, 0);
      }

      jboolean isCopy = JNI_FALSE;
      const PRUnichar* buf = nsnull;
      if (data) {
        buf = env->GetStringChars(data, &isCopy);
      }

      nsString* str = new nsString(buf);
      if (isCopy) {
        env->ReleaseStringChars((jstring)data, buf);
      }

      aVariant.val.p = aVariant.ptr = str;
      aVariant.flags = nsXPTCVariant::PTR_IS_DATA | nsXPTCVariant::VAL_IS_DOMSTR;
    }
    break;

    case nsXPTType::T_UTF8STRING:
    case nsXPTType::T_CSTRING:
    {
      LOG("StringUTF\n");
      jstring data = nsnull;
      if (!aParamInfo.IsOut()) {
        data = (jstring) aParam;
      } else {
        if (aParam)
          data = (jstring) env->GetObjectArrayElement((jobjectArray) aParam, 0);
      }

      jboolean isCopy = JNI_FALSE;
      const char* buf = nsnull;
      if (data) {
        buf = env->GetStringUTFChars(data, &isCopy);
      }

      nsCString* str = new nsCString(buf);
      if (isCopy) {
        env->ReleaseStringUTFChars((jstring)aParam, buf);
      }

      aVariant.val.p = aVariant.ptr = str;
      aVariant.flags = nsXPTCVariant::PTR_IS_DATA;
      if (tag == nsXPTType::T_CSTRING) {
        aVariant.flags |= nsXPTCVariant::VAL_IS_CSTR;
      } else {
        aVariant.flags |= nsXPTCVariant::VAL_IS_UTF8STR;
      }
    }
    break;

    // XXX How should this be handled?
    case nsXPTType::T_VOID:
    {
      if (env->IsInstanceOf(aParam, intClass))
      {
        if (env->IsInstanceOf(aParam, intArrayClass))
        {
          LOG("int[] (void*)\n");
          jboolean isCopy = JNI_FALSE;
          jint* buf = nsnull;
          if (aParam) {
            buf = env->GetIntArrayElements((jintArray) aParam, &isCopy);
          }

          aVariant.ptr = buf;
          aVariant.flags = nsXPTCVariant::PTR_IS_DATA;
          if (isCopy) {
            aVariant.flags |= nsXPTCVariant::VAL_IS_ALLOCD;
          }
        } else {
          LOG("int (void*)\n");
          NS_ASSERTION(type.IsPointer(), "T_VOID 'int' handler received non-pointer type");
          aVariant.val.p = (void*) env->CallIntMethod(aParam, intValueMID);
        }
      } else {
        NS_WARNING("Unhandled T_VOID");
        return NS_ERROR_UNEXPECTED;
      }
    }
    break;

    case nsXPTType::T_ARRAY:
      NS_WARNING("array types are not yet supported");
      return NS_ERROR_NOT_IMPLEMENTED;

    case nsXPTType::T_PSTRING_SIZE_IS:
    case nsXPTType::T_PWSTRING_SIZE_IS:
    default:
      NS_WARNING("unexpected parameter type");
      return NS_ERROR_UNEXPECTED;
  }

  return rv;
}

nsresult
FinalizeParams(JNIEnv *env, const jobject aParam,
                const nsXPTParamInfo &aParamInfo,
                const nsXPTMethodInfo* aMethodInfo,
                nsIInterfaceInfo* aIInfo,
                PRUint16 aMethodIndex,
                nsXPTCVariant* aDispatchParams,
                nsXPTCVariant &aVariant)
{
  nsresult rv = NS_OK;
  const nsXPTType &type = aParamInfo.GetType();

  // XXX Not sure if this is necessary
  // Only write the array elements back if the parameter is an output param
  jint mode = 0;
  if (!aParamInfo.IsOut() && !aParamInfo.IsRetval())
    mode = JNI_ABORT;

  switch (type.TagPart())
  {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
    {
      if (aVariant.flags & nsXPTCVariant::VAL_IS_ALLOCD) {
        env->ReleaseByteArrayElements((jbyteArray) aParam,
                                      (jbyte*) aVariant.val.p, mode);
      }
    }
    break;

    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
    {
      if (aVariant.flags & nsXPTCVariant::VAL_IS_ALLOCD) {
        env->ReleaseShortArrayElements((jshortArray) aParam,
                                       (jshort*) aVariant.val.p, mode);
      }
    }
    break;

    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
    {
      if (aVariant.flags & nsXPTCVariant::VAL_IS_ALLOCD) {
        env->ReleaseIntArrayElements((jintArray) aParam,
                                     (jint*) aVariant.val.p, mode);
      }
    }
    break;

    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
    {
      if (aVariant.flags & nsXPTCVariant::VAL_IS_ALLOCD) {
        env->ReleaseLongArrayElements((jlongArray) aParam,
                                      (jlong*) aVariant.val.p, mode);
      }
    }
    break;

    case nsXPTType::T_FLOAT:
    {
      if (aVariant.flags & nsXPTCVariant::VAL_IS_ALLOCD) {
        env->ReleaseFloatArrayElements((jfloatArray) aParam,
                                       (jfloat*) aVariant.val.p, mode);
      }
    }
    break;

    case nsXPTType::T_DOUBLE:
    {
      if (aVariant.flags & nsXPTCVariant::VAL_IS_ALLOCD) {
        env->ReleaseDoubleArrayElements((jdoubleArray) aParam,
                                        (jdouble*) aVariant.val.p, mode);
      }
    }
    break;

    case nsXPTType::T_BOOL:
    {
      if (aVariant.flags & nsXPTCVariant::VAL_IS_ALLOCD) {
        env->ReleaseBooleanArrayElements((jbooleanArray) aParam,
                                         (jboolean*) aVariant.val.p, mode);
      }
    }
    break;

    case nsXPTType::T_CHAR:
    case nsXPTType::T_WCHAR:
    {
      if (aVariant.flags & nsXPTCVariant::VAL_IS_ALLOCD) {
        env->ReleaseCharArrayElements((jcharArray) aParam,
                                      (jchar*) aVariant.val.p, mode);
      }
    }
    break;

    case nsXPTType::T_CHAR_STR:
    {
      // release Java string buffer
      if (aVariant.flags & nsXPTCVariant::VAL_IS_ALLOCD) {
        jstring data = nsnull;
        if (!aParamInfo.IsOut()) {
          data = (jstring) aParam;
        } else {
          data = (jstring) env->GetObjectArrayElement((jobjectArray) aParam, 0);
        }
        env->ReleaseStringUTFChars(data, (const char*) aVariant.val.p);
      }

      // If this is an output parameter, then create the string to return
      if (aParam && aParamInfo.IsOut()) {
        jstring str = env->NewStringUTF((const char*) aVariant.val.p);
        env->SetObjectArrayElement((jobjectArray) aParam, 0, str);
      }
    }
    break;

    case nsXPTType::T_WCHAR_STR:
    {
      // release Java string buffer
      if (aVariant.flags & nsXPTCVariant::VAL_IS_ALLOCD) {
        jstring data = nsnull;
        if (!aParamInfo.IsOut()) {
          data = (jstring) aParam;
        } else {
          data = (jstring) env->GetObjectArrayElement((jobjectArray) aParam, 0);
        }
        env->ReleaseStringChars(data, (const jchar*) aVariant.val.p);
      }

      // If this is an output parameter, then create the string to return
      if (aParam && aParamInfo.IsOut()) {
        PRUint32 length = nsCRT::strlen((const PRUnichar*) aVariant.val.p);
        jstring str = env->NewString((const jchar*) aVariant.val.p, length);
        env->SetObjectArrayElement((jobjectArray) aParam, 0, str);
      }
    }
    break;

    case nsXPTType::T_IID:
    {
      nsID* iid = (nsID*) aVariant.val.p;

      // If this is an output parameter, then create the string to return
      if (iid && aParamInfo.IsOut()) {
        char* iid_str = iid->ToString();
        jstring str = env->NewStringUTF(iid_str);
        PR_Free(iid_str);
        env->SetObjectArrayElement((jobjectArray) aParam, 0, str);
      }

      // XXX Cannot delete this until we've handled all of the params.  See
      //  comment in CallXPCOMMethod
//      delete iid;
    }
    break;

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
      if (aVariant.val.p && aParamInfo.IsOut()) {
        jobject java_obj = GetMatchingJavaObject(env, aVariant.val.p);

        if (java_obj == nsnull) {
          // wrap xpcom instance
          nsID iid;
          JavaXPCOMInstance* inst;
          rv = GetIIDForMethodParam(aIInfo, aMethodInfo, aParamInfo,
                                    aMethodIndex, aDispatchParams, PR_TRUE, iid);
          if (NS_FAILED(rv))
            return rv;

          nsISupports* variant =
                              NS_REINTERPRET_CAST(nsISupports*, aVariant.val.p);
          inst = CreateJavaXPCOMInstance(variant, &iid);
          NS_RELEASE(variant);   // JavaXPCOMInstance has owning ref

          if (inst) {
            // create java stub
            char* iface_name;
            inst->InterfaceInfo()->GetName(&iface_name);
            java_obj = CreateJavaWrapper(env, iface_name);

            if (java_obj) {
              // Associate XPCOM object w/ Java stub
              AddJavaXPCOMBinding(env, java_obj, inst);
            }
          }
        }

        env->SetObjectArrayElement((jobjectArray) aParam, 0, java_obj);
      }

      // If VAL_IS_ALLOCD is set, that means that an XPCOM object was created
      // is SetupParams that now needs to be released.
      if (aVariant.val.p &&
          (aVariant.flags & nsXPTCVariant::VAL_IS_ALLOCD)) {
        nsISupports* variant = NS_STATIC_CAST(nsISupports*, aVariant.val.p);
        NS_RELEASE(variant);
      }
    }
    break;

    case nsXPTType::T_ASTRING:
    case nsXPTType::T_DOMSTRING:
    {
      nsString* str = (nsString*) aVariant.val.p;

      if (str && aParamInfo.IsOut()) {
        jstring jstr = env->NewString((const jchar*) str->get(), str->Length());
        env->SetObjectArrayElement((jobjectArray) aParam, 0, jstr);
      }

      delete str;
    }
    break;

    case nsXPTType::T_UTF8STRING:
    case nsXPTType::T_CSTRING:
    {
      nsCString* str = (nsCString*) aVariant.val.p;

      if (str && aParamInfo.IsOut()) {
        jstring jstr = env->NewStringUTF((const char*) str->get());
        env->SetObjectArrayElement((jobjectArray) aParam, 0, jstr);
      }

      delete str;
    }
    break;

    case nsXPTType::T_VOID:
    {
      if (aVariant.flags & nsXPTCVariant::VAL_IS_ALLOCD) {
        env->ReleaseIntArrayElements((jintArray) aParam,
                                     (jint*) aVariant.val.p, mode);
      }
    }
    break;
  }

  return rv;
}

nsresult
SetRetval(JNIEnv *env, const nsXPTParamInfo &aParamInfo,
          const nsXPTMethodInfo* aMethodInfo, nsIInterfaceInfo* aIInfo,
          PRUint16 aMethodIndex, nsXPTCVariant* aDispatchParams,
          nsXPTCVariant &aVariant, jvalue &aResult)
{
  nsresult rv = NS_OK;
  const nsXPTType &type = aParamInfo.GetType();

  switch (type.TagPart())
  {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
      aResult.b = aVariant.val.u8;
      break;

    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
      aResult.s = aVariant.val.u16;
      break;

    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
      aResult.i = aVariant.val.u32;
      break;

    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
      aResult.j = aVariant.val.u64;
      break;

    case nsXPTType::T_FLOAT:
      aResult.f = aVariant.val.f;
      break;

    case nsXPTType::T_DOUBLE:
      aResult.d = aVariant.val.d;
      break;

    case nsXPTType::T_BOOL:
      aResult.z = aVariant.val.b;
      break;

    case nsXPTType::T_CHAR:
      aResult.c = aVariant.val.c;
      break;

    case nsXPTType::T_WCHAR:
      aResult.c = aVariant.val.wc;
      break;

    case nsXPTType::T_CHAR_STR:
    {
      if (aVariant.ptr)
        aResult.l = env->NewStringUTF((const char*) aVariant.ptr);
      else
        aResult.l = nsnull;
    }
    break;

    case nsXPTType::T_WCHAR_STR:
    {
      if (aVariant.ptr) {
        PRUint32 length = nsCRT::strlen((const PRUnichar*) aVariant.ptr);
        aResult.l = env->NewString((const jchar*) aVariant.ptr, length);
      } else {
        aResult.l = nsnull;
      }
    }
    break;

    case nsXPTType::T_IID:
    {
      if (aVariant.ptr) {
        nsID* iid = (nsID*) aVariant.ptr;
        char* iid_str = iid->ToString();
        aResult.l = env->NewStringUTF(iid_str);
        PR_Free(iid_str);
      } else {
        aResult.l = nsnull;
      }
    }
    break;

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
      if (aVariant.val.p) {
        jobject java_obj = GetMatchingJavaObject(env, aVariant.val.p);

        if (java_obj == nsnull) {
          nsID iid;
          JavaXPCOMInstance* inst;
          rv = GetIIDForMethodParam(aIInfo, aMethodInfo, aParamInfo,
                                    aMethodIndex, aDispatchParams, PR_TRUE, iid);
          if (NS_FAILED(rv))
            return rv;

          nsISupports* variant =
                              NS_REINTERPRET_CAST(nsISupports*, aVariant.val.p);
          inst = CreateJavaXPCOMInstance(variant, &iid);
          NS_RELEASE(variant);   // JavaXPCOMInstance has owning ref

          if (inst) {
            // create java stub
            char* iface_name;
            inst->InterfaceInfo()->GetName(&iface_name);
            java_obj = CreateJavaWrapper(env, iface_name);

            if (java_obj)
              AddJavaXPCOMBinding(env, java_obj, inst);
          }
        }

        // XXX not sure if this is necessary
        // If returned object is an nsJavaXPTCStub, release it.
        nsISupports* xpcom_obj = NS_STATIC_CAST(nsISupports*, aVariant.val.p);
        nsJavaXPTCStub* stub = nsnull;
        xpcom_obj->QueryInterface(NS_GET_IID(nsJavaXPTCStub), (void**) &stub);
        if (stub) {
          NS_RELEASE(xpcom_obj);
          NS_RELEASE(stub);
        }

        aResult.l = java_obj;
      } else {
        aResult.l = nsnull;
      }
    }
    break;

    case nsXPTType::T_ASTRING:
    case nsXPTType::T_DOMSTRING:
    {
      if (aVariant.ptr) {
        nsString* str = (nsString*) aVariant.ptr;
        aResult.l = env->NewString(str->get(), str->Length());
      } else {
        aResult.l = nsnull;
      }
    }
    break;

    case nsXPTType::T_UTF8STRING:
    case nsXPTType::T_CSTRING:
    {
      if (aVariant.ptr) {
        nsCString* str = (nsCString*) aVariant.ptr;
        aResult.l = env->NewStringUTF(str->get());
      } else {
        aResult.l = nsnull;
      }
    }
    break;

    case nsXPTType::T_VOID:
      // handle "void *" as an "int" in Java
      LOG(" returns int (void*)");
      aResult.i = (jint) aVariant.val.p;
      break;

    case nsXPTType::T_ARRAY:
      NS_WARNING("array types are not yet supported");
      return NS_ERROR_NOT_IMPLEMENTED;

    default:
      NS_WARNING("unexpected parameter type");
      return NS_ERROR_UNEXPECTED;
  }

  return rv;
}

void
CallXPCOMMethod(JNIEnv *env, jclass that, jobject aJavaObject,
                jint aMethodIndex, jobjectArray aParams, jvalue &aResult)
{
  // Find corresponding XPCOM object
  void* xpcomObj = GetMatchingXPCOMObject(env, aJavaObject);
  NS_ASSERTION(xpcomObj != nsnull, "Failed to get matching XPCOM object");
  if (xpcomObj == nsnull) {
    ThrowXPCOMException(env, 0);
    return;
  }

  NS_ASSERTION(!IsXPTCStub(xpcomObj), "Expected JavaXPCOMInstance, but got nsJavaXPTCStub");
  JavaXPCOMInstance* inst = (JavaXPCOMInstance*) xpcomObj;

  // Get method info
  const nsXPTMethodInfo* methodInfo;
  nsIInterfaceInfo* iinfo = inst->InterfaceInfo();
  nsresult rv = iinfo->GetMethodInfo(aMethodIndex, &methodInfo);
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetMethodInfo failed");
  
#ifdef DEBUG
  const char* ifaceName;
  iinfo->GetNameShared(&ifaceName);
  LOG("=> Calling %s::%s()\n", ifaceName, methodInfo->GetName());
#endif

  PRUint8 paramCount = methodInfo->GetParamCount();
  nsXPTCVariant* params = nsnull;
  if (paramCount)
  {
    params = new nsXPTCVariant[paramCount];

    for (PRUint8 i = 0; i < paramCount && NS_SUCCEEDED(rv); i++)
    {
      LOG("\t Param %d: ", i);
      const nsXPTParamInfo &paramInfo = methodInfo->GetParam(i);

      if (!paramInfo.IsRetval() && !paramInfo.IsDipper()) {
        rv = SetupParams(env, env->GetObjectArrayElement(aParams, i), paramInfo,
                         methodInfo, iinfo, aMethodIndex, params, params[i]);
      } else if (paramInfo.IsDipper()) {
        LOG("dipper");
        const nsXPTType &type = paramInfo.GetType();
        switch (type.TagPart())
        {
          case nsXPTType::T_ASTRING:
          case nsXPTType::T_DOMSTRING:
            params[i].val.p = params[i].ptr = new nsString();
            params[i].type = type;
            params[i].flags = nsXPTCVariant::PTR_IS_DATA |
                              nsXPTCVariant::VAL_IS_DOMSTR;
            break;

          case nsXPTType::T_UTF8STRING:
          case nsXPTType::T_CSTRING:
            params[i].val.p = params[i].ptr = new nsCString();
            params[i].type = type;
            params[i].flags = nsXPTCVariant::PTR_IS_DATA |
                              nsXPTCVariant::VAL_IS_CSTR;
            break;

          default:
            LOG("unhandled dipper type\n");
            rv = NS_ERROR_UNEXPECTED;
        }
      } else {
        LOG("retval\n");
        params[i].ptr = &(params[i].val);
        params[i].type = paramInfo.GetType();
        params[i].flags = nsXPTCVariant::PTR_IS_DATA;
      }
    }
    if (NS_FAILED(rv)) {
      NS_WARNING("SetupParams failed");
      ThrowXPCOMException(env, rv);
      return;
    }
  }

  nsresult invokeResult;
  invokeResult = XPTC_InvokeByIndex(inst->GetInstance(), aMethodIndex,
                                    paramCount, params);

  for (PRUint8 i = 0; i < paramCount && NS_SUCCEEDED(rv); i++)
  {
    const nsXPTParamInfo &paramInfo = methodInfo->GetParam(i);

    if (!paramInfo.IsRetval()) {
      rv = FinalizeParams(env, env->GetObjectArrayElement(aParams, i),
                          paramInfo, methodInfo, iinfo, aMethodIndex,
                          params, params[i]);
    } else {
      rv = SetRetval(env, paramInfo, methodInfo, iinfo, aMethodIndex, params,
                     params[i], aResult);
    }
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("FinalizeParams failed");
    ThrowXPCOMException(env, rv);
    return;
  }

  // XXX Normally, we would delete any created nsID object in the above loop.
  //  However, GetIIDForMethodParam may need some of the nsID params when it's
  //  looking for the IID of an INTERFACE_IS.  Therefore, we can't delete it
  //  until we've gone through the 'Finalize' loop once and created the result.
  for (PRUint8 i = 0; i < paramCount && NS_SUCCEEDED(rv); i++)
  {
    const nsXPTParamInfo &paramInfo = methodInfo->GetParam(i);
    const nsXPTType &type = paramInfo.GetType();
    if (type.TagPart() == nsXPTType::T_IID) {
      nsID* iid = (nsID*) params[i].ptr;
      delete iid;
    }
  }

  if (params) {
    delete params;
  }

  // If the XPCOM method invocation failed, we don't immediately throw an
  // exception and return so that we can clean up any parameters.
  if (NS_FAILED(invokeResult)) {
    ThrowXPCOMException(env, invokeResult);
  }

  return;
}

// XXX Use org.mozilla.classfile.ClassFileWriter for stubs?
jobject
CreateJavaWrapper(JNIEnv* env, const char* aClassName)
{
  jobject java_stub = nsnull;

  // Create stub class name
  nsCAutoString class_name("org/mozilla/xpcom/stubs/");
  class_name.Append(aClassName);
  class_name.Append("_Stub");

  // Create Java stub for XPCOM object
  jclass clazz;
  clazz = env->FindClass(class_name.get());

  if (clazz) {
    jmethodID constructor = env->GetMethodID(clazz, "<init>", "()V");
    if (constructor) {
      java_stub = env->NewObject(clazz, constructor);
    }
  }

  return java_stub;
//  return env->NewGlobalRef(java_stub);
}

