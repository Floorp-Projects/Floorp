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

#include "jscntxt.h"
#include "jsapi.h"
#include "prlink.h"
#include "ffi.h"

#include "js/HashTable.h"

namespace js {
namespace ctypes {

/*******************************************************************************
** Utility classes
*******************************************************************************/

template<class T>
class OperatorDelete
{
public:
  static void destroy(T* ptr) { UnwantedForeground::delete_(ptr); }
};

template<class T>
class OperatorArrayDelete
{
public:
  static void destroy(T* ptr) { UnwantedForeground::array_delete(ptr); }
};

// Class that takes ownership of a pointer T*, and calls cx->delete_() or
// cx->array_delete() upon destruction.
template<class T, class DeleteTraits = OperatorDelete<T> >
class AutoPtr {
private:
  typedef AutoPtr<T, DeleteTraits> self_type;

public:
  // An AutoPtr variant that calls js_array_delete() instead.
  typedef AutoPtr<T, OperatorArrayDelete<T> > Array;

  AutoPtr() : mPtr(NULL) { }
  explicit AutoPtr(T* ptr) : mPtr(ptr) { }
  ~AutoPtr() { DeleteTraits::destroy(mPtr); }

  T*   operator->()         { return mPtr; }
  bool operator!()          { return mPtr == NULL; }
  T&   operator[](size_t i) { return *(mPtr + i); }
  // Note: we cannot safely provide an 'operator T*()', since this would allow
  // the compiler to perform implicit conversion from one AutoPtr to another
  // via the constructor AutoPtr(T*).

  T*   get()         { return mPtr; }
  void set(T* other) { JS_ASSERT(mPtr == NULL); mPtr = other; }
  T*   forget()      { T* result = mPtr; mPtr = NULL; return result; }

  self_type& operator=(T* rhs) { mPtr = rhs; return *this; }

private:
  // Do not allow copy construction or assignment from another AutoPtr.
  template<class U> AutoPtr(AutoPtr<T, U>&);
  template<class U> self_type& operator=(AutoPtr<T, U>& rhs);

  T* mPtr;
};

// Container class for Vector, using SystemAllocPolicy.
template<class T, size_t N = 0>
class Array : public Vector<T, N, SystemAllocPolicy>
{
};

// String and AutoString classes, based on Vector.
typedef Vector<jschar,  0, SystemAllocPolicy> String;
typedef Vector<jschar, 64, SystemAllocPolicy> AutoString;
typedef Vector<char,    0, SystemAllocPolicy> CString;
typedef Vector<char,   64, SystemAllocPolicy> AutoCString;

// Convenience functions to append, insert, and compare Strings.
template <class T, size_t N, class AP, size_t ArrayLength>
void
AppendString(Vector<T, N, AP> &v, const char (&array)[ArrayLength])
{
  // Don't include the trailing '\0'.
  size_t alen = ArrayLength - 1;
  size_t vlen = v.length();
  if (!v.resize(vlen + alen))
    return;

  for (size_t i = 0; i < alen; ++i)
    v[i + vlen] = array[i];
}

template <class T, size_t N, size_t M, class AP>
void
AppendString(Vector<T, N, AP> &v, Vector<T, M, AP> &w)
{
  v.append(w.begin(), w.length());
}

template <size_t N, class AP>
void
AppendString(Vector<jschar, N, AP> &v, JSString* str)
{
  JS_ASSERT(str);
  const jschar *chars = str->getChars(NULL);
  if (!chars)
    return;
  v.append(chars, str->length());
}

template <size_t N, class AP>
void
AppendString(Vector<char, N, AP> &v, JSString* str)
{
  JS_ASSERT(str);
  size_t vlen = v.length();
  size_t alen = str->length();
  if (!v.resize(vlen + alen))
    return;

  const jschar *chars = str->getChars(NULL);
  if (!chars)
    return;

  for (size_t i = 0; i < alen; ++i)
    v[i + vlen] = char(chars[i]);
}

template <class T, size_t N, class AP, size_t ArrayLength>
void
PrependString(Vector<T, N, AP> &v, const char (&array)[ArrayLength])
{
  // Don't include the trailing '\0'.
  size_t alen = ArrayLength - 1;
  size_t vlen = v.length();
  if (!v.resize(vlen + alen))
    return;

  // Move vector data forward. This is safe since we've already resized.
  memmove(v.begin() + alen, v.begin(), vlen * sizeof(T));

  // Copy data to insert.
  for (size_t i = 0; i < alen; ++i)
    v[i] = array[i];
}

template <size_t N, class AP>
void
PrependString(Vector<jschar, N, AP> &v, JSString* str)
{
  JS_ASSERT(str);
  size_t vlen = v.length();
  size_t alen = str->length();
  if (!v.resize(vlen + alen))
    return;

  const jschar *chars = str->getChars(NULL);
  if (!chars)
    return;

  // Move vector data forward. This is safe since we've already resized.
  memmove(v.begin() + alen, v.begin(), vlen * sizeof(jschar));

  // Copy data to insert.
  memcpy(v.begin(), chars, alen * sizeof(jschar));
}

/*******************************************************************************
** Function and struct API definitions
*******************************************************************************/

JS_ALWAYS_INLINE void
ASSERT_OK(JSBool ok)
{
  JS_ASSERT(ok);
}

// for JS error reporting
enum ErrorNum {
#define MSG_DEF(name, number, count, exception, format) \
  name = number,
#include "ctypes.msg"
#undef MSG_DEF
  CTYPESERR_LIMIT
};

const JSErrorFormatString*
GetErrorMessage(void* userRef, const char* locale, const unsigned errorNumber);
JSBool TypeError(JSContext* cx, const char* expected, jsval actual);

/**
 * ABI constants that specify the calling convention to use.
 * ctypes.default_abi corresponds to the cdecl convention, and in almost all
 * cases is the correct choice. ctypes.stdcall_abi is provided for calling
 * stdcall functions on Win32, and implies stdcall symbol name decoration;
 * ctypes.winapi_abi is just stdcall but without decoration.
 */
enum ABICode {
  ABI_DEFAULT,
  ABI_STDCALL,
  ABI_WINAPI,
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

// Descriptor of one field in a StructType. The name of the field is stored
// as the key to the hash entry.
struct FieldInfo
{
  JSObject* mType;    // CType of the field
  size_t    mIndex;   // index of the field in the struct (first is 0)
  size_t    mOffset;  // offset of the field in the struct, in bytes
};

// Hash policy for FieldInfos.
struct FieldHashPolicy
{
  typedef JSFlatString* Key;
  typedef Key Lookup;

  static uint32_t hash(const Lookup &l) {
    const jschar* s = l->chars();
    size_t n = l->length();
    uint32_t hash = 0;
    for (; n > 0; s++, n--)
      hash = hash * 33 + *s;
    return hash;
  }

  static JSBool match(const Key &k, const Lookup &l) {
    if (k == l)
      return true;

    if (k->length() != l->length())
      return false;

    return memcmp(k->chars(), l->chars(), k->length() * sizeof(jschar)) == 0;
  }
};

typedef HashMap<JSFlatString*, FieldInfo, FieldHashPolicy, SystemAllocPolicy> FieldInfoHash;

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
  Array<JSObject*> mArgTypes; 

  // A variable array of ffi_type*s corresponding to both known parameter
  // types and dynamic (variadic) parameter types. Longer than mArgTypes
  // only if mIsVariadic.
  Array<ffi_type*> mFFITypes;

  // Flag indicating whether the function behaves like a C function with
  // ... as the final formal parameter.
  bool mIsVariadic;
};

// Parameters necessary for invoking a JS function from a C closure.
struct ClosureInfo
{
  JSContext* cx;         // JSContext to use
  JSRuntime* rt;         // Used in the destructor, where cx might have already
                         // been GCed.
  JSObject* closureObj;  // CClosure object
  JSObject* typeObj;     // FunctionType describing the C function
  JSObject* thisObj;     // 'this' object to use for the JS function call
  JSObject* jsfnObj;     // JS function
  void* errResult;       // Result that will be returned if the closure throws
  ffi_closure* closure;  // The C closure itself

  // Anything conditionally freed in the destructor should be initialized to
  // NULL here.
  ClosureInfo(JSRuntime* runtime)
    : rt(runtime)
    , errResult(NULL)
    , closure(NULL)
  {}

  ~ClosureInfo() {
    if (closure)
      ffi_closure_free(closure);
    if (errResult)
      rt->free_(errResult);
  };
};

bool IsCTypesGlobal(JSObject* obj);

JSCTypesCallbacks* GetCallbacks(JSObject* obj);

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

enum CTypesGlobalSlot {
  SLOT_CALLBACKS = 0, // pointer to JSCTypesCallbacks struct
  CTYPESGLOBAL_SLOTS
};

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
  SLOT_OURDATAPROTO      = 11, // the data prototype corresponding to this object
  SLOT_CLOSURECX         = 12, // JSContext for use with FunctionType closures
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
  SLOT_FIELDINFO = 8, // (StructTypes only) FieldInfoHash table
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
    TypeCode type, JSString* name, jsval size, jsval align, ffi_type* ffiType);

  JSObject* DefineBuiltin(JSContext* cx, JSObject* parent, const char* propName,
    JSObject* typeProto, JSObject* dataProto, const char* name, TypeCode type,
    jsval size, jsval align, ffi_type* ffiType);

  bool IsCType(JSObject* obj);
  bool IsCTypeProto(JSObject* obj);
  TypeCode GetTypeCode(JSObject* typeObj);
  bool TypesEqual(JSObject* t1, JSObject* t2);
  size_t GetSize(JSObject* obj);
  bool GetSafeSize(JSObject* obj, size_t* result);
  bool IsSizeDefined(JSObject* obj);
  size_t GetAlignment(JSObject* obj);
  ffi_type* GetFFIType(JSContext* cx, JSObject* obj);
  JSString* GetName(JSContext* cx, JSObject* obj);
  JSObject* GetProtoFromCtor(JSObject* obj, CTypeProtoSlot slot);
  JSObject* GetProtoFromType(JSObject* obj, CTypeProtoSlot slot);
  JSCTypesCallbacks* GetCallbacksFromType(JSObject* obj);
}

namespace PointerType {
  JSObject* CreateInternal(JSContext* cx, JSObject* baseType);

  JSObject* GetBaseType(JSObject* obj);
}

namespace ArrayType {
  JSObject* CreateInternal(JSContext* cx, JSObject* baseType, size_t length,
    bool lengthDefined);

  JSObject* GetBaseType(JSObject* obj);
  size_t GetLength(JSObject* obj);
  bool GetSafeLength(JSObject* obj, size_t* result);
  ffi_type* BuildFFIType(JSContext* cx, JSObject* obj);
}

namespace StructType {
  JSBool DefineInternal(JSContext* cx, JSObject* typeObj, JSObject* fieldsObj);

  const FieldInfoHash* GetFieldInfo(JSObject* obj);
  const FieldInfo* LookupField(JSContext* cx, JSObject* obj, JSFlatString *name);
  JSObject* BuildFieldsArray(JSContext* cx, JSObject* obj);
  ffi_type* BuildFFIType(JSContext* cx, JSObject* obj);
}

namespace FunctionType {
  JSObject* CreateInternal(JSContext* cx, jsval abi, jsval rtype,
    jsval* argtypes, jsuint arglen);

  JSObject* ConstructWithObject(JSContext* cx, JSObject* typeObj,
    JSObject* refObj, PRFuncPtr fnptr, JSObject* result);

  FunctionInfo* GetFunctionInfo(JSObject* obj);
  void BuildSymbolName(JSString* name, JSObject* typeObj,
    AutoCString& result);
}

namespace CClosure {
  JSObject* Create(JSContext* cx, JSObject* typeObj, JSObject* fnObj,
    JSObject* thisObj, jsval errVal, PRFuncPtr* fnptr);
}

namespace CData {
  JSObject* Create(JSContext* cx, JSObject* typeObj, JSObject* refObj,
    void* data, bool ownResult);

  JSObject* GetCType(JSObject* dataObj);
  void* GetData(JSObject* dataObj);
  bool IsCData(JSObject* obj);
  bool IsCDataProto(JSObject* obj);

  // Attached by JSAPI as the function 'ctypes.cast'
  JSBool Cast(JSContext* cx, unsigned argc, jsval* vp);
  // Attached by JSAPI as the function 'ctypes.getRuntime'
  JSBool GetRuntime(JSContext* cx, unsigned argc, jsval* vp);
}

namespace Int64 {
  bool IsInt64(JSObject* obj);
}

namespace UInt64 {
  bool IsUInt64(JSObject* obj);
}

}
}

#endif
