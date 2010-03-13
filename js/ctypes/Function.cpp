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

#include "jscntxt.h"
#include "Function.h"
#include "Library.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace ctypes {

/*******************************************************************************
** Static helpers
*******************************************************************************/

static bool
GetABI(JSContext* cx, jsval aCallType, ffi_abi& aResult)
{
  if (JSVAL_IS_PRIMITIVE(aCallType))
    return false;

  ABICode abi = GetABICode(cx, JSVAL_TO_OBJECT(aCallType));

  // determine the ABI from the subset of those available on the
  // given platform. ABI_DEFAULT specifies the default
  // C calling convention (cdecl) on each platform.
  switch (abi) {
  case ABI_DEFAULT:
    aResult = FFI_DEFAULT_ABI;
    return true;
  case ABI_STDCALL:
#if (defined(_WIN32) && !defined(_WIN64)) || defined(_OS2)
    aResult = FFI_STDCALL;
    return true;
#endif
  case INVALID_ABI:
    break;
  }
  return false;
}

static JSBool
PrepareType(JSContext* aContext, jsval aType, Type& aResult)
{
  if (JSVAL_IS_PRIMITIVE(aType) ||
      !CType::IsCType(aContext, JSVAL_TO_OBJECT(aType))) {
    JS_ReportError(aContext, "not a ctypes type");
    return false;
  }

  JSObject* typeObj = JSVAL_TO_OBJECT(aType);
  TypeCode typeCode = CType::GetTypeCode(aContext, typeObj);

  if (typeCode == TYPE_array) {
    // convert array argument types to pointers, just like C.
    // ImplicitConvert will do the same, when passing an array as data.
    JSObject* baseType = ArrayType::GetBaseType(aContext, typeObj);
    typeObj = PointerType::CreateInternal(aContext, NULL, baseType, NULL);
    if (!typeObj)
      return false;

  } else if (typeCode == TYPE_void_t) {
    // disallow void argument types
    JS_ReportError(aContext, "Cannot have void argument type");
    return false;
  }

  // libffi cannot pass types of zero size by value.
  JS_ASSERT(CType::GetSize(aContext, typeObj) != 0);

  aResult.mType = typeObj;
  aResult.mFFIType = *CType::GetFFIType(aContext, typeObj);
  return true;
}

static JSBool
PrepareResultType(JSContext* aContext, jsval aType, Type& aResult)
{
  if (JSVAL_IS_PRIMITIVE(aType) ||
      !CType::IsCType(aContext, JSVAL_TO_OBJECT(aType))) {
    JS_ReportError(aContext, "not a ctypes type");
    return false;
  }

  JSObject* typeObj = JSVAL_TO_OBJECT(aType);
  TypeCode typeCode = CType::GetTypeCode(aContext, typeObj);

  // Arrays can never be return types.
  if (typeCode == TYPE_array) {
    JS_ReportError(aContext, "Result type cannot be an array");
    return false;
  }

  // libffi cannot pass types of zero size by value.
  JS_ASSERT(typeCode == TYPE_void_t || CType::GetSize(aContext, typeObj) != 0);

  aResult.mType = typeObj;
  aResult.mFFIType = *CType::GetFFIType(aContext, typeObj);
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

JSBool
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
  if (!PrepareResultType(aContext, aResultType, mResultType))
    return false;

  // prepare the argument types
  mArgTypes.SetCapacity(aArgLength);
  for (PRUint32 i = 0; i < aArgLength; ++i) {
    if (!PrepareType(aContext, aArgTypes[i], *mArgTypes.AppendElement()))
      return false;

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

JSBool
Function::Execute(JSContext* cx, PRUint32 argc, jsval* vp)
{
  if (argc != mArgTypes.Length()) {
    JS_ReportError(cx, "Number of arguments does not match declaration");
    return false;
  }

  // prepare the values for each argument
  nsAutoTArray<AutoValue, 16> values;
  nsAutoTArray<AutoValue, 16> strings;
  for (PRUint32 i = 0; i < mArgTypes.Length(); ++i) {
    AutoValue& value = *values.AppendElement();
    jsval arg = JS_ARGV(cx, vp)[i];
    bool freePointer = false;
    if (!value.SizeToType(cx, mArgTypes[i].mType)) {
      JS_ReportAllocationOverflow(cx);
      return false;
    }

    if (!ImplicitConvert(cx, arg, mArgTypes[i].mType, value.mData, true, &freePointer))
      return false;

    if (freePointer) {
      // ImplicitConvert converted a string for us, which we have to free.
      // Keep track of it.
      strings.AppendElement()->mData = *static_cast<char**>(value.mData);
    }
  }

  // initialize a pointer to an appropriate location, for storing the result
  AutoValue resultValue;
  if (CType::GetTypeCode(cx, mResultType.mType) != TYPE_void_t &&
      !resultValue.SizeToType(cx, mResultType.mType)) {
    JS_ReportAllocationOverflow(cx);
    return false;
  }

  // suspend the request before we call into the function, since the call
  // may block or otherwise take a long time to return.
  jsrefcount rc = JS_SuspendRequest(cx);

  ffi_call(&mCIF, FFI_FN(mFunc), resultValue.mData, reinterpret_cast<void**>(values.Elements()));

  JS_ResumeRequest(cx, rc);

  // prepare a JS object from the result
  jsval rval;
  if (!ConvertToJS(cx, mResultType.mType, NULL, resultValue.mData, false, &rval))
    return false;

  JS_SET_RVAL(cx, vp, rval);
  return true;
}

void
Function::Trace(JSTracer *trc)
{
  // Identify the result CType to the tracer.
  JS_CALL_TRACER(trc, mResultType.mType, JSTRACE_OBJECT, "CType");

  // Identify each argument CType to the tracer.
  for (PRUint32 i = 0; i < mArgTypes.Length(); ++i)
    JS_CALL_TRACER(trc, mArgTypes[i].mType, JSTRACE_OBJECT, "CType");
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
  if (!self) {
    JS_ReportOutOfMemory(aContext);
    return NULL;
  }

  // deduce and check the ABI and parameter types
  if (!self->Init(aContext, aFunc, aCallType, aResultType, aArgTypes, aArgLength))
    return NULL;

  // create and root the new JS function object
  JSFunction* fn = JS_NewFunction(aContext, JSNative(Function::Call),
                     aArgLength, JSFUN_FAST_NATIVE, NULL, aName);
  if (!fn)
    return NULL;

  JSObject* fnObj = JS_GetFunctionObject(fn);
  js::AutoValueRooter fnRoot(aContext, fnObj);

  // stash a pointer to self, which Function::Call will need at call time
  if (!JS_SetReservedSlot(aContext, fnObj, SLOT_FUNCTION, PRIVATE_TO_JSVAL(self.get())))
    return NULL;

  // make a strong reference to the library for GC-safety
  if (!JS_SetReservedSlot(aContext, fnObj, SLOT_LIBRARYOBJ, OBJECT_TO_JSVAL(aLibrary)))
    return NULL;

  // Tell the library we exist, so it can (a) identify our CTypes to the tracer
  // and (b) delete our Function instance when it comes time to finalize.
  // (JS functions don't have finalizers.)
  if (!Library::AddFunction(aContext, aLibrary, self))
    return NULL;

  self.forget();
  return fnObj;
}

static Function*
GetFunction(JSContext* cx, JSObject* obj)
{
  jsval slot;
  JS_GetReservedSlot(cx, obj, SLOT_FUNCTION, &slot);
  return static_cast<Function*>(JSVAL_TO_PRIVATE(slot));
}

JSBool
Function::Call(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));

  jsval slot;
  JS_GetReservedSlot(cx, callee, SLOT_LIBRARYOBJ, &slot);

  PRLibrary* library = Library::GetLibrary(cx, JSVAL_TO_OBJECT(slot));
  if (!library) {
    JS_ReportError(cx, "library is not open");
    return JS_FALSE;
  }

  return GetFunction(cx, callee)->Execute(cx, argc, vp);
}

}
}

