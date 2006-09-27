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

#define JAVAPROXY_NATIVE(func) Java_org_mozilla_xpcom_XPCOMJavaProxy_##func

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
          java_obj = (jobject)
                       env->GetObjectArrayElement((jobjectArray) aParam, 0);
      }

      nsISupports* xpcom_obj;
      if (java_obj) {
        // Get IID for this param
        nsID iid;
        rv = GetIIDForMethodParam(aIInfo, aMethodInfo, aParamInfo,
                                  aMethodIndex, aDispatchParams, PR_TRUE,
                                  iid);
        if (NS_FAILED(rv))
          break;

        // If the requested interface is nsIWeakReference, then we look for or
        // create a stub for the nsISupports interface.  Then we create a weak
        // reference from that stub.
        PRBool isWeakRef;
        if (iid.Equals(NS_GET_IID(nsIWeakReference))) {
          isWeakRef = PR_TRUE;
          iid = NS_GET_IID(nsISupports);
        } else {
          isWeakRef = PR_FALSE;
        }

        PRBool isXPTCStub;
        rv = GetNewOrUsedXPCOMObject(env, java_obj, iid, &xpcom_obj,
                                     &isXPTCStub);
        if (NS_FAILED(rv))
          break;

        // If the function expects a weak reference, then we need to
        // create it here.
        if (isWeakRef) {
          if (isXPTCStub) {
            nsJavaXPTCStub* stub = NS_STATIC_CAST(nsJavaXPTCStub*,
                                                 NS_STATIC_CAST(nsXPTCStubBase*,
                                                                xpcom_obj));
            nsJavaXPTCStubWeakRef* weakref;
            weakref = new nsJavaXPTCStubWeakRef(env, java_obj, stub);
            if (!weakref) {
              rv = NS_ERROR_OUT_OF_MEMORY;
              break;
            }
            NS_RELEASE(xpcom_obj);
            xpcom_obj = weakref;
            NS_ADDREF(xpcom_obj);
          } else { // if is native XPCOM object
            nsCOMPtr<nsISupportsWeakReference> supportsweak =
                                                   do_QueryInterface(xpcom_obj);
            if (supportsweak) {
              nsWeakPtr weakref;
              supportsweak->GetWeakReference(getter_AddRefs(weakref));
              NS_RELEASE(xpcom_obj);
              xpcom_obj = weakref;
              NS_ADDREF(xpcom_obj);
            } else {
              xpcom_obj = nsnull;
            }
          }
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
      nsISupports* xpcom_obj = NS_STATIC_CAST(nsISupports*, aVariant.val.p);

      if (aParamInfo.IsOut() && aParam) { // 'inout' & 'out'
        jobject java_obj = nsnull;
        if (xpcom_obj) {
          nsID iid;
          rv = GetIIDForMethodParam(aIInfo, aMethodInfo, aParamInfo,
                                    aMethodIndex, aDispatchParams, PR_TRUE, iid);
          if (NS_FAILED(rv))
            break;

          // Get matching Java object for given xpcom object
          PRBool isNewProxy;
          rv = GetNewOrUsedJavaObject(env, xpcom_obj, iid, &java_obj,
                                      &isNewProxy);
          if (NS_FAILED(rv))
            break;
          if (isNewProxy)
            NS_RELEASE(xpcom_obj);   // Java proxy owns ref to object
        }

        // put new Java object into output array
        env->SetObjectArrayElement((jobjectArray) aParam, 0, java_obj);
      }

      NS_IF_RELEASE(xpcom_obj);
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

    case nsXPTType::T_ARRAY:
      NS_WARNING("array types are not yet supported");
      return NS_ERROR_NOT_IMPLEMENTED;

    default:
      NS_WARNING("unexpected parameter type");
      return NS_ERROR_UNEXPECTED;
  }

  return rv;
}

/**
 * Handles 'retval' and 'dipper' params.
 */
nsresult
SetRetval(JNIEnv *env, const nsXPTParamInfo &aParamInfo,
          const nsXPTMethodInfo* aMethodInfo, nsIInterfaceInfo* aIInfo,
          PRUint16 aMethodIndex, nsXPTCVariant* aDispatchParams,
          nsXPTCVariant &aVariant, jobject* result)
{
  nsresult rv = NS_OK;

  PRUint8 type = aParamInfo.GetType().TagPart();
  switch (type)
  {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
      *result = env->NewObject(byteClass, byteInitMID, aVariant.val.u8);
      break;

    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
      *result = env->NewObject(shortClass, shortInitMID, aVariant.val.u16);
      break;

    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
      *result = env->NewObject(intClass, intInitMID, aVariant.val.u32);
      break;

    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
      *result = env->NewObject(longClass, longInitMID, aVariant.val.u64);
      break;

    case nsXPTType::T_FLOAT:
      *result = env->NewObject(floatClass, floatInitMID, aVariant.val.f);
      break;

    case nsXPTType::T_DOUBLE:
      *result = env->NewObject(doubleClass, doubleInitMID, aVariant.val.d);
      break;

    case nsXPTType::T_BOOL:
      *result = env->NewObject(booleanClass, booleanInitMID, aVariant.val.b);
      break;

    case nsXPTType::T_CHAR:
      *result = env->NewObject(charClass, charInitMID, aVariant.val.c);
      break;

    case nsXPTType::T_WCHAR:
      *result = env->NewObject(charClass, charInitMID, aVariant.val.wc);
      break;

    case nsXPTType::T_CHAR_STR:
    {
      if (aVariant.val.p) {
        *result = env->NewStringUTF((const char*) aVariant.val.p);
        if (*result == nsnull) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
      }
    }
    break;

    case nsXPTType::T_WCHAR_STR:
    {
      if (aVariant.val.p) {
        PRUint32 length = nsCRT::strlen((const PRUnichar*) aVariant.val.p);
        *result = env->NewString((const jchar*) aVariant.val.p, length);
        if (*result == nsnull) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
      }
    }
    break;

    case nsXPTType::T_IID:
    {
      if (aVariant.val.p) {
        nsID* iid = (nsID*) aVariant.val.p;
        char* iid_str = iid->ToString();
        if (iid_str) {
          *result = env->NewStringUTF(iid_str);
        }
        if (iid_str == nsnull || *result == nsnull) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
        PR_Free(iid_str);
      }
    }
    break;

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
      nsISupports* xpcom_obj = NS_STATIC_CAST(nsISupports*, aVariant.val.p);
      if (xpcom_obj) {
        nsID iid;
        rv = GetIIDForMethodParam(aIInfo, aMethodInfo, aParamInfo, aMethodIndex,
                                  aDispatchParams, PR_TRUE, iid);
        if (NS_FAILED(rv))
          break;

        // Get matching Java object for given xpcom object
        jobject java_obj;
        PRBool isNewProxy;
        rv = GetNewOrUsedJavaObject(env, xpcom_obj, iid, &java_obj,
                                    &isNewProxy);
        if (NS_FAILED(rv))
          break;
        if (isNewProxy)
          xpcom_obj->Release();   // Java proxy owns ref to object

        // If returned object is an nsJavaXPTCStub, release it.
        nsJavaXPTCStub* stub = nsnull;
        xpcom_obj->QueryInterface(NS_GET_IID(nsJavaXPTCStub), (void**) &stub);
        if (stub) {
          NS_RELEASE(xpcom_obj);
          NS_RELEASE(stub);
        }

        *result = java_obj;
      }
    }
    break;

    case nsXPTType::T_ASTRING:
    case nsXPTType::T_DOMSTRING:
    {
      if (aVariant.val.p) {
        nsString* str = NS_STATIC_CAST(nsString*, aVariant.val.p);
        *result = env->NewString(str->get(), str->Length());
        if (*result == nsnull) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
        delete str;
      }
    }
    break;

    case nsXPTType::T_UTF8STRING:
    case nsXPTType::T_CSTRING:
    {
      if (aVariant.val.p) {
        nsCString* str = NS_STATIC_CAST(nsCString*, aVariant.val.p);
        *result = env->NewStringUTF(str->get());
        if (*result == nsnull) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
        delete str;
      }
    }
    break;

    case nsXPTType::T_VOID:
      // handle "void *" as an "int" in Java
      LOG((" returns int (void*)"));
      *result = env->NewObject(intClass, intInitMID, aVariant.val.p);
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

/**
 * Given an interface info struct and a method name, returns the method info
 * and index, if that method exists.
 *
 * Most method names are lower case.  Unfortunately, the method names of some
 * interfaces (such as nsIAppShell) start with a capital letter.  This function
 * will try all of the permutations.
 */
nsresult
QueryMethodInfo(nsIInterfaceInfo* aIInfo, const char* aMethodName,
                PRUint16* aMethodIndex, const nsXPTMethodInfo** aMethodInfo)
{
  // The common case is that the method name is lower case, so we check
  // that first.
  nsresult rv;
  rv = aIInfo->GetMethodInfoForName(aMethodName, aMethodIndex, aMethodInfo);
  if (NS_SUCCEEDED(rv))
    return rv;

  // If there is no method called <aMethodName>, then maybe it is an
  // 'attribute'.  An 'attribute' will start with "get" or "set".  But first,
  // we check the length, in order to skip over method names that match exactly
  // "get" or "set".
  if (strlen(aMethodName) > 3) {
    if (strncmp("get", aMethodName, 3) == 0) {
      char* getterName = strdup(aMethodName + 3);
      getterName[0] = tolower(getterName[0]);
      rv = aIInfo->GetMethodInfoForName(getterName, aMethodIndex, aMethodInfo);
      free(getterName);
    } else if (strncmp("set", aMethodName, 3) == 0) {
      char* setterName = strdup(aMethodName + 3);
      setterName[0] = tolower(setterName[0]);
      rv = aIInfo->GetMethodInfoForName(setterName, aMethodIndex, aMethodInfo);
      if (NS_SUCCEEDED(rv)) {
        // If this succeeded, GetMethodInfoForName will have returned the
        // method info for the 'getter'.  We want the 'setter', so increase
        // method index by one ('setter' immediately follows the 'getter'),
        // and get its method info.
        (*aMethodIndex)++;
        rv = aIInfo->GetMethodInfo(*aMethodIndex, aMethodInfo);
        if (NS_SUCCEEDED(rv)) {
          // Double check that this methodInfo matches the given method.
          if (!(*aMethodInfo)->IsSetter() ||
              strcmp(setterName, (*aMethodInfo)->name) != 0) {
            rv = NS_ERROR_FAILURE;
          }
        }
      }
      free(setterName);
    }
  }
  if (NS_SUCCEEDED(rv))
    return rv;

  // If we get here, then maybe the method name is capitalized.
  char* methodName = strdup(aMethodName);
  methodName[0] = toupper(methodName[0]);
  rv = aIInfo->GetMethodInfoForName(methodName, aMethodIndex, aMethodInfo);
  free(methodName);
  if (NS_SUCCEEDED(rv))
    return rv;

  // If there is no method called <aMethodName>, then maybe it is an
  // 'attribute'.
  if (strlen(aMethodName) > 3) {
    if (strncmp("get", aMethodName, 3) == 0) {
      char* getterName = strdup(aMethodName + 3);
      rv = aIInfo->GetMethodInfoForName(getterName, aMethodIndex, aMethodInfo);
      free(getterName);
    } else if (strncmp("set", aMethodName, 3) == 0) {
      char* setterName = strdup(aMethodName + 3);
      rv = aIInfo->GetMethodInfoForName(setterName, aMethodIndex, aMethodInfo);
      if (NS_SUCCEEDED(rv)) {
        // If this succeeded, GetMethodInfoForName will have returned the
        // method info for the 'getter'.  We want the 'setter', so increase
        // method index by one ('setter' immediately follows the 'getter'),
        // and get its method info.
        (*aMethodIndex)++;
        rv = aIInfo->GetMethodInfo(*aMethodIndex, aMethodInfo);
        if (NS_SUCCEEDED(rv)) {
          // Double check that this methodInfo matches the given method.
          if (!(*aMethodInfo)->IsSetter() ||
              strcmp(setterName, (*aMethodInfo)->name) != 0) {
            rv = NS_ERROR_FAILURE;
          }
        }
      }
      free(setterName);
    }
  }

  return rv;
}

/**
 *  org.mozilla.xpcom.XPCOMJavaProxy.callXPCOMMethod
 */
extern "C" JX_EXPORT jobject JNICALL
JAVAPROXY_NATIVE(callXPCOMMethod) (JNIEnv *env, jclass that, jobject aJavaProxy,
                                   jstring aMethodName, jobjectArray aParams)
{
  nsresult rv;

  // Get native XPCOM instance
  void* xpcom_obj;
  rv = GetXPCOMInstFromProxy(env, aJavaProxy, &xpcom_obj);
  if (NS_FAILED(rv)) {
    ThrowException(env, 0, "Failed to get matching XPCOM object");
    return nsnull;
  }
  JavaXPCOMInstance* inst = NS_STATIC_CAST(JavaXPCOMInstance*, xpcom_obj);

  // Get method info
  PRUint16 methodIndex;
  const nsXPTMethodInfo* methodInfo;
  nsIInterfaceInfo* iinfo = inst->InterfaceInfo();
  const char* methodName = env->GetStringUTFChars(aMethodName, nsnull);
  rv = QueryMethodInfo(iinfo, methodName, &methodIndex, &methodInfo);
  env->ReleaseStringUTFChars(aMethodName, methodName);

  if (NS_FAILED(rv)) {
    ThrowException(env, rv, "GetMethodInfoForName failed");
    return nsnull;
  }

#ifdef DEBUG_JAVAXPCOM
  const char* ifaceName;
  iinfo->GetNameShared(&ifaceName);
  LOG(("===> (XPCOM) %s::%s()\n", ifaceName, methodInfo->GetName()));
#endif

  // Convert the Java params
  PRUint8 paramCount = methodInfo->GetParamCount();
  nsXPTCVariant* params = nsnull;
  if (paramCount)
  {
    params = new nsXPTCVariant[paramCount];
    if (!params) {
      ThrowException(env, NS_ERROR_OUT_OF_MEMORY, "Can't create params array");
      return nsnull;
    }

    for (PRUint8 i = 0; i < paramCount && NS_SUCCEEDED(rv); i++)
    {
      LOG(("\t Param %d: ", i));
      const nsXPTParamInfo &paramInfo = methodInfo->GetParam(i);

      if (paramInfo.IsIn() && !paramInfo.IsDipper()) {
        rv = SetupParams(env, env->GetObjectArrayElement(aParams, i), paramInfo,
                         methodInfo, iinfo, methodIndex, params, params[i]);
      } else if (paramInfo.IsDipper()) {
        LOG(("dipper\n"));
        const nsXPTType &type = paramInfo.GetType();
        switch (type.TagPart())
        {
          case nsXPTType::T_ASTRING:
          case nsXPTType::T_DOMSTRING:
            params[i].val.p = new nsString();
            if (params[i].val.p == nsnull) {
              rv = NS_ERROR_OUT_OF_MEMORY;
              break;
            }
            params[i].type = type;
            params[i].flags = nsXPTCVariant::VAL_IS_DOMSTR;
            break;

          case nsXPTType::T_UTF8STRING:
          case nsXPTType::T_CSTRING:
            params[i].val.p = new nsCString();
            if (params[i].val.p == nsnull) {
              rv = NS_ERROR_OUT_OF_MEMORY;
              break;
            }
            params[i].type = type;
            params[i].flags = nsXPTCVariant::VAL_IS_CSTR;
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
      return nsnull;
    }
  }

  // Call the XPCOM method
  nsresult invokeResult;
  invokeResult = XPTC_InvokeByIndex(inst->GetInstance(), methodIndex,
                                    paramCount, params);

  // Clean up params
  jobject result = nsnull;
  for (PRUint8 i = 0; i < paramCount && NS_SUCCEEDED(rv); i++)
  {
    const nsXPTParamInfo &paramInfo = methodInfo->GetParam(i);

    if (!paramInfo.IsRetval()) {
      rv = FinalizeParams(env, env->GetObjectArrayElement(aParams, i),
                          paramInfo, methodInfo, iinfo, methodIndex,
                          params, params[i]);
    } else {
      rv = SetRetval(env, paramInfo, methodInfo, iinfo, methodIndex, params,
                     params[i], &result);
    }
  }
  if (NS_FAILED(rv)) {
    ThrowException(env, rv, "FinalizeParams/SetRetval failed");
    return nsnull;
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
      nsID* iid = (nsID*) params[j].val.p;
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

  LOG(("<=== (XPCOM) %s::%s()\n", ifaceName, methodInfo->GetName()));
  return result;
}

nsresult
CreateJavaProxy(JNIEnv* env, nsISupports* aXPCOMObject, const nsIID& aIID,
                jobject* aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

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
  const char* iface_name;
  rv = info->GetNameShared(&iface_name);

  if (NS_SUCCEEDED(rv)) {
    jobject java_obj = nsnull;

    // Create proper Java interface name
    nsCAutoString class_name("org/mozilla/xpcom/");
    class_name.AppendASCII(iface_name);
    jclass ifaceClass = env->FindClass(class_name.get());

    if (ifaceClass) {
      java_obj = env->CallStaticObjectMethod(xpcomJavaProxyClass,
                                             createProxyMID, ifaceClass,
                                             NS_REINTERPRET_CAST(jlong, inst));
      if (env->ExceptionCheck())
        java_obj = nsnull;
    }

    if (java_obj) {
#ifdef DEBUG_JAVAXPCOM
      char* iid_str = aIID.ToString();
      LOG(("+ CreateJavaProxy (Java=%08x | XPCOM=%08x | IID=%s)\n",
           (PRUint32) env->CallIntMethod(java_obj, hashCodeMID),
           (PRUint32) aXPCOMObject, iid_str));
      PR_Free(iid_str);
#endif

      // Associate XPCOM object with Java proxy
      rv = gNativeToJavaProxyMap->Add(env, aXPCOMObject, aIID, java_obj);
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

nsresult
GetXPCOMInstFromProxy(JNIEnv* env, jobject aJavaObject, void** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  long xpcom_obj = env->CallStaticLongMethod(xpcomJavaProxyClass,
                                             getNativeXPCOMInstMID, aJavaObject);

  if (!xpcom_obj || env->ExceptionCheck()) {
    return NS_ERROR_FAILURE;
  }

  *aResult = NS_REINTERPRET_CAST(void*, xpcom_obj);
#ifdef DEBUG_JAVAXPCOM
  JavaXPCOMInstance* inst = NS_STATIC_CAST(JavaXPCOMInstance*, *aResult);
  nsIID* iid;
  inst->InterfaceInfo()->GetInterfaceIID(&iid);
  char* iid_str = iid->ToString();
  LOG(("< GetXPCOMInstFromProxy (Java=%08x | XPCOM=%08x | IID=%s)\n",
       (PRUint32) env->CallIntMethod(aJavaObject, hashCodeMID),
       (PRUint32) inst->GetInstance(), iid_str));
  PR_Free(iid_str);
  nsMemory::Free(iid);
#endif
  return NS_OK;
}

/**
 *  org.mozilla.xpcom.XPCOMJavaProxy.finalizeProxy
 */
extern "C" JX_EXPORT void JNICALL
JAVAPROXY_NATIVE(finalizeProxy) (JNIEnv *env, jclass that, jobject aJavaProxy)
{
#ifdef DEBUG_JAVAXPCOM
  PRUint32 xpcom_addr = 0;
#endif

  // Due to Java's garbage collection, this finalize statement may get called
  // after FreeJavaGlobals().  So check to make sure that everything is still
  // initialized.
  if (gJavaXPCOMLock) {
    nsAutoLock lock(gJavaXPCOMLock);

    // If may be possible for the lock to be acquired here when FreeGlobals is
    // in the middle of running.  If so, then this thread will sleep until
    // FreeGlobals releases its lock.  At that point, we resume this thread
    // here, but JavaXPCOM may no longer be initialized.  So we need to check
    // that everything is legit after acquiring the lock.
    if (gJavaXPCOMInitialized) {
      // Get native XPCOM instance
      void* xpcom_obj;
      nsresult rv = GetXPCOMInstFromProxy(env, aJavaProxy, &xpcom_obj);
      if (NS_SUCCEEDED(rv)) {
        JavaXPCOMInstance* inst = NS_STATIC_CAST(JavaXPCOMInstance*, xpcom_obj);
#ifdef DEBUG_JAVAXPCOM
        xpcom_addr = NS_REINTERPRET_CAST(PRUint32, inst->GetInstance());
#endif
        nsIID* iid;
        rv = inst->InterfaceInfo()->GetInterfaceIID(&iid);
        if (NS_SUCCEEDED(rv)) {
          rv = gNativeToJavaProxyMap->Remove(env, inst->GetInstance(), *iid);
          nsMemory::Free(iid);
        }
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to RemoveJavaProxy");
        delete inst;
      }
    }
  }

#ifdef DEBUG_JAVAXPCOM
  LOG(("- Finalize (Java=%08x | XPCOM=%08x)\n",
       (PRUint32) env->CallIntMethod(aJavaProxy, hashCodeMID), xpcom_addr));
#endif
}

