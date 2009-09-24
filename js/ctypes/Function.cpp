/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * The Original Code is js-ctypes.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Finkle <mark.finkle@gmail.com>, <mfinkle@mozilla.com>
 *  Fredrik Larsson <nossralf@gmail.com>
 *  Dan Witte <dwitte@mozilla.com>
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

#include "Function.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIXPConnect.h"
#include "nsCRT.h"

namespace mozilla {
namespace ctypes {

/*******************************************************************************
** Static helpers
*******************************************************************************/

template<class IntegerType>
static bool
jsvalToIntStrict(jsval aValue, IntegerType *aResult)
{
  if (JSVAL_IS_INT(aValue)) {
    jsint i = JSVAL_TO_INT(aValue);
    *aResult = IntegerType(i);

    // Make sure the integer fits in the alotted precision, and has the right sign.
    return jsint(*aResult) == i &&
           (i < 0) == (*aResult < 0);
  }
  if (JSVAL_IS_DOUBLE(aValue)) {
    jsdouble d = *JSVAL_TO_DOUBLE(aValue);
    *aResult = IntegerType(d);

    // Don't silently lose bits here -- check that aValue really is an
    // integer value, and has the right sign.
    return jsdouble(*aResult) == d &&
           (d < 0) == (*aResult < 0);
  }
  if (JSVAL_IS_BOOLEAN(aValue)) {
    // Implicitly promote boolean values to 0 or 1, like C.
    *aResult = JSVAL_TO_BOOLEAN(aValue);
    NS_ASSERTION(*aResult == 0 || *aResult == 1, "invalid boolean");
    return true;
  }
  // Don't silently convert null to an integer. It's probably a mistake.
  return false;
}

static bool
jsvalToDoubleStrict(jsval aValue, jsdouble *dp)
{
  // Don't silently convert true to 1.0 or false to 0.0, even though C/C++
  // does it. It's likely to be a mistake.
  if (JSVAL_IS_INT(aValue)) {
    *dp = JSVAL_TO_INT(aValue);
    return true;
  }
  if (JSVAL_IS_DOUBLE(aValue)) {
    *dp = *JSVAL_TO_DOUBLE(aValue);
    return true;
  }
  return false;
}

JSErrorFormatString ErrorFormatString[CTYPESERR_LIMIT] = {
#define MSG_DEF(name, number, count, exception, format) \
  { format, count, exception } ,
#include "ctypes.msg"
#undef MSG_DEF
};

const JSErrorFormatString*
GetErrorMessage(void* userRef, const char* locale, const uintN errorNumber)
{
  if (0 < errorNumber && errorNumber < CTYPESERR_LIMIT)
    return &ErrorFormatString[errorNumber];
  return NULL;
}

static const char*
ToSource(JSContext* cx, jsval vp)
{
  JSString* str = JS_ValueToSource(cx, vp);
  if (str)
    return JS_GetStringBytes(str);

  JS_ClearPendingException(cx);
  return "<<error converting value to string>>";
}

static nsresult
TypeError(JSContext* cx, const char* expected, jsval actual)
{
  const char* src = ToSource(cx, actual);
  JS_ReportErrorNumber(cx, GetErrorMessage, NULL,
                       CTYPESMSG_TYPE_ERROR, expected, src);
  return NS_ERROR_FAILURE;
}

static nsresult
GetABI(PRUint16 aCallType, ffi_abi& aResult)
{
  // determine the ABI from the subset of those available on the
  // given platform. nsIForeignLibrary::DEFAULT specifies the default
  // C calling convention (cdecl) on each platform.
  switch (aCallType) {
  case nsIForeignLibrary::DEFAULT:
    aResult = FFI_DEFAULT_ABI;
    return NS_OK;
#if defined(XP_WIN32)
  case nsIForeignLibrary::STDCALL:
    aResult = FFI_STDCALL;
    return NS_OK;
#endif
  default:
    return NS_ERROR_INVALID_ARG;
  }
}

static nsresult
PrepareType(JSContext* aContext, jsval aType, Type& aResult)
{
  // for now, the only types we accept are integer values.
  if (!JSVAL_IS_INT(aType)) {
    JS_ReportError(aContext, "Invalid type specification");
    return NS_ERROR_FAILURE;
  }

  PRInt32 type = JSVAL_TO_INT(aType);

  switch (type) {
  case nsIForeignLibrary::VOID:
    aResult.mFFIType = ffi_type_void;
    break;
  case nsIForeignLibrary::INT8:
    aResult.mFFIType = ffi_type_sint8;
    break;
  case nsIForeignLibrary::INT16:
    aResult.mFFIType = ffi_type_sint16;
    break;
  case nsIForeignLibrary::INT32:
    aResult.mFFIType = ffi_type_sint32;
    break;
  case nsIForeignLibrary::INT64:
    aResult.mFFIType = ffi_type_sint64;
    break;
  case nsIForeignLibrary::BOOL:
  case nsIForeignLibrary::UINT8:
    aResult.mFFIType = ffi_type_uint8;
    break;
  case nsIForeignLibrary::UINT16:
    aResult.mFFIType = ffi_type_uint16;
    break;
  case nsIForeignLibrary::UINT32:
    aResult.mFFIType = ffi_type_uint32;
    break;
  case nsIForeignLibrary::UINT64:
    aResult.mFFIType = ffi_type_uint64;
    break;
  case nsIForeignLibrary::FLOAT:
    aResult.mFFIType = ffi_type_float;
    break;
  case nsIForeignLibrary::DOUBLE:
    aResult.mFFIType = ffi_type_double;
    break;
  case nsIForeignLibrary::STRING:
  case nsIForeignLibrary::USTRING:
    aResult.mFFIType = ffi_type_pointer;
    break;
  default:
    JS_ReportError(aContext, "Invalid type specification");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  aResult.mType = type;

  return NS_OK;
}

static nsresult
PrepareValue(JSContext* aContext, const Type& aType, jsval aValue, Value& aResult)
{
  jsdouble d;

  switch (aType.mType) {
  case nsIForeignLibrary::BOOL:
    // Do not implicitly lose bits, but allow the values 0, 1, and -0.
    // Programs can convert explicitly, if needed, using `Boolean(v)` or `!!v`.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mUint8) ||
        aResult.mValue.mUint8 > 1)
      return TypeError(aContext, "boolean", aValue);

    aResult.mData = &aResult.mValue.mUint8;
    break;
  case nsIForeignLibrary::INT8:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mInt8))
      return TypeError(aContext, "int8", aValue);

    aResult.mData = &aResult.mValue.mInt8;
    break;
  case nsIForeignLibrary::INT16:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mInt16))
      return TypeError(aContext, "int16", aValue);

    aResult.mData = &aResult.mValue.mInt16;
    break;
  case nsIForeignLibrary::INT32:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mInt32))
      return TypeError(aContext, "int32", aValue);

    aResult.mData = &aResult.mValue.mInt32;
  case nsIForeignLibrary::INT64:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mInt64))
      return TypeError(aContext, "int64", aValue);

    aResult.mData = &aResult.mValue.mInt64;
    break;
  case nsIForeignLibrary::UINT8:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mUint8))
      return TypeError(aContext, "uint8", aValue);

    aResult.mData = &aResult.mValue.mUint8;
    break;
  case nsIForeignLibrary::UINT16:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mUint16))
      return TypeError(aContext, "uint16", aValue);

    aResult.mData = &aResult.mValue.mUint16;
    break;
  case nsIForeignLibrary::UINT32:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mUint32))
      return TypeError(aContext, "uint32", aValue);

    aResult.mData = &aResult.mValue.mUint32;
  case nsIForeignLibrary::UINT64:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mUint64))
      return TypeError(aContext, "uint64", aValue);

    aResult.mData = &aResult.mValue.mUint64;
    break;
  case nsIForeignLibrary::FLOAT:
    if (!jsvalToDoubleStrict(aValue, &d))
      return TypeError(aContext, "float", aValue);

    // The following cast silently throws away some bits, but there's
    // no good way around it. Sternly requiring that the 64-bit double
    // argument be exactly representable as a 32-bit float is
    // unrealistic: it would allow 1/2 to pass but not 1/3.
    aResult.mValue.mFloat = float(d);
    aResult.mData = &aResult.mValue.mFloat;
    break;
  case nsIForeignLibrary::DOUBLE:
    if (!jsvalToDoubleStrict(aValue, &d))
      return TypeError(aContext, "double", aValue);

    aResult.mValue.mDouble = d;
    aResult.mData = &aResult.mValue.mDouble;
    break;
  case nsIForeignLibrary::STRING:
    if (JSVAL_IS_NULL(aValue)) {
      // Allow passing a null pointer.
      aResult.mValue.mPointer = nsnull;
    } else if (JSVAL_IS_STRING(aValue)) {
      aResult.mValue.mPointer = JS_GetStringBytes(JSVAL_TO_STRING(aValue));
    } else {
      // Don't implicitly convert to string. Users can implicitly convert
      // with `String(x)` or `""+x`.
      return TypeError(aContext, "string", aValue);
    }

    aResult.mData = &aResult.mValue.mPointer;
    break;
  case nsIForeignLibrary::USTRING:
    if (JSVAL_IS_NULL(aValue)) {
      // Allow passing a null pointer.
      aResult.mValue.mPointer = nsnull;
    } else if (JSVAL_IS_STRING(aValue)) {
      aResult.mValue.mPointer = JS_GetStringChars(JSVAL_TO_STRING(aValue));
    } else {
      // Don't implicitly convert to string. Users can implicitly convert
      // with `String(x)` or `""+x`.
      return TypeError(aContext, "ustring", aValue);
    }

    aResult.mData = &aResult.mValue.mPointer;
    break;
  default:
    NS_NOTREACHED("invalid type");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static void
PrepareReturnValue(const Type& aType, Value& aResult)
{
  switch (aType.mType) {
  case nsIForeignLibrary::VOID:
    aResult.mData = nsnull;
    break;
  case nsIForeignLibrary::INT8:
    aResult.mData = &aResult.mValue.mInt8;
    break;
  case nsIForeignLibrary::INT16:
    aResult.mData = &aResult.mValue.mInt16;
    break;
  case nsIForeignLibrary::INT32:
    aResult.mData = &aResult.mValue.mInt32;
    break;
  case nsIForeignLibrary::INT64:
    aResult.mData = &aResult.mValue.mInt64;
    break;
  case nsIForeignLibrary::BOOL:
  case nsIForeignLibrary::UINT8:
    aResult.mData = &aResult.mValue.mUint8;
    break;
  case nsIForeignLibrary::UINT16:
    aResult.mData = &aResult.mValue.mUint16;
    break;
  case nsIForeignLibrary::UINT32:
    aResult.mData = &aResult.mValue.mUint32;
    break;
  case nsIForeignLibrary::UINT64:
    aResult.mData = &aResult.mValue.mUint64;
    break;
  case nsIForeignLibrary::FLOAT:
    aResult.mData = &aResult.mValue.mFloat;
    break;
  case nsIForeignLibrary::DOUBLE:
    aResult.mData = &aResult.mValue.mDouble;
    break;
  case nsIForeignLibrary::STRING:
  case nsIForeignLibrary::USTRING:
    aResult.mData = &aResult.mValue.mPointer;
    break;
  default:
    NS_NOTREACHED("invalid type");
    break;
  }
}

static nsresult
ConvertReturnValue(JSContext* aContext,
                   const Type& aResultType,
                   const Value& aResultValue,
                   jsval* aValue)
{
  switch (aResultType.mType) {
  case nsIForeignLibrary::VOID:
    *aValue = JSVAL_VOID;
    break;
  case nsIForeignLibrary::BOOL:
    *aValue = aResultValue.mValue.mUint8 ? JSVAL_TRUE : JSVAL_FALSE;
    break;
  case nsIForeignLibrary::INT8:
    *aValue = INT_TO_JSVAL(aResultValue.mValue.mInt8);
    break;
  case nsIForeignLibrary::INT16:
    *aValue = INT_TO_JSVAL(aResultValue.mValue.mInt16);
    break;
  case nsIForeignLibrary::INT32:
    if (!JS_NewNumberValue(aContext, jsdouble(aResultValue.mValue.mInt32), aValue))
      return NS_ERROR_OUT_OF_MEMORY;
    break;
  case nsIForeignLibrary::INT64:
    // Implicit conversion with loss of bits.  :-[
    if (!JS_NewNumberValue(aContext, jsdouble(aResultValue.mValue.mInt64), aValue))
      return NS_ERROR_OUT_OF_MEMORY;
    break;
  case nsIForeignLibrary::UINT8:
    *aValue = INT_TO_JSVAL(aResultValue.mValue.mUint8);
    break;
  case nsIForeignLibrary::UINT16:
    *aValue = INT_TO_JSVAL(aResultValue.mValue.mUint16);
    break;
  case nsIForeignLibrary::UINT32:
    if (!JS_NewNumberValue(aContext, jsdouble(aResultValue.mValue.mUint32), aValue))
      return NS_ERROR_OUT_OF_MEMORY;
    break;
  case nsIForeignLibrary::UINT64:
    // Implicit conversion with loss of bits.  :-[
    if (!JS_NewNumberValue(aContext, jsdouble(aResultValue.mValue.mUint64), aValue))
      return NS_ERROR_OUT_OF_MEMORY;
    break;
  case nsIForeignLibrary::FLOAT:
    if (!JS_NewNumberValue(aContext, jsdouble(aResultValue.mValue.mFloat), aValue))
      return NS_ERROR_OUT_OF_MEMORY;
    break;
  case nsIForeignLibrary::DOUBLE:
    if (!JS_NewNumberValue(aContext, jsdouble(aResultValue.mValue.mDouble), aValue))
      return NS_ERROR_OUT_OF_MEMORY;
    break;
  case nsIForeignLibrary::STRING: {
    if (!aResultValue.mValue.mPointer) {
      // Allow returning a null pointer.
      *aValue = JSVAL_NULL;
    } else {
      JSString *jsstring = JS_NewStringCopyZ(aContext,
                             reinterpret_cast<const char*>(aResultValue.mValue.mPointer));
      if (!jsstring)
        return NS_ERROR_OUT_OF_MEMORY;

      *aValue = STRING_TO_JSVAL(jsstring);
    }
    break;
  }
  case nsIForeignLibrary::USTRING: {
    if (!aResultValue.mValue.mPointer) {
      // Allow returning a null pointer.
      *aValue = JSVAL_NULL;
    } else {
      JSString *jsstring = JS_NewUCStringCopyZ(aContext,
                             reinterpret_cast<const jschar*>(aResultValue.mValue.mPointer));
      if (!jsstring)
        return NS_ERROR_OUT_OF_MEMORY;

      *aValue = STRING_TO_JSVAL(jsstring);
    }
    break;
  }
  default:
    NS_NOTREACHED("invalid type");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

/*******************************************************************************
** Function
*******************************************************************************/

NS_IMPL_ISUPPORTS1(Function, nsIXPCScriptable)

Function::Function()
  : mFunc(nsnull)
{
}

Function::~Function()
{
}

nsresult
Function::Init(JSContext* aContext,
               Library* aLibrary,
               PRFuncPtr aFunc,
               PRUint16 aCallType,
               jsval aResultType,
               const nsTArray<jsval>& aArgTypes)
{
  nsresult rv;

  mLibrary = aLibrary;
  mFunc = aFunc;

  // determine the ABI
  rv = GetABI(aCallType, mCallType);
  if (NS_FAILED(rv)) {
    JS_ReportError(aContext, "Invalid ABI specification");
    return rv;
  }

  // prepare the result type
  rv = PrepareType(aContext, aResultType, mResultType);
  NS_ENSURE_SUCCESS(rv, rv);

  // prepare the argument types
  for (PRUint32 i = 0; i < aArgTypes.Length(); ++i) {
    rv = PrepareType(aContext, aArgTypes[i], *mArgTypes.AppendElement());
    NS_ENSURE_SUCCESS(rv, rv);

    // disallow void argument types
    if (mArgTypes[i].mType == nsIForeignLibrary::VOID) {
      JS_ReportError(aContext, "Cannot have void argument type");
      return NS_ERROR_INVALID_ARG;
    }

    // ffi_prep_cif requires an array of ffi_types; prepare it separately.
    mFFITypes.AppendElement(&mArgTypes[i].mFFIType);
  }

  ffi_status status = ffi_prep_cif(&mCIF, mCallType, mFFITypes.Length(),
                                   &mResultType.mFFIType, mFFITypes.Elements());
  switch (status) {
  case FFI_OK:
    return NS_OK;
  case FFI_BAD_ABI:
    JS_ReportError(aContext, "Invalid ABI specification");
    return NS_ERROR_INVALID_ARG;
  case FFI_BAD_TYPEDEF:
    JS_ReportError(aContext, "Invalid type specification");
    return NS_ERROR_INVALID_ARG;
  default:
    JS_ReportError(aContext, "Unknown libffi error");
    return NS_ERROR_FAILURE;
  }
}

PRBool
Function::Execute(JSContext* aContext, PRUint32 aArgc, jsval* aArgv, jsval* aValue)
{
  nsresult rv;

  // prepare the values for each argument
  nsAutoTArray<Value, 16> values;
  for (PRUint32 i = 0; i < mArgTypes.Length(); ++i) {
    rv = PrepareValue(aContext, mArgTypes[i], aArgv[i], *values.AppendElement());
    if (NS_FAILED(rv)) return PR_FALSE;
  }

  // create an array of pointers to each value, for passing to ffi_call
  nsAutoTArray<void*, 16> ffiValues;
  for (PRUint32 i = 0; i < mArgTypes.Length(); ++i) {
    ffiValues.AppendElement(values[i].mData);
  }

  // initialize a pointer to an appropriate location, for storing the result
  Value resultValue;
  PrepareReturnValue(mResultType, resultValue);

  // suspend the request before we call into the function, since the call
  // may block or otherwise take a long time to return.
  jsrefcount rc = JS_SuspendRequest(aContext);

  ffi_call(&mCIF, mFunc, resultValue.mData, ffiValues.Elements());

  JS_ResumeRequest(aContext, rc);

  // prepare a JS object from the result
  rv = ConvertReturnValue(aContext, mResultType, resultValue, aValue);
  if (NS_FAILED(rv)) return PR_FALSE;

  return PR_TRUE;
}

/*******************************************************************************
** nsIXPCScriptable implementation
*******************************************************************************/

#define XPC_MAP_CLASSNAME Function
#define XPC_MAP_QUOTED_CLASSNAME "Function"
#define XPC_MAP_WANT_CALL
#define XPC_MAP_FLAGS nsIXPCScriptable::WANT_CALL

#include "xpc_map_end.h"

NS_IMETHODIMP
Function::Call(nsIXPConnectWrappedNative* wrapper,
               JSContext* cx,
               JSObject* obj, 
               PRUint32 argc, 
               jsval* argv, 
               jsval* vp, 
               PRBool* _retval)
{
  if (!mLibrary->IsOpen()) {
    JS_ReportError(cx, "Library is not open");
    *_retval = PR_FALSE;
    return NS_ERROR_FAILURE;
  }

  if (argc != mArgTypes.Length()) {
    JS_ReportError(cx, "Number of arguments does not match declaration");
    *_retval = PR_FALSE;
    return NS_ERROR_FAILURE;
  }

  *_retval = Execute(cx, argc, argv, vp);
  if (!*_retval)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

}
}

