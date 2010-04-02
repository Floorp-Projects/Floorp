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

#ifndef CTYPES_H
#define CTYPES_H

#include "jsapi.h"
#include "nsString.h"
#include "nsTArray.h"
#include "prlink.h"
#include "ffi.h"

namespace mozilla {
namespace ctypes {

/*******************************************************************************
** Function and struct API definitions
*******************************************************************************/

// for JS error reporting
enum ErrorNum {
#define MSG_DEF(name, number, count, exception, format) \
  name = number,
#include "ctypes.msg"
#undef MSG_DEF
  CTYPESERR_LIMIT
};

const JSErrorFormatString*
GetErrorMessage(void* userRef, const char* locale, const uintN errorNumber);
JSBool TypeError(JSContext* cx, const char* expected, jsval actual);

/**
 * ABI constants that specify the calling convention to use.
 * ctypes.default_abi corresponds to the cdecl convention, and in almost all
 * cases is the correct choice. ctypes.stdcall_abi is provided for calling
 * functions in the Microsoft Win32 API.
 */
enum ABICode {
  ABI_DEFAULT,
  ABI_STDCALL,
  INVALID_ABI
};

enum TypeCode {
  TYPE_void_t,
#define DEFINE_TYPE(name, type, ffiType) TYPE_##name,
#include "typedefs.h"
  TYPE_pointer,
  TYPE_function,
  TYPE_array,
  TYPE_struct
};

struct FieldInfo
{
  nsString  mName;
  JSObject* mType;
  size_t    mOffset;
};

// Just like JSPropertySpec, but with a Unicode name.
struct PropertySpec
{
  const jschar* name;
  size_t namelen;
  uint8 flags;
  JSPropertyOp getter;
  JSPropertyOp setter;
};

// Descriptor of ABI, return type, argument types, and variadicity for a
// FunctionType.
struct FunctionInfo
{
  // Initialized in NewFunctionInfo when !mIsVariadic, but only later, in
  // FunctionType::Call, when mIsVariadic. Not always consistent with
  // mFFITypes, due to lazy initialization when mIsVariadic.
  ffi_cif mCIF;

  // Calling convention of the function. Convert to ffi_abi using GetABI
  // and OBJECT_TO_JSVAL. Stored as a JSObject* for ease of tracing.
  JSObject* mABI;                

  // The CType of the value returned by the function.
  JSObject* mReturnType;

  // A fixed array of known parameter types, excluding any variadic
  // parameters (if mIsVariadic).
  nsTArray<JSObject*> mArgTypes; 

  // A variable array of ffi_type*s corresponding to both known parameter
  // types and dynamic (variadic) parameter types. Longer than mArgTypes
  // only if mIsVariadic.
  nsTArray<ffi_type*> mFFITypes;

  // Flag indicating whether the function behaves like a C function with
  // ... as the final formal parameter.
  bool mIsVariadic;
};

// Parameters necessary for invoking a JS function from a C closure.
struct ClosureInfo
{
  JSContext* cx;         // JSContext to use
  JSObject* closureObj;  // CClosure object
  JSObject* typeObj;     // FunctionType describing the C function
  JSObject* thisObj;     // 'this' object to use for the JS function call
  JSObject* jsfnObj;     // JS function
  ffi_closure* closure;  // The C closure itself
#ifdef DEBUG
  PRThread* thread;      // The thread the closure was created on
#endif
};

JSBool InitTypeClasses(JSContext* cx, JSObject* parent);

JSBool ConvertToJS(JSContext* cx, JSObject* typeObj, JSObject* dataObj,
  void* data, bool wantPrimitive, bool ownResult, jsval* result);

JSBool ImplicitConvert(JSContext* cx, jsval val, JSObject* targetType,
  void* buffer, bool isArgument, bool* freePointer);

JSBool ExplicitConvert(JSContext* cx, jsval val, JSObject* targetType,
  void* buffer);

/*******************************************************************************
** JSClass reserved slot definitions
*******************************************************************************/

enum CABISlot {
  SLOT_ABICODE = 0, // ABICode of the CABI object
  CABI_SLOTS
};

enum CTypeProtoSlot {
  SLOT_POINTERPROTO      = 0,  // ctypes.PointerType.prototype object
  SLOT_ARRAYPROTO        = 1,  // ctypes.ArrayType.prototype object
  SLOT_STRUCTPROTO       = 2,  // ctypes.StructType.prototype object
  SLOT_FUNCTIONPROTO     = 3,  // ctypes.FunctionType.prototype object
  SLOT_CDATAPROTO        = 4,  // ctypes.CData.prototype object
  SLOT_POINTERDATAPROTO  = 5,  // common ancestor of all CData objects of PointerType
  SLOT_ARRAYDATAPROTO    = 6,  // common ancestor of all CData objects of ArrayType
  SLOT_STRUCTDATAPROTO   = 7,  // common ancestor of all CData objects of StructType
  SLOT_FUNCTIONDATAPROTO = 8,  // common ancestor of all CData objects of FunctionType
  SLOT_INT64PROTO        = 9,  // ctypes.Int64.prototype object
  SLOT_UINT64PROTO       = 10, // ctypes.UInt64.prototype object
  SLOT_CLOSURECX         = 11, // JSContext for use with FunctionType closures
  CTYPEPROTO_SLOTS
};

enum CTypeSlot {
  SLOT_PROTO     = 0, // 'prototype' property of the CType object
  SLOT_TYPECODE  = 1, // TypeCode of the CType object
  SLOT_FFITYPE   = 2, // ffi_type representing the type
  SLOT_NAME      = 3, // name of the type
  SLOT_SIZE      = 4, // size of the type, in bytes
  SLOT_ALIGN     = 5, // alignment of the type, in bytes
  SLOT_PTR       = 6, // cached PointerType object for type.ptr
  // Note that some of the slots below can overlap, since they're for
  // mutually exclusive types.
  SLOT_TARGET_T  = 7, // (PointerTypes only) 'targetType' property
  SLOT_ELEMENT_T = 7, // (ArrayTypes only) 'elementType' property
  SLOT_LENGTH    = 8, // (ArrayTypes only) 'length' property
  SLOT_FIELDS    = 7, // (StructTypes only) 'fields' property
  SLOT_FIELDINFO = 8, // (StructTypes only) FieldInfo array
  SLOT_FNINFO    = 7, // (FunctionTypes only) FunctionInfo struct
  SLOT_ARGS_T    = 8, // (FunctionTypes only) 'argTypes' property (cached)
  CTYPE_SLOTS
};

enum CDataSlot {
  SLOT_CTYPE    = 0, // CType object representing the underlying type
  SLOT_REFERENT = 1, // JSObject this object must keep alive, if any
  SLOT_DATA     = 2, // pointer to a buffer containing the binary data
  SLOT_OWNS     = 3, // JSVAL_TRUE if this CData owns its own buffer
  CDATA_SLOTS
};

enum CClosureSlot {
  SLOT_CLOSUREINFO = 0, // ClosureInfo struct
  CCLOSURE_SLOTS
};

enum TypeCtorSlot {
  SLOT_FN_CTORPROTO = 0 // ctypes.{Pointer,Array,Struct}Type.prototype
  // JSFunction objects always get exactly two slots.
};

enum Int64Slot {
  SLOT_INT64 = 0, // pointer to a 64-bit buffer containing the integer
  INT64_SLOTS
};

enum Int64FunctionSlot {
  SLOT_FN_INT64PROTO = 0 // ctypes.{Int64,UInt64}.prototype object
  // JSFunction objects always get exactly two slots.
};

/*******************************************************************************
** Object API definitions
*******************************************************************************/

namespace CType {
  JSObject* Create(JSContext* cx, JSObject* typeProto, JSObject* dataProto,
    TypeCode type, JSString* name, jsval size, jsval align, ffi_type* ffiType, 
    PropertySpec* ps);

  JSObject* DefineBuiltin(JSContext* cx, JSObject* parent, const char* propName,
    JSObject* typeProto, JSObject* dataProto, const char* name, TypeCode type,
    jsval size, jsval align, ffi_type* ffiType);

  bool IsCType(JSContext* cx, JSObject* obj);
  TypeCode GetTypeCode(JSContext* cx, JSObject* typeObj);
  bool TypesEqual(JSContext* cx, JSObject* t1, JSObject* t2);
  size_t GetSize(JSContext* cx, JSObject* obj);
  bool GetSafeSize(JSContext* cx, JSObject* obj, size_t* result);
  bool IsSizeDefined(JSContext* cx, JSObject* obj);
  size_t GetAlignment(JSContext* cx, JSObject* obj);
  ffi_type* GetFFIType(JSContext* cx, JSObject* obj);
  JSString* GetName(JSContext* cx, JSObject* obj);
  JSObject* GetProtoFromCtor(JSContext* cx, JSObject* obj, CTypeProtoSlot slot);
  JSObject* GetProtoFromType(JSContext* cx, JSObject* obj, CTypeProtoSlot slot);
}

namespace PointerType {
  JSObject* CreateInternal(JSContext* cx, JSObject* ctor, JSObject* baseType,
    JSString* name);

  JSObject* GetBaseType(JSContext* cx, JSObject* obj);
}

namespace ArrayType {
  JSObject* CreateInternal(JSContext* cx, JSObject* baseType, size_t length,
    bool lengthDefined);

  JSObject* GetBaseType(JSContext* cx, JSObject* obj);
  size_t GetLength(JSContext* cx, JSObject* obj);
  bool GetSafeLength(JSContext* cx, JSObject* obj, size_t* result);
}

namespace StructType {
  nsTArray<FieldInfo>* GetFieldInfo(JSContext* cx, JSObject* obj);
  FieldInfo* LookupField(JSContext* cx, JSObject* obj, jsval idval);
}

namespace FunctionType {
  JSObject* CreateInternal(JSContext* cx, jsval abi, jsval rtype,
    jsval* argtypes, jsuint arglen);

  JSObject* ConstructWithObject(JSContext* cx, JSObject* typeObj,
    JSObject* refObj, PRFuncPtr fnptr, JSObject* result);

  FunctionInfo* GetFunctionInfo(JSContext* cx, JSObject* obj);
  JSObject* GetLibrary(JSContext* cx, JSObject* obj);
}

namespace CClosure {
  JSObject* Create(JSContext* cx, JSObject* typeObj, JSObject* fnObj,
    JSObject* thisObj, PRFuncPtr* fnptr);
}

namespace CData {
  JSObject* Create(JSContext* cx, JSObject* typeObj, JSObject* refObj,
    void* data, bool ownResult);

  JSObject* GetCType(JSContext* cx, JSObject* dataObj);
  void* GetData(JSContext* cx, JSObject* dataObj);
  bool IsCData(JSContext* cx, JSObject* obj);

  // Attached by JSAPI as the function 'ctypes.cast'
  JSBool Cast(JSContext* cx, uintN argc, jsval* vp);
}

namespace Int64 {
  bool IsInt64(JSContext* cx, JSObject* obj);
}

namespace UInt64 {
  bool IsUInt64(JSContext* cx, JSObject* obj);
}

}
}

#endif
