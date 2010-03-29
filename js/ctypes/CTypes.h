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

// Descriptor of ABI, return type, and argument types for a FunctionType.
struct FunctionInfo
{
  ffi_cif mCIF;
  JSObject* mABI;
  JSObject* mReturnType;
  nsTArray<JSObject*> mArgTypes;
  nsTArray<ffi_type*> mFFITypes;
};

JSBool InitTypeClasses(JSContext* cx, JSObject* parent);

JSBool ConvertToJS(JSContext* cx, JSObject* typeObj, JSObject* dataObj, void* data, bool wantPrimitive, bool ownResult, jsval* result);
JSBool ImplicitConvert(JSContext* cx, jsval val, JSObject* targetType, void* buffer, bool isArgument, bool* freePointer);
JSBool ExplicitConvert(JSContext* cx, jsval val, JSObject* targetType, void* buffer);

// Contents of the various slots on each JSClass. The slot indexes are given
// enumerated names for readability.
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

class CType {
public:
  static JSObject* Create(JSContext* cx, JSObject* typeProto, JSObject* dataProto, TypeCode type, JSString* name, jsval size, jsval align, ffi_type* ffiType, PropertySpec* ps);
  static JSObject* DefineBuiltin(JSContext* cx, JSObject* parent, const char* propName, JSObject* typeProto, JSObject* dataProto, const char* name, TypeCode type, jsval size, jsval align, ffi_type* ffiType);
  static void Trace(JSTracer* trc, JSObject* obj);
  static void Finalize(JSContext* cx, JSObject* obj);

  static JSBool ConstructAbstract(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
  static JSBool ConstructData(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
  static JSBool ConstructBasic(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);

  static bool IsCType(JSContext* cx, JSObject* obj);
  static TypeCode GetTypeCode(JSContext* cx, JSObject* typeObj);
  static bool TypesEqual(JSContext* cx, JSObject* t1, JSObject* t2);
  static size_t GetSize(JSContext* cx, JSObject* obj);
  static bool GetSafeSize(JSContext* cx, JSObject* obj, size_t* result);
  static bool IsSizeDefined(JSContext* cx, JSObject* obj);
  static size_t GetAlignment(JSContext* cx, JSObject* obj);
  static ffi_type* GetFFIType(JSContext* cx, JSObject* obj);
  static JSString* GetName(JSContext* cx, JSObject* obj);
  static JSObject* GetProtoFromCtor(JSContext* cx, JSObject* obj, CTypeProtoSlot slot);
  static JSObject* GetProtoFromType(JSContext* cx, JSObject* obj, CTypeProtoSlot slot);

  static JSBool PrototypeGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool NameGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool SizeGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool PtrGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool Array(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ToString(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ToSource(JSContext* cx, uintN argc, jsval* vp);
  static JSBool HasInstance(JSContext* cx, JSObject* obj, jsval v, JSBool* bp);
};

class PointerType {
public:
  static JSBool Create(JSContext* cx, uintN argc, jsval* vp);
  static JSObject* CreateInternal(JSContext* cx, JSObject* ctor, JSObject* baseType, JSString* name);

  static JSBool ConstructData(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);

  static JSObject* GetBaseType(JSContext* cx, JSObject* obj);

  static JSBool TargetTypeGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool ContentsGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool ContentsSetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
};

class ArrayType {
public:
  static JSBool Create(JSContext* cx, uintN argc, jsval* vp);
  static JSObject* CreateInternal(JSContext* cx, JSObject* baseType, size_t length, bool lengthDefined);

  static JSBool ConstructData(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);

  static JSObject* GetBaseType(JSContext* cx, JSObject* obj);
  static size_t GetLength(JSContext* cx, JSObject* obj);
  static bool GetSafeLength(JSContext* cx, JSObject* obj, size_t* result);

  static JSBool ElementTypeGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool LengthGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool Getter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool Setter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool AddressOfElement(JSContext* cx, uintN argc, jsval* vp);
};

class StructType {
public:
  static JSBool Create(JSContext* cx, uintN argc, jsval* vp);

  static JSBool ConstructData(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);

  static nsTArray<FieldInfo>* GetFieldInfo(JSContext* cx, JSObject* obj);
  static FieldInfo* LookupField(JSContext* cx, JSObject* obj, jsval idval);

  static JSBool FieldsArrayGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool FieldGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool FieldSetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool AddressOfField(JSContext* cx, uintN argc, jsval* vp);
};

// Helper class for handling allocation of function arguments.
struct AutoValue
{
  AutoValue() : mData(NULL) { }

  ~AutoValue()
  {
    delete[] static_cast<char*>(mData);
  }

  bool SizeToType(JSContext* cx, JSObject* type)
  {
    size_t size = CType::GetSize(cx, type);
    mData = new char[size];
    if (mData)
      memset(mData, 0, size);
    return mData != NULL;
  }

  void* mData;
};

class FunctionType {
public:
  static JSBool Create(JSContext* cx, uintN argc, jsval* vp);
  static JSObject* CreateInternal(JSContext* cx, jsval abi, jsval rtype, jsval* argtypes, jsuint arglen);

  static JSBool ConstructData(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);
  static JSObject* ConstructWithLibrary(JSContext* cx, JSObject* typeObj, JSObject* libraryObj, PRFuncPtr fnptr);

  static JSBool Call(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);

  static FunctionInfo* GetFunctionInfo(JSContext* cx, JSObject* obj);
  static JSObject* GetLibrary(JSContext* cx, JSObject* obj);
  static JSBool ArgTypesGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool ReturnTypeGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool ABIGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
};

class CData {
public:
  static JSObject* Create(JSContext* cx, JSObject* typeObj, JSObject* refObj, void* data, bool ownResult);
  static void Finalize(JSContext* cx, JSObject* obj);

  static JSObject* GetCType(JSContext* cx, JSObject* dataObj);
  static void* GetData(JSContext* cx, JSObject* dataObj);
  static bool IsCData(JSContext* cx, JSObject* obj);

  static JSBool ValueGetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool ValueSetter(JSContext* cx, JSObject* obj, jsval idval, jsval* vp);
  static JSBool Address(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Cast(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ReadString(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ToSource(JSContext* cx, uintN argc, jsval* vp);
};

class Int64Base {
public:
  static JSObject* Construct(JSContext* cx, JSObject* proto, PRUint64 data, bool isUnsigned);
  static void Finalize(JSContext* cx, JSObject* obj);

  static PRUint64 GetInt(JSContext* cx, JSObject* obj);

  static JSBool ToString(JSContext* cx, JSObject* obj, uintN argc, jsval* vp, bool isUnsigned);
  static JSBool ToSource(JSContext* cx, JSObject* obj, uintN argc, jsval* vp, bool isUnsigned);
};

class Int64 : public Int64Base {
public:
  static JSBool Construct(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);

  static bool IsInt64(JSContext* cx, JSObject* obj);

  static JSBool ToString(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ToSource(JSContext* cx, uintN argc, jsval* vp);

  // ctypes.Int64 static functions
  static JSBool Compare(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Lo(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Hi(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Join(JSContext* cx, uintN argc, jsval* vp);
};

class UInt64 : public Int64Base {
public:
  static JSBool Construct(JSContext* cx, JSObject* obj, uintN argc, jsval* argv, jsval* rval);

  static bool IsUInt64(JSContext* cx, JSObject* obj);

  static JSBool ToString(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ToSource(JSContext* cx, uintN argc, jsval* vp);

  // ctypes.UInt64 static functions
  static JSBool Compare(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Lo(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Hi(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Join(JSContext* cx, uintN argc, jsval* vp);
};

}
}

#endif
