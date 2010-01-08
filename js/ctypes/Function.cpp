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
#include "Library.h"
#include "nsAutoPtr.h"
#include "jscntxt.h"

namespace mozilla {
namespace ctypes {

/*******************************************************************************
** Static helpers
*******************************************************************************/

template<class IntegerType>
static IntegerType
Convert(jsdouble d)
{
  return IntegerType(d);
}

#ifdef _MSC_VER
// MSVC can't perform double to unsigned __int64 conversion when the
// double is greater than 2^63 - 1. Help it along a little.
template<>
static PRUint64
Convert<PRUint64>(jsdouble d)
{
  return d > 0x7fffffffffffffffui64 ?
         PRUint64(d - 0x8000000000000000ui64) + 0x8000000000000000ui64 :
         PRUint64(d);
}
#endif

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
    *aResult = Convert<IntegerType>(d);

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

static bool
TypeError(JSContext* cx, const char* expected, jsval actual)
{
  const char* src = ToSource(cx, actual);
  JS_ReportErrorNumber(cx, GetErrorMessage, NULL,
                       CTYPESMSG_TYPE_ERROR, expected, src);
  return false;
}

static bool
GetABI(JSContext* cx, jsval aCallType, ffi_abi& aResult)
{
  ABICode abi = Module::GetABICode(cx, aCallType);

  // determine the ABI from the subset of those available on the
  // given platform. TYPE_DEFAULT specifies the default
  // C calling convention (cdecl) on each platform.
  switch (abi) {
  case ABI_default_abi:
    aResult = FFI_DEFAULT_ABI;
    return true;
#if (defined(_WIN32) && !defined(_WIN64)) || defined(_OS2)
  case ABI_stdcall_abi:
    aResult = FFI_STDCALL;
    return true;
#endif
  default:
    return false;
  }
}

static bool
PrepareType(JSContext* aContext, jsval aType, Type& aResult)
{
  aResult.mType = Module::GetTypeCode(aContext, aType);

  switch (aResult.mType) {
  case TYPE_void_t:
    aResult.mFFIType = ffi_type_void;
    break;
  case TYPE_int8_t:
    aResult.mFFIType = ffi_type_sint8;
    break;
  case TYPE_int16_t:
    aResult.mFFIType = ffi_type_sint16;
    break;
  case TYPE_int32_t:
    aResult.mFFIType = ffi_type_sint32;
    break;
  case TYPE_int64_t:
    aResult.mFFIType = ffi_type_sint64;
    break;
  case TYPE_bool:
  case TYPE_uint8_t:
    aResult.mFFIType = ffi_type_uint8;
    break;
  case TYPE_uint16_t:
    aResult.mFFIType = ffi_type_uint16;
    break;
  case TYPE_uint32_t:
    aResult.mFFIType = ffi_type_uint32;
    break;
  case TYPE_uint64_t:
    aResult.mFFIType = ffi_type_uint64;
    break;
  case TYPE_float:
    aResult.mFFIType = ffi_type_float;
    break;
  case TYPE_double:
    aResult.mFFIType = ffi_type_double;
    break;
  case TYPE_string:
  case TYPE_ustring:
    aResult.mFFIType = ffi_type_pointer;
    break;
  default:
    JS_ReportError(aContext, "Invalid type specification");
    return false;
  }

  return true;
}

static bool
PrepareValue(JSContext* aContext, const Type& aType, jsval aValue, Value& aResult)
{
  jsdouble d;

  switch (aType.mType) {
  case TYPE_bool:
    // Do not implicitly lose bits, but allow the values 0, 1, and -0.
    // Programs can convert explicitly, if needed, using `Boolean(v)` or `!!v`.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mUint8) ||
        aResult.mValue.mUint8 > 1)
      return TypeError(aContext, "boolean", aValue);

    aResult.mData = &aResult.mValue.mUint8;
    break;
  case TYPE_int8_t:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mInt8))
      return TypeError(aContext, "int8", aValue);

    aResult.mData = &aResult.mValue.mInt8;
    break;
  case TYPE_int16_t:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mInt16))
      return TypeError(aContext, "int16", aValue);

    aResult.mData = &aResult.mValue.mInt16;
    break;
  case TYPE_int32_t:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mInt32))
      return TypeError(aContext, "int32", aValue);

    aResult.mData = &aResult.mValue.mInt32;
    break;
  case TYPE_int64_t:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mInt64))
      return TypeError(aContext, "int64", aValue);

    aResult.mData = &aResult.mValue.mInt64;
    break;
  case TYPE_uint8_t:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mUint8))
      return TypeError(aContext, "uint8", aValue);

    aResult.mData = &aResult.mValue.mUint8;
    break;
  case TYPE_uint16_t:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mUint16))
      return TypeError(aContext, "uint16", aValue);

    aResult.mData = &aResult.mValue.mUint16;
    break;
  case TYPE_uint32_t:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mUint32))
      return TypeError(aContext, "uint32", aValue);

    aResult.mData = &aResult.mValue.mUint32;
    break;
  case TYPE_uint64_t:
    // Do not implicitly lose bits.
    if (!jsvalToIntStrict(aValue, &aResult.mValue.mUint64))
      return TypeError(aContext, "uint64", aValue);

    aResult.mData = &aResult.mValue.mUint64;
    break;
  case TYPE_float:
    if (!jsvalToDoubleStrict(aValue, &d))
      return TypeError(aContext, "float", aValue);

    // The following cast silently throws away some bits, but there's
    // no good way around it. Sternly requiring that the 64-bit double
    // argument be exactly representable as a 32-bit float is
    // unrealistic: it would allow 1/2 to pass but not 1/3.
    aResult.mValue.mFloat = float(d);
    aResult.mData = &aResult.mValue.mFloat;
    break;
  case TYPE_double:
    if (!jsvalToDoubleStrict(aValue, &d))
      return TypeError(aContext, "double", aValue);

    aResult.mValue.mDouble = d;
    aResult.mData = &aResult.mValue.mDouble;
    break;
  case TYPE_string:
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
  case TYPE_ustring:
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
    return false;
  }

  return true;
}

static void
PrepareReturnValue(const Type& aType, Value& aResult)
{
  switch (aType.mType) {
  case TYPE_void_t:
    aResult.mData = nsnull;
    break;
  case TYPE_int8_t:
    aResult.mData = &aResult.mValue.mInt8;
    break;
  case TYPE_int16_t:
    aResult.mData = &aResult.mValue.mInt16;
    break;
  case TYPE_int32_t:
    aResult.mData = &aResult.mValue.mInt32;
    break;
  case TYPE_int64_t:
    aResult.mData = &aResult.mValue.mInt64;
    break;
  case TYPE_bool:
  case TYPE_uint8_t:
    aResult.mData = &aResult.mValue.mUint8;
    break;
  case TYPE_uint16_t:
    aResult.mData = &aResult.mValue.mUint16;
    break;
  case TYPE_uint32_t:
    aResult.mData = &aResult.mValue.mUint32;
    break;
  case TYPE_uint64_t:
    aResult.mData = &aResult.mValue.mUint64;
    break;
  case TYPE_float:
    aResult.mData = &aResult.mValue.mFloat;
    break;
  case TYPE_double:
    aResult.mData = &aResult.mValue.mDouble;
    break;
  case TYPE_string:
  case TYPE_ustring:
    aResult.mData = &aResult.mValue.mPointer;
    break;
  default:
    NS_NOTREACHED("invalid type");
    break;
  }
}

static bool
ConvertReturnValue(JSContext* aContext,
                   const Type& aResultType,
                   const Value& aResultValue,
                   jsval* aValue)
{
  switch (aResultType.mType) {
  case TYPE_void_t:
    *aValue = JSVAL_VOID;
    break;
  case TYPE_bool:
    *aValue = aResultValue.mValue.mUint8 ? JSVAL_TRUE : JSVAL_FALSE;
    break;
  case TYPE_int8_t:
    *aValue = INT_TO_JSVAL(aResultValue.mValue.mInt8);
    break;
  case TYPE_int16_t:
    *aValue = INT_TO_JSVAL(aResultValue.mValue.mInt16);
    break;
  case TYPE_int32_t:
    if (!JS_NewNumberValue(aContext, jsdouble(aResultValue.mValue.mInt32), aValue))
      return false;
    break;
  case TYPE_int64_t:
    // Implicit conversion with loss of bits.  :-[
    if (!JS_NewNumberValue(aContext, jsdouble(aResultValue.mValue.mInt64), aValue))
      return false;
    break;
  case TYPE_uint8_t:
    *aValue = INT_TO_JSVAL(aResultValue.mValue.mUint8);
    break;
  case TYPE_uint16_t:
    *aValue = INT_TO_JSVAL(aResultValue.mValue.mUint16);
    break;
  case TYPE_uint32_t:
    if (!JS_NewNumberValue(aContext, jsdouble(aResultValue.mValue.mUint32), aValue))
      return false;
    break;
  case TYPE_uint64_t:
    // Implicit conversion with loss of bits.  :-[
    if (!JS_NewNumberValue(aContext, jsdouble(aResultValue.mValue.mUint64), aValue))
      return false;
    break;
  case TYPE_float:
    if (!JS_NewNumberValue(aContext, jsdouble(aResultValue.mValue.mFloat), aValue))
      return false;
    break;
  case TYPE_double:
    if (!JS_NewNumberValue(aContext, jsdouble(aResultValue.mValue.mDouble), aValue))
      return false;
    break;
  case TYPE_string: {
    if (!aResultValue.mValue.mPointer) {
      // Allow returning a null pointer.
      *aValue = JSVAL_NULL;
    } else {
      JSString *jsstring = JS_NewStringCopyZ(aContext,
                             reinterpret_cast<const char*>(aResultValue.mValue.mPointer));
      if (!jsstring)
        return false;

      *aValue = STRING_TO_JSVAL(jsstring);
    }
    break;
  }
  case TYPE_ustring: {
    if (!aResultValue.mValue.mPointer) {
      // Allow returning a null pointer.
      *aValue = JSVAL_NULL;
    } else {
      JSString *jsstring = JS_NewUCStringCopyZ(aContext,
                             reinterpret_cast<const jschar*>(aResultValue.mValue.mPointer));
      if (!jsstring)
        return false;

      *aValue = STRING_TO_JSVAL(jsstring);
    }
    break;
  }
  default:
    NS_NOTREACHED("invalid type");
    return false;
  }

  return true;
}

/*******************************************************************************
** Function implementation
*******************************************************************************/

Function::Function()
  : mNext(NULL)
{
}

Function::~Function()
{
}

bool
Function::Init(JSContext* aContext,
               PRFuncPtr aFunc,
               jsval aCallType,
               jsval aResultType,
               jsval* aArgTypes,
               uintN aArgLength)
{
  mFunc = aFunc;

  // determine the ABI
  if (!GetABI(aContext, aCallType, mCallType)) {
    JS_ReportError(aContext, "Invalid ABI specification");
    return false;
  }

  // prepare the result type
  if (!PrepareType(aContext, aResultType, mResultType))
    return false;

  // prepare the argument types
  mArgTypes.SetCapacity(aArgLength);
  for (PRUint32 i = 0; i < aArgLength; ++i) {
    if (!PrepareType(aContext, aArgTypes[i], *mArgTypes.AppendElement()))
      return false;

    // disallow void argument types
    if (mArgTypes[i].mType == TYPE_void_t) {
      JS_ReportError(aContext, "Cannot have void argument type");
      return false;
    }

    // ffi_prep_cif requires an array of ffi_types; prepare it separately.
    mFFITypes.AppendElement(&mArgTypes[i].mFFIType);
  }

  ffi_status status = ffi_prep_cif(&mCIF, mCallType, mFFITypes.Length(),
                                   &mResultType.mFFIType, mFFITypes.Elements());
  switch (status) {
  case FFI_OK:
    return true;
  case FFI_BAD_ABI:
    JS_ReportError(aContext, "Invalid ABI specification");
    return false;
  case FFI_BAD_TYPEDEF:
    JS_ReportError(aContext, "Invalid type specification");
    return false;
  default:
    JS_ReportError(aContext, "Unknown libffi error");
    return false;
  }
}

bool
Function::Execute(JSContext* cx, PRUint32 argc, jsval* vp)
{
  if (argc != mArgTypes.Length()) {
    JS_ReportError(cx, "Number of arguments does not match declaration");
    return false;
  }

  // prepare the values for each argument
  nsAutoTArray<Value, 16> values;
  for (PRUint32 i = 0; i < mArgTypes.Length(); ++i) {
    if (!PrepareValue(cx, mArgTypes[i], JS_ARGV(cx, vp)[i], *values.AppendElement()))
      return false;
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
  jsrefcount rc = JS_SuspendRequest(cx);

  ffi_call(&mCIF, FFI_FN(mFunc), resultValue.mData, ffiValues.Elements());

  JS_ResumeRequest(cx, rc);

  // prepare a JS object from the result
  jsval rval;
  if (!ConvertReturnValue(cx, mResultType, resultValue, &rval))
    return false;

  JS_SET_RVAL(cx, vp, rval);
  return true;
}

/*******************************************************************************
** JSObject implementation
*******************************************************************************/

JSObject*
Function::Create(JSContext* aContext,
                 JSObject* aLibrary,
                 PRFuncPtr aFunc,
                 const char* aName,
                 jsval aCallType,
                 jsval aResultType,
                 jsval* aArgTypes,
                 uintN aArgLength)
{
  // create new Function instance
  nsAutoPtr<Function> self(new Function());
  if (!self)
    return NULL;

  // deduce and check the ABI and parameter types
  if (!self->Init(aContext, aFunc, aCallType, aResultType, aArgTypes, aArgLength))
    return NULL;

  // create and root the new JS function object
  JSFunction* fn = JS_NewFunction(aContext, JSNative(Function::Call),
                     aArgLength, JSFUN_FAST_NATIVE, NULL, aName);
  if (!fn)
    return NULL;

  JSObject* fnObj = JS_GetFunctionObject(fn);
  JSAutoTempValueRooter fnRoot(aContext, fnObj);

  // stash a pointer to self, which Function::Call will need at call time
  if (!JS_SetReservedSlot(aContext, fnObj, 0, PRIVATE_TO_JSVAL(self.get())))
    return NULL;

  // make a strong reference to the library for GC-safety
  if (!JS_SetReservedSlot(aContext, fnObj, 1, OBJECT_TO_JSVAL(aLibrary)))
    return NULL;

  // tell the library we exist, so it can delete our Function instance
  // when it comes time to finalize. (JS functions don't have finalizers.)
  if (!Library::AddFunction(aContext, aLibrary, self))
    return NULL;

  self.forget();
  return fnObj;
}

static Function*
GetFunction(JSContext* cx, JSObject* obj)
{
  jsval slot;
  JS_GetReservedSlot(cx, obj, 0, &slot);
  return static_cast<Function*>(JSVAL_TO_PRIVATE(slot));
}

JSBool
Function::Call(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));

  jsval slot;
  JS_GetReservedSlot(cx, callee, 1, &slot);

  PRLibrary* library = Library::GetLibrary(cx, JSVAL_TO_OBJECT(slot));
  if (!library) {
    JS_ReportError(cx, "library is not open");
    return JS_FALSE;
  }

  return GetFunction(cx, callee)->Execute(cx, argc, vp);
}

}
}

