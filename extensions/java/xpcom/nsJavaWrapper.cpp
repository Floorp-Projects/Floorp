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
#include "nsCRT.h"
#include "prmem.h"

static nsID nullID = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};


/**
 * Handle 'in' and 'inout' params.
 */
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
      LOG(("byte\n"));
      if (!aParamInfo.IsOut()) {  // 'in'
        aVariant.val.u8 = env->CallByteMethod(aParam, byteValueMID);
      } else {  // 'inout'
        if (aParam) {
          env->GetByteArrayRegion((jbyteArray) aParam, 0, 1,
                                  (jbyte*) &(aVariant.val.u8));
          aVariant.ptr = &aVariant.val;
        } else {
          aVariant.ptr = nsnull;
        }
        aVariant.SetPtrIsData();
      }
    }
    break;

    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
    {
      LOG(("short\n"));
      if (!aParamInfo.IsOut()) {  // 'in'
        aVariant.val.u16 = env->CallShortMethod(aParam, shortValueMID);
      } else {  // 'inout'
        if (aParam) {
          env->GetShortArrayRegion((jshortArray) aParam, 0, 1,
                                   (jshort*) &(aVariant.val.u16));
          aVariant.ptr = &aVariant.val;
        } else {
          aVariant.ptr = nsnull;
        }
        aVariant.SetPtrIsData();
      }
    }
    break;

    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
    {
      LOG(("int\n"));
      if (!aParamInfo.IsOut()) {  // 'in'
        aVariant.val.u32 = env->CallIntMethod(aParam, intValueMID);
      } else {  // 'inout'
        if (aParam) {
          env->GetIntArrayRegion((jintArray) aParam, 0, 1,
                                 (jint*) &(aVariant.val.u32));
          aVariant.ptr = &aVariant.val;
        } else {
          aVariant.ptr = nsnull;
        }
        aVariant.SetPtrIsData();
      }
    }
    break;

    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
    {
      LOG(("long\n"));
      if (!aParamInfo.IsOut()) {  // 'in'
        aVariant.val.u64 = env->CallLongMethod(aParam, longValueMID);
      } else {  // 'inout'
        if (aParam) {
          env->GetLongArrayRegion((jlongArray) aParam, 0, 1,
                                  (jlong*) &(aVariant.val.u64));
          aVariant.ptr = &aVariant.val;
        } else {
          aVariant.ptr = nsnull;
        }
        aVariant.SetPtrIsData();
      }
    }
    break;

    case nsXPTType::T_FLOAT:
    {
      LOG(("float\n"));
      if (!aParamInfo.IsOut()) {  // 'in'
        aVariant.val.f = env->CallFloatMethod(aParam, floatValueMID);
      } else {  // 'inout'
        if (aParam) {
          env->GetFloatArrayRegion((jfloatArray) aParam, 0, 1,
                                   (jfloat*) &(aVariant.val.f));
          aVariant.ptr = &aVariant.val;
        } else {
          aVariant.ptr = nsnull;
        }
        aVariant.SetPtrIsData();
      }
    }
    break;

    case nsXPTType::T_DOUBLE:
    {
      LOG(("double\n"));
      if (!aParamInfo.IsOut()) {  // 'in'
        aVariant.val.d = env->CallDoubleMethod(aParam, doubleValueMID);
      } else {  // 'inout'
        if (aParam) {
          env->GetDoubleArrayRegion((jdoubleArray) aParam, 0, 1,
                                    (jdouble*) &(aVariant.val.d));
          aVariant.ptr = &aVariant.val;
        } else {
          aVariant.ptr = nsnull;
        }
        aVariant.SetPtrIsData();
      }
    }
    break;

    case nsXPTType::T_BOOL:
    {
      LOG(("boolean\n"));
      if (!aParamInfo.IsOut()) {  // 'in'
        aVariant.val.b = env->CallBooleanMethod(aParam, booleanValueMID);
      } else {  // 'inout'
        if (aParam) {
          env->GetBooleanArrayRegion((jbooleanArray) aParam, 0, 1,
                                     (jboolean*) &(aVariant.val.b));
          aVariant.ptr = &aVariant.val;
        } else {
          aVariant.ptr = nsnull;
        }
        aVariant.SetPtrIsData();
      }
    }
    break;

    case nsXPTType::T_CHAR:
    case nsXPTType::T_WCHAR:
    {
      LOG(("char\n"));
      if (!aParamInfo.IsOut()) {  // 'in'
        if (tag == nsXPTType::T_CHAR)
          aVariant.val.c = env->CallCharMethod(aParam, charValueMID);
        else
          aVariant.val.wc = env->CallCharMethod(aParam, charValueMID);
      } else {  // 'inout'
        if (aParam) {
          if (tag == nsXPTType::T_CHAR)
            env->GetCharArrayRegion((jcharArray) aParam, 0, 1,
                                    (jchar*) &(aVariant.val.c));
          else
            env->GetCharArrayRegion((jcharArray) aParam, 0, 1,
                                    (jchar*) &(aVariant.val.wc));
          aVariant.ptr = &aVariant.val;
        } else {
          aVariant.ptr = nsnull;
        }
        aVariant.SetPtrIsData();
      }
    }
    break;

    case nsXPTType::T_CHAR_STR:
    case nsXPTType::T_WCHAR_STR:
    {
      LOG(("String\n"));
      jstring data = nsnull;
      if (!aParamInfo.IsOut()) {  // 'in'
        data = (jstring) aParam;
      } else {  // 'inout'
        if (aParam)
          data = (jstring) env->GetObjectArrayElement((jobjectArray) aParam, 0);
      }

      void* buf = nsnull;
      if (data) {
        jsize uniLength = env->GetStringLength(data);
        if (uniLength > 0) {
          if (tag == nsXPTType::T_CHAR_STR) {
            jsize utf8Length = env->GetStringUTFLength(data);
            buf = nsMemory::Alloc(utf8Length + 1);
            if (!buf) {
              rv = NS_ERROR_OUT_OF_MEMORY;
              break;
            }

            env->GetStringUTFRegion(data, 0, uniLength, (char*) buf);
            ((char*)buf)[utf8Length] = '\0';

          } else {  // if T_WCHAR_STR
            buf = nsMemory::Alloc((uniLength + 1) * sizeof(jchar));
            if (!buf) {
              rv = NS_ERROR_OUT_OF_MEMORY;
              break;
            }

            env->GetStringRegion(data, 0, uniLength, (jchar*) buf);
            ((jchar*)buf)[uniLength] = '\0';
          }
        } else {
          // create empty string
          buf = nsMemory::Alloc(2);
          if (!buf) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            break;
          }
          ((jchar*)buf)[0] = '\0';
        }
      }

      aVariant.val.p = buf;
      if (aParamInfo.IsOut()) { // 'inout'
        aVariant.ptr = &aVariant.val;
        aVariant.SetPtrIsData();
      }
    }
    break;

    case nsXPTType::T_IID:
    {
      LOG(("String(IID)\n"));
      jstring data = nsnull;
      if (!aParamInfo.IsOut()) {  // 'in'
        data = (jstring) aParam;
      } else {  // 'inout'
        if (aParam)
          data = (jstring) env->GetObjectArrayElement((jobjectArray) aParam, 0);
      }

      nsID* iid = new nsID;
      if (!iid) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        break;
      }
      if (data) {
        // extract IID string from Java string
        const char* str = env->GetStringUTFChars(data, nsnull);
        if (!str) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }

        // parse string into IID object
        iid->Parse(str);
        env->ReleaseStringUTFChars(data, str);
      } else {
        *iid = nullID;
      }

      aVariant.val.p = iid;
      if (aParamInfo.IsOut()) { // 'inout'
        aVariant.ptr = &aVariant.val;
        aVariant.SetPtrIsData();
      }
    }
    break;

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
      LOG(("nsISupports\n"));
      jobject java_obj = nsnull;
      if (!aParamInfo.IsOut()) {  // 'in'
        java_obj = (jobject) aParam;
      } else {  // 'inout'
        if (aParam)
          java_obj = (jobject) env->GetObjectArrayElement((jobjectArray) aParam, 0);
      }

      void* xpcom_obj;
      if (java_obj) {
        // Check if we already have a corresponding XPCOM object
        void* inst = gBindings->GetXPCOMObject(env, java_obj);

        // Get IID for this param
        nsID iid;
        rv = GetIIDForMethodParam(aIInfo, aMethodInfo, aParamInfo,
                                  aMethodIndex, aDispatchParams, PR_TRUE,
                                  iid);
        if (NS_FAILED(rv))
          break;

        PRBool isWeakRef = iid.Equals(NS_GET_IID(nsIWeakReference));

        if (inst == nsnull && !isWeakRef) {
          // If there is not corresponding XPCOM object, then that means that the
          // parameter is non-generated class (that is, it is not one of our
          // Java stubs that represent an exising XPCOM object).  So we need to
          // create an XPCOM stub, that can route any method calls to the class.

          // Get interface info for class
          nsCOMPtr<nsIInterfaceInfoManager> iim = XPTI_GetInterfaceInfoManager();
          nsCOMPtr<nsIInterfaceInfo> iinfo;
          rv = iim->GetInfoForIID(&iid, getter_AddRefs(iinfo));
          if (NS_FAILED(rv))
            break;

          // Create XPCOM stub
          nsJavaXPTCStub* xpcomStub = new nsJavaXPTCStub(env, java_obj, iinfo);
          if (!xpcomStub) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            break;
          }
          inst = SetAsXPTCStub(xpcomStub);
          gBindings->AddBinding(env, java_obj, inst);
        }

        if (isWeakRef) {
          // If the function expects an weak reference, then we need to
          // create it here.
          nsJavaXPTCStubWeakRef* weakref =
                                      new nsJavaXPTCStubWeakRef(env, java_obj);
          if (!weakref) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            break;
          }
          NS_ADDREF(weakref);
          xpcom_obj = (void*) weakref;
          aVariant.SetValIsAllocated();

        } else if (IsXPTCStub(inst)) {
          nsJavaXPTCStub* xpcomStub = GetXPTCStubAddr(inst);
          NS_ADDREF(xpcomStub);
          xpcom_obj = (void*) xpcomStub;
          aVariant.SetValIsAllocated();

        } else {
          JavaXPCOMInstance* xpcomInst = (JavaXPCOMInstance*) inst;
          xpcom_obj = (void*) xpcomInst->GetInstance();
        }
      } else {
        xpcom_obj = nsnull;
      }

      aVariant.val.p = xpcom_obj;
      aVariant.SetValIsInterface();
      if (aParamInfo.IsOut()) { // 'inout'
        aVariant.ptr = &aVariant.val;
        aVariant.SetPtrIsData();
      }
    }
    break;

    case nsXPTType::T_ASTRING:
    case nsXPTType::T_DOMSTRING:
    {
      LOG(("String\n"));
      jstring data = nsnull;
      if (!aParamInfo.IsOut()) {  // 'in'
        data = (jstring) aParam;
      } else {  // 'inout'
        if (aParam)
          data = (jstring) env->GetObjectArrayElement((jobjectArray) aParam, 0);
      }

      nsAString* str;
      if (data) {
        str = jstring_to_nsAString(env, data);
        if (!str) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
      } else {
        str = nsnull;
      }

      aVariant.val.p = str;
      aVariant.SetValIsDOMString();
      if (aParamInfo.IsOut()) { // 'inout'
        aVariant.ptr = &aVariant.val;
        aVariant.SetPtrIsData();
      }
    }
    break;

    case nsXPTType::T_UTF8STRING:
    case nsXPTType::T_CSTRING:
    {
      LOG(("StringUTF\n"));
      jstring data = nsnull;
      if (!aParamInfo.IsOut()) {  // 'in'
        data = (jstring) aParam;
      } else {  // 'inout'
        if (aParam)
          data = (jstring) env->GetObjectArrayElement((jobjectArray) aParam, 0);
      }

      nsACString* str;
      if (data) {
        str = jstring_to_nsACString(env, data);
        if (!str) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
      } else {
        str = nsnull;
      }

      aVariant.val.p = str;
      if (tag == nsXPTType::T_CSTRING) {
        aVariant.SetValIsCString();
      } else {
        aVariant.SetValIsUTF8String();
      }
      if (aParamInfo.IsOut()) { // 'inout'
        aVariant.ptr = &aVariant.val;
        aVariant.SetPtrIsData();
      }
    }
    break;

    // handle "void *" as an "int" in Java
    case nsXPTType::T_VOID:
    {
      LOG(("int (void*)\n"));
      if (!aParamInfo.IsOut()) {  // 'in'
        aVariant.val.p = (void*) env->CallIntMethod(aParam, intValueMID);
      } else {  // 'inout'
        if (aParam) {
          env->GetIntArrayRegion((jintArray) aParam, 0, 1,
                                 (jint*) &(aVariant.val.p));
          aVariant.ptr = &aVariant.val;
        } else {
          aVariant.ptr = nsnull;
        }
        aVariant.SetPtrIsData();
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

/**
 * Handles 'in', 'out', and 'inout' params.
 */
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

  PRUint8 tag = type.TagPart();
  switch (tag)
  {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
    {
      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        env->SetByteArrayRegion((jbyteArray) aParam, 0, 1,
                                (jbyte*) aVariant.ptr);
      }
    }
    break;

    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
    {
      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        env->SetShortArrayRegion((jshortArray) aParam, 0, 1,
                                 (jshort*) aVariant.ptr);
      }
    }
    break;

    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
    {
      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        env->SetIntArrayRegion((jintArray) aParam, 0, 1,
                               (jint*) aVariant.ptr);
      }
    }
    break;

    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
    {
      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        env->SetLongArrayRegion((jlongArray) aParam, 0, 1,
                                (jlong*) aVariant.ptr);
      }
    }
    break;

    case nsXPTType::T_FLOAT:
    {
      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        env->SetFloatArrayRegion((jfloatArray) aParam, 0, 1,
                                 (jfloat*) aVariant.ptr);
      }
    }
    break;

    case nsXPTType::T_DOUBLE:
    {
      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        env->SetDoubleArrayRegion((jdoubleArray) aParam, 0, 1,
                                  (jdouble*) aVariant.ptr);
      }
    }
    break;

    case nsXPTType::T_BOOL:
    {
      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        env->SetBooleanArrayRegion((jbooleanArray) aParam, 0, 1,
                                   (jboolean*) aVariant.ptr);
      }
    }
    break;

    case nsXPTType::T_CHAR:
    case nsXPTType::T_WCHAR:
    {
      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        env->SetCharArrayRegion((jcharArray) aParam, 0, 1,
                                (jchar*) aVariant.ptr);
      }
    }
    break;

    case nsXPTType::T_CHAR_STR:
    case nsXPTType::T_WCHAR_STR:
    {
      if (aParamInfo.IsOut() && aParam) {  // ''inout' & 'out'
        // create new string from data
        jstring str;
        if (aVariant.val.p) {
          if (tag == nsXPTType::T_CHAR_STR) {
            str = env->NewStringUTF((const char*) aVariant.val.p);
          } else {
            PRUint32 length = nsCRT::strlen((const PRUnichar*) aVariant.val.p);
            str = env->NewString((const jchar*) aVariant.val.p, length);
          }
          nsMemory::Free(aVariant.val.p);
          if (!str) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            break;
          }
        } else {
          str = nsnull;
        }

        // put new string into output array
        env->SetObjectArrayElement((jobjectArray) aParam, 0, str);
      }

      // Delete for 'in', 'inout', and 'out'
      if (aVariant.val.p)
        nsMemory::Free(aVariant.val.p);
    }
    break;

    case nsXPTType::T_IID:
    {
      nsID* iid = (nsID*) aVariant.val.p;

      if (aParamInfo.IsOut() && aParam) {  // 'inout' & 'out'
        // Create the string from nsID
        jstring str = nsnull;
        if (iid) {
          char* iid_str = iid->ToString();
          if (iid_str) {
            str = env->NewStringUTF(iid_str);
          }
          if (!iid_str || !str) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            break;
          }
          PR_Free(iid_str);
        }

        // put new string into output array
        env->SetObjectArrayElement((jobjectArray) aParam, 0, str);
      }

      // Ordinarily, we would delete 'iid' here.  But we cannot do that until
      // we've handled all of the params.  See comment in CallXPCOMMethod
    }
    break;

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
      void* xpcom_obj = aVariant.val.p;

      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        jobject java_obj = nsnull;
        if (xpcom_obj) {
          nsID iid;
          rv = GetIIDForMethodParam(aIInfo, aMethodInfo, aParamInfo,
                                    aMethodIndex, aDispatchParams, PR_TRUE, iid);
          if (NS_FAILED(rv))
            break;

          // Get matching Java object for given xpcom object
          rv = gBindings->GetJavaObject(env, xpcom_obj, iid, PR_TRUE, &java_obj);
          if (NS_FAILED(rv))
            break;
        }

        // put new Java object into output array
        env->SetObjectArrayElement((jobjectArray) aParam, 0, java_obj);
      }

      // If VAL_IS_ALLOCD is set, that means that an XPCOM object was created
      // is SetupParams that now needs to be released.
      if (xpcom_obj && aVariant.IsValAllocated()) {
        nsISupports* variant = NS_STATIC_CAST(nsISupports*, xpcom_obj);
        NS_RELEASE(variant);
      }
    }
    break;

    case nsXPTType::T_ASTRING:
    case nsXPTType::T_DOMSTRING:
    {
      nsString* str = (nsString*) aVariant.val.p;

      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        // Create Java string from returned nsString
        jstring jstr;
        if (str) {
          jstr = env->NewString((const jchar*) str->get(), str->Length());
          if (!jstr) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            break;
          }
        } else {
          jstr = nsnull;
        }

        // put new Java string into output array
        env->SetObjectArrayElement((jobjectArray) aParam, 0, jstr);
      }

      if (str) {
        delete str;
      }
    }
    break;

    case nsXPTType::T_UTF8STRING:
    case nsXPTType::T_CSTRING:
    {
      nsCString* str = (nsCString*) aVariant.val.p;

      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        // Create Java string from returned nsString
        jstring jstr;
        if (str) {
          jstr = env->NewStringUTF((const char*) str->get());
          if (!jstr) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            break;
          }
        } else {
          jstr = nsnull;
        }

        // put new Java string into output array
        env->SetObjectArrayElement((jobjectArray) aParam, 0, jstr);
      }

      if (str) {
        delete str;
      }
    }
    break;

    case nsXPTType::T_VOID:
    {
      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        env->SetIntArrayRegion((jintArray) aParam, 0, 1,
                               (jint*) aVariant.ptr);
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
      if (aVariant.ptr) {
        aResult.l = env->NewStringUTF((const char*) aVariant.ptr);
        if (aResult.l == nsnull) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
      } else {
        aResult.l = nsnull;
      }
    }
    break;

    case nsXPTType::T_WCHAR_STR:
    {
      if (aVariant.ptr) {
        PRUint32 length = nsCRT::strlen((const PRUnichar*) aVariant.ptr);
        aResult.l = env->NewString((const jchar*) aVariant.ptr, length);
        if (aResult.l == nsnull) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
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
        if (iid_str) {
          aResult.l = env->NewStringUTF(iid_str);
        }
        if (iid_str == nsnull || aResult.l == nsnull) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
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
        nsID iid;
        rv = GetIIDForMethodParam(aIInfo, aMethodInfo, aParamInfo, aMethodIndex,
                                  aDispatchParams, PR_TRUE, iid);
        if (NS_FAILED(rv))
          break;

        // Get matching Java object for given xpcom object
        jobject java_obj;
        rv = gBindings->GetJavaObject(env, aVariant.val.p, iid, PR_TRUE,
                                      &java_obj);
        if (NS_FAILED(rv))
          break;

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
        if (aResult.l == nsnull) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
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
        if (aResult.l == nsnull) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
      } else {
        aResult.l = nsnull;
      }
    }
    break;

    case nsXPTType::T_VOID:
      // handle "void *" as an "int" in Java
      LOG((" returns int (void*)"));
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
  void* xpcomObj = gBindings->GetXPCOMObject(env, aJavaObject);
  if (xpcomObj == nsnull) {
    ThrowException(env, 0, "Failed to get matching XPCOM object");
    return;
  }

  NS_ASSERTION(!IsXPTCStub(xpcomObj),
               "Expected JavaXPCOMInstance, but got nsJavaXPTCStub");
  JavaXPCOMInstance* inst = (JavaXPCOMInstance*) xpcomObj;

  // Get method info
  const nsXPTMethodInfo* methodInfo;
  nsIInterfaceInfo* iinfo = inst->InterfaceInfo();
  nsresult rv = iinfo->GetMethodInfo(aMethodIndex, &methodInfo);
  if (NS_FAILED(rv)) {
    ThrowException(env, rv, "GetMethodInfo failed");
    return;
  }

#ifdef DEBUG_pedemonte
  const char* ifaceName;
  iinfo->GetNameShared(&ifaceName);
  LOG(("=> Calling %s::%s()\n", ifaceName, methodInfo->GetName()));
#endif

  // Convert the Java params
  PRUint8 paramCount = methodInfo->GetParamCount();
  nsXPTCVariant* params = nsnull;
  if (paramCount)
  {
    params = new nsXPTCVariant[paramCount];
    if (!params) {
      ThrowException(env, NS_ERROR_OUT_OF_MEMORY, "Can't create params array");
      return;
    }

    for (PRUint8 i = 0; i < paramCount && NS_SUCCEEDED(rv); i++)
    {
      LOG(("\t Param %d: ", i));
      const nsXPTParamInfo &paramInfo = methodInfo->GetParam(i);

      if (paramInfo.IsIn() && !paramInfo.IsDipper()) {
        rv = SetupParams(env, env->GetObjectArrayElement(aParams, i), paramInfo,
                         methodInfo, iinfo, aMethodIndex, params, params[i]);
      } else if (paramInfo.IsDipper()) {
        LOG(("dipper"));
        const nsXPTType &type = paramInfo.GetType();
        switch (type.TagPart())
        {
          case nsXPTType::T_ASTRING:
          case nsXPTType::T_DOMSTRING:
            params[i].val.p = params[i].ptr = new nsString();
            if (params[i].val.p == nsnull) {
              rv = NS_ERROR_OUT_OF_MEMORY;
              break;
            }
            params[i].type = type;
            params[i].flags = nsXPTCVariant::PTR_IS_DATA |
                              nsXPTCVariant::VAL_IS_DOMSTR;
            break;

          case nsXPTType::T_UTF8STRING:
          case nsXPTType::T_CSTRING:
            params[i].val.p = params[i].ptr = new nsCString();
            if (params[i].val.p == nsnull) {
              rv = NS_ERROR_OUT_OF_MEMORY;
              break;
            }
            params[i].type = type;
            params[i].flags = nsXPTCVariant::PTR_IS_DATA |
                              nsXPTCVariant::VAL_IS_CSTR;
            break;

          default:
            LOG(("unhandled dipper type\n"));
            rv = NS_ERROR_UNEXPECTED;
        }
      } else {
        LOG(("out/retval\n"));
        params[i].ptr = &(params[i].val);
        params[i].type = paramInfo.GetType();
        params[i].flags = nsXPTCVariant::PTR_IS_DATA;
      }
    }
    if (NS_FAILED(rv)) {
      ThrowException(env, rv, "SetupParams failed");
      return;
    }
  }

  // Call the XPCOM method
  nsresult invokeResult;
  invokeResult = XPTC_InvokeByIndex(inst->GetInstance(), aMethodIndex,
                                    paramCount, params);

  // Clean up params
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
    ThrowException(env, rv, "FinalizeParams/SetRetval failed");
    return;
  }

  // Normally, we would delete any created nsID object in the above loop.
  // However, GetIIDForMethodParam may need some of the nsID params when it's
  // looking for the IID of an INTERFACE_IS.  Therefore, we can't delete it
  // until we've gone through the 'Finalize' loop once and created the result.
  for (PRUint8 j = 0; j < paramCount && NS_SUCCEEDED(rv); j++)
  {
    const nsXPTParamInfo &paramInfo = methodInfo->GetParam(j);
    const nsXPTType &type = paramInfo.GetType();
    if (type.TagPart() == nsXPTType::T_IID) {
      nsID* iid = (nsID*) params[j].ptr;
      delete iid;
    }
  }

  if (params) {
    delete params;
  }

  // If the XPCOM method invocation failed, we don't immediately throw an
  // exception and return so that we can clean up any parameters.
  if (NS_FAILED(invokeResult)) {
    nsCAutoString message("The function \"");
    message.AppendASCII(methodInfo->GetName());
    message.AppendLiteral("\" returned an error condition");
    ThrowException(env, invokeResult, message.get());
  }

  return;
}

nsresult
CreateJavaProxy(JNIEnv* env, nsISupports* aXPCOMObject, const nsIID& aIID,
                jobject* aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  jobject java_obj = nsnull;

  nsCOMPtr<nsIInterfaceInfoManager> iim = XPTI_GetInterfaceInfoManager();
  NS_ASSERTION(iim != nsnull, "Failed to get InterfaceInfoManager");
  if (!iim)
    return NS_ERROR_FAILURE;

  // Get interface info for class
  nsCOMPtr<nsIInterfaceInfo> info;
  nsresult rv = iim->GetInfoForIID(&aIID, getter_AddRefs(info));
  if (NS_FAILED(rv))
    return rv;

  // Wrap XPCOM object
  JavaXPCOMInstance* inst = new JavaXPCOMInstance(aXPCOMObject, info);
  if (!inst)
    return NS_ERROR_OUT_OF_MEMORY;

  // Get interface name
  char* iface_name;
  rv = info->GetName(&iface_name);

  if (NS_SUCCEEDED(rv)) {
    // Create proxy class name
    nsCAutoString class_name("org/mozilla/xpcom/stubs/");
    class_name.AppendASCII(iface_name);
    class_name.AppendLiteral("_Stub");
    nsMemory::Free(iface_name);

    // Create java proxy object
    jclass clazz;
    clazz = env->FindClass(class_name.get());
    if (clazz) {
      jmethodID constructor = env->GetMethodID(clazz, "<init>", "()V");
      if (constructor) {
        java_obj = env->NewObject(clazz, constructor);
      }
    }

    if (java_obj) {
      // Associate XPCOM object with Java proxy
      rv = gBindings->AddBinding(env, java_obj, inst);
      if (NS_SUCCEEDED(rv)) {
        *aResult = java_obj;
        return NS_OK;
      }
    } else {
      rv = NS_ERROR_FAILURE;
    }
  }

  // If there was an error, clean up.
  delete inst;
  return rv;
}

