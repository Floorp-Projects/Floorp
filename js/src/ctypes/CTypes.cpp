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

#include "CTypes.h"
#include "Library.h"
#include "jsnum.h"
#include <limits>

#include <math.h>
#if defined(XP_WIN) || defined(XP_OS2)
#include <float.h>
#endif

#if defined(SOLARIS)
#include <ieeefp.h>
#endif

#ifdef HAVE_SSIZE_T
#include <sys/types.h>
#endif

using namespace std;

namespace js {
namespace ctypes {

/*******************************************************************************
** Helper classes
*******************************************************************************/

class ScopedContextThread
{
public:
  ScopedContextThread(JSContext* cx) : mCx(cx) { JS_SetContextThread(cx); }
  ~ScopedContextThread() { JS_ClearContextThread(mCx); }
private:
  JSContext* mCx;
};

/*******************************************************************************
** JSAPI function prototypes
*******************************************************************************/

static JSBool ConstructAbstract(JSContext* cx, uintN argc, jsval* vp);

namespace CType {
  static JSBool ConstructData(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ConstructBasic(JSContext* cx, JSObject* obj, uintN argc, jsval* vp);

  static void Trace(JSTracer* trc, JSObject* obj);
  static void Finalize(JSContext* cx, JSObject* obj);
  static void FinalizeProtoClass(JSContext* cx, JSObject* obj);

  static JSBool PrototypeGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool NameGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool SizeGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool PtrGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp);
  static JSBool CreateArray(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ToString(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ToSource(JSContext* cx, uintN argc, jsval* vp);
  static JSBool HasInstance(JSContext* cx, JSObject* obj, const jsval* v, JSBool* bp);
}

namespace PointerType {
  static JSBool Create(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ConstructData(JSContext* cx, JSObject* obj, uintN argc, jsval* vp);

  static JSBool TargetTypeGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool ContentsGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool ContentsSetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool IsNull(JSContext* cx, uintN argc, jsval* vp);
}

namespace ArrayType {
  static JSBool Create(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ConstructData(JSContext* cx, JSObject* obj, uintN argc, jsval* vp);

  static JSBool ElementTypeGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool LengthGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool Getter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp);
  static JSBool Setter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp);
  static JSBool AddressOfElement(JSContext* cx, uintN argc, jsval* vp);
}

namespace StructType {
  static JSBool Create(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ConstructData(JSContext* cx, JSObject* obj, uintN argc, jsval* vp);

  static JSBool FieldsArrayGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool FieldGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool FieldSetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool AddressOfField(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Define(JSContext* cx, uintN argc, jsval* vp);
}

namespace FunctionType {
  static JSBool Create(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ConstructData(JSContext* cx, JSObject* typeObj,
    JSObject* dataObj, JSObject* fnObj, JSObject* thisObj);

  static JSBool Call(JSContext* cx, uintN argc, jsval* vp);

  static JSBool ArgTypesGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool ReturnTypeGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool ABIGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp);
  static JSBool IsVariadicGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
}

namespace CClosure {
  static void Trace(JSTracer* trc, JSObject* obj);
  static void Finalize(JSContext* cx, JSObject* obj);

  // libffi callback
  static void ClosureStub(ffi_cif* cif, void* result, void** args,
    void* userData);
}

namespace CData {
  static void Finalize(JSContext* cx, JSObject* obj);

  static JSBool ValueGetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool ValueSetter(JSContext* cx, JSObject* obj, jsid idval,
    jsval* vp);
  static JSBool Address(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ReadString(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ToSource(JSContext* cx, uintN argc, jsval* vp);
}

// Int64Base provides functions common to Int64 and UInt64.
namespace Int64Base {
  JSObject* Construct(JSContext* cx, JSObject* proto, JSUint64 data,
    bool isUnsigned);

  JSUint64 GetInt(JSContext* cx, JSObject* obj);

  JSBool ToString(JSContext* cx, JSObject* obj, uintN argc, jsval* vp,
    bool isUnsigned);

  JSBool ToSource(JSContext* cx, JSObject* obj, uintN argc, jsval* vp,
    bool isUnsigned);

  static void Finalize(JSContext* cx, JSObject* obj);
}

namespace Int64 {
  static JSBool Construct(JSContext* cx, uintN argc, jsval* vp);

  static JSBool ToString(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ToSource(JSContext* cx, uintN argc, jsval* vp);

  static JSBool Compare(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Lo(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Hi(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Join(JSContext* cx, uintN argc, jsval* vp);
}

namespace UInt64 {
  static JSBool Construct(JSContext* cx, uintN argc, jsval* vp);

  static JSBool ToString(JSContext* cx, uintN argc, jsval* vp);
  static JSBool ToSource(JSContext* cx, uintN argc, jsval* vp);

  static JSBool Compare(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Lo(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Hi(JSContext* cx, uintN argc, jsval* vp);
  static JSBool Join(JSContext* cx, uintN argc, jsval* vp);
}

/*******************************************************************************
** JSClass definitions and initialization functions
*******************************************************************************/

// Class representing the 'ctypes' object itself. This exists to contain the
// JSCTypesCallbacks set of function pointers.
static JSClass sCTypesGlobalClass = {
  "ctypes",
  JSCLASS_HAS_RESERVED_SLOTS(CTYPESGLOBAL_SLOTS),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass sCABIClass = {
  "CABI",
  JSCLASS_HAS_RESERVED_SLOTS(CABI_SLOTS),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

// Class representing ctypes.{C,Pointer,Array,Struct,Function}Type.prototype.
// This exists to give said prototypes a class of "CType", and to provide
// reserved slots for stashing various other prototype objects.
static JSClass sCTypeProtoClass = {
  "CType",
  JSCLASS_HAS_RESERVED_SLOTS(CTYPEPROTO_SLOTS),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, CType::FinalizeProtoClass,
  NULL, NULL, ConstructAbstract, ConstructAbstract, NULL, NULL, NULL, NULL
};

// Class representing ctypes.CData.prototype and the 'prototype' properties
// of CTypes. This exists to give said prototypes a class of "CData".
static JSClass sCDataProtoClass = {
  "CData",
  0,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass sCTypeClass = {
  "CType",
  JSCLASS_HAS_RESERVED_SLOTS(CTYPE_SLOTS) | JSCLASS_MARK_IS_TRACE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, CType::Finalize,
  NULL, NULL, CType::ConstructData, CType::ConstructData, NULL,
  CType::HasInstance, JS_CLASS_TRACE(CType::Trace), NULL
};

static JSClass sCDataClass = {
  "CData",
  JSCLASS_HAS_RESERVED_SLOTS(CDATA_SLOTS),
  JS_PropertyStub, JS_PropertyStub, ArrayType::Getter, ArrayType::Setter,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, CData::Finalize,
  NULL, NULL, FunctionType::Call, FunctionType::Call, NULL, NULL, NULL, NULL
};

static JSClass sCClosureClass = {
  "CClosure",
  JSCLASS_HAS_RESERVED_SLOTS(CCLOSURE_SLOTS) | JSCLASS_MARK_IS_TRACE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, CClosure::Finalize,
  NULL, NULL, NULL, NULL, NULL, NULL, JS_CLASS_TRACE(CClosure::Trace), NULL
};

#define CTYPESFN_FLAGS \
  (JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)

#define CTYPESCTOR_FLAGS \
  (CTYPESFN_FLAGS | JSFUN_CONSTRUCTOR)

#define CTYPESPROP_FLAGS \
  (JSPROP_SHARED | JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)

#define CDATAFN_FLAGS \
  (JSPROP_READONLY | JSPROP_PERMANENT)

static JSPropertySpec sCTypeProps[] = {
  { "name", 0, CTYPESPROP_FLAGS, CType::NameGetter, NULL },
  { "size", 0, CTYPESPROP_FLAGS, CType::SizeGetter, NULL },
  { "ptr", 0, CTYPESPROP_FLAGS, CType::PtrGetter, NULL },
  { "prototype", 0, CTYPESPROP_FLAGS, CType::PrototypeGetter, NULL },
  { 0, 0, 0, NULL, NULL }
};

static JSFunctionSpec sCTypeFunctions[] = {
  JS_FN("array", CType::CreateArray, 0, CTYPESFN_FLAGS),
  JS_FN("toString", CType::ToString, 0, CTYPESFN_FLAGS),
  JS_FN("toSource", CType::ToSource, 0, CTYPESFN_FLAGS),
  JS_FS_END
};

static JSPropertySpec sCDataProps[] = {
  { "value", 0, JSPROP_SHARED | JSPROP_PERMANENT,
    CData::ValueGetter, CData::ValueSetter },
  { 0, 0, 0, NULL, NULL }
};

static JSFunctionSpec sCDataFunctions[] = {
  JS_FN("address", CData::Address, 0, CDATAFN_FLAGS),
  JS_FN("readString", CData::ReadString, 0, CDATAFN_FLAGS),
  JS_FN("toSource", CData::ToSource, 0, CDATAFN_FLAGS),
  JS_FN("toString", CData::ToSource, 0, CDATAFN_FLAGS),
  JS_FS_END
};

static JSFunctionSpec sPointerFunction =
  JS_FN("PointerType", PointerType::Create, 1, CTYPESCTOR_FLAGS);

static JSPropertySpec sPointerProps[] = {
  { "targetType", 0, CTYPESPROP_FLAGS, PointerType::TargetTypeGetter, NULL },
  { 0, 0, 0, NULL, NULL }
};

static JSFunctionSpec sPointerInstanceFunctions[] = {
  JS_FN("isNull", PointerType::IsNull, 0, CTYPESFN_FLAGS),
  JS_FS_END
};
  
static JSPropertySpec sPointerInstanceProps[] = {
  { "contents", 0, JSPROP_SHARED | JSPROP_PERMANENT,
    PointerType::ContentsGetter, PointerType::ContentsSetter },
  { 0, 0, 0, NULL, NULL }
};

static JSFunctionSpec sArrayFunction =
  JS_FN("ArrayType", ArrayType::Create, 1, CTYPESCTOR_FLAGS);

static JSPropertySpec sArrayProps[] = {
  { "elementType", 0, CTYPESPROP_FLAGS, ArrayType::ElementTypeGetter, NULL },
  { "length", 0, CTYPESPROP_FLAGS, ArrayType::LengthGetter, NULL },
  { 0, 0, 0, NULL, NULL }
};

static JSFunctionSpec sArrayInstanceFunctions[] = {
  JS_FN("addressOfElement", ArrayType::AddressOfElement, 1, CDATAFN_FLAGS),
  JS_FS_END
};

static JSPropertySpec sArrayInstanceProps[] = {
  { "length", 0, JSPROP_SHARED | JSPROP_READONLY | JSPROP_PERMANENT,
    ArrayType::LengthGetter, NULL },
  { 0, 0, 0, NULL, NULL }
};

static JSFunctionSpec sStructFunction =
  JS_FN("StructType", StructType::Create, 2, CTYPESCTOR_FLAGS);

static JSPropertySpec sStructProps[] = {
  { "fields", 0, CTYPESPROP_FLAGS, StructType::FieldsArrayGetter, NULL },
  { 0, 0, 0, NULL, NULL }
};

static JSFunctionSpec sStructFunctions[] = {
  JS_FN("define", StructType::Define, 1, CDATAFN_FLAGS),
  JS_FS_END
};

static JSFunctionSpec sStructInstanceFunctions[] = {
  JS_FN("addressOfField", StructType::AddressOfField, 1, CDATAFN_FLAGS),
  JS_FS_END
};

static JSFunctionSpec sFunctionFunction =
  JS_FN("FunctionType", FunctionType::Create, 2, CTYPESCTOR_FLAGS);

static JSPropertySpec sFunctionProps[] = {
  { "argTypes", 0, CTYPESPROP_FLAGS, FunctionType::ArgTypesGetter, NULL },
  { "returnType", 0, CTYPESPROP_FLAGS, FunctionType::ReturnTypeGetter, NULL },
  { "abi", 0, CTYPESPROP_FLAGS, FunctionType::ABIGetter, NULL },
  { "isVariadic", 0, CTYPESPROP_FLAGS, FunctionType::IsVariadicGetter, NULL },
  { 0, 0, 0, NULL, NULL }
};

static JSClass sInt64ProtoClass = {
  "Int64",
  0,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass sUInt64ProtoClass = {
  "UInt64",
  0,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass sInt64Class = {
  "Int64",
  JSCLASS_HAS_RESERVED_SLOTS(INT64_SLOTS),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Int64Base::Finalize,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSClass sUInt64Class = {
  "UInt64",
  JSCLASS_HAS_RESERVED_SLOTS(INT64_SLOTS),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Int64Base::Finalize,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec sInt64StaticFunctions[] = {
  JS_FN("compare", Int64::Compare, 2, CTYPESFN_FLAGS),
  JS_FN("lo", Int64::Lo, 1, CTYPESFN_FLAGS),
  JS_FN("hi", Int64::Hi, 1, CTYPESFN_FLAGS),
  JS_FN("join", Int64::Join, 2, CTYPESFN_FLAGS),
  JS_FS_END
};

static JSFunctionSpec sUInt64StaticFunctions[] = {
  JS_FN("compare", UInt64::Compare, 2, CTYPESFN_FLAGS),
  JS_FN("lo", UInt64::Lo, 1, CTYPESFN_FLAGS),
  JS_FN("hi", UInt64::Hi, 1, CTYPESFN_FLAGS),
  JS_FN("join", UInt64::Join, 2, CTYPESFN_FLAGS),
  JS_FS_END
};

static JSFunctionSpec sInt64Functions[] = {
  JS_FN("toString", Int64::ToString, 0, CTYPESFN_FLAGS),
  JS_FN("toSource", Int64::ToSource, 0, CTYPESFN_FLAGS),
  JS_FS_END
};

static JSFunctionSpec sUInt64Functions[] = {
  JS_FN("toString", UInt64::ToString, 0, CTYPESFN_FLAGS),
  JS_FN("toSource", UInt64::ToSource, 0, CTYPESFN_FLAGS),
  JS_FS_END
};

static JSFunctionSpec sModuleFunctions[] = {
  JS_FN("open", Library::Open, 1, CTYPESFN_FLAGS),
  JS_FN("cast", CData::Cast, 2, CTYPESFN_FLAGS),
  JS_FN("libraryName", Library::Name, 1, CTYPESFN_FLAGS),
  JS_FS_END
};

static inline bool FloatIsFinite(jsdouble f) {
#ifdef WIN32
  return _finite(f) != 0;
#else
  return finite(f);
#endif
}

JS_ALWAYS_INLINE JSString*
NewUCString(JSContext* cx, const AutoString& from)
{
  return JS_NewUCStringCopyN(cx, from.begin(), from.length());
}

JS_ALWAYS_INLINE size_t
Align(size_t val, size_t align)
{
  return ((val - 1) | (align - 1)) + 1;
}

static ABICode
GetABICode(JSContext* cx, JSObject* obj)
{
  // make sure we have an object representing a CABI class,
  // and extract the enumerated class type from the reserved slot.
  if (JS_GET_CLASS(cx, obj) != &sCABIClass)
    return INVALID_ABI;

  jsval result;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_ABICODE, &result));

  return ABICode(JSVAL_TO_INT(result));
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

JSBool
TypeError(JSContext* cx, const char* expected, jsval actual)
{
  JSString* str = JS_ValueToSource(cx, actual);
  js::AutoStringRooter root(cx, str);

  const char* src;
  if (str) {
    src = JS_GetStringBytesZ(cx, str);
    if (!src)
      return false;
  } else {
    JS_ClearPendingException(cx);
    src = "<<error converting value to string>>";
  }

  JS_ReportErrorNumber(cx, GetErrorMessage, NULL,
                       CTYPESMSG_TYPE_ERROR, expected, src);
  return false;
}

static JSObject*
InitCTypeClass(JSContext* cx, JSObject* parent)
{
  JSFunction* fun = JS_DefineFunction(cx, parent, "CType", ConstructAbstract, 0,
                      CTYPESCTOR_FLAGS);
  if (!fun)
    return NULL;

  JSObject* ctor = JS_GetFunctionObject(fun);
  JSObject* fnproto = JS_GetPrototype(cx, ctor);
  JS_ASSERT(ctor);
  JS_ASSERT(fnproto);

  // Set up ctypes.CType.prototype.
  JSObject* prototype = JS_NewObject(cx, &sCTypeProtoClass, fnproto, parent);
  if (!prototype)
    return NULL;

  if (!JS_DefineProperty(cx, ctor, "prototype", OBJECT_TO_JSVAL(prototype),
         NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return NULL;

  if (!JS_DefineProperty(cx, prototype, "constructor", OBJECT_TO_JSVAL(ctor),
         NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return NULL;

  // Define properties and functions common to all CTypes.
  if (!JS_DefineProperties(cx, prototype, sCTypeProps) ||
      !JS_DefineFunctions(cx, prototype, sCTypeFunctions))
    return NULL;

  if (!JS_FreezeObject(cx, ctor) || !JS_FreezeObject(cx, prototype))
    return NULL;

  return prototype;
}

static JSObject*
InitCDataClass(JSContext* cx, JSObject* parent, JSObject* CTypeProto)
{
  JSFunction* fun = JS_DefineFunction(cx, parent, "CData", ConstructAbstract, 0,
                      CTYPESCTOR_FLAGS);
  if (!fun)
    return NULL;

  JSObject* ctor = JS_GetFunctionObject(fun);
  JS_ASSERT(ctor);

  // Set up ctypes.CData.__proto__ === ctypes.CType.prototype.
  // (Note that 'ctypes.CData instanceof Function' is still true, thanks to the
  // prototype chain.)
  if (!JS_SetPrototype(cx, ctor, CTypeProto))
    return NULL;

  // Set up ctypes.CData.prototype.
  JSObject* prototype = JS_NewObject(cx, &sCDataProtoClass, NULL, parent);
  if (!prototype)
    return NULL;

  if (!JS_DefineProperty(cx, ctor, "prototype", OBJECT_TO_JSVAL(prototype),
         NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return NULL;

  if (!JS_DefineProperty(cx, prototype, "constructor", OBJECT_TO_JSVAL(ctor),
         NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return NULL;

  // Define properties and functions common to all CDatas.
  if (!JS_DefineProperties(cx, prototype, sCDataProps) ||
      !JS_DefineFunctions(cx, prototype, sCDataFunctions))
    return NULL;

  if (//!JS_FreezeObject(cx, prototype) || // XXX fixme - see bug 541212!
      !JS_FreezeObject(cx, ctor))
    return NULL;

  return prototype;
}

static JSBool
DefineABIConstant(JSContext* cx,
                  JSObject* parent,
                  const char* name,
                  ABICode code)
{
  JSObject* obj = JS_DefineObject(cx, parent, name, &sCABIClass, NULL,
                    JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
  if (!obj)
    return false;
  if (!JS_SetReservedSlot(cx, obj, SLOT_ABICODE, INT_TO_JSVAL(code)))
    return false;
  return JS_FreezeObject(cx, obj);
}

// Set up a single type constructor for
// ctypes.{Pointer,Array,Struct,Function}Type.
static JSBool
InitTypeConstructor(JSContext* cx,
                    JSObject* parent,
                    JSObject* CTypeProto,
                    JSObject* CDataProto,
                    JSFunctionSpec spec,
                    JSFunctionSpec* fns,
                    JSPropertySpec* props,
                    JSFunctionSpec* instanceFns,
                    JSPropertySpec* instanceProps,
                    JSObject*& typeProto,
                    JSObject*& dataProto)
{
  JSFunction* fun = JS_DefineFunction(cx, parent, spec.name, spec.call, 
                      spec.nargs, spec.flags);
  if (!fun)
    return false;

  JSObject* obj = JS_GetFunctionObject(fun);
  if (!obj)
    return false;

  // Set up the .prototype and .prototype.constructor properties.
  typeProto = JS_NewObject(cx, &sCTypeProtoClass, CTypeProto, parent);
  if (!typeProto)
    return false;

  // Define property before proceeding, for GC safety.
  if (!JS_DefineProperty(cx, obj, "prototype", OBJECT_TO_JSVAL(typeProto),
         NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  if (fns && !JS_DefineFunctions(cx, typeProto, fns))
    return false;

  if (!JS_DefineProperties(cx, typeProto, props))
    return false;

  if (!JS_DefineProperty(cx, typeProto, "constructor", OBJECT_TO_JSVAL(obj),
         NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  // Stash ctypes.{Pointer,Array,Struct}Type.prototype on a reserved slot of
  // the type constructor, for faster lookup.
  if (!JS_SetReservedSlot(cx, obj, SLOT_FN_CTORPROTO, OBJECT_TO_JSVAL(typeProto)))
    return false;

  // Create an object to serve as the common ancestor for all CData objects
  // created from the given type constructor. This has ctypes.CData.prototype
  // as its prototype, such that it inherits the properties and functions
  // common to all CDatas.
  dataProto = JS_NewObject(cx, &sCDataProtoClass, CDataProto, parent);
  if (!dataProto)
    return false;
  js::AutoObjectRooter protoroot(cx, dataProto);

  // Define functions and properties on the 'dataProto' object that are common
  // to all CData objects created from this type constructor. (These will
  // become functions and properties on CData objects created from this type.)
  if (instanceFns && !JS_DefineFunctions(cx, dataProto, instanceFns))
    return false;

  if (instanceProps && !JS_DefineProperties(cx, dataProto, instanceProps))
    return false;

  if (!JS_FreezeObject(cx, obj) ||
      //!JS_FreezeObject(cx, dataProto) || // XXX fixme - see bug 541212!
      !JS_FreezeObject(cx, typeProto))
    return false;

  return true;
}

JSObject*
InitInt64Class(JSContext* cx,
               JSObject* parent,
               JSClass* clasp,
               JSNative construct,
               JSFunctionSpec* fs,
               JSFunctionSpec* static_fs)
{
  // Init type class and constructor
  JSObject* prototype = JS_InitClass(cx, parent, NULL, clasp, construct,
    0, NULL, fs, NULL, static_fs);
  if (!prototype)
    return NULL;

  JSObject* ctor = JS_GetConstructor(cx, prototype);
  if (!ctor)
    return NULL;
  if (!JS_FreezeObject(cx, ctor))
    return NULL;

  // Stash ctypes.{Int64,UInt64}.prototype on a reserved slot of the 'join'
  // function.
  jsval join;
  ASSERT_OK(JS_GetProperty(cx, ctor, "join", &join));
  if (!JS_SetReservedSlot(cx, JSVAL_TO_OBJECT(join), SLOT_FN_INT64PROTO,
         OBJECT_TO_JSVAL(prototype)))
    return NULL;

  if (!JS_FreezeObject(cx, prototype))
    return NULL;

  return prototype;
}

static JSBool
AttachProtos(JSContext* cx, JSObject* proto, JSObject** protos)
{
  // For a given 'proto' of [[Class]] "CTypeProto", attach each of the 'protos'
  // to the appropriate CTypeProtoSlot. (SLOT_UINT64PROTO is the last slot
  // of [[Class]] "CTypeProto".)
  for (JSUint32 i = 0; i <= SLOT_UINT64PROTO; ++i) {
    if (!JS_SetReservedSlot(cx, proto, i, OBJECT_TO_JSVAL(protos[i])))
      return false;
  }

  return true;
}

JSBool
InitTypeClasses(JSContext* cx, JSObject* parent)
{
  // Initialize the ctypes.CType class. This acts as an abstract base class for
  // the various types, and provides the common API functions. It has:
  //   * [[Class]] "Function"
  //   * __proto__ === Function.prototype
  //   * A constructor that throws a TypeError. (You can't construct an
  //     abstract type!)
  //   * 'prototype' property:
  //     * [[Class]] "CTypeProto"
  //     * __proto__ === Function.prototype
  //     * A constructor that throws a TypeError. (You can't construct an
  //       abstract type instance!)
  //     * 'constructor' property === ctypes.CType
  //     * Provides properties and functions common to all CTypes.
  JSObject* CTypeProto = InitCTypeClass(cx, parent);
  if (!CTypeProto)
    return false;

  // Initialize the ctypes.CData class. This acts as an abstract base class for
  // instances of the various types, and provides the common API functions.
  // It has:
  //   * [[Class]] "Function"
  //   * __proto__ === Function.prototype
  //   * A constructor that throws a TypeError. (You can't construct an
  //     abstract type instance!)
  //   * 'prototype' property:
  //     * [[Class]] "CDataProto"
  //     * 'constructor' property === ctypes.CData
  //     * Provides properties and functions common to all CDatas.
  JSObject* CDataProto = InitCDataClass(cx, parent, CTypeProto);
  if (!CDataProto)
    return false;

  // Create and attach the special class constructors: ctypes.PointerType,
  // ctypes.ArrayType, ctypes.StructType, and ctypes.FunctionType.
  // Each of these constructors 'c' has, respectively:
  //   * [[Class]] "Function"
  //   * __proto__ === Function.prototype
  //   * A constructor that creates a user-defined type.
  //   * 'prototype' property:
  //     * [[Class]] "CTypeProto"
  //     * __proto__ === ctypes.CType.prototype
  //     * 'constructor' property === 'c'
  // We also construct an object 'p' to serve, given a type object 't'
  // constructed from one of these type constructors, as
  // 't.prototype.__proto__'. This object has:
  //   * [[Class]] "CDataProto"
  //   * __proto__ === ctypes.CData.prototype
  //   * Properties and functions common to all CDatas.
  // Therefore an instance 't' of ctypes.{Pointer,Array,Struct,Function}Type
  // will have, resp.:
  //   * [[Class]] "CType"
  //   * __proto__ === ctypes.{Pointer,Array,Struct,Function}Type.prototype
  //   * A constructor which creates and returns a CData object, containing
  //     binary data of the given type.
  //   * 'prototype' property:
  //     * [[Class]] "CDataProto"
  //     * __proto__ === 'p', the prototype object from above
  //     * 'constructor' property === 't'
  JSObject* protos[CTYPEPROTO_SLOTS];
  if (!InitTypeConstructor(cx, parent, CTypeProto, CDataProto,
         sPointerFunction, NULL, sPointerProps,
         sPointerInstanceFunctions, sPointerInstanceProps,
         protos[SLOT_POINTERPROTO], protos[SLOT_POINTERDATAPROTO]))
    return false;
  js::AutoObjectRooter proot(cx, protos[SLOT_POINTERDATAPROTO]);

  if (!InitTypeConstructor(cx, parent, CTypeProto, CDataProto,
         sArrayFunction, NULL, sArrayProps,
         sArrayInstanceFunctions, sArrayInstanceProps,
         protos[SLOT_ARRAYPROTO], protos[SLOT_ARRAYDATAPROTO]))
    return false;
  js::AutoObjectRooter aroot(cx, protos[SLOT_ARRAYDATAPROTO]);

  if (!InitTypeConstructor(cx, parent, CTypeProto, CDataProto,
         sStructFunction, sStructFunctions, sStructProps,
         sStructInstanceFunctions, NULL,
         protos[SLOT_STRUCTPROTO], protos[SLOT_STRUCTDATAPROTO]))
    return false;
  js::AutoObjectRooter sroot(cx, protos[SLOT_STRUCTDATAPROTO]);

  if (!InitTypeConstructor(cx, parent, CTypeProto, CDataProto,
         sFunctionFunction, NULL, sFunctionProps, NULL, NULL,
         protos[SLOT_FUNCTIONPROTO], protos[SLOT_FUNCTIONDATAPROTO]))
    return false;
  js::AutoObjectRooter froot(cx, protos[SLOT_FUNCTIONDATAPROTO]);

  protos[SLOT_CDATAPROTO] = CDataProto;

  // Create and attach the ctypes.{Int64,UInt64} constructors.
  // Each of these has, respectively:
  //   * [[Class]] "Function"
  //   * __proto__ === Function.prototype
  //   * A constructor that creates a ctypes.{Int64,UInt64} object, respectively.
  //   * 'prototype' property:
  //     * [[Class]] {"Int64Proto","UInt64Proto"}
  //     * 'constructor' property === ctypes.{Int64,UInt64}
  protos[SLOT_INT64PROTO] = InitInt64Class(cx, parent, &sInt64ProtoClass,
    Int64::Construct, sInt64Functions, sInt64StaticFunctions);
  if (!protos[SLOT_INT64PROTO])
    return false;
  protos[SLOT_UINT64PROTO] = InitInt64Class(cx, parent, &sUInt64ProtoClass,
    UInt64::Construct, sUInt64Functions, sUInt64StaticFunctions);
  if (!protos[SLOT_UINT64PROTO])
    return false;

  // Attach the prototypes just created to each of ctypes.CType.prototype,
  // and the special type constructors, so we can access them when constructing
  // instances of those types. 
  if (!AttachProtos(cx, CTypeProto, protos) ||
      !AttachProtos(cx, protos[SLOT_POINTERPROTO], protos) ||
      !AttachProtos(cx, protos[SLOT_ARRAYPROTO], protos) ||
      !AttachProtos(cx, protos[SLOT_STRUCTPROTO], protos) ||
      !AttachProtos(cx, protos[SLOT_FUNCTIONPROTO], protos))
     return false;

  // Attach objects representing ABI constants.
  if (!DefineABIConstant(cx, parent, "default_abi", ABI_DEFAULT) ||
      !DefineABIConstant(cx, parent, "stdcall_abi", ABI_STDCALL) ||
      !DefineABIConstant(cx, parent, "winapi_abi", ABI_WINAPI))
    return false;

  // Create objects representing the builtin types, and attach them to the
  // ctypes object. Each type object 't' has:
  //   * [[Class]] "CType"
  //   * __proto__ === ctypes.CType.prototype
  //   * A constructor which creates and returns a CData object, containing
  //     binary data of the given type.
  //   * 'prototype' property:
  //     * [[Class]] "CDataProto"
  //     * __proto__ === ctypes.CData.prototype
  //     * 'constructor' property === 't'
#define DEFINE_TYPE(name, type, ffiType)                                       \
  JSObject* typeObj_##name =                                                   \
    CType::DefineBuiltin(cx, parent, #name, CTypeProto, CDataProto, #name,     \
      TYPE_##name, INT_TO_JSVAL(sizeof(type)),                                 \
      INT_TO_JSVAL(ffiType.alignment), &ffiType);                              \
  if (!typeObj_##name)                                                         \
    return false;
#include "typedefs.h"

  // Alias 'ctypes.unsigned' as 'ctypes.unsigned_int', since they represent
  // the same type in C.
  if (!JS_DefineProperty(cx, parent, "unsigned",
         OBJECT_TO_JSVAL(typeObj_unsigned_int), NULL, NULL,
         JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  // Create objects representing the special types void_t and voidptr_t.
  JSObject* typeObj =
    CType::DefineBuiltin(cx, parent, "void_t", CTypeProto, CDataProto, "void",
      TYPE_void_t, JSVAL_VOID, JSVAL_VOID, &ffi_type_void);
  if (!typeObj)
    return false;

  typeObj = PointerType::CreateInternal(cx, typeObj);
  if (!typeObj)
    return false;
  if (!JS_DefineProperty(cx, parent, "voidptr_t", OBJECT_TO_JSVAL(typeObj),
         NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  return true;
}

bool
IsCTypesGlobal(JSContext* cx, JSObject* obj)
{
  return JS_GET_CLASS(cx, obj) == &sCTypesGlobalClass;
}

// Get the JSCTypesCallbacks struct from the 'ctypes' object 'obj'.
JSCTypesCallbacks*
GetCallbacks(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(IsCTypesGlobal(cx, obj));

  jsval result;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_CALLBACKS, &result));
  if (JSVAL_IS_VOID(result))
    return NULL;

  return static_cast<JSCTypesCallbacks*>(JSVAL_TO_PRIVATE(result));
}

JS_BEGIN_EXTERN_C

JS_PUBLIC_API(JSBool)
JS_InitCTypesClass(JSContext* cx, JSObject* global)
{
  // attach ctypes property to global object
  JSObject* ctypes = JS_NewObject(cx, &sCTypesGlobalClass, NULL, NULL);
  if (!ctypes)
    return false;

  if (!JS_DefineProperty(cx, global, "ctypes", OBJECT_TO_JSVAL(ctypes),
         JS_PropertyStub, JS_PropertyStub, JSPROP_READONLY | JSPROP_PERMANENT)) {
    return false;
  }

  if (!InitTypeClasses(cx, ctypes))
    return false;

  // attach API functions
  if (!JS_DefineFunctions(cx, ctypes, sModuleFunctions))
    return false;

  // Seal the ctypes object, to prevent modification.
  return JS_FreezeObject(cx, ctypes);
}

JS_PUBLIC_API(JSBool)
JS_SetCTypesCallbacks(JSContext* cx,
                      JSObject* ctypesObj,
                      JSCTypesCallbacks* callbacks)
{
  JS_ASSERT(callbacks);
  JS_ASSERT(IsCTypesGlobal(cx, ctypesObj));

  // Set the callbacks on a reserved slot.
  return JS_SetReservedSlot(cx, ctypesObj, SLOT_CALLBACKS,
    PRIVATE_TO_JSVAL(callbacks));
}

JS_END_EXTERN_C

/*******************************************************************************
** Type conversion functions
*******************************************************************************/

// Enforce some sanity checks on type widths and properties.
// Where the architecture is 64-bit, make sure it's LP64 or LLP64. (ctypes.int
// autoconverts to a primitive JS number; to support ILP64 architectures, it
// would need to autoconvert to an Int64 object instead. Therefore we enforce
// this invariant here.)
JS_STATIC_ASSERT(sizeof(bool) == 1 || sizeof(bool) == 4);
JS_STATIC_ASSERT(sizeof(char) == 1);
JS_STATIC_ASSERT(sizeof(short) == 2);
JS_STATIC_ASSERT(sizeof(int) == 4);
JS_STATIC_ASSERT(sizeof(unsigned) == 4);
JS_STATIC_ASSERT(sizeof(long) == 4 || sizeof(long) == 8);
JS_STATIC_ASSERT(sizeof(long long) == 8);
JS_STATIC_ASSERT(sizeof(size_t) == sizeof(uintptr_t));
JS_STATIC_ASSERT(sizeof(float) == 4);
JS_STATIC_ASSERT(sizeof(PRFuncPtr) == sizeof(void*));
JS_STATIC_ASSERT(numeric_limits<double>::is_signed);

// Templated helper to convert FromType to TargetType, for the default case
// where the trivial POD constructor will do.
template<class TargetType, class FromType>
struct ConvertImpl {
  static JS_ALWAYS_INLINE TargetType Convert(FromType d) {
    return TargetType(d);
  }
};

#ifdef _MSC_VER
// MSVC can't perform double to unsigned __int64 conversion when the
// double is greater than 2^63 - 1. Help it along a little.
template<>
struct ConvertImpl<JSUint64, jsdouble> {
  static JS_ALWAYS_INLINE JSUint64 Convert(jsdouble d) {
    return d > 0x7fffffffffffffffui64 ?
           JSUint64(d - 0x8000000000000000ui64) + 0x8000000000000000ui64 :
           JSUint64(d);
  }
};
#endif

template<class TargetType, class FromType>
static JS_ALWAYS_INLINE TargetType Convert(FromType d)
{
  return ConvertImpl<TargetType, FromType>::Convert(d);
}

template<class TargetType, class FromType>
static JS_ALWAYS_INLINE bool IsAlwaysExact()
{
  // Return 'true' if TargetType can always exactly represent FromType.
  // This means that:
  // 1) TargetType must be the same or more bits wide as FromType. For integers
  //    represented in 'n' bits, unsigned variants will have 'n' digits while
  //    signed will have 'n - 1'. For floating point types, 'digits' is the
  //    mantissa width.
  // 2) If FromType is signed, TargetType must also be signed. (Floating point
  //    types are always signed.)
  // 3) If TargetType is an exact integral type, FromType must be also.
  if (numeric_limits<TargetType>::digits < numeric_limits<FromType>::digits)
    return false;

  if (numeric_limits<FromType>::is_signed &&
      !numeric_limits<TargetType>::is_signed)
    return false;

  if (!numeric_limits<FromType>::is_exact &&
      numeric_limits<TargetType>::is_exact)
    return false;

  return true;
}

// Templated helper to determine if FromType 'i' converts losslessly to
// TargetType 'j'. Default case where both types are the same signedness.
template<class TargetType, class FromType, bool TargetSigned, bool FromSigned>
struct IsExactImpl {
  static JS_ALWAYS_INLINE bool Test(FromType i, TargetType j) {
    JS_STATIC_ASSERT(numeric_limits<TargetType>::is_exact);
    return FromType(j) == i;
  }
};

// Specialization where TargetType is unsigned, FromType is signed.
template<class TargetType, class FromType>
struct IsExactImpl<TargetType, FromType, false, true> {
  static JS_ALWAYS_INLINE bool Test(FromType i, TargetType j) {
    JS_STATIC_ASSERT(numeric_limits<TargetType>::is_exact);
    return i >= 0 && FromType(j) == i;
  }
};

// Specialization where TargetType is signed, FromType is unsigned.
template<class TargetType, class FromType>
struct IsExactImpl<TargetType, FromType, true, false> {
  static JS_ALWAYS_INLINE bool Test(FromType i, TargetType j) {
    JS_STATIC_ASSERT(numeric_limits<TargetType>::is_exact);
    return TargetType(i) >= 0 && FromType(j) == i;
  }
};

// Convert FromType 'i' to TargetType 'result', returning true iff 'result'
// is an exact representation of 'i'.
template<class TargetType, class FromType>
static JS_ALWAYS_INLINE bool ConvertExact(FromType i, TargetType* result)
{
  // Require that TargetType is integral, to simplify conversion.
  JS_STATIC_ASSERT(numeric_limits<TargetType>::is_exact);

  *result = Convert<TargetType>(i);

  // See if we can avoid a dynamic check.
  if (IsAlwaysExact<TargetType, FromType>())
    return true;

  // Return 'true' if 'i' is exactly representable in 'TargetType'.
  return IsExactImpl<TargetType,
                     FromType,
                     numeric_limits<TargetType>::is_signed,
                     numeric_limits<FromType>::is_signed>::Test(i, *result);
}

// Templated helper to determine if Type 'i' is negative. Default case
// where IntegerType is unsigned.
template<class Type, bool IsSigned>
struct IsNegativeImpl {
  static JS_ALWAYS_INLINE bool Test(Type i) {
    return false;
  }
};

// Specialization where Type is signed.
template<class Type>
struct IsNegativeImpl<Type, true> {
  static JS_ALWAYS_INLINE bool Test(Type i) {
    return i < 0;
  }
};

// Determine whether Type 'i' is negative.
template<class Type>
static JS_ALWAYS_INLINE bool IsNegative(Type i)
{
  return IsNegativeImpl<Type, numeric_limits<Type>::is_signed>::Test(i);
}

// Implicitly convert val to bool, allowing JSBool, jsint, and jsdouble
// arguments numerically equal to 0 or 1.
static bool
jsvalToBool(JSContext* cx, jsval val, bool* result)
{
  if (JSVAL_IS_BOOLEAN(val)) {
    *result = JSVAL_TO_BOOLEAN(val) != JS_FALSE;
    return true;
  }
  if (JSVAL_IS_INT(val)) {
    jsint i = JSVAL_TO_INT(val);
    *result = i != 0;
    return i == 0 || i == 1;
  }
  if (JSVAL_IS_DOUBLE(val)) {
    jsdouble d = JSVAL_TO_DOUBLE(val);
    *result = d != 0;
    // Allow -0.
    return d == 1 || d == 0;
  }
  // Don't silently convert null to bool. It's probably a mistake.
  return false;
}

// Implicitly convert val to IntegerType, allowing JSBool, jsint, jsdouble,
// Int64, UInt64, and CData integer types 't' where all values of 't' are
// representable by IntegerType.
template<class IntegerType>
static bool
jsvalToInteger(JSContext* cx, jsval val, IntegerType* result)
{
  JS_STATIC_ASSERT(numeric_limits<IntegerType>::is_exact);

  if (JSVAL_IS_INT(val)) {
    // Make sure the integer fits in the alotted precision, and has the right
    // sign.
    jsint i = JSVAL_TO_INT(val);
    return ConvertExact(i, result);
  }
  if (JSVAL_IS_DOUBLE(val)) {
    // Don't silently lose bits here -- check that val really is an
    // integer value, and has the right sign.
    jsdouble d = JSVAL_TO_DOUBLE(val);
    return ConvertExact(d, result);
  }
  if (!JSVAL_IS_PRIMITIVE(val)) {
    JSObject* obj = JSVAL_TO_OBJECT(val);
    if (CData::IsCData(cx, obj)) {
      JSObject* typeObj = CData::GetCType(cx, obj);
      void* data = CData::GetData(cx, obj);

      // Check whether the source type is always representable, with exact
      // precision, by the target type. If it is, convert the value.
      switch (CType::GetTypeCode(cx, typeObj)) {
#define DEFINE_INT_TYPE(name, fromType, ffiType)                               \
      case TYPE_##name:                                                        \
        if (!IsAlwaysExact<IntegerType, fromType>())                           \
          return false;                                                        \
        *result = IntegerType(*static_cast<fromType*>(data));                  \
        return true;
#define DEFINE_WRAPPED_INT_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#include "typedefs.h"
      case TYPE_void_t:
      case TYPE_bool:
      case TYPE_float:
      case TYPE_double:
      case TYPE_float32_t:
      case TYPE_float64_t:
      case TYPE_char:
      case TYPE_signed_char:
      case TYPE_unsigned_char:
      case TYPE_jschar:
      case TYPE_pointer:
      case TYPE_function:
      case TYPE_array:
      case TYPE_struct:
        // Not a compatible number type.
        return false;
      }
    }

    if (Int64::IsInt64(cx, obj)) {
      // Make sure the integer fits in IntegerType.
      JSInt64 i = Int64Base::GetInt(cx, obj);
      return ConvertExact(i, result);
    }

    if (UInt64::IsUInt64(cx, obj)) {
      // Make sure the integer fits in IntegerType.
      JSUint64 i = Int64Base::GetInt(cx, obj);
      return ConvertExact(i, result);
    }

    return false; 
  }
  if (JSVAL_IS_BOOLEAN(val)) {
    // Implicitly promote boolean values to 0 or 1, like C.
    *result = JSVAL_TO_BOOLEAN(val);
    JS_ASSERT(*result == 0 || *result == 1);
    return true;
  }
  // Don't silently convert null to an integer. It's probably a mistake.
  return false;
}

// Implicitly convert val to FloatType, allowing jsint, jsdouble,
// Int64, UInt64, and CData numeric types 't' where all values of 't' are
// representable by FloatType.
template<class FloatType>
static bool
jsvalToFloat(JSContext *cx, jsval val, FloatType* result)
{
  JS_STATIC_ASSERT(!numeric_limits<FloatType>::is_exact);

  // The following casts may silently throw away some bits, but there's
  // no good way around it. Sternly requiring that the 64-bit double
  // argument be exactly representable as a 32-bit float is
  // unrealistic: it would allow 1/2 to pass but not 1/3.
  if (JSVAL_IS_INT(val)) {
    *result = FloatType(JSVAL_TO_INT(val));
    return true;
  }
  if (JSVAL_IS_DOUBLE(val)) {
    *result = FloatType(JSVAL_TO_DOUBLE(val));
    return true;
  }
  if (!JSVAL_IS_PRIMITIVE(val)) {
    JSObject* obj = JSVAL_TO_OBJECT(val);
    if (CData::IsCData(cx, obj)) {
      JSObject* typeObj = CData::GetCType(cx, obj);
      void* data = CData::GetData(cx, obj);

      // Check whether the source type is always representable, with exact
      // precision, by the target type. If it is, convert the value.
      switch (CType::GetTypeCode(cx, typeObj)) {
#define DEFINE_FLOAT_TYPE(name, fromType, ffiType)                             \
      case TYPE_##name:                                                        \
        if (!IsAlwaysExact<FloatType, fromType>())                             \
          return false;                                                        \
        *result = FloatType(*static_cast<fromType*>(data));                    \
        return true;
#define DEFINE_INT_TYPE(x, y, z) DEFINE_FLOAT_TYPE(x, y, z)
#define DEFINE_WRAPPED_INT_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#include "typedefs.h"
      case TYPE_void_t:
      case TYPE_bool:
      case TYPE_char:
      case TYPE_signed_char:
      case TYPE_unsigned_char:
      case TYPE_jschar:
      case TYPE_pointer:
      case TYPE_function:
      case TYPE_array:
      case TYPE_struct:
        // Not a compatible number type.
        return false;
      }
    }
  }
  // Don't silently convert true to 1.0 or false to 0.0, even though C/C++
  // does it. It's likely to be a mistake.
  return false;
}

template<class IntegerType>
static bool
StringToInteger(JSContext* cx, JSString* string, IntegerType* result)
{
  JS_STATIC_ASSERT(numeric_limits<IntegerType>::is_exact);

  const jschar* cp = string->chars();
  const jschar* end = cp + string->length();
  if (cp == end)
    return false;

  IntegerType sign = 1;
  if (cp[0] == '-') {
    if (!numeric_limits<IntegerType>::is_signed)
      return false;

    sign = -1;
    ++cp;
  }

  // Assume base-10, unless the string begins with '0x' or '0X'.
  IntegerType base = 10;
  if (end - cp > 2 && cp[0] == '0' && (cp[1] == 'x' || cp[1] == 'X')) {
    cp += 2;
    base = 16;
  }

  // Scan the string left to right and build the number,
  // checking for valid characters 0 - 9, a - f, A - F and overflow.
  IntegerType i = 0;
  while (cp != end) {
    jschar c = *cp++;
    if (c >= '0' && c <= '9')
      c -= '0';
    else if (base == 16 && c >= 'a' && c <= 'f')
      c = c - 'a' + 10;
    else if (base == 16 && c >= 'A' && c <= 'F')
      c = c - 'A' + 10;
    else
      return false;

    IntegerType ii = i;
    i = ii * base + sign * c;
    if (i / base != ii) // overflow
      return false;
  }

  *result = i;
  return true;
}

// Implicitly convert val to IntegerType, allowing jsint, jsdouble,
// Int64, UInt64, and optionally a decimal or hexadecimal string argument.
// (This is common code shared by jsvalToSize and the Int64/UInt64 constructors.)
template<class IntegerType>
static bool
jsvalToBigInteger(JSContext* cx,
                  jsval val,
                  bool allowString,
                  IntegerType* result)
{
  JS_STATIC_ASSERT(numeric_limits<IntegerType>::is_exact);

  if (JSVAL_IS_INT(val)) {
    // Make sure the integer fits in the alotted precision, and has the right
    // sign.
    jsint i = JSVAL_TO_INT(val);
    return ConvertExact(i, result);
  }
  if (JSVAL_IS_DOUBLE(val)) {
    // Don't silently lose bits here -- check that val really is an
    // integer value, and has the right sign.
    jsdouble d = JSVAL_TO_DOUBLE(val);
    return ConvertExact(d, result);
  }
  if (allowString && JSVAL_IS_STRING(val)) {
    // Allow conversion from base-10 or base-16 strings, provided the result
    // fits in IntegerType. (This allows an Int64 or UInt64 object to be passed
    // to the JS array element operator, which will automatically call
    // toString() on the object for us.)
    return StringToInteger(cx, JSVAL_TO_STRING(val), result);
  }
  if (!JSVAL_IS_PRIMITIVE(val)) {
    // Allow conversion from an Int64 or UInt64 object directly.
    JSObject* obj = JSVAL_TO_OBJECT(val);

    if (UInt64::IsUInt64(cx, obj)) {
      // Make sure the integer fits in IntegerType.
      JSUint64 i = Int64Base::GetInt(cx, obj);
      return ConvertExact(i, result);
    }

    if (Int64::IsInt64(cx, obj)) {
      // Make sure the integer fits in IntegerType.
      JSInt64 i = Int64Base::GetInt(cx, obj);
      return ConvertExact(i, result);
    }
  }
  return false;
}

// Implicitly convert val to a size value, where the size value is represented
// by size_t but must also fit in a jsdouble.
static bool
jsvalToSize(JSContext* cx, jsval val, bool allowString, size_t* result)
{
  if (!jsvalToBigInteger(cx, val, allowString, result))
    return false;

  // Also check that the result fits in a jsdouble.
  return Convert<size_t>(jsdouble(*result)) == *result;
}

// Implicitly convert val to IntegerType, allowing jsint, jsdouble,
// Int64, UInt64, and optionally a decimal or hexadecimal string argument.
// (This is common code shared by jsvalToSize and the Int64/UInt64 constructors.)
template<class IntegerType>
static bool
jsidToBigInteger(JSContext* cx,
                  jsid val,
                  bool allowString,
                  IntegerType* result)
{
  JS_STATIC_ASSERT(numeric_limits<IntegerType>::is_exact);

  if (JSID_IS_INT(val)) {
    // Make sure the integer fits in the alotted precision, and has the right
    // sign.
    jsint i = JSID_TO_INT(val);
    return ConvertExact(i, result);
  }
  if (allowString && JSID_IS_STRING(val)) {
    // Allow conversion from base-10 or base-16 strings, provided the result
    // fits in IntegerType. (This allows an Int64 or UInt64 object to be passed
    // to the JS array element operator, which will automatically call
    // toString() on the object for us.)
    return StringToInteger(cx, JSID_TO_STRING(val), result);
  }
  if (JSID_IS_OBJECT(val)) {
    // Allow conversion from an Int64 or UInt64 object directly.
    JSObject* obj = JSID_TO_OBJECT(val);

    if (UInt64::IsUInt64(cx, obj)) {
      // Make sure the integer fits in IntegerType.
      JSUint64 i = Int64Base::GetInt(cx, obj);
      return ConvertExact(i, result);
    }

    if (Int64::IsInt64(cx, obj)) {
      // Make sure the integer fits in IntegerType.
      JSInt64 i = Int64Base::GetInt(cx, obj);
      return ConvertExact(i, result);
    }
  }
  return false;
}

// Implicitly convert val to a size value, where the size value is represented
// by size_t but must also fit in a jsdouble.
static bool
jsidToSize(JSContext* cx, jsid val, bool allowString, size_t* result)
{
  if (!jsidToBigInteger(cx, val, allowString, result))
    return false;

  // Also check that the result fits in a jsdouble.
  return Convert<size_t>(jsdouble(*result)) == *result;
}

// Implicitly convert a size value to a jsval, ensuring that the size_t value
// fits in a jsdouble.
static JSBool
SizeTojsval(JSContext* cx, size_t size, jsval* result)
{
  if (Convert<size_t>(jsdouble(size)) != size) {
    JS_ReportError(cx, "size overflow");
    return false;
  }

  return JS_NewNumberValue(cx, jsdouble(size), result);
}

// Forcefully convert val to IntegerType when explicitly requested.
template<class IntegerType>
static bool
jsvalToIntegerExplicit(JSContext* cx, jsval val, IntegerType* result)
{
  JS_STATIC_ASSERT(numeric_limits<IntegerType>::is_exact);

  if (JSVAL_IS_DOUBLE(val)) {
    // Convert -Inf, Inf, and NaN to 0; otherwise, convert by C-style cast.
    jsdouble d = JSVAL_TO_DOUBLE(val);
    *result = FloatIsFinite(d) ? IntegerType(d) : 0;
    return true;
  }
  if (!JSVAL_IS_PRIMITIVE(val)) {
    // Convert Int64 and UInt64 values by C-style cast.
    JSObject* obj = JSVAL_TO_OBJECT(val);
    if (Int64::IsInt64(cx, obj)) {
      JSInt64 i = Int64Base::GetInt(cx, obj);
      *result = IntegerType(i);
      return true;
    }
    if (UInt64::IsUInt64(cx, obj)) {
      JSUint64 i = Int64Base::GetInt(cx, obj);
      *result = IntegerType(i);
      return true;
    }
  }
  return false;
}

// Forcefully convert val to a pointer value when explicitly requested.
static bool
jsvalToPtrExplicit(JSContext* cx, jsval val, uintptr_t* result)
{
  if (JSVAL_IS_INT(val)) {
    // jsint always fits in intptr_t. If the integer is negative, cast through
    // an intptr_t intermediate to sign-extend.
    jsint i = JSVAL_TO_INT(val);
    *result = i < 0 ? uintptr_t(intptr_t(i)) : uintptr_t(i);
    return true;
  }
  if (JSVAL_IS_DOUBLE(val)) {
    jsdouble d = JSVAL_TO_DOUBLE(val);
    if (d < 0) {
      // Cast through an intptr_t intermediate to sign-extend.
      intptr_t i = Convert<intptr_t>(d);
      if (jsdouble(i) != d)
        return false;

      *result = uintptr_t(i);
      return true;
    }

    // Don't silently lose bits here -- check that val really is an
    // integer value, and has the right sign.
    *result = Convert<uintptr_t>(d);
    return jsdouble(*result) == d;
  }
  if (!JSVAL_IS_PRIMITIVE(val)) {
    JSObject* obj = JSVAL_TO_OBJECT(val);
    if (Int64::IsInt64(cx, obj)) {
      JSInt64 i = Int64Base::GetInt(cx, obj);
      intptr_t p = intptr_t(i);

      // Make sure the integer fits in the alotted precision.
      if (JSInt64(p) != i)
        return false;
      *result = uintptr_t(p);
      return true;
    }

    if (UInt64::IsUInt64(cx, obj)) {
      JSUint64 i = Int64Base::GetInt(cx, obj);

      // Make sure the integer fits in the alotted precision.
      *result = uintptr_t(i);
      return JSUint64(*result) == i;
    }
  }
  return false;
}

template<class IntegerType, class CharType, size_t N, class AP>
void
IntegerToString(IntegerType i, jsuint radix, Vector<CharType, N, AP>& result)
{
  JS_STATIC_ASSERT(numeric_limits<IntegerType>::is_exact);

  // The buffer must be big enough for all the bits of IntegerType to fit,
  // in base-2, including '-'.
  CharType buffer[sizeof(IntegerType) * 8 + 1];
  CharType* end = buffer + sizeof(buffer) / sizeof(CharType);
  CharType* cp = end;

  // Build the string in reverse. We use multiplication and subtraction
  // instead of modulus because that's much faster.
  const bool isNegative = IsNegative(i);
  size_t sign = isNegative ? -1 : 1;
  do {
    IntegerType ii = i / IntegerType(radix);
    size_t index = sign * size_t(i - ii * IntegerType(radix));
    *--cp = "0123456789abcdefghijklmnopqrstuvwxyz"[index];
    i = ii;
  } while (i != 0);

  if (isNegative)
    *--cp = '-';

  JS_ASSERT(cp >= buffer);
  result.append(cp, end);
}

template<class CharType>
static size_t
strnlen(const CharType* begin, size_t max)
{
  for (const CharType* s = begin; s != begin + max; ++s)
    if (*s == 0)
      return s - begin;

  return max;
}

// Convert C binary value 'data' of CType 'typeObj' to a JS primitive, where
// possible; otherwise, construct and return a CData object. The following
// semantics apply when constructing a CData object for return:
// * If 'wantPrimitive' is true, the caller indicates that 'result' must be
//   a JS primitive, and ConvertToJS will fail if 'result' would be a CData
//   object. Otherwise:
// * If a CData object 'parentObj' is supplied, the new CData object is
//   dependent on the given parent and its buffer refers to a slice of the
//   parent's buffer.
// * If 'parentObj' is null, the new CData object may or may not own its
//   resulting buffer depending on the 'ownResult' argument.
JSBool
ConvertToJS(JSContext* cx,
            JSObject* typeObj,
            JSObject* parentObj,
            void* data,
            bool wantPrimitive,
            bool ownResult,
            jsval* result)
{
  JS_ASSERT(!parentObj || CData::IsCData(cx, parentObj));
  JS_ASSERT(!parentObj || !ownResult);
  JS_ASSERT(!wantPrimitive || !ownResult);

  TypeCode typeCode = CType::GetTypeCode(cx, typeObj);

  switch (typeCode) {
  case TYPE_void_t:
    *result = JSVAL_VOID;
    break;
  case TYPE_bool:
    *result = *static_cast<bool*>(data) ? JSVAL_TRUE : JSVAL_FALSE;
    break;
#define DEFINE_INT_TYPE(name, type, ffiType)                                   \
  case TYPE_##name: {                                                          \
    type value = *static_cast<type*>(data);                                    \
    if (sizeof(type) < 4)                                                      \
      *result = INT_TO_JSVAL(jsint(value));                                    \
    else if (!JS_NewNumberValue(cx, jsdouble(value), result))                  \
      return false;                                                            \
    break;                                                                     \
  }
#define DEFINE_WRAPPED_INT_TYPE(name, type, ffiType)                           \
  case TYPE_##name: {                                                          \
    /* Return an Int64 or UInt64 object - do not convert to a JS number. */    \
    JSUint64 value;                                                            \
    JSObject* proto;                                                           \
    if (!numeric_limits<type>::is_signed) {                                    \
      value = *static_cast<type*>(data);                                       \
      /* Get ctypes.UInt64.prototype from ctypes.CType.prototype. */           \
      proto = CType::GetProtoFromType(cx, typeObj, SLOT_UINT64PROTO);          \
    } else {                                                                   \
      value = JSInt64(*static_cast<type*>(data));                              \
      /* Get ctypes.Int64.prototype from ctypes.CType.prototype. */            \
      proto = CType::GetProtoFromType(cx, typeObj, SLOT_INT64PROTO);           \
    }                                                                          \
                                                                               \
    JSObject* obj = Int64Base::Construct(cx, proto, value,                     \
      !numeric_limits<type>::is_signed);                                       \
    if (!obj)                                                                  \
      return false;                                                            \
    *result = OBJECT_TO_JSVAL(obj);                                            \
    break;                                                                     \
  }
#define DEFINE_FLOAT_TYPE(name, type, ffiType)                                 \
  case TYPE_##name: {                                                          \
    type value = *static_cast<type*>(data);                                    \
    if (!JS_NewNumberValue(cx, jsdouble(value), result))                       \
      return false;                                                            \
    break;                                                                     \
  }
#define DEFINE_CHAR_TYPE(name, type, ffiType)                                  \
  case TYPE_##name:                                                            \
    /* Convert to an integer. We have no idea what character encoding to */    \
    /* use, if any. */                                                         \
    *result = INT_TO_JSVAL(*static_cast<type*>(data));                         \
    break;
#include "typedefs.h"
  case TYPE_jschar: {
    // Convert the jschar to a 1-character string.
    JSString* str = JS_NewUCStringCopyN(cx, static_cast<jschar*>(data), 1);
    if (!str)
      return false;

    *result = STRING_TO_JSVAL(str);
    break;
  }
  case TYPE_pointer:
  case TYPE_array:
  case TYPE_struct: {
    // We're about to create a new CData object to return. If the caller doesn't
    // want this, return early.
    if (wantPrimitive) {
      JS_ReportError(cx, "cannot convert to primitive value");
      return false;
    }

    JSObject* obj = CData::Create(cx, typeObj, parentObj, data, ownResult);
    if (!obj)
      return false;

    *result = OBJECT_TO_JSVAL(obj);
    break;
  }
  case TYPE_function:
    JS_NOT_REACHED("cannot return a FunctionType");
  }

  return true;
}

// Implicitly convert jsval 'val' to a C binary representation of CType
// 'targetType', storing the result in 'buffer'. Adequate space must be
// provided in 'buffer' by the caller. This function generally does minimal
// coercion between types. There are two cases in which this function is used:
// 1) The target buffer is internal to a CData object; we simply write data
//    into it.
// 2) We are converting an argument for an ffi call, in which case 'isArgument'
//    will be true. This allows us to handle a special case: if necessary,
//    we can autoconvert a JS string primitive to a pointer-to-character type.
//    In this case, ownership of the allocated string is handed off to the
//    caller; 'freePointer' will be set to indicate this.
JSBool
ImplicitConvert(JSContext* cx,
                jsval val,
                JSObject* targetType,
                void* buffer,
                bool isArgument,
                bool* freePointer)
{
  JS_ASSERT(CType::IsSizeDefined(cx, targetType));

  // First, check if val is a CData object of type targetType.
  JSObject* sourceData = NULL;
  JSObject* sourceType = NULL;
  if (!JSVAL_IS_PRIMITIVE(val) &&
      CData::IsCData(cx, JSVAL_TO_OBJECT(val))) {
    sourceData = JSVAL_TO_OBJECT(val);
    sourceType = CData::GetCType(cx, sourceData);

    // If the types are equal, copy the buffer contained within the CData.
    // (Note that the buffers may overlap partially or completely.)
    if (CType::TypesEqual(cx, sourceType, targetType)) {
      size_t size = CType::GetSize(cx, sourceType);
      memmove(buffer, CData::GetData(cx, sourceData), size);
      return true;
    }
  }

  TypeCode targetCode = CType::GetTypeCode(cx, targetType);

  switch (targetCode) {
  case TYPE_bool: {
    // Do not implicitly lose bits, but allow the values 0, 1, and -0.
    // Programs can convert explicitly, if needed, using `Boolean(v)` or `!!v`.
    bool result;
    if (!jsvalToBool(cx, val, &result))
      return TypeError(cx, "boolean", val);
    *static_cast<bool*>(buffer) = result;
    break;
  }
#define DEFINE_INT_TYPE(name, type, ffiType)                                   \
  case TYPE_##name: {                                                          \
    /* Do not implicitly lose bits. */                                         \
    type result;                                                               \
    if (!jsvalToInteger(cx, val, &result))                                     \
      return TypeError(cx, #name, val);                                        \
    *static_cast<type*>(buffer) = result;                                      \
    break;                                                                     \
  }
#define DEFINE_WRAPPED_INT_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_FLOAT_TYPE(name, type, ffiType)                                 \
  case TYPE_##name: {                                                          \
    type result;                                                               \
    if (!jsvalToFloat(cx, val, &result))                                       \
      return TypeError(cx, #name, val);                                        \
    *static_cast<type*>(buffer) = result;                                      \
    break;                                                                     \
  }
#define DEFINE_CHAR_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_JSCHAR_TYPE(name, type, ffiType)                                \
  case TYPE_##name: {                                                          \
    /* Convert from a 1-character string, regardless of encoding, */           \
    /* or from an integer, provided the result fits in 'type'. */              \
    type result;                                                               \
    if (JSVAL_IS_STRING(val)) {                                                \
      JSString* str = JSVAL_TO_STRING(val);                                    \
      if (str->length() != 1)                                                  \
        return TypeError(cx, #name, val);                                      \
                                                                               \
      result = str->chars()[0];                                                \
                                                                               \
    } else if (!jsvalToInteger(cx, val, &result)) {                            \
      return TypeError(cx, #name, val);                                        \
    }                                                                          \
    *static_cast<type*>(buffer) = result;                                      \
    break;                                                                     \
  }
#include "typedefs.h"
  case TYPE_pointer: {
    if (JSVAL_IS_NULL(val)) {
      // Convert to a null pointer.
      *static_cast<void**>(buffer) = NULL;
      break;
    }

    JSObject* baseType = PointerType::GetBaseType(cx, targetType);
    if (sourceData) {
      // First, determine if the targetType is ctypes.void_t.ptr.
      TypeCode sourceCode = CType::GetTypeCode(cx, sourceType);
      void* sourceBuffer = CData::GetData(cx, sourceData);
      bool voidptrTarget = CType::GetTypeCode(cx, baseType) == TYPE_void_t;

      if (sourceCode == TYPE_pointer && voidptrTarget) {
        // Autoconvert if targetType is ctypes.voidptr_t.
        *static_cast<void**>(buffer) = *static_cast<void**>(sourceBuffer);
        break;
      }
      if (sourceCode == TYPE_array) {
        // Autoconvert an array to a ctypes.void_t.ptr or to
        // sourceType.elementType.ptr, just like C.
        JSObject* elementType = ArrayType::GetBaseType(cx, sourceType);
        if (voidptrTarget || CType::TypesEqual(cx, baseType, elementType)) {
          *static_cast<void**>(buffer) = sourceBuffer;
          break;
        }
      }

    } else if (isArgument && JSVAL_IS_STRING(val)) {
      // Convert the string for the ffi call. This requires allocating space
      // which the caller assumes ownership of.
      // TODO: Extend this so we can safely convert strings at other times also.
      JSString* sourceString = JSVAL_TO_STRING(val);
      const jschar* sourceChars = sourceString->chars();
      size_t sourceLength = sourceString->length();

      switch (CType::GetTypeCode(cx, baseType)) {
      case TYPE_char:
      case TYPE_signed_char:
      case TYPE_unsigned_char: {
        // Convert from UTF-16 to UTF-8.
        size_t nbytes =
          js_GetDeflatedUTF8StringLength(cx, sourceChars, sourceLength);
        if (nbytes == (size_t) -1)
          return false;

        char** charBuffer = static_cast<char**>(buffer);
        *charBuffer = new char[nbytes + 1];
        if (!*charBuffer) {
          JS_ReportAllocationOverflow(cx);
          return false;
        }

        ASSERT_OK(js_DeflateStringToUTF8Buffer(cx, sourceChars, sourceLength,
                    *charBuffer, &nbytes));
        (*charBuffer)[nbytes] = 0;
        *freePointer = true;
        break;
      }
      case TYPE_jschar: {
        // Copy the jschar string data. (We could provide direct access to the
        // JSString's buffer, but this approach is safer if the caller happens
        // to modify the string.)
        jschar** jscharBuffer = static_cast<jschar**>(buffer);
        *jscharBuffer = new jschar[sourceLength + 1];
        if (!*jscharBuffer) {
          JS_ReportAllocationOverflow(cx);
          return false;
        }

        *freePointer = true;
        memcpy(*jscharBuffer, sourceChars, sourceLength * sizeof(jschar));
        (*jscharBuffer)[sourceLength] = 0;
        break;
      }
      default:
        return TypeError(cx, "pointer", val);
      }
      break;
    }
    return TypeError(cx, "pointer", val);
  }
  case TYPE_array: {
    JSObject* baseType = ArrayType::GetBaseType(cx, targetType);
    size_t targetLength = ArrayType::GetLength(cx, targetType);

    if (JSVAL_IS_STRING(val)) {
      JSString* sourceString = JSVAL_TO_STRING(val);
      const jschar* sourceChars = sourceString->chars();
      size_t sourceLength = sourceString->length();

      switch (CType::GetTypeCode(cx, baseType)) {
      case TYPE_char:
      case TYPE_signed_char:
      case TYPE_unsigned_char: {
        // Convert from UTF-16 to UTF-8.
        size_t nbytes =
          js_GetDeflatedUTF8StringLength(cx, sourceChars, sourceLength);
        if (nbytes == (size_t) -1)
          return false;

        if (targetLength < nbytes) {
          JS_ReportError(cx, "ArrayType has insufficient length");
          return false;
        }

        char* charBuffer = static_cast<char*>(buffer);
        ASSERT_OK(js_DeflateStringToUTF8Buffer(cx, sourceChars, sourceLength,
                    charBuffer, &nbytes));

        if (targetLength > nbytes)
          charBuffer[nbytes] = 0;

        break;
      }
      case TYPE_jschar: {
        // Copy the string data, jschar for jschar, including the terminator
        // if there's space.
        if (targetLength < sourceLength) {
          JS_ReportError(cx, "ArrayType has insufficient length");
          return false;
        }

        memcpy(buffer, sourceChars, sourceLength * sizeof(jschar));
        if (targetLength > sourceLength)
          static_cast<jschar*>(buffer)[sourceLength] = 0;

        break;
      }
      default:
        return TypeError(cx, "array", val);
      }

    } else if (!JSVAL_IS_PRIMITIVE(val) &&
               JS_IsArrayObject(cx, JSVAL_TO_OBJECT(val))) {
      // Convert each element of the array by calling ImplicitConvert.
      JSObject* sourceArray = JSVAL_TO_OBJECT(val);
      jsuint sourceLength;
      if (!JS_GetArrayLength(cx, sourceArray, &sourceLength) ||
          targetLength != size_t(sourceLength)) {
        JS_ReportError(cx, "ArrayType length does not match source array length");
        return false;
      }

      // Convert into an intermediate, in case of failure.
      size_t elementSize = CType::GetSize(cx, baseType);
      size_t arraySize = elementSize * targetLength;
      AutoPtr<char>::Array intermediate(new char[arraySize]);
      if (!intermediate) {
        JS_ReportAllocationOverflow(cx);
        return false;
      }

      for (jsuint i = 0; i < sourceLength; ++i) {
        js::AutoValueRooter item(cx);
        if (!JS_GetElement(cx, sourceArray, i, item.jsval_addr()))
          return false;

        char* data = intermediate.get() + elementSize * i;
        if (!ImplicitConvert(cx, item.jsval_value(), baseType, data, false, NULL))
          return false;
      }

      memcpy(buffer, intermediate.get(), arraySize);

    } else {
      // Don't implicitly convert to string. Users can implicitly convert
      // with `String(x)` or `""+x`.
      return TypeError(cx, "array", val);
    }
    break;
  }
  case TYPE_struct: {
    if (!JSVAL_IS_PRIMITIVE(val) && !sourceData) {
      // Enumerate the properties of the object; if they match the struct
      // specification, convert the fields.
      JSObject* obj = JSVAL_TO_OBJECT(val);
      JSObject* iter = JS_NewPropertyIterator(cx, obj);
      if (!iter)
        return false;
      js::AutoObjectRooter iterroot(cx, iter);

      // Convert into an intermediate, in case of failure.
      size_t structSize = CType::GetSize(cx, targetType);
      AutoPtr<char>::Array intermediate(new char[structSize]);
      if (!intermediate) {
        JS_ReportAllocationOverflow(cx);
        return false;
      }

      jsid id;
      size_t i = 0;
      while (1) {
        if (!JS_NextProperty(cx, iter, &id))
          return false;
        if (JSID_IS_VOID(id))
          break;

        js::AutoValueRooter fieldVal(cx);
        JS_IdToValue(cx, id, fieldVal.jsval_addr());
        if (!JSVAL_IS_STRING(fieldVal.jsval_value())) {
          JS_ReportError(cx, "property name is not a string");
          return false;
        }

        const FieldInfo* field = StructType::LookupField(cx, targetType,
                                                         JSVAL_TO_STRING(fieldVal.jsval_value()));
        if (!field)
          return false;

        JSString* name = JSVAL_TO_STRING(fieldVal.jsval_value());
        js::AutoValueRooter prop(cx);
        if (!JS_GetUCProperty(cx, obj, name->chars(), name->length(), prop.jsval_addr()))
          return false;

        // Convert the field via ImplicitConvert().
        char* fieldData = intermediate.get() + field->mOffset;
        if (!ImplicitConvert(cx, prop.jsval_value(), field->mType, fieldData, false, NULL))
          return false;

        ++i;
      }

      const FieldInfoHash* fields = StructType::GetFieldInfo(cx, targetType);
      if (i != fields->count()) {
        JS_ReportError(cx, "missing fields");
        return false;
      }

      memcpy(buffer, intermediate.get(), structSize);
      break;
    }

    return TypeError(cx, "struct", val);
  }
  case TYPE_void_t:
  case TYPE_function:
    JS_NOT_REACHED("invalid type");
    return false;
  }

  return true;
}

// Convert jsval 'val' to a C binary representation of CType 'targetType',
// storing the result in 'buffer'. This function is more forceful than
// ImplicitConvert.
JSBool
ExplicitConvert(JSContext* cx, jsval val, JSObject* targetType, void* buffer)
{
  // If ImplicitConvert succeeds, use that result.
  if (ImplicitConvert(cx, val, targetType, buffer, false, NULL))
    return true;

  // If ImplicitConvert failed, and there is no pending exception, then assume
  // hard failure (out of memory, or some other similarly serious condition).
  // We store any pending exception in case we need to re-throw it.
  js::AutoValueRooter ex(cx);
  if (!JS_GetPendingException(cx, ex.jsval_addr()))
    return false;

  // Otherwise, assume soft failure. Clear the pending exception so that we
  // can throw a different one as required.
  JS_ClearPendingException(cx);

  TypeCode type = CType::GetTypeCode(cx, targetType);

  switch (type) {
  case TYPE_bool: {
    // Convert according to the ECMAScript ToBoolean() function.
    JSBool result;
    ASSERT_OK(JS_ValueToBoolean(cx, val, &result));
    *static_cast<bool*>(buffer) = result != JS_FALSE;
    break;
  }
#define DEFINE_INT_TYPE(name, type, ffiType)                                   \
  case TYPE_##name: {                                                          \
    /* Convert numeric values with a C-style cast, and */                      \
    /* allow conversion from a base-10 or base-16 string. */                   \
    type result;                                                               \
    if (!jsvalToIntegerExplicit(cx, val, &result) &&                           \
        (!JSVAL_IS_STRING(val) ||                                              \
         !StringToInteger(cx, JSVAL_TO_STRING(val), &result)))                 \
      return TypeError(cx, #name, val);                                        \
    *static_cast<type*>(buffer) = result;                                      \
    break;                                                                     \
  }
#define DEFINE_WRAPPED_INT_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_CHAR_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_JSCHAR_TYPE(x, y, z) DEFINE_CHAR_TYPE(x, y, z)
#include "typedefs.h"
  case TYPE_pointer: {
    // Convert a number, Int64 object, or UInt64 object to a pointer.
    uintptr_t result;
    if (!jsvalToPtrExplicit(cx, val, &result))
      return TypeError(cx, "pointer", val);
    *static_cast<uintptr_t*>(buffer) = result;
    break;
  }
  case TYPE_float32_t:
  case TYPE_float64_t:
  case TYPE_float:
  case TYPE_double:
  case TYPE_array:
  case TYPE_struct:
    // ImplicitConvert is sufficient. Re-throw the exception it generated.
    JS_SetPendingException(cx, ex.jsval_value());
    return false;
  case TYPE_void_t:
  case TYPE_function:
    JS_NOT_REACHED("invalid type");
    return false;
  }
  return true;
}

// Given a CType 'typeObj', generate a string describing the C type declaration
// corresponding to 'typeObj'. For instance, the CType constructed from
// 'ctypes.int32_t.ptr.array(4).ptr.ptr' will result in the type string
// 'int32_t*(**)[4]'.
static JSString*
BuildTypeName(JSContext* cx, JSObject* typeObj)
{
  AutoString result;

  // Walk the hierarchy of types, outermost to innermost, building up the type
  // string. This consists of the base type, which goes on the left.
  // Derived type modifiers (* and []) build from the inside outward, with
  // pointers on the left and arrays on the right. An excellent description
  // of the rules for building C type declarations can be found at:
  // http://unixwiz.net/techtips/reading-cdecl.html
  TypeCode prevGrouping = CType::GetTypeCode(cx, typeObj), currentGrouping;
  while (1) {
    currentGrouping = CType::GetTypeCode(cx, typeObj);
    switch (currentGrouping) {
    case TYPE_pointer: {
      // Pointer types go on the left.
      PrependString(result, "*");

      typeObj = PointerType::GetBaseType(cx, typeObj);
      prevGrouping = currentGrouping;
      continue;
    }
    case TYPE_array: {
      if (prevGrouping == TYPE_pointer) {
        // Outer type is pointer, inner type is array. Grouping is required.
        PrependString(result, "(");
        AppendString(result, ")");
      } 

      // Array types go on the right.
      AppendString(result, "[");
      size_t length;
      if (ArrayType::GetSafeLength(cx, typeObj, &length))
        IntegerToString(length, 10, result);

      AppendString(result, "]");

      typeObj = ArrayType::GetBaseType(cx, typeObj);
      prevGrouping = currentGrouping;
      continue;
    }
    case TYPE_function: {
      FunctionInfo* fninfo = FunctionType::GetFunctionInfo(cx, typeObj);

      // Add in the calling convention, if it's not cdecl.
      ABICode abi = GetABICode(cx, fninfo->mABI);
      if (abi == ABI_STDCALL)
        PrependString(result, "__stdcall ");
      else if (abi == ABI_WINAPI)
        PrependString(result, "WINAPI ");

      // Wrap the entire expression so far with parens.
      PrependString(result, "(");
      AppendString(result, ")");

      // Argument list goes on the right.
      AppendString(result, "(");
      for (size_t i = 0; i < fninfo->mArgTypes.length(); ++i) {
        JSString* argName = CType::GetName(cx, fninfo->mArgTypes[i]);
        AppendString(result, argName);
        if (i != fninfo->mArgTypes.length() - 1 ||
            fninfo->mIsVariadic)
          AppendString(result, ", ");
      }
      if (fninfo->mIsVariadic)
        AppendString(result, "...");
      AppendString(result, ")");

      // Set 'typeObj' to the return type, and let the loop process it.
      // 'prevGrouping' doesn't matter here, because functions cannot return
      // arrays -- thus the parenthetical rules don't get tickled.
      typeObj = fninfo->mReturnType;
      continue;
    }
    default:
      // Either a basic or struct type. Use the type's name as the base type.
      break;
    }
    break;
  }

  // Stick the base type and derived type parts together.
  JSString* baseName = CType::GetName(cx, typeObj);
  PrependString(result, baseName);
  return NewUCString(cx, result);
}

// Given a CType 'typeObj', generate a string 'result' such that 'eval(result)'
// would construct the same CType. If 'makeShort' is true, assume that any
// StructType 't' is bound to an in-scope variable of name 't.name', and use
// that variable in place of generating a string to construct the type 't'.
// (This means the type comparison function CType::TypesEqual will return true
// when comparing the input and output of BuildTypeSource, since struct
// equality is determined by strict JSObject pointer equality.)
static void
BuildTypeSource(JSContext* cx,
                JSObject* typeObj, 
                bool makeShort, 
                AutoString& result)
{
  // Walk the types, building up the toSource() string.
  switch (CType::GetTypeCode(cx, typeObj)) {
  case TYPE_void_t:
#define DEFINE_TYPE(name, type, ffiType)  \
  case TYPE_##name:
#include "typedefs.h"
  {
    AppendString(result, "ctypes.");
    JSString* nameStr = CType::GetName(cx, typeObj);
    AppendString(result, nameStr);
    break;
  }
  case TYPE_pointer: {
    JSObject* baseType = PointerType::GetBaseType(cx, typeObj);

    // Specialcase ctypes.voidptr_t.
    if (CType::GetTypeCode(cx, baseType) == TYPE_void_t) {
      AppendString(result, "ctypes.voidptr_t");
      break;
    }

    // Recursively build the source string, and append '.ptr'.
    BuildTypeSource(cx, baseType, makeShort, result);
    AppendString(result, ".ptr");
    break;
  }
  case TYPE_function: {
    FunctionInfo* fninfo = FunctionType::GetFunctionInfo(cx, typeObj);

    AppendString(result, "ctypes.FunctionType(");

    switch (GetABICode(cx, fninfo->mABI)) {
    case ABI_DEFAULT:
      AppendString(result, "ctypes.default_abi, ");
      break;
    case ABI_STDCALL:
      AppendString(result, "ctypes.stdcall_abi, ");
      break;
    case ABI_WINAPI:
      AppendString(result, "ctypes.winapi_abi, ");
      break;
    case INVALID_ABI:
      JS_NOT_REACHED("invalid abi");
      break;
    }

    // Recursively build the source string describing the function return and
    // argument types.
    BuildTypeSource(cx, fninfo->mReturnType, true, result);

    if (fninfo->mArgTypes.length() > 0) {
      AppendString(result, ", [");
      for (size_t i = 0; i < fninfo->mArgTypes.length(); ++i) {
        BuildTypeSource(cx, fninfo->mArgTypes[i], true, result);
        if (i != fninfo->mArgTypes.length() - 1 ||
            fninfo->mIsVariadic)
          AppendString(result, ", ");
      }
      if (fninfo->mIsVariadic)
        AppendString(result, "\"...\"");
      AppendString(result, "]");
    }

    AppendString(result, ")");
    break;
  }
  case TYPE_array: {
    // Recursively build the source string, and append '.array(n)',
    // where n is the array length, or the empty string if the array length
    // is undefined.
    JSObject* baseType = ArrayType::GetBaseType(cx, typeObj);
    BuildTypeSource(cx, baseType, makeShort, result);
    AppendString(result, ".array(");

    size_t length;
    if (ArrayType::GetSafeLength(cx, typeObj, &length))
      IntegerToString(length, 10, result);

    AppendString(result, ")");
    break;
  }
  case TYPE_struct: {
    JSString* name = CType::GetName(cx, typeObj);

    if (makeShort) {
      // Shorten the type declaration by assuming that StructType 't' is bound
      // to an in-scope variable of name 't.name'.
      AppendString(result, name);
      break;
    }

    // Write the full struct declaration.
    AppendString(result, "ctypes.StructType(\"");
    AppendString(result, name);
    AppendString(result, "\"");

    // If it's an opaque struct, we're done.
    if (!CType::IsSizeDefined(cx, typeObj)) {
      AppendString(result, ")");
      break;
    }

    AppendString(result, ", [");

    const FieldInfoHash* fields = StructType::GetFieldInfo(cx, typeObj);
    size_t length = fields->count();
    Array<const FieldInfoHash::Entry*, 64> fieldsArray;
    if (!fieldsArray.resize(length))
      break;

    for (FieldInfoHash::Range r = fields->all(); !r.empty(); r.popFront())
      fieldsArray[r.front().value.mIndex] = &r.front();

    for (size_t i = 0; i < length; ++i) {
      const FieldInfoHash::Entry* entry = fieldsArray[i];
      AppendString(result, "{ \"");
      AppendString(result, entry->key);
      AppendString(result, "\": ");
      BuildTypeSource(cx, entry->value.mType, true, result);
      AppendString(result, " }");
      if (i != length - 1)
        AppendString(result, ", ");
    }

    AppendString(result, "])");
    break;
  }
  }
}

// Given a CData object of CType 'typeObj' with binary value 'data', generate a
// string 'result' such that 'eval(result)' would construct a CData object with
// the same CType and containing the same binary value. This assumes that any
// StructType 't' is bound to an in-scope variable of name 't.name'. (This means
// the type comparison function CType::TypesEqual will return true when
// comparing the types, since struct equality is determined by strict JSObject
// pointer equality.) Further, if 'isImplicit' is true, ensure that the
// resulting string can ImplicitConvert successfully if passed to another data
// constructor. (This is important when called recursively, since fields of
// structs and arrays are converted with ImplicitConvert.)
static JSBool
BuildDataSource(JSContext* cx,
                JSObject* typeObj, 
                void* data, 
                bool isImplicit, 
                AutoString& result)
{
  TypeCode type = CType::GetTypeCode(cx, typeObj);
  switch (type) {
  case TYPE_bool:
    if (*static_cast<bool*>(data))
      AppendString(result, "true");
    else
      AppendString(result, "false");
    break;
#define DEFINE_INT_TYPE(name, type, ffiType)                                   \
  case TYPE_##name:                                                            \
    /* Serialize as a primitive decimal integer. */                            \
    IntegerToString(*static_cast<type*>(data), 10, result);                    \
    break;
#define DEFINE_WRAPPED_INT_TYPE(name, type, ffiType)                           \
  case TYPE_##name:                                                            \
    /* Serialize as a wrapped decimal integer. */                              \
    if (!numeric_limits<type>::is_signed)                                      \
      AppendString(result, "ctypes.UInt64(\"");                                \
    else                                                                       \
      AppendString(result, "ctypes.Int64(\"");                                 \
                                                                               \
    IntegerToString(*static_cast<type*>(data), 10, result);                    \
    AppendString(result, "\")");                                               \
    break;
#define DEFINE_FLOAT_TYPE(name, type, ffiType)                                 \
  case TYPE_##name: {                                                          \
    /* Serialize as a primitive double. */                                     \
    double fp = *static_cast<type*>(data);                                     \
    ToCStringBuf cbuf;                                                         \
    char* str = NumberToCString(cx, &cbuf, fp);                                \
    if (!str) {                                                                \
      JS_ReportOutOfMemory(cx);                                                \
      return false;                                                            \
    }                                                                          \
                                                                               \
    result.append(str, strlen(str));                                           \
    break;                                                                     \
  }
#define DEFINE_CHAR_TYPE(name, type, ffiType)                                  \
  case TYPE_##name:                                                            \
    /* Serialize as an integer. */                                             \
    IntegerToString(*static_cast<type*>(data), 10, result);                    \
    break;
#include "typedefs.h"
  case TYPE_jschar: {
    // Serialize as a 1-character JS string.
    JSString* str = JS_NewUCStringCopyN(cx, static_cast<jschar*>(data), 1);
    if (!str)
      return false;

    // Escape characters, and quote as necessary.
    JSString* src = JS_ValueToSource(cx, STRING_TO_JSVAL(str));
    if (!src)
      return false;

    AppendString(result, src);
    break;
  }
  case TYPE_pointer:
  case TYPE_function: {
    if (isImplicit) {
      // The result must be able to ImplicitConvert successfully.
      // Wrap in a type constructor, then serialize for ExplicitConvert.
      BuildTypeSource(cx, typeObj, true, result);
      AppendString(result, "(");
    }

    // Serialize the pointer value as a wrapped hexadecimal integer.
    uintptr_t ptr = *static_cast<uintptr_t*>(data);
    AppendString(result, "ctypes.UInt64(\"0x");
    IntegerToString(ptr, 16, result);
    AppendString(result, "\")");

    if (isImplicit)
      AppendString(result, ")");

    break;
  }
  case TYPE_array: {
    // Serialize each element of the array recursively. Each element must
    // be able to ImplicitConvert successfully.
    JSObject* baseType = ArrayType::GetBaseType(cx, typeObj);
    AppendString(result, "[");

    size_t length = ArrayType::GetLength(cx, typeObj);
    size_t elementSize = CType::GetSize(cx, baseType);
    for (size_t i = 0; i < length; ++i) {
      char* element = static_cast<char*>(data) + elementSize * i;
      if (!BuildDataSource(cx, baseType, element, true, result))
        return false;

      if (i + 1 < length)
        AppendString(result, ", ");
    }
    AppendString(result, "]");
    break;
  }
  case TYPE_struct: {
    if (isImplicit) {
      // The result must be able to ImplicitConvert successfully.
      // Serialize the data as an object with properties, rather than
      // a sequence of arguments to the StructType constructor.
      AppendString(result, "{");
    }

    // Serialize each field of the struct recursively. Each field must
    // be able to ImplicitConvert successfully.
    const FieldInfoHash* fields = StructType::GetFieldInfo(cx, typeObj);
    size_t length = fields->count();
    Array<const FieldInfoHash::Entry*, 64> fieldsArray;
    if (!fieldsArray.resize(length))
      return false;

    for (FieldInfoHash::Range r = fields->all(); !r.empty(); r.popFront())
      fieldsArray[r.front().value.mIndex] = &r.front();

    for (size_t i = 0; i < length; ++i) {
      const FieldInfoHash::Entry* entry = fieldsArray[i];

      if (isImplicit) {
        AppendString(result, "\"");
        AppendString(result, entry->key);
        AppendString(result, "\": ");
      }

      char* fieldData = static_cast<char*>(data) + entry->value.mOffset;
      if (!BuildDataSource(cx, entry->value.mType, fieldData, true, result))
        return false;

      if (i + 1 != length)
        AppendString(result, ", ");
    }

    if (isImplicit)
      AppendString(result, "}");

    break;
  }
  case TYPE_void_t:
    JS_NOT_REACHED("invalid type");
    break;
  }

  return true;
}

/*******************************************************************************
** JSAPI callback function implementations
*******************************************************************************/

JSBool
ConstructAbstract(JSContext* cx,
                  uintN argc,
                  jsval* vp)
{
  // Calling an abstract base class constructor is disallowed.
  JS_ReportError(cx, "cannot construct from abstract type");
  return JS_FALSE;
}

/*******************************************************************************
** CType implementation
*******************************************************************************/

JSBool
CType::ConstructData(JSContext* cx,
                     uintN argc,
                     jsval* vp)
{
  // get the callee object...
  JSObject* obj = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
  if (!CType::IsCType(cx, obj)) {
    JS_ReportError(cx, "not a CType");
    return JS_FALSE;
  }

  // How we construct the CData object depends on what type we represent.
  // An instance 'd' of a CData object of type 't' has:
  //   * [[Class]] "CData"
  //   * __proto__ === t.prototype
  switch (GetTypeCode(cx, obj)) {
  case TYPE_void_t:
    JS_ReportError(cx, "cannot construct from void_t");
    return JS_FALSE;
  case TYPE_function:
    JS_ReportError(cx, "cannot construct from FunctionType; use FunctionType.ptr instead");
    return JS_FALSE;
  case TYPE_pointer:
    return PointerType::ConstructData(cx, obj, argc, vp);
  case TYPE_array:
    return ArrayType::ConstructData(cx, obj, argc, vp);
  case TYPE_struct:
    return StructType::ConstructData(cx, obj, argc, vp);
  default:
    return ConstructBasic(cx, obj, argc, vp);
  }
}

JSBool
CType::ConstructBasic(JSContext* cx,
                      JSObject* obj,
                      uintN argc,
                      jsval* vp)
{
  if (argc > 1) {
    JS_ReportError(cx, "CType constructor takes zero or one argument");
    return JS_FALSE;
  }

  // construct a CData object
  JSObject* result = CData::Create(cx, obj, NULL, NULL, true);
  if (!result)
    return JS_FALSE;

  if (argc == 1) {
    if (!ExplicitConvert(cx, JS_ARGV(cx, vp)[0], obj, CData::GetData(cx, result)))
      return JS_FALSE;
  }

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
  return JS_TRUE;
}

JSObject*
CType::Create(JSContext* cx,
              JSObject* typeProto,
              JSObject* dataProto,
              TypeCode type,
              JSString* name,
              jsval size,
              jsval align,
              ffi_type* ffiType)
{
  JSObject* parent = JS_GetParent(cx, typeProto);
  JS_ASSERT(parent);

  // Create a CType object with the properties and slots common to all CTypes.
  // Each type object 't' has:
  //   * [[Class]] "CType"
  //   * __proto__ === 'typeProto'; one of ctypes.{CType,PointerType,ArrayType,
  //     StructType}.prototype
  //   * A constructor which creates and returns a CData object, containing
  //     binary data of the given type.
  //   * 'prototype' property:
  //     * [[Class]] "CDataProto"
  //     * __proto__ === 'dataProto'; an object containing properties and
  //       functions common to all CData objects of types derived from
  //       'typeProto'. (For instance, this could be ctypes.CData.prototype
  //       for simple types, or something representing structs for StructTypes.)
  //     * 'constructor' property === 't'
  //     * Additional properties specified by 'ps', as appropriate for the
  //       specific type instance 't'.
  JSObject* typeObj = JS_NewObject(cx, &sCTypeClass, typeProto, parent);
  if (!typeObj)
    return NULL;
  js::AutoObjectRooter root(cx, typeObj);

  // Set up the reserved slots.
  if (!JS_SetReservedSlot(cx, typeObj, SLOT_TYPECODE, INT_TO_JSVAL(type)) ||
      (ffiType && !JS_SetReservedSlot(cx, typeObj, SLOT_FFITYPE, PRIVATE_TO_JSVAL(ffiType))) ||
      (name && !JS_SetReservedSlot(cx, typeObj, SLOT_NAME, STRING_TO_JSVAL(name))) ||
      !JS_SetReservedSlot(cx, typeObj, SLOT_SIZE, size) ||
      !JS_SetReservedSlot(cx, typeObj, SLOT_ALIGN, align))
    return NULL;

  if (dataProto) {
    // Set up the 'prototype' and 'prototype.constructor' properties.
    JSObject* prototype = JS_NewObject(cx, &sCDataProtoClass, dataProto, parent);
    if (!prototype)
      return NULL;
    js::AutoObjectRooter protoroot(cx, prototype);

    if (!JS_DefineProperty(cx, prototype, "constructor", OBJECT_TO_JSVAL(typeObj),
           NULL, NULL, JSPROP_READONLY | JSPROP_PERMANENT))
      return NULL;

    // Set the 'prototype' object.
    if (//!JS_FreezeObject(cx, prototype) || // XXX fixme - see bug 541212!
        !JS_SetReservedSlot(cx, typeObj, SLOT_PROTO, OBJECT_TO_JSVAL(prototype)))
      return NULL;
  }

  if (!JS_FreezeObject(cx, typeObj))
    return NULL;

  // Assert a sanity check on size and alignment: size % alignment should always
  // be zero.
  JS_ASSERT_IF(IsSizeDefined(cx, typeObj),
               GetSize(cx, typeObj) % GetAlignment(cx, typeObj) == 0);

  return typeObj;
}

JSObject*
CType::DefineBuiltin(JSContext* cx,
                     JSObject* parent,
                     const char* propName,
                     JSObject* typeProto,
                     JSObject* dataProto,
                     const char* name,
                     TypeCode type,
                     jsval size,
                     jsval align,
                     ffi_type* ffiType)
{
  JSString* nameStr = JS_NewStringCopyZ(cx, name);
  if (!nameStr)
    return NULL;
  js::AutoStringRooter nameRoot(cx, nameStr);

  // Create a new CType object with the common properties and slots.
  JSObject* typeObj = Create(cx, typeProto, dataProto, type, nameStr, size,
                        align, ffiType);
  if (!typeObj)
    return NULL;

  // Define the CType as a 'propName' property on 'parent'.
  if (!JS_DefineProperty(cx, parent, propName, OBJECT_TO_JSVAL(typeObj),
         NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return NULL;

  return typeObj;
}

void
CType::Finalize(JSContext* cx, JSObject* obj)
{
  // Make sure our TypeCode slot is legit. If it's not, bail.
  jsval slot;
  if (!JS_GetReservedSlot(cx, obj, SLOT_TYPECODE, &slot) || JSVAL_IS_VOID(slot))
    return;

  // The contents of our slots depends on what kind of type we are.
  switch (TypeCode(JSVAL_TO_INT(slot))) {
  case TYPE_function: {
    // Free the FunctionInfo.
    ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_FNINFO, &slot));
    if (!JSVAL_IS_VOID(slot))
      delete static_cast<FunctionInfo*>(JSVAL_TO_PRIVATE(slot));
    break;
  }
  case TYPE_struct:
    // Free the FieldInfo array.
    ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_FIELDINFO, &slot));
    if (!JSVAL_IS_VOID(slot)) {
      void* info = JSVAL_TO_PRIVATE(slot);
      delete static_cast<FieldInfoHash*>(info);
    }

    // Fall through.
  case TYPE_array: {
    // Free the ffi_type info.
    jsval slot;
    ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_FFITYPE, &slot));
    if (!JSVAL_IS_VOID(slot)) {
      ffi_type* ffiType = static_cast<ffi_type*>(JSVAL_TO_PRIVATE(slot));
      delete[] ffiType->elements;
      delete ffiType;
    }

    break;
  }
  default:
    // Nothing to do here.
    break;
  }
}

void
CType::FinalizeProtoClass(JSContext* cx, JSObject* obj)
{
  // Finalize the CTypeProto class. The only important bit here is our
  // SLOT_CLOSURECX -- it contains the JSContext that was (lazily) instantiated
  // for use with FunctionType closures. And if we're here, in this finalizer,
  // we're guaranteed to not need it anymore. Note that this slot will only
  // be set for the object (of class CTypeProto) ctypes.FunctionType.prototype.
  jsval slot;
  if (!JS_GetReservedSlot(cx, obj, SLOT_CLOSURECX, &slot) || JSVAL_IS_VOID(slot))
    return;

  JSContext* closureCx = static_cast<JSContext*>(JSVAL_TO_PRIVATE(slot));
  JS_SetContextThread(closureCx);
  JS_DestroyContextNoGC(closureCx);
}

void
CType::Trace(JSTracer* trc, JSObject* obj)
{
  JSContext* cx = trc->context;

  // Make sure our TypeCode slot is legit. If it's not, bail.
  jsval slot;
  if (!JS_GetReservedSlot(cx, obj, SLOT_TYPECODE, &slot) || JSVAL_IS_VOID(slot))
    return;

  // The contents of our slots depends on what kind of type we are.
  switch (TypeCode(JSVAL_TO_INT(slot))) {
  case TYPE_struct: {
    ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_FIELDINFO, &slot));
    if (JSVAL_IS_VOID(slot))
      return;

    FieldInfoHash* fields =
      static_cast<FieldInfoHash*>(JSVAL_TO_PRIVATE(slot));
    for (FieldInfoHash::Range r = fields->all(); !r.empty(); r.popFront()) {
      JS_CALL_TRACER(trc, r.front().key, JSTRACE_STRING, "fieldName");
      JS_CALL_TRACER(trc, r.front().value.mType, JSTRACE_OBJECT, "fieldType");
    }

    break;
  }
  case TYPE_function: {
    // Check if we have a FunctionInfo.
    ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_FNINFO, &slot));
    if (JSVAL_IS_VOID(slot))
      return;

    FunctionInfo* fninfo = static_cast<FunctionInfo*>(JSVAL_TO_PRIVATE(slot));
    JS_ASSERT(fninfo);

    // Identify our objects to the tracer.
    JS_CALL_TRACER(trc, fninfo->mABI, JSTRACE_OBJECT, "abi");
    JS_CALL_TRACER(trc, fninfo->mReturnType, JSTRACE_OBJECT, "returnType");
    for (size_t i = 0; i < fninfo->mArgTypes.length(); ++i)
      JS_CALL_TRACER(trc, fninfo->mArgTypes[i], JSTRACE_OBJECT, "argType");

    break;
  }
  default:
    // Nothing to do here.
    break;
  }
}

bool
CType::IsCType(JSContext* cx, JSObject* obj)
{
  return JS_GET_CLASS(cx, obj) == &sCTypeClass;
}

TypeCode
CType::GetTypeCode(JSContext* cx, JSObject* typeObj)
{
  JS_ASSERT(IsCType(cx, typeObj));

  jsval result;
  ASSERT_OK(JS_GetReservedSlot(cx, typeObj, SLOT_TYPECODE, &result));
  return TypeCode(JSVAL_TO_INT(result));
}

bool
CType::TypesEqual(JSContext* cx, JSObject* t1, JSObject* t2)
{
  JS_ASSERT(IsCType(cx, t1) && IsCType(cx, t2));

  // Fast path: check for object equality.
  if (t1 == t2)
    return true;

  // First, perform shallow comparison.
  TypeCode c1 = GetTypeCode(cx, t1);
  TypeCode c2 = GetTypeCode(cx, t2);
  if (c1 != c2)
    return false;

  // Determine whether the types require shallow or deep comparison.
  switch (c1) {
  case TYPE_pointer: {
    // Compare base types.
    JSObject* b1 = PointerType::GetBaseType(cx, t1);
    JSObject* b2 = PointerType::GetBaseType(cx, t2);
    return TypesEqual(cx, b1, b2);
  }
  case TYPE_function: {
    FunctionInfo* f1 = FunctionType::GetFunctionInfo(cx, t1);
    FunctionInfo* f2 = FunctionType::GetFunctionInfo(cx, t2);

    // Compare abi, return type, and argument types.
    if (f1->mABI != f2->mABI)
      return false;

    if (!TypesEqual(cx, f1->mReturnType, f2->mReturnType))
      return false;

    if (f1->mArgTypes.length() != f2->mArgTypes.length())
      return false;

    if (f1->mIsVariadic != f2->mIsVariadic)
      return false;

    for (size_t i = 0; i < f1->mArgTypes.length(); ++i) {
      if (!TypesEqual(cx, f1->mArgTypes[i], f2->mArgTypes[i]))
        return false;
    }

    return true;
  }
  case TYPE_array: {
    // Compare length, then base types.
    // An undefined length array matches other undefined length arrays.
    size_t s1 = 0, s2 = 0;
    bool d1 = ArrayType::GetSafeLength(cx, t1, &s1);
    bool d2 = ArrayType::GetSafeLength(cx, t2, &s2);
    if (d1 != d2 || (d1 && s1 != s2))
      return false;

    JSObject* b1 = ArrayType::GetBaseType(cx, t1);
    JSObject* b2 = ArrayType::GetBaseType(cx, t2);
    return TypesEqual(cx, b1, b2);
  }
  case TYPE_struct:
    // Require exact type object equality.
    return false;
  default:
    // Shallow comparison is sufficient.
    return true;
  }
}

bool
CType::GetSafeSize(JSContext* cx, JSObject* obj, size_t* result)
{
  JS_ASSERT(CType::IsCType(cx, obj));

  jsval size;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_SIZE, &size));

  // The "size" property can be a jsint, a jsdouble, or JSVAL_VOID
  // (for arrays of undefined length), and must always fit in a size_t.
  if (JSVAL_IS_INT(size)) {
    *result = JSVAL_TO_INT(size);
    return true;
  }
  if (JSVAL_IS_DOUBLE(size)) {
    *result = Convert<size_t>(JSVAL_TO_DOUBLE(size));
    return true;
  }

  JS_ASSERT(JSVAL_IS_VOID(size));
  return false;
}

size_t
CType::GetSize(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(cx, obj));

  jsval size;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_SIZE, &size));

  JS_ASSERT(!JSVAL_IS_VOID(size));

  // The "size" property can be a jsint, a jsdouble, or JSVAL_VOID
  // (for arrays of undefined length), and must always fit in a size_t.
  // For callers who know it can never be JSVAL_VOID, return a size_t directly.
  if (JSVAL_IS_INT(size))
    return JSVAL_TO_INT(size);
  return Convert<size_t>(JSVAL_TO_DOUBLE(size));
}

bool
CType::IsSizeDefined(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(cx, obj));

  jsval size;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_SIZE, &size));

  // The "size" property can be a jsint, a jsdouble, or JSVAL_VOID
  // (for arrays of undefined length), and must always fit in a size_t.
  JS_ASSERT(JSVAL_IS_INT(size) || JSVAL_IS_DOUBLE(size) || JSVAL_IS_VOID(size));
  return !JSVAL_IS_VOID(size);
}

size_t
CType::GetAlignment(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(cx, obj));

  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_ALIGN, &slot));
  return static_cast<size_t>(JSVAL_TO_INT(slot));
}

ffi_type*
CType::GetFFIType(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(cx, obj));

  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_FFITYPE, &slot));

  if (!JSVAL_IS_VOID(slot)) {
    return static_cast<ffi_type*>(JSVAL_TO_PRIVATE(slot));
  }

  AutoPtr<ffi_type> result;
  switch (CType::GetTypeCode(cx, obj)) {
  case TYPE_array:
    result = ArrayType::BuildFFIType(cx, obj);
    break;

  case TYPE_struct:
    result = StructType::BuildFFIType(cx, obj);
    break;

  default:
    JS_NOT_REACHED("simple types must have an ffi_type");
  }

  if (!result ||
      !JS_SetReservedSlot(cx, obj, SLOT_FFITYPE, PRIVATE_TO_JSVAL(result.get())))
    return NULL;

  return result.forget();
}

JSString*
CType::GetName(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(cx, obj));

  jsval string;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_NAME, &string));
  if (JSVAL_IS_VOID(string)) {
    // Build the type name lazily.
    JSString* name = BuildTypeName(cx, obj);
    if (!name || !JS_SetReservedSlot(cx, obj, SLOT_NAME, STRING_TO_JSVAL(name)))
      return NULL;

    return name;
  }

  return JSVAL_TO_STRING(string);
}

JSObject*
CType::GetProtoFromCtor(JSContext* cx, JSObject* obj, CTypeProtoSlot slot)
{
  // Get ctypes.{Pointer,Array,Struct}Type.prototype from a reserved slot
  // on the type constructor.
  jsval protoslot;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_FN_CTORPROTO, &protoslot));
  JSObject* proto = JSVAL_TO_OBJECT(protoslot);
  JS_ASSERT(proto);
  JS_ASSERT(JS_GET_CLASS(cx, proto) == &sCTypeProtoClass);

  // Get the desired prototype.
  jsval result;
  ASSERT_OK(JS_GetReservedSlot(cx, proto, slot, &result));
  return JSVAL_TO_OBJECT(result);
}

JSObject*
CType::GetProtoFromType(JSContext* cx, JSObject* obj, CTypeProtoSlot slot)
{
  JS_ASSERT(IsCType(cx, obj));

  // Get the prototype of the type object.
  JSObject* proto = JS_GetPrototype(cx, obj);
  JS_ASSERT(proto);
  JS_ASSERT(JS_GET_CLASS(cx, proto) == &sCTypeProtoClass);

  // Get the requested ctypes.{Pointer,Array,Struct,Function}Type.prototype.
  jsval result;
  ASSERT_OK(JS_GetReservedSlot(cx, proto, slot, &result));
  return JSVAL_TO_OBJECT(result);
}

JSBool
CType::PrototypeGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!CType::IsCType(cx, obj)) {
    JS_ReportError(cx, "not a CType");
    return JS_FALSE;
  }

  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_PROTO, vp));
  JS_ASSERT(!JSVAL_IS_PRIMITIVE(*vp) || JSVAL_IS_VOID(*vp));
  return JS_TRUE;
}

JSBool
CType::NameGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!CType::IsCType(cx, obj)) {
    JS_ReportError(cx, "not a CType");
    return JS_FALSE;
  }

  JSString* name = CType::GetName(cx, obj);
  if (!name)
    return JS_FALSE;

  *vp = STRING_TO_JSVAL(name);
  return JS_TRUE;
}

JSBool
CType::SizeGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!CType::IsCType(cx, obj)) {
    JS_ReportError(cx, "not a CType");
    return JS_FALSE;
  }

  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_SIZE, vp));
  JS_ASSERT(JSVAL_IS_NUMBER(*vp) || JSVAL_IS_VOID(*vp));
  return JS_TRUE;
}

JSBool
CType::PtrGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!CType::IsCType(cx, obj)) {
    JS_ReportError(cx, "not a CType");
    return JS_FALSE;
  }

  JSObject* pointerType = PointerType::CreateInternal(cx, obj);
  if (!pointerType)
    return JS_FALSE;

  *vp = OBJECT_TO_JSVAL(pointerType);
  return JS_TRUE;
}

JSBool
CType::CreateArray(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* baseType = JS_THIS_OBJECT(cx, vp);
  if (!baseType || !CType::IsCType(cx, baseType)) {
    JS_ReportError(cx, "not a CType");
    return JS_FALSE;
  }

  // Construct and return a new ArrayType object.
  if (argc > 1) {
    JS_ReportError(cx, "array takes zero or one argument");
    return JS_FALSE;
  }

  // Convert the length argument to a size_t.
  jsval* argv = JS_ARGV(cx, vp);
  size_t length = 0;
  if (argc == 1 && !jsvalToSize(cx, argv[0], false, &length)) {
    JS_ReportError(cx, "argument must be a nonnegative integer");
    return JS_FALSE;
  }

  JSObject* result = ArrayType::CreateInternal(cx, baseType, length, argc == 1);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
  return JS_TRUE;
}

JSBool
CType::ToString(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj || !CType::IsCType(cx, obj)) {
    JS_ReportError(cx, "not a CType");
    return JS_FALSE;
  }

  AutoString type;
  AppendString(type, "type ");
  AppendString(type, GetName(cx, obj));

  JSString* result = NewUCString(cx, type);
  if (!result)
    return JS_FALSE;
  
  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(result));
  return JS_TRUE;
}

JSBool
CType::ToSource(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj || !CType::IsCType(cx, obj)) {
    JS_ReportError(cx, "not a CType");
    return JS_FALSE;
  }

  AutoString source;
  BuildTypeSource(cx, obj, false, source);
  JSString* result = NewUCString(cx, source);
  if (!result)
    return JS_FALSE;
  
  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(result));
  return JS_TRUE;
}

JSBool
CType::HasInstance(JSContext* cx, JSObject* obj, const jsval* v, JSBool* bp)
{
  JS_ASSERT(CType::IsCType(cx, obj));

  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_PROTO, &slot));
  JSObject* prototype = JSVAL_TO_OBJECT(slot);
  JS_ASSERT(prototype);
  JS_ASSERT(JS_GET_CLASS(cx, prototype) == &sCDataProtoClass);

  *bp = JS_FALSE;
  if (JSVAL_IS_PRIMITIVE(*v))
    return JS_TRUE;

  JSObject* proto = JSVAL_TO_OBJECT(*v);
  while ((proto = JS_GetPrototype(cx, proto))) {
    if (proto == prototype) {
      *bp = JS_TRUE;
      break;
    }
  }
  return JS_TRUE;
}

/*******************************************************************************
** PointerType implementation
*******************************************************************************/

JSBool
PointerType::Create(JSContext* cx, uintN argc, jsval* vp)
{
  // Construct and return a new PointerType object.
  if (argc != 1) {
    JS_ReportError(cx, "PointerType takes one argument");
    return JS_FALSE;
  }

  jsval arg = JS_ARGV(cx, vp)[0];
  if (JSVAL_IS_PRIMITIVE(arg) || !CType::IsCType(cx, JSVAL_TO_OBJECT(arg))) {
    JS_ReportError(cx, "first argument must be a CType");
    return JS_FALSE;
  }

  JSObject* result = CreateInternal(cx, JSVAL_TO_OBJECT(arg));
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
  return JS_TRUE;
}

JSObject*
PointerType::CreateInternal(JSContext* cx, JSObject* baseType)
{
  // check if we have a cached PointerType on our base CType.
  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, baseType, SLOT_PTR, &slot));
  if (!JSVAL_IS_VOID(slot))
    return JSVAL_TO_OBJECT(slot);

  // Get ctypes.PointerType.prototype and the common prototype for CData objects
  // of this type.
  JSObject* typeProto;
  JSObject* dataProto;
  typeProto = CType::GetProtoFromType(cx, baseType, SLOT_POINTERPROTO);
  dataProto = CType::GetProtoFromType(cx, baseType, SLOT_POINTERDATAPROTO);

  // Create a new CType object with the common properties and slots.
  JSObject* typeObj = CType::Create(cx, typeProto, dataProto, TYPE_pointer,
                        NULL, INT_TO_JSVAL(sizeof(void*)),
                        INT_TO_JSVAL(ffi_type_pointer.alignment),
                        &ffi_type_pointer);
  if (!typeObj)
    return NULL;
  js::AutoObjectRooter root(cx, typeObj);

  // Set the target type. (This will be 'null' for an opaque pointer type.)
  if (!JS_SetReservedSlot(cx, typeObj, SLOT_TARGET_T, OBJECT_TO_JSVAL(baseType)))
    return NULL;

  // Finally, cache our newly-created PointerType on our pointed-to CType.
  if (!JS_SetReservedSlot(cx, baseType, SLOT_PTR, OBJECT_TO_JSVAL(typeObj)))
    return NULL;

  return typeObj;
}

JSBool
PointerType::ConstructData(JSContext* cx,
                           JSObject* obj,
                           uintN argc,
                           jsval* vp)
{
  if (!CType::IsCType(cx, obj) || CType::GetTypeCode(cx, obj) != TYPE_pointer) {
    JS_ReportError(cx, "not a PointerType");
    return JS_FALSE;
  }

  if (argc > 2) {
    JS_ReportError(cx, "constructor takes 0, 1, or 2 arguments");
    return JS_FALSE;
  }

  JSObject* result = CData::Create(cx, obj, NULL, NULL, true);
  if (!result)
    return JS_FALSE;

  // Set return value early, must not observe *vp after
  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));

  if (argc == 0) {
    // Construct a null pointer.
    return JS_TRUE;
  }

  jsval* argv = JS_ARGV(cx, vp);
  if (argc >= 1) {
    JSObject* baseObj = PointerType::GetBaseType(cx, obj);
    if (CType::GetTypeCode(cx, baseObj) == TYPE_function &&
        JSVAL_IS_OBJECT(argv[0]) &&
        JS_ObjectIsFunction(cx, JSVAL_TO_OBJECT(argv[0]))) {
      // Construct a FunctionType.ptr from a JS function, and allow an
      // optional 'this' argument.
      JSObject* thisObj = NULL;
      if (argc == 2) {
        if (JSVAL_IS_OBJECT(argv[1])) {
          thisObj = JSVAL_TO_OBJECT(argv[1]);
        } else if (!JS_ValueToObject(cx, argv[1], &thisObj)) {
          return JS_FALSE;
        }
      }

      JSObject* fnObj = JSVAL_TO_OBJECT(argv[0]);
      return FunctionType::ConstructData(cx, baseObj, result, fnObj, thisObj);
    }

    if (argc == 2) {
      JS_ReportError(cx, "first argument must be a function");
      return JS_FALSE;
    }
  }

  // Construct from a raw pointer value.
  return ExplicitConvert(cx, argv[0], obj, CData::GetData(cx, result));
}

JSObject*
PointerType::GetBaseType(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::GetTypeCode(cx, obj) == TYPE_pointer);

  jsval type;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_TARGET_T, &type));
  JS_ASSERT(!JSVAL_IS_NULL(type));
  return JSVAL_TO_OBJECT(type);
}

JSBool
PointerType::TargetTypeGetter(JSContext* cx,
                              JSObject* obj,
                              jsid idval,
                              jsval* vp)
{
  if (!CType::IsCType(cx, obj) || CType::GetTypeCode(cx, obj) != TYPE_pointer) {
    JS_ReportError(cx, "not a PointerType");
    return JS_FALSE;
  }

  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_TARGET_T, vp));
  JS_ASSERT(JSVAL_IS_OBJECT(*vp));
  return JS_TRUE;
}

JSBool
PointerType::IsNull(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj || !CData::IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  // Get pointer type and base type.
  JSObject* typeObj = CData::GetCType(cx, obj);
  if (CType::GetTypeCode(cx, typeObj) != TYPE_pointer) {
    JS_ReportError(cx, "not a PointerType");
    return JS_FALSE;
  }

  void* data = *static_cast<void**>(CData::GetData(cx, obj));
  jsval result = BOOLEAN_TO_JSVAL(data == NULL);
  JS_SET_RVAL(cx, vp, result);
  return JS_TRUE;
}

JSBool
PointerType::ContentsGetter(JSContext* cx,
                            JSObject* obj,
                            jsid idval,
                            jsval* vp)
{
  if (!CData::IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  // Get pointer type and base type.
  JSObject* typeObj = CData::GetCType(cx, obj);
  if (CType::GetTypeCode(cx, typeObj) != TYPE_pointer) {
    JS_ReportError(cx, "not a PointerType");
    return JS_FALSE;
  }

  JSObject* baseType = GetBaseType(cx, typeObj);
  if (!CType::IsSizeDefined(cx, baseType)) {
    JS_ReportError(cx, "cannot get contents of undefined size");
    return JS_FALSE;
  }

  void* data = *static_cast<void**>(CData::GetData(cx, obj));
  if (data == NULL) {
    JS_ReportError(cx, "cannot read contents of null pointer");
    return JS_FALSE;
  }

  jsval result;
  if (!ConvertToJS(cx, baseType, NULL, data, false, false, &result))
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, result);
  return JS_TRUE;
}

JSBool
PointerType::ContentsSetter(JSContext* cx,
                            JSObject* obj,
                            jsid idval,
                            jsval* vp)
{
  if (!CData::IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  // Get pointer type and base type.
  JSObject* typeObj = CData::GetCType(cx, obj);
  if (CType::GetTypeCode(cx, typeObj) != TYPE_pointer) {
    JS_ReportError(cx, "not a PointerType");
    return JS_FALSE;
  }

  JSObject* baseType = GetBaseType(cx, typeObj);
  if (!CType::IsSizeDefined(cx, baseType)) {
    JS_ReportError(cx, "cannot set contents of undefined size");
    return JS_FALSE;
  }

  void* data = *static_cast<void**>(CData::GetData(cx, obj));
  if (data == NULL) {
    JS_ReportError(cx, "cannot write contents to null pointer");
    return JS_FALSE;
  }

  return ImplicitConvert(cx, *vp, baseType, data, false, NULL);
}

/*******************************************************************************
** ArrayType implementation
*******************************************************************************/

JSBool
ArrayType::Create(JSContext* cx, uintN argc, jsval* vp)
{
  // Construct and return a new ArrayType object.
  if (argc < 1 || argc > 2) {
    JS_ReportError(cx, "ArrayType takes one or two arguments");
    return JS_FALSE;
  }

  jsval* argv = JS_ARGV(cx, vp);
  if (JSVAL_IS_PRIMITIVE(argv[0]) ||
      !CType::IsCType(cx, JSVAL_TO_OBJECT(argv[0]))) {
    JS_ReportError(cx, "first argument must be a CType");
    return JS_FALSE;
  }

  // Convert the length argument to a size_t.
  size_t length = 0;
  if (argc == 2 && !jsvalToSize(cx, argv[1], false, &length)) {
    JS_ReportError(cx, "second argument must be a nonnegative integer");
    return JS_FALSE;
  }

  JSObject* baseType = JSVAL_TO_OBJECT(argv[0]);
  JSObject* result = CreateInternal(cx, baseType, length, argc == 2);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
  return JS_TRUE;
}

JSObject*
ArrayType::CreateInternal(JSContext* cx,
                          JSObject* baseType,
                          size_t length,
                          bool lengthDefined)
{
  // Get ctypes.ArrayType.prototype and the common prototype for CData objects
  // of this type, from ctypes.CType.prototype.
  JSObject* typeProto = CType::GetProtoFromType(cx, baseType, SLOT_ARRAYPROTO);
  JSObject* dataProto = CType::GetProtoFromType(cx, baseType, SLOT_ARRAYDATAPROTO);

  // Determine the size of the array from the base type, if possible.
  // The size of the base type must be defined.
  // If our length is undefined, both our size and length will be undefined.
  size_t baseSize;
  if (!CType::GetSafeSize(cx, baseType, &baseSize)) {
    JS_ReportError(cx, "base size must be defined");
    return NULL;
  }

  jsval sizeVal = JSVAL_VOID;
  jsval lengthVal = JSVAL_VOID;
  if (lengthDefined) {
    // Check for overflow, and convert to a jsint or jsdouble as required.
    size_t size = length * baseSize;
    if (length > 0 && size / length != baseSize) {
      JS_ReportError(cx, "size overflow");
      return NULL;
    }
    if (!SizeTojsval(cx, size, &sizeVal) ||
        !SizeTojsval(cx, length, &lengthVal))
      return NULL;
  }

  size_t align = CType::GetAlignment(cx, baseType);

  // Create a new CType object with the common properties and slots.
  JSObject* typeObj = CType::Create(cx, typeProto, dataProto, TYPE_array, NULL,
                        sizeVal, INT_TO_JSVAL(align), NULL);
  if (!typeObj)
    return NULL;
  js::AutoObjectRooter root(cx, typeObj);

  // Set the element type.
  if (!JS_SetReservedSlot(cx, typeObj, SLOT_ELEMENT_T, OBJECT_TO_JSVAL(baseType)))
    return NULL;

  // Set the length.
  if (!JS_SetReservedSlot(cx, typeObj, SLOT_LENGTH, lengthVal))
    return NULL;

  return typeObj;
}

JSBool
ArrayType::ConstructData(JSContext* cx,
                         JSObject* obj,
                         uintN argc,
                         jsval* vp)
{
  if (!CType::IsCType(cx, obj) || CType::GetTypeCode(cx, obj) != TYPE_array) {
    JS_ReportError(cx, "not an ArrayType");
    return JS_FALSE;
  }

  // Decide whether we have an object to initialize from. We'll override this
  // if we get a length argument instead.
  bool convertObject = argc == 1;

  // Check if we're an array of undefined length. If we are, allow construction
  // with a length argument, or with an actual JS array.
  if (CType::IsSizeDefined(cx, obj)) {
    if (argc > 1) {
      JS_ReportError(cx, "constructor takes zero or one argument");
      return JS_FALSE;
    }

  } else {
    if (argc != 1) {
      JS_ReportError(cx, "constructor takes one argument");
      return JS_FALSE;
    }

    JSObject* baseType = GetBaseType(cx, obj);

    jsval* argv = JS_ARGV(cx, vp);
    size_t length;
    if (jsvalToSize(cx, argv[0], false, &length)) {
      // Have a length, rather than an object to initialize from.
      convertObject = false;

    } else if (!JSVAL_IS_PRIMITIVE(argv[0])) {
      // We were given an object with a .length property.
      // This could be a JS array, or a CData array.
      JSObject* arg = JSVAL_TO_OBJECT(argv[0]);
      js::AutoValueRooter lengthVal(cx);
      if (!JS_GetProperty(cx, arg, "length", lengthVal.jsval_addr()) ||
          !jsvalToSize(cx, lengthVal.jsval_value(), false, &length)) {
        JS_ReportError(cx, "argument must be an array object or length");
        return JS_FALSE;
      }

    } else if (JSVAL_IS_STRING(argv[0])) {
      // We were given a string. Size the array to the appropriate length,
      // including space for the terminator.
      JSString* sourceString = JSVAL_TO_STRING(argv[0]);
      const jschar* sourceChars = sourceString->chars();
      size_t sourceLength = sourceString->length();

      switch (CType::GetTypeCode(cx, baseType)) {
      case TYPE_char:
      case TYPE_signed_char:
      case TYPE_unsigned_char: {
        // Determine the UTF-8 length.
        length = js_GetDeflatedUTF8StringLength(cx, sourceChars, sourceLength);
        if (length == (size_t) -1)
          return false;

        ++length;
        break;
      }
      case TYPE_jschar:
        length = sourceLength + 1;
        break;
      default:
        return TypeError(cx, "array", argv[0]);
      }

    } else {
      JS_ReportError(cx, "argument must be an array object or length");
      return JS_FALSE;
    }

    // Construct a new ArrayType of defined length, for the new CData object.
    obj = CreateInternal(cx, baseType, length, true);
    if (!obj)
      return JS_FALSE;
  }

  // Root the CType object, in case we created one above.
  js::AutoObjectRooter root(cx, obj);

  JSObject* result = CData::Create(cx, obj, NULL, NULL, true);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));

  if (convertObject) {
    if (!ExplicitConvert(cx, JS_ARGV(cx, vp)[0], obj, CData::GetData(cx, result)))
      return JS_FALSE;
  }

  return JS_TRUE;
}

JSObject*
ArrayType::GetBaseType(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(cx, obj));
  JS_ASSERT(CType::GetTypeCode(cx, obj) == TYPE_array);

  jsval type;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_ELEMENT_T, &type));
  JS_ASSERT(!JSVAL_IS_NULL(type));
  return JSVAL_TO_OBJECT(type);
}

bool
ArrayType::GetSafeLength(JSContext* cx, JSObject* obj, size_t* result)
{
  JS_ASSERT(CType::IsCType(cx, obj));
  JS_ASSERT(CType::GetTypeCode(cx, obj) == TYPE_array);

  jsval length;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_LENGTH, &length));

  // The "length" property can be a jsint, a jsdouble, or JSVAL_VOID
  // (for arrays of undefined length), and must always fit in a size_t.
  if (JSVAL_IS_INT(length)) {
    *result = JSVAL_TO_INT(length);
    return true;
  }
  if (JSVAL_IS_DOUBLE(length)) {
    *result = Convert<size_t>(JSVAL_TO_DOUBLE(length));
    return true;
  }

  JS_ASSERT(JSVAL_IS_VOID(length));
  return false;
}

size_t
ArrayType::GetLength(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(cx, obj));
  JS_ASSERT(CType::GetTypeCode(cx, obj) == TYPE_array);

  jsval length;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_LENGTH, &length));

  JS_ASSERT(!JSVAL_IS_VOID(length));

  // The "length" property can be a jsint, a jsdouble, or JSVAL_VOID
  // (for arrays of undefined length), and must always fit in a size_t.
  // For callers who know it can never be JSVAL_VOID, return a size_t directly.
  if (JSVAL_IS_INT(length))
    return JSVAL_TO_INT(length);
  return Convert<size_t>(JSVAL_TO_DOUBLE(length));
}

ffi_type*
ArrayType::BuildFFIType(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(cx, obj));
  JS_ASSERT(CType::GetTypeCode(cx, obj) == TYPE_array);
  JS_ASSERT(CType::IsSizeDefined(cx, obj));

  JSObject* baseType = ArrayType::GetBaseType(cx, obj);
  ffi_type* ffiBaseType = CType::GetFFIType(cx, baseType);
  if (!ffiBaseType)
    return NULL;

  size_t length = ArrayType::GetLength(cx, obj);

  // Create an ffi_type to represent the array. This is necessary for the case
  // where the array is part of a struct. Since libffi has no intrinsic
  // support for array types, we approximate it by creating a struct type
  // with elements of type 'baseType' and with appropriate size and alignment
  // values. It would be nice to not do all the work of setting up 'elements',
  // but some libffi platforms currently require that it be meaningful. I'm
  // looking at you, x86_64.
  AutoPtr<ffi_type> ffiType(new ffi_type);
  if (!ffiType) {
    JS_ReportOutOfMemory(cx);
    return NULL;
  }

  ffiType->type = FFI_TYPE_STRUCT;
  ffiType->size = CType::GetSize(cx, obj);
  ffiType->alignment = CType::GetAlignment(cx, obj);
  ffiType->elements = new ffi_type*[length + 1];
  if (!ffiType->elements) {
    JS_ReportAllocationOverflow(cx);
    return NULL;
  }

  for (size_t i = 0; i < length; ++i)
    ffiType->elements[i] = ffiBaseType;
  ffiType->elements[length] = NULL;

  return ffiType.forget();
}

JSBool
ArrayType::ElementTypeGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!CType::IsCType(cx, obj) || CType::GetTypeCode(cx, obj) != TYPE_array) {
    JS_ReportError(cx, "not an ArrayType");
    return JS_FALSE;
  }

  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_ELEMENT_T, vp));
  JS_ASSERT(!JSVAL_IS_PRIMITIVE(*vp));
  return JS_TRUE;
}

JSBool
ArrayType::LengthGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  // This getter exists for both CTypes and CDatas of the ArrayType persuasion.
  // If we're dealing with a CData, get the CType from it.
  if (CData::IsCData(cx, obj))
    obj = CData::GetCType(cx, obj);

  if (!CType::IsCType(cx, obj) || CType::GetTypeCode(cx, obj) != TYPE_array) {
    JS_ReportError(cx, "not an ArrayType");
    return JS_FALSE;
  }

  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_LENGTH, vp));
  JS_ASSERT(JSVAL_IS_NUMBER(*vp) || JSVAL_IS_VOID(*vp));
  return JS_TRUE;
}

JSBool
ArrayType::Getter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  // This should never happen, but we'll check to be safe.
  if (!CData::IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  // Bail early if we're not an ArrayType. (This setter is present for all
  // CData, regardless of CType.)
  JSObject* typeObj = CData::GetCType(cx, obj);
  if (CType::GetTypeCode(cx, typeObj) != TYPE_array)
    return JS_TRUE;

  // Convert the index to a size_t and bounds-check it.
  size_t index;
  size_t length = GetLength(cx, typeObj);
  bool ok = jsidToSize(cx, idval, true, &index);
  if (!ok && JSID_IS_STRING(idval)) {
    // String either isn't a number, or doesn't fit in size_t.
    // Chances are it's a regular property lookup, so return.
    return JS_TRUE;
  }
  if (!ok || index >= length) {
    JS_ReportError(cx, "invalid index");
    return JS_FALSE;
  }

  JSObject* baseType = GetBaseType(cx, typeObj);
  size_t elementSize = CType::GetSize(cx, baseType);
  char* data = static_cast<char*>(CData::GetData(cx, obj)) + elementSize * index;
  return ConvertToJS(cx, baseType, obj, data, false, false, vp);
}

JSBool
ArrayType::Setter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  // This should never happen, but we'll check to be safe.
  if (!CData::IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  // Bail early if we're not an ArrayType. (This setter is present for all
  // CData, regardless of CType.)
  JSObject* typeObj = CData::GetCType(cx, obj);
  if (CType::GetTypeCode(cx, typeObj) != TYPE_array)
    return JS_TRUE;

  // Convert the index to a size_t and bounds-check it.
  size_t index;
  size_t length = GetLength(cx, typeObj);
  bool ok = jsidToSize(cx, idval, true, &index);
  if (!ok && JSID_IS_STRING(idval)) {
    // String either isn't a number, or doesn't fit in size_t.
    // Chances are it's a regular property lookup, so return.
    return JS_TRUE;
  }
  if (!ok || index >= length) {
    JS_ReportError(cx, "invalid index");
    return JS_FALSE;
  }

  JSObject* baseType = GetBaseType(cx, typeObj);
  size_t elementSize = CType::GetSize(cx, baseType);
  char* data = static_cast<char*>(CData::GetData(cx, obj)) + elementSize * index;
  return ImplicitConvert(cx, *vp, baseType, data, false, NULL);
}

JSBool
ArrayType::AddressOfElement(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj || !CData::IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  JSObject* typeObj = CData::GetCType(cx, obj);
  if (CType::GetTypeCode(cx, typeObj) != TYPE_array) {
    JS_ReportError(cx, "not an ArrayType");
    return JS_FALSE;
  }

  if (argc != 1) {
    JS_ReportError(cx, "addressOfElement takes one argument");
    return JS_FALSE;
  }

  JSObject* baseType = GetBaseType(cx, typeObj);
  JSObject* pointerType = PointerType::CreateInternal(cx, baseType);
  if (!pointerType)
    return JS_FALSE;
  js::AutoObjectRooter root(cx, pointerType);

  // Create a PointerType CData object containing null.
  JSObject* result = CData::Create(cx, pointerType, NULL, NULL, true);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));

  // Convert the index to a size_t and bounds-check it.
  size_t index;
  size_t length = GetLength(cx, typeObj);
  if (!jsvalToSize(cx, JS_ARGV(cx, vp)[0], false, &index) ||
      index >= length) {
    JS_ReportError(cx, "invalid index");
    return JS_FALSE;
  }

  // Manually set the pointer inside the object, so we skip the conversion step.
  void** data = static_cast<void**>(CData::GetData(cx, result));
  size_t elementSize = CType::GetSize(cx, baseType);
  *data = static_cast<char*>(CData::GetData(cx, obj)) + elementSize * index;
  return JS_TRUE;
}

/*******************************************************************************
** StructType implementation
*******************************************************************************/

// For a struct field descriptor 'val' of the form { name : type }, extract
// 'name' and 'type'.
static JSString*
ExtractStructField(JSContext* cx, jsval val, JSObject** typeObj)
{
  if (JSVAL_IS_PRIMITIVE(val)) {
    JS_ReportError(cx, "struct field descriptors require a valid name and type");
    return NULL;
  }

  JSObject* obj = JSVAL_TO_OBJECT(val);
  JSObject* iter = JS_NewPropertyIterator(cx, obj);
  if (!iter)
    return NULL;
  js::AutoObjectRooter iterroot(cx, iter);

  jsid id;
  if (!JS_NextProperty(cx, iter, &id))
    return NULL;
  if (JSID_IS_VOID(id)) {
    JS_ReportError(cx, "struct field descriptors require a valid name and type");
    return NULL;
  }

  js::AutoValueRooter nameVal(cx);
  JS_IdToValue(cx, id, nameVal.jsval_addr());
  if (!JSVAL_IS_STRING(nameVal.jsval_value())) {
    JS_ReportError(cx, "struct field descriptors require a valid name and type");
    return NULL;
  }
  JSString* name = JSVAL_TO_STRING(nameVal.jsval_value());

  // make sure we have one, and only one, property
  if (!JS_NextProperty(cx, iter, &id))
    return NULL;
  if (!JSID_IS_VOID(id)) {
    JS_ReportError(cx, "struct field descriptors must contain one property");
    return NULL;
  }

  js::AutoValueRooter propVal(cx);
  if (!JS_GetUCProperty(cx, obj, name->chars(), name->length(), propVal.jsval_addr()))
    return NULL;

  if (propVal.value().isPrimitive() ||
      !CType::IsCType(cx, JSVAL_TO_OBJECT(propVal.jsval_value()))) {
    JS_ReportError(cx, "struct field descriptors require a valid name and type");
    return NULL;
  }

  // Undefined size or zero size struct members are illegal.
  // (Zero-size arrays are legal as struct members in C++, but libffi will
  // choke on a zero-size struct, so we disallow them.)
  *typeObj = JSVAL_TO_OBJECT(propVal.jsval_value());
  size_t size;
  if (!CType::GetSafeSize(cx, *typeObj, &size) || size == 0) {
    JS_ReportError(cx, "struct field types must have defined and nonzero size");
    return NULL;
  }

  return name;
}

// For a struct field with 'name' and 'type', add an element of the form
// { name : type }.
static JSBool
AddFieldToArray(JSContext* cx,
                jsval* element,
                JSString* name,
                JSObject* typeObj)
{
  JSObject* fieldObj = JS_NewObject(cx, NULL, NULL, NULL);
  if (!fieldObj)
    return false;

  *element = OBJECT_TO_JSVAL(fieldObj);

  if (!JS_DefineUCProperty(cx, fieldObj,
         name->chars(), name->length(),
         OBJECT_TO_JSVAL(typeObj), NULL, NULL,
         JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  return JS_FreezeObject(cx, fieldObj);
}

JSBool
StructType::Create(JSContext* cx, uintN argc, jsval* vp)
{
  // Construct and return a new StructType object.
  if (argc < 1 || argc > 2) {
    JS_ReportError(cx, "StructType takes one or two arguments");
    return JS_FALSE;
  }

  jsval* argv = JS_ARGV(cx, vp);
  jsval name = argv[0];
  if (!JSVAL_IS_STRING(name)) {
    JS_ReportError(cx, "first argument must be a string");
    return JS_FALSE;
  }

  // Get ctypes.StructType.prototype from the ctypes.StructType constructor.
  JSObject* callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
  JSObject* typeProto = CType::GetProtoFromCtor(cx, callee, SLOT_STRUCTPROTO);

  // Create a simple StructType with no defined fields. The result will be
  // non-instantiable as CData, will have no 'prototype' property, and will
  // have undefined size and alignment and no ffi_type.
  JSObject* result = CType::Create(cx, typeProto, NULL, TYPE_struct,
                       JSVAL_TO_STRING(name), JSVAL_VOID, JSVAL_VOID, NULL);
  if (!result)
    return JS_FALSE;
  js::AutoObjectRooter root(cx, result);

  if (argc == 2) {
    if (JSVAL_IS_PRIMITIVE(argv[1]) ||
        !JS_IsArrayObject(cx, JSVAL_TO_OBJECT(argv[1]))) {
      JS_ReportError(cx, "second argument must be an array");
      return JS_FALSE;
    }

    // Define the struct fields.
    if (!DefineInternal(cx, result, JSVAL_TO_OBJECT(argv[1])))
      return JS_FALSE;
  }

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
  return JS_TRUE;
}

JSBool
StructType::DefineInternal(JSContext* cx, JSObject* typeObj, JSObject* fieldsObj)
{
  jsuint len;
  ASSERT_OK(JS_GetArrayLength(cx, fieldsObj, &len));

  // Get the common prototype for CData objects of this type from
  // ctypes.CType.prototype.
  JSObject* dataProto =
    CType::GetProtoFromType(cx, typeObj, SLOT_STRUCTDATAPROTO);

  // Set up the 'prototype' and 'prototype.constructor' properties.
  // The prototype will reflect the struct fields as properties on CData objects
  // created from this type.
  JSObject* prototype = JS_NewObject(cx, &sCDataProtoClass, dataProto, NULL);
  if (!prototype)
    return JS_FALSE;
  js::AutoObjectRooter protoroot(cx, prototype);

  if (!JS_DefineProperty(cx, prototype, "constructor", OBJECT_TO_JSVAL(typeObj),
         NULL, NULL, JSPROP_READONLY | JSPROP_PERMANENT))
    return JS_FALSE;

  // Create a hash of FieldInfo objects to stash on the type object.
  FieldInfoHash* fields(new FieldInfoHash);
  if (!fields || !fields->init(len)) {
    JS_ReportOutOfMemory(cx);
    delete fields;
    return JS_FALSE;
  }

  // Stash the FieldInfo hash in a reserved slot now, for GC safety of its
  // constituents.
  if (!JS_SetReservedSlot(cx, typeObj, SLOT_FIELDINFO,
         PRIVATE_TO_JSVAL(fields))) {
    delete fields;
    return JS_FALSE;
  }

  // Process the field types.
  size_t structSize, structAlign;
  if (len != 0) {
    structSize = 0;
    structAlign = 0;

    for (jsuint i = 0; i < len; ++i) {
      js::AutoValueRooter item(cx);
      if (!JS_GetElement(cx, fieldsObj, i, item.jsval_addr()))
        return JS_FALSE;

      JSObject* fieldType = NULL;
      JSString* name = ExtractStructField(cx, item.jsval_value(), &fieldType);
      if (!name)
        return JS_FALSE;

      // Make sure each field name is unique, and add it to the hash.
      FieldInfoHash::AddPtr entryPtr = fields->lookupForAdd(name);
      if (entryPtr) {
        JS_ReportError(cx, "struct fields must have unique names");
        return JS_FALSE;
      }
      ASSERT_OK(fields->add(entryPtr, name, FieldInfo()));
      FieldInfo& info = entryPtr->value;
      info.mType = fieldType;
      info.mIndex = i;

      // Add the field to the StructType's 'prototype' property.
      if (!JS_DefineUCProperty(cx, prototype,
             name->chars(), name->length(), JSVAL_VOID,
             StructType::FieldGetter, StructType::FieldSetter,
             JSPROP_SHARED | JSPROP_ENUMERATE | JSPROP_PERMANENT))
        return JS_FALSE;

      size_t fieldSize = CType::GetSize(cx, fieldType);
      size_t fieldAlign = CType::GetAlignment(cx, fieldType);
      size_t fieldOffset = Align(structSize, fieldAlign);
      // Check for overflow. Since we hold invariant that fieldSize % fieldAlign
      // be zero, we can safely check fieldOffset + fieldSize without first
      // checking fieldOffset for overflow.
      if (fieldOffset + fieldSize < structSize) {
        JS_ReportError(cx, "size overflow");
        return JS_FALSE;
      }
      info.mOffset = fieldOffset;
      structSize = fieldOffset + fieldSize;

      if (fieldAlign > structAlign)
        structAlign = fieldAlign;
    }

    // Pad the struct tail according to struct alignment.
    size_t structTail = Align(structSize, structAlign);
    if (structTail < structSize) {
      JS_ReportError(cx, "size overflow");
      return JS_FALSE;
    }
    structSize = structTail;

  } else {
    // Empty structs are illegal in C, but are legal and have a size of
    // 1 byte in C++. We're going to allow them, and trick libffi into
    // believing this by adding a char member. The resulting struct will have
    // no getters or setters, and will be initialized to zero.
    structSize = 1;
    structAlign = 1;
  }

  jsval sizeVal;
  if (!SizeTojsval(cx, structSize, &sizeVal))
    return JS_FALSE;

  if (!JS_SetReservedSlot(cx, typeObj, SLOT_SIZE, sizeVal) ||
      !JS_SetReservedSlot(cx, typeObj, SLOT_ALIGN, INT_TO_JSVAL(structAlign)) ||
      //!JS_FreezeObject(cx, prototype) || // XXX fixme - see bug 541212!
      !JS_SetReservedSlot(cx, typeObj, SLOT_PROTO, OBJECT_TO_JSVAL(prototype)))
    return JS_FALSE;

  return JS_TRUE;
}

ffi_type*
StructType::BuildFFIType(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(cx, obj));
  JS_ASSERT(CType::GetTypeCode(cx, obj) == TYPE_struct);
  JS_ASSERT(CType::IsSizeDefined(cx, obj));

  const FieldInfoHash* fields = GetFieldInfo(cx, obj);
  size_t len = fields->count();

  size_t structSize = CType::GetSize(cx, obj);
  size_t structAlign = CType::GetAlignment(cx, obj);

  AutoPtr<ffi_type> ffiType(new ffi_type);
  if (!ffiType) {
    JS_ReportOutOfMemory(cx);
    return NULL;
  }
  ffiType->type = FFI_TYPE_STRUCT;

  AutoPtr<ffi_type*>::Array elements;
  if (len != 0) {
    elements = new ffi_type*[len + 1];
    if (!elements) {
      JS_ReportOutOfMemory(cx);
      return NULL;
    }
    elements[len] = NULL;

    for (FieldInfoHash::Range r = fields->all(); !r.empty(); r.popFront()) {
      const FieldInfoHash::Entry& entry = r.front();
      ffi_type* fieldType = CType::GetFFIType(cx, entry.value.mType);
      if (!fieldType)
        return NULL;
      elements[entry.value.mIndex] = fieldType;
    }

  } else {
    // Represent an empty struct as having a size of 1 byte, just like C++.
    JS_ASSERT(structSize == 1);
    JS_ASSERT(structAlign == 1);
    elements = new ffi_type*[2];
    if (!elements) {
      JS_ReportOutOfMemory(cx);
      return NULL;
    }
    elements[0] = &ffi_type_uint8;
    elements[1] = NULL;
  }

  ffiType->elements = elements.get();

#ifdef DEBUG
  // Perform a sanity check: the result of our struct size and alignment
  // calculations should match libffi's. We force it to do this calculation
  // by calling ffi_prep_cif.
  ffi_cif cif;
  ffiType->size = 0;
  ffiType->alignment = 0;
  ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 0, ffiType.get(), NULL);
  JS_ASSERT(status == FFI_OK);
  JS_ASSERT(structSize == ffiType->size);
  JS_ASSERT(structAlign == ffiType->alignment);
#else
  // Fill in the ffi_type's size and align fields. This makes libffi treat the
  // type as initialized; it will not recompute the values. (We assume
  // everything agrees; if it doesn't, we really want to know about it, which
  // is the purpose of the above debug-only check.)
  ffiType->size = structSize;
  ffiType->alignment = structAlign;
#endif

  elements.forget();
  return ffiType.forget();
}

JSBool
StructType::Define(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj ||
      !CType::IsCType(cx, obj) ||
      CType::GetTypeCode(cx, obj) != TYPE_struct) {
    JS_ReportError(cx, "not a StructType");
    return JS_FALSE;
  }

  if (CType::IsSizeDefined(cx, obj)) {
    JS_ReportError(cx, "StructType has already been defined");
    return JS_FALSE;
  }

  if (argc != 1) {
    JS_ReportError(cx, "define takes one argument");
    return JS_FALSE;
  }

  jsval arg = JS_ARGV(cx, vp)[0];
  if (JSVAL_IS_PRIMITIVE(arg) ||
      !JS_IsArrayObject(cx, JSVAL_TO_OBJECT(arg))) {
    JS_ReportError(cx, "argument must be an array");
    return JS_FALSE;
  }

  return DefineInternal(cx, obj, JSVAL_TO_OBJECT(arg));
}

JSBool
StructType::ConstructData(JSContext* cx,
                          JSObject* obj,
                          uintN argc,
                          jsval* vp)
{
  if (!CType::IsCType(cx, obj) || CType::GetTypeCode(cx, obj) != TYPE_struct) {
    JS_ReportError(cx, "not a StructType");
    return JS_FALSE;
  }

  if (!CType::IsSizeDefined(cx, obj)) {
    JS_ReportError(cx, "cannot construct an opaque StructType");
    return JS_FALSE;
  }

  JSObject* result = CData::Create(cx, obj, NULL, NULL, true);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));

  if (argc == 0)
    return JS_TRUE;

  char* buffer = static_cast<char*>(CData::GetData(cx, result));
  const FieldInfoHash* fields = GetFieldInfo(cx, obj);

  jsval* argv = JS_ARGV(cx, vp);
  if (argc == 1) {
    // There are two possible interpretations of the argument:
    // 1) It may be an object '{ ... }' with properties representing the
    //    struct fields intended to ExplicitConvert wholesale to our StructType.
    // 2) If the struct contains one field, the arg may be intended to
    //    ImplicitConvert directly to that arg's CType.
    // Thankfully, the conditions for these two possibilities to succeed
    // are mutually exclusive, so we can pick the right one.

    // Try option 1) first.
    if (ExplicitConvert(cx, argv[0], obj, buffer))
      return JS_TRUE;

    if (fields->count() != 1)
      return JS_FALSE;

    // If ExplicitConvert failed, and there is no pending exception, then assume
    // hard failure (out of memory, or some other similarly serious condition).
    if (!JS_IsExceptionPending(cx))
      return JS_FALSE;

    // Otherwise, assume soft failure, and clear the pending exception so that we
    // can throw a different one as required.
    JS_ClearPendingException(cx);

    // Fall through to try option 2).
  }

  // We have a type constructor of the form 'ctypes.StructType(a, b, c, ...)'.
  // ImplicitConvert each field.
  if (argc == fields->count()) {
    for (FieldInfoHash::Range r = fields->all(); !r.empty(); r.popFront()) {
      const FieldInfo& field = r.front().value;
      if (!ImplicitConvert(cx, argv[field.mIndex], field.mType,
             buffer + field.mOffset,
             false, NULL))
        return JS_FALSE;
    }

    return JS_TRUE;
  }

  JS_ReportError(cx, "constructor takes 0, 1, or %u arguments",
    fields->count());
  return JS_FALSE;
}

const FieldInfoHash*
StructType::GetFieldInfo(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(cx, obj));
  JS_ASSERT(CType::GetTypeCode(cx, obj) == TYPE_struct);

  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_FIELDINFO, &slot));
  JS_ASSERT(!JSVAL_IS_VOID(slot) && JSVAL_TO_PRIVATE(slot));

  return static_cast<const FieldInfoHash*>(JSVAL_TO_PRIVATE(slot));
}

const FieldInfo*
StructType::LookupField(JSContext* cx, JSObject* obj, JSString *name)
{
  JS_ASSERT(CType::IsCType(cx, obj));
  JS_ASSERT(CType::GetTypeCode(cx, obj) == TYPE_struct);

  FieldInfoHash::Ptr ptr = GetFieldInfo(cx, obj)->lookup(name);
  if (ptr)
    return &ptr->value;

  const char* bytes = JS_GetStringBytesZ(cx, name);
  if (!bytes)
    return NULL;

  JS_ReportError(cx, "%s does not name a field", bytes);
  return NULL;
}

JSObject*
StructType::BuildFieldsArray(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(cx, obj));
  JS_ASSERT(CType::GetTypeCode(cx, obj) == TYPE_struct);
  JS_ASSERT(CType::IsSizeDefined(cx, obj));

  const FieldInfoHash* fields = GetFieldInfo(cx, obj);
  size_t len = fields->count();

  // Prepare a new array for the 'fields' property of the StructType.
  Array<jsval, 16> fieldsVec;
  if (!fieldsVec.resize(len))
    return NULL;

  for (FieldInfoHash::Range r = fields->all(); !r.empty(); r.popFront()) {
    const FieldInfoHash::Entry& entry = r.front();
    // Add the field descriptor to the array.
    if (!AddFieldToArray(cx, &fieldsVec[entry.value.mIndex],
                         entry.key, entry.value.mType))
      return NULL;
  }

  JSObject* fieldsProp = JS_NewArrayObject(cx, len, fieldsVec.begin());
  if (!fieldsProp)
    return NULL;

  // Seal the fields array.
  if (!JS_FreezeObject(cx, fieldsProp))
    return NULL;

  return fieldsProp;
}

JSBool
StructType::FieldsArrayGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!CType::IsCType(cx, obj) || CType::GetTypeCode(cx, obj) != TYPE_struct) {
    JS_ReportError(cx, "not a StructType");
    return JS_FALSE;
  }

  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_FIELDS, vp));

  if (!CType::IsSizeDefined(cx, obj)) {
    JS_ASSERT(JSVAL_IS_VOID(*vp));
    return JS_TRUE;
  }

  if (JSVAL_IS_VOID(*vp)) {
    // Build the 'fields' array lazily.
    JSObject* fields = BuildFieldsArray(cx, obj);
    if (!fields ||
        !JS_SetReservedSlot(cx, obj, SLOT_FIELDS, OBJECT_TO_JSVAL(fields)))
      return JS_FALSE;

    *vp = OBJECT_TO_JSVAL(fields);
  }

  JS_ASSERT(!JSVAL_IS_PRIMITIVE(*vp) &&
            JS_IsArrayObject(cx, JSVAL_TO_OBJECT(*vp)));
  return JS_TRUE;
}

JSBool
StructType::FieldGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!CData::IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  JSObject* typeObj = CData::GetCType(cx, obj);
  if (CType::GetTypeCode(cx, typeObj) != TYPE_struct) {
    JS_ReportError(cx, "not a StructType");
    return JS_FALSE;
  }

  const FieldInfo* field = LookupField(cx, typeObj, JSID_TO_STRING(idval));
  if (!field)
    return JS_FALSE;

  char* data = static_cast<char*>(CData::GetData(cx, obj)) + field->mOffset;
  return ConvertToJS(cx, field->mType, obj, data, false, false, vp);
}

JSBool
StructType::FieldSetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!CData::IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  JSObject* typeObj = CData::GetCType(cx, obj);
  if (CType::GetTypeCode(cx, typeObj) != TYPE_struct) {
    JS_ReportError(cx, "not a StructType");
    return JS_FALSE;
  }

  const FieldInfo* field = LookupField(cx, typeObj, JSID_TO_STRING(idval));
  if (!field)
    return JS_FALSE;

  char* data = static_cast<char*>(CData::GetData(cx, obj)) + field->mOffset;
  return ImplicitConvert(cx, *vp, field->mType, data, false, NULL);
}

JSBool
StructType::AddressOfField(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj || !CData::IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  JSObject* typeObj = CData::GetCType(cx, obj);
  if (CType::GetTypeCode(cx, typeObj) != TYPE_struct) {
    JS_ReportError(cx, "not a StructType");
    return JS_FALSE;
  }

  if (argc != 1) {
    JS_ReportError(cx, "addressOfField takes one argument");
    return JS_FALSE;
  }

  const FieldInfo* field = LookupField(cx, typeObj,
                                       JSVAL_TO_STRING(JS_ARGV(cx, vp)[0]));
  if (!field)
    return JS_FALSE;

  JSObject* baseType = field->mType;
  JSObject* pointerType = PointerType::CreateInternal(cx, baseType);
  if (!pointerType)
    return JS_FALSE;
  js::AutoObjectRooter root(cx, pointerType);

  // Create a PointerType CData object containing null.
  JSObject* result = CData::Create(cx, pointerType, NULL, NULL, true);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));

  // Manually set the pointer inside the object, so we skip the conversion step.
  void** data = static_cast<void**>(CData::GetData(cx, result));
  *data = static_cast<char*>(CData::GetData(cx, obj)) + field->mOffset;
  return JS_TRUE;
}

/*******************************************************************************
** FunctionType implementation
*******************************************************************************/

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
    // Allocate a minimum of sizeof(ffi_arg) to handle small integers.
    size_t size = Align(CType::GetSize(cx, type), sizeof(ffi_arg));
    mData = new char[size];
    if (mData)
      memset(mData, 0, size);
    return mData != NULL;
  }

  void* mData;
};

static bool
GetABI(JSContext* cx, jsval abiType, ffi_abi* result)
{
  if (JSVAL_IS_PRIMITIVE(abiType))
    return false;

  ABICode abi = GetABICode(cx, JSVAL_TO_OBJECT(abiType));

  // determine the ABI from the subset of those available on the
  // given platform. ABI_DEFAULT specifies the default
  // C calling convention (cdecl) on each platform.
  switch (abi) {
  case ABI_DEFAULT:
    *result = FFI_DEFAULT_ABI;
    return true;
  case ABI_STDCALL:
  case ABI_WINAPI:
#if (defined(_WIN32) && !defined(_WIN64)) || defined(_OS2)
    *result = FFI_STDCALL;
    return true;
#endif
  case INVALID_ABI:
    break;
  }
  return false;
}

static JSObject*
PrepareType(JSContext* cx, jsval type)
{
  if (JSVAL_IS_PRIMITIVE(type) ||
      !CType::IsCType(cx, JSVAL_TO_OBJECT(type))) {
    JS_ReportError(cx, "not a ctypes type");
    return NULL;
  }

  JSObject* result = JSVAL_TO_OBJECT(type);
  TypeCode typeCode = CType::GetTypeCode(cx, result);

  if (typeCode == TYPE_array) {
    // convert array argument types to pointers, just like C.
    // ImplicitConvert will do the same, when passing an array as data.
    JSObject* baseType = ArrayType::GetBaseType(cx, result);
    result = PointerType::CreateInternal(cx, baseType);
    if (!result)
      return NULL;

  } else if (typeCode == TYPE_void_t || typeCode == TYPE_function) {
    // disallow void or function argument types
    JS_ReportError(cx, "Cannot have void or function argument type");
    return NULL;
  }

  if (!CType::IsSizeDefined(cx, result)) {
    JS_ReportError(cx, "Argument type must have defined size");
    return NULL;
  }

  // libffi cannot pass types of zero size by value.
  JS_ASSERT(CType::GetSize(cx, result) != 0);

  return result;
}

static JSObject*
PrepareReturnType(JSContext* cx, jsval type)
{
  if (JSVAL_IS_PRIMITIVE(type) ||
      !CType::IsCType(cx, JSVAL_TO_OBJECT(type))) {
    JS_ReportError(cx, "not a ctypes type");
    return NULL;
  }

  JSObject* result = JSVAL_TO_OBJECT(type);
  TypeCode typeCode = CType::GetTypeCode(cx, result);

  // Arrays and functions can never be return types.
  if (typeCode == TYPE_array || typeCode == TYPE_function) {
    JS_ReportError(cx, "Return type cannot be an array or function");
    return NULL;
  }

  if (typeCode != TYPE_void_t && !CType::IsSizeDefined(cx, result)) {
    JS_ReportError(cx, "Return type must have defined size");
    return NULL;
  }

  // libffi cannot pass types of zero size by value.
  JS_ASSERT(typeCode == TYPE_void_t || CType::GetSize(cx, result) != 0);

  return result;
}

static JS_ALWAYS_INLINE bool
IsEllipsis(jsval v)
{
  if (!JSVAL_IS_STRING(v))
    return false;
  JSString* str = JSVAL_TO_STRING(v);
  if (str->length() != 3)
    return false;
  const jschar* chars = str->chars(), dot('.');
  return (chars[0] == dot &&
          chars[1] == dot &&
          chars[2] == dot);
}

static JSBool
PrepareCIF(JSContext* cx,
           FunctionInfo* fninfo)
{
  ffi_abi abi;
  if (!GetABI(cx, OBJECT_TO_JSVAL(fninfo->mABI), &abi)) {
    JS_ReportError(cx, "Invalid ABI specification");
    return false;
  }

  ffi_type* rtype = CType::GetFFIType(cx, fninfo->mReturnType);
  if (!rtype)
    return false;

  ffi_status status =
    ffi_prep_cif(&fninfo->mCIF,
                 abi,
                 fninfo->mFFITypes.length(),
                 rtype,
                 fninfo->mFFITypes.begin());

  switch (status) {
  case FFI_OK:
    return true;
  case FFI_BAD_ABI:
    JS_ReportError(cx, "Invalid ABI specification");
    return false;
  case FFI_BAD_TYPEDEF:
    JS_ReportError(cx, "Invalid type specification");
    return false;
  default:
    JS_ReportError(cx, "Unknown libffi error");
    return false;
  }
}

void
FunctionType::BuildSymbolName(JSContext* cx,
                              JSString* name,
                              JSObject* typeObj,
                              AutoCString& result)
{
  FunctionInfo* fninfo = GetFunctionInfo(cx, typeObj);

  switch (GetABICode(cx, fninfo->mABI)) {
  case ABI_DEFAULT:
  case ABI_WINAPI:
    // For cdecl or WINAPI functions, no mangling is necessary.
    AppendString(result, name);
    break;

  case ABI_STDCALL: {
    // On WIN32, stdcall functions look like:
    //   _foo@40
    // where 'foo' is the function name, and '40' is the aligned size of the
    // arguments.
    AppendString(result, "_");
    AppendString(result, name);
    AppendString(result, "@");

    // Compute the suffix by aligning each argument to sizeof(ffi_arg).
    size_t size = 0;
    for (size_t i = 0; i < fninfo->mArgTypes.length(); ++i) {
      JSObject* argType = fninfo->mArgTypes[i];
      size += Align(CType::GetSize(cx, argType), sizeof(ffi_arg));
    }

    IntegerToString(size, 10, result);
    break;
  }

  case INVALID_ABI:
    JS_NOT_REACHED("invalid abi");
    break;
  }
}

static FunctionInfo*
NewFunctionInfo(JSContext* cx,
                jsval abiType,
                jsval returnType,
                jsval* argTypes,
                uintN argLength)
{
  AutoPtr<FunctionInfo> fninfo(new FunctionInfo());
  if (!fninfo) {
    JS_ReportOutOfMemory(cx);
    return NULL;
  }

  ffi_abi abi;
  if (!GetABI(cx, abiType, &abi)) {
    JS_ReportError(cx, "Invalid ABI specification");
    return NULL;
  }
  fninfo->mABI = JSVAL_TO_OBJECT(abiType);

  // prepare the result type
  fninfo->mReturnType = PrepareReturnType(cx, returnType);
  if (!fninfo->mReturnType)
    return NULL;

  // prepare the argument types
  if (!fninfo->mArgTypes.reserve(argLength) ||
      !fninfo->mFFITypes.reserve(argLength)) {
    JS_ReportOutOfMemory(cx);
    return NULL;
  }

  fninfo->mIsVariadic = false;

  for (JSUint32 i = 0; i < argLength; ++i) {
    if (IsEllipsis(argTypes[i])) {
      fninfo->mIsVariadic = true;
      if (i < 1) {
        JS_ReportError(cx, "\"...\" may not be the first and only parameter "
                       "type of a variadic function declaration");
        return NULL;
      }
      if (i < argLength - 1) {
        JS_ReportError(cx, "\"...\" must be the last parameter type of a "
                       "variadic function declaration");
        return NULL;
      }
      if (GetABICode(cx, fninfo->mABI) != ABI_DEFAULT) {
        JS_ReportError(cx, "Variadic functions must use the __cdecl calling "
                       "convention");
        return NULL;
      }
      break;
    }

    JSObject* argType = PrepareType(cx, argTypes[i]);
    if (!argType)
      return NULL;

    ffi_type* ffiType = CType::GetFFIType(cx, argType);
    if (!ffiType)
      return NULL;

    fninfo->mArgTypes.append(argType);
    fninfo->mFFITypes.append(ffiType);
  }

  if (fninfo->mIsVariadic)
    // wait to PrepareCIF until function is called
    return fninfo.forget();

  if (!PrepareCIF(cx, fninfo.get()))
    return NULL;

  return fninfo.forget();
}

JSBool
FunctionType::Create(JSContext* cx, uintN argc, jsval* vp)
{
  // Construct and return a new FunctionType object.
  if (argc < 2 || argc > 3) {
    JS_ReportError(cx, "FunctionType takes two or three arguments");
    return JS_FALSE;
  }

  jsval* argv = JS_ARGV(cx, vp);
  Array<jsval, 16> argTypes;
  JSObject* arrayObj = NULL;

  if (argc == 3) {
    // Prepare an array of jsvals for the arguments.
    if (JSVAL_IS_PRIMITIVE(argv[2]) ||
        !JS_IsArrayObject(cx, JSVAL_TO_OBJECT(argv[2]))) {
      JS_ReportError(cx, "third argument must be an array");
      return JS_FALSE;
    }

    arrayObj = JSVAL_TO_OBJECT(argv[2]);
    jsuint len;
    ASSERT_OK(JS_GetArrayLength(cx, arrayObj, &len));

    if (!argTypes.resize(len)) {
      JS_ReportOutOfMemory(cx);
      return JS_FALSE;
    }

    for (jsuint i = 0; i < len; ++i)
      argTypes[i] = JSVAL_VOID;
  }

  // Pull out the argument types from the array, if any.
  JS_ASSERT(!argTypes.length() || arrayObj);
  js::AutoArrayRooter items(cx, argTypes.length(), argTypes.begin());
  for (jsuint i = 0; i < argTypes.length(); ++i) {
    if (!JS_GetElement(cx, arrayObj, i, &argTypes[i]))
      return JS_FALSE;
  }

  JSObject* result = CreateInternal(cx, argv[0], argv[1],
      argTypes.begin(), argTypes.length());
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
  return JS_TRUE;
}

JSObject*
FunctionType::CreateInternal(JSContext* cx,
                             jsval abi,
                             jsval rtype,
                             jsval* argtypes,
                             jsuint arglen)
{
  // Determine and check the types, and prepare the function CIF.
  AutoPtr<FunctionInfo> fninfo(NewFunctionInfo(cx, abi, rtype, argtypes, arglen));
  if (!fninfo)
    return NULL;

  // Get ctypes.FunctionType.prototype and the common prototype for CData objects
  // of this type, from ctypes.CType.prototype.
  JSObject* typeProto = CType::GetProtoFromType(cx, fninfo->mReturnType,
                                                SLOT_FUNCTIONPROTO);
  JSObject* dataProto = CType::GetProtoFromType(cx, fninfo->mReturnType,
                                                SLOT_FUNCTIONDATAPROTO);

  // Create a new CType object with the common properties and slots.
  JSObject* typeObj = CType::Create(cx, typeProto, dataProto, TYPE_function,
                        NULL, JSVAL_VOID, JSVAL_VOID, NULL);
  if (!typeObj)
    return NULL;
  js::AutoObjectRooter root(cx, typeObj);

  // Stash the FunctionInfo in a reserved slot.
  if (!JS_SetReservedSlot(cx, typeObj, SLOT_FNINFO,
         PRIVATE_TO_JSVAL(fninfo.get())))
    return NULL;
  fninfo.forget();

  return typeObj;
}

// Construct a function pointer to a JS function (see CClosure::Create()).
// Regular function pointers are constructed directly in
// PointerType::ConstructData().
JSBool
FunctionType::ConstructData(JSContext* cx,
                            JSObject* typeObj,
                            JSObject* dataObj,
                            JSObject* fnObj,
                            JSObject* thisObj)
{
  JS_ASSERT(CType::GetTypeCode(cx, typeObj) == TYPE_function);

  PRFuncPtr* data = static_cast<PRFuncPtr*>(CData::GetData(cx, dataObj));

  FunctionInfo* fninfo = FunctionType::GetFunctionInfo(cx, typeObj);
  if (fninfo->mIsVariadic) {
    JS_ReportError(cx, "Can't declare a variadic callback function");
    return JS_FALSE;
  }
  if (GetABICode(cx, fninfo->mABI) == ABI_WINAPI) {
    JS_ReportError(cx, "Can't declare a ctypes.winapi_abi callback function, "
                   "use ctypes.stdcall_abi instead");
    return JS_FALSE;
  }

  JSObject* closureObj = CClosure::Create(cx, typeObj, fnObj, thisObj, data);
  if (!closureObj)
    return JS_FALSE;
  js::AutoObjectRooter root(cx, closureObj);

  // Set the closure object as the referent of the new CData object.
  if (!JS_SetReservedSlot(cx, dataObj, SLOT_REFERENT,
         OBJECT_TO_JSVAL(closureObj)))
    return JS_FALSE;

  // Seal the CData object, to prevent modification of the function pointer.
  // This permanently associates this object with the closure, and avoids
  // having to do things like reset SLOT_REFERENT when someone tries to
  // change the pointer value.
  // XXX This will need to change when bug 541212 is fixed -- CData::ValueSetter
  // could be called on a frozen object.
  return JS_FreezeObject(cx, dataObj);
}

typedef Array<AutoValue, 16> AutoValueAutoArray;

static JSBool
ConvertArgument(JSContext* cx,
                jsval arg,
                JSObject* type,
                AutoValue* value,
                AutoValueAutoArray* strings)
{
  if (!value->SizeToType(cx, type)) {
    JS_ReportAllocationOverflow(cx);
    return false;
  }

  bool freePointer = false;
  if (!ImplicitConvert(cx, arg, type, value->mData, true, &freePointer))
    return false;

  if (freePointer) {
    // ImplicitConvert converted a string for us, which we have to free.
    // Keep track of it.
    if (!strings->growBy(1)) {
      JS_ReportOutOfMemory(cx);
      return false;
    }
    strings->back().mData = *static_cast<char**>(value->mData);
  }

  return true;
}

JSBool
FunctionType::Call(JSContext* cx,
                   uintN argc,
                   jsval* vp)
{
  // get the callee object...
  JSObject* obj = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
  if (!CData::IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  JSObject* typeObj = CData::GetCType(cx, obj);
  if (CType::GetTypeCode(cx, typeObj) != TYPE_pointer) {
    JS_ReportError(cx, "not a FunctionType.ptr");
    return false;
  }

  typeObj = PointerType::GetBaseType(cx, typeObj);
  if (CType::GetTypeCode(cx, typeObj) != TYPE_function) {
    JS_ReportError(cx, "not a FunctionType.ptr");
    return false;
  }

  FunctionInfo* fninfo = GetFunctionInfo(cx, typeObj);
  JSUint32 argcFixed = fninfo->mArgTypes.length();

  if ((!fninfo->mIsVariadic && argc != argcFixed) ||
      (fninfo->mIsVariadic && argc < argcFixed)) {
    JS_ReportError(cx, "Number of arguments does not match declaration");
    return false;
  }

  // Check if we have a Library object. If we do, make sure it's open.
  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_REFERENT, &slot));
  if (!JSVAL_IS_VOID(slot) && Library::IsLibrary(cx, JSVAL_TO_OBJECT(slot))) {
    PRLibrary* library = Library::GetLibrary(cx, JSVAL_TO_OBJECT(slot));
    if (!library) {
      JS_ReportError(cx, "library is not open");
      return false;
    }
  }

  // prepare the values for each argument
  AutoValueAutoArray values;
  AutoValueAutoArray strings;
  if (!values.resize(argc)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  jsval* argv = JS_ARGV(cx, vp);
  for (jsuint i = 0; i < argcFixed; ++i)
    if (!ConvertArgument(cx, argv[i], fninfo->mArgTypes[i], &values[i], &strings))
      return false;

  if (fninfo->mIsVariadic) {
    if (!fninfo->mFFITypes.resize(argc)) {
      JS_ReportOutOfMemory(cx);
      return false;
    }

    JSObject* obj;  // Could reuse obj instead of declaring a second
    JSObject* type; // JSObject*, but readability would suffer.

    for (JSUint32 i = argcFixed; i < argc; ++i) {
      if (JSVAL_IS_PRIMITIVE(argv[i]) ||
          !CData::IsCData(cx, obj = JSVAL_TO_OBJECT(argv[i]))) {
        // Since we know nothing about the CTypes of the ... arguments,
        // they absolutely must be CData objects already.
        JS_ReportError(cx, "argument %d of type %s is not a CData object",
                       i, JS_GetTypeName(cx, JS_TypeOfValue(cx, argv[i])));
        return false;
      }
      if (!(type = CData::GetCType(cx, obj)) ||
          !(type = PrepareType(cx, OBJECT_TO_JSVAL(type))) ||
          // Relying on ImplicitConvert only for the limited purpose of
          // converting one CType to another (e.g., T[] to T*).
          !ConvertArgument(cx, argv[i], type, &values[i], &strings) ||
          !(fninfo->mFFITypes[i] = CType::GetFFIType(cx, type))) {
        // These functions report their own errors.
        return false;
      }
    }
    if (!PrepareCIF(cx, fninfo))
      return false;
  }

  // initialize a pointer to an appropriate location, for storing the result
  AutoValue returnValue;
  TypeCode typeCode = CType::GetTypeCode(cx, fninfo->mReturnType);
  if (typeCode != TYPE_void_t &&
      !returnValue.SizeToType(cx, fninfo->mReturnType)) {
    JS_ReportAllocationOverflow(cx);
    return false;
  }

  uintptr_t fn = *reinterpret_cast<uintptr_t*>(CData::GetData(cx, obj));

  // suspend the request before we call into the function, since the call
  // may block or otherwise take a long time to return.
  {
    JSAutoSuspendRequest suspend(cx);
    ffi_call(&fninfo->mCIF, FFI_FN(fn), returnValue.mData,
             reinterpret_cast<void**>(values.begin()));
  }

  // Small integer types get returned as a word-sized ffi_arg. Coerce it back
  // into the correct size for ConvertToJS.
  switch (typeCode) {
#define DEFINE_INT_TYPE(name, type, ffiType)                                   \
  case TYPE_##name:                                                            \
    if (sizeof(type) < sizeof(ffi_arg)) {                                      \
      ffi_arg data = *static_cast<ffi_arg*>(returnValue.mData);                \
      *static_cast<type*>(returnValue.mData) = static_cast<type>(data);        \
    }                                                                          \
    break;
#define DEFINE_WRAPPED_INT_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_BOOL_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_CHAR_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_JSCHAR_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#include "typedefs.h"
  default:
    break;
  }

  // prepare a JS object from the result
  return ConvertToJS(cx, fninfo->mReturnType, NULL, returnValue.mData,
                     false, true, vp);
}

FunctionInfo*
FunctionType::GetFunctionInfo(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(cx, obj));
  JS_ASSERT(CType::GetTypeCode(cx, obj) == TYPE_function);

  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_FNINFO, &slot));
  JS_ASSERT(!JSVAL_IS_VOID(slot) && JSVAL_TO_PRIVATE(slot));

  return static_cast<FunctionInfo*>(JSVAL_TO_PRIVATE(slot));
}

static JSBool
CheckFunctionType(JSContext* cx, JSObject* obj)
{
  if (!CType::IsCType(cx, obj) || CType::GetTypeCode(cx, obj) != TYPE_function) {
    JS_ReportError(cx, "not a FunctionType");
    return JS_FALSE;
  }
  return JS_TRUE;
}

JSBool
FunctionType::ArgTypesGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!CheckFunctionType(cx, obj))
    return JS_FALSE;

  // Check if we have a cached argTypes array.
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_ARGS_T, vp));
  if (!JSVAL_IS_VOID(*vp))
    return JS_TRUE;

  FunctionInfo* fninfo = GetFunctionInfo(cx, obj);
  size_t len = fninfo->mArgTypes.length();

  // Prepare a new array.
  Array<jsval, 16> vec;
  if (!vec.resize(len))
    return JS_FALSE;

  for (size_t i = 0; i < len; ++i)
    vec[i] = OBJECT_TO_JSVAL(fninfo->mArgTypes[i]);

  JSObject* argTypes = JS_NewArrayObject(cx, len, vec.begin());
  if (!argTypes)
    return JS_FALSE;

  // Seal and cache it.
  if (!JS_FreezeObject(cx, argTypes) ||
      !JS_SetReservedSlot(cx, obj, SLOT_ARGS_T, OBJECT_TO_JSVAL(argTypes)))
    return JS_FALSE;

  *vp = OBJECT_TO_JSVAL(argTypes);
  return JS_TRUE;
}

JSBool
FunctionType::ReturnTypeGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!CheckFunctionType(cx, obj))
    return JS_FALSE;

  // Get the returnType object from the FunctionInfo.
  *vp = OBJECT_TO_JSVAL(GetFunctionInfo(cx, obj)->mReturnType);
  return JS_TRUE;
}

JSBool
FunctionType::ABIGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!CheckFunctionType(cx, obj))
    return JS_FALSE;

  // Get the abi object from the FunctionInfo.
  *vp = OBJECT_TO_JSVAL(GetFunctionInfo(cx, obj)->mABI);
  return JS_TRUE;
}

JSBool
FunctionType::IsVariadicGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!CheckFunctionType(cx, obj))
    return JS_FALSE;

  *vp = BOOLEAN_TO_JSVAL(GetFunctionInfo(cx, obj)->mIsVariadic);
  return JS_TRUE;
}

/*******************************************************************************
** CClosure implementation
*******************************************************************************/

JSObject*
CClosure::Create(JSContext* cx,
                 JSObject* typeObj,
                 JSObject* fnObj,
                 JSObject* thisObj,
                 PRFuncPtr* fnptr)
{
  JS_ASSERT(fnObj);

  JSObject* result = JS_NewObject(cx, &sCClosureClass, NULL, NULL);
  if (!result)
    return NULL;
  js::AutoObjectRooter root(cx, result);

  // Get the FunctionInfo from the FunctionType.
  FunctionInfo* fninfo = FunctionType::GetFunctionInfo(cx, typeObj);
  JS_ASSERT(!fninfo->mIsVariadic);
  JS_ASSERT(GetABICode(cx, fninfo->mABI) != ABI_WINAPI);

  AutoPtr<ClosureInfo> cinfo(new ClosureInfo());
  if (!cinfo) {
    JS_ReportOutOfMemory(cx);
    return NULL;
  }

  // Get the prototype of the FunctionType object, of class CTypeProto,
  // which stores our JSContext for use with the closure.
  JSObject* proto = JS_GetPrototype(cx, typeObj);
  JS_ASSERT(proto);
  JS_ASSERT(JS_GET_CLASS(cx, proto) == &sCTypeProtoClass);

  // Get a JSContext for use with the closure.
  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, proto, SLOT_CLOSURECX, &slot));
  if (!JSVAL_IS_VOID(slot)) {
    // Use the existing JSContext.
    cinfo->cx = static_cast<JSContext*>(JSVAL_TO_PRIVATE(slot));
    JS_ASSERT(cinfo->cx);
  } else {
    // Lazily instantiate a new JSContext, and stash it on
    // ctypes.FunctionType.prototype.
    JSRuntime* runtime = JS_GetRuntime(cx);
    cinfo->cx = JS_NewContext(runtime, 8192);
    if (!cinfo->cx) {
      JS_ReportOutOfMemory(cx);
      return NULL;
    }

    if (!JS_SetReservedSlot(cx, proto, SLOT_CLOSURECX,
           PRIVATE_TO_JSVAL(cinfo->cx))) {
      JS_DestroyContextNoGC(cinfo->cx);
      return NULL;
    }

    JS_ClearContextThread(cinfo->cx);
  }

#ifdef DEBUG
  // We want *this* context's thread here so use cx instead of cinfo->cx.
  cinfo->cxThread = JS_GetContextThread(cx);
#endif

  cinfo->closureObj = result;
  cinfo->typeObj = typeObj;
  cinfo->thisObj = thisObj;
  cinfo->jsfnObj = fnObj;

  // Create an ffi_closure object and initialize it.
  void* code;
  cinfo->closure =
    static_cast<ffi_closure*>(ffi_closure_alloc(sizeof(ffi_closure), &code));
  if (!cinfo->closure || !code) {
    JS_ReportError(cx, "couldn't create closure - libffi error");
    return NULL;
  }

  ffi_status status = ffi_prep_closure_loc(cinfo->closure, &fninfo->mCIF,
    CClosure::ClosureStub, cinfo.get(), code);
  if (status != FFI_OK) {
    ffi_closure_free(cinfo->closure);
    JS_ReportError(cx, "couldn't create closure - libffi error");
    return NULL;
  }

  // Stash the ClosureInfo struct on our new object.
  if (!JS_SetReservedSlot(cx, result, SLOT_CLOSUREINFO,
         PRIVATE_TO_JSVAL(cinfo.get()))) {
    ffi_closure_free(cinfo->closure);
    return NULL;
  }
  cinfo.forget();

  // Casting between void* and a function pointer is forbidden in C and C++.
  // Do it via an integral type.
  *fnptr = reinterpret_cast<PRFuncPtr>(reinterpret_cast<uintptr_t>(code));
  return result;
}

void
CClosure::Trace(JSTracer* trc, JSObject* obj)
{
  JSContext* cx = trc->context;

  // Make sure our ClosureInfo slot is legit. If it's not, bail.
  jsval slot;
  if (!JS_GetReservedSlot(cx, obj, SLOT_CLOSUREINFO, &slot) ||
      JSVAL_IS_VOID(slot))
    return;

  ClosureInfo* cinfo = static_cast<ClosureInfo*>(JSVAL_TO_PRIVATE(slot));

  // Identify our objects to the tracer. (There's no need to identify
  // 'closureObj', since that's us.)
  JS_CALL_OBJECT_TRACER(trc, cinfo->typeObj, "typeObj");
  JS_CALL_OBJECT_TRACER(trc, cinfo->jsfnObj, "jsfnObj");
  if (cinfo->thisObj)
    JS_CALL_OBJECT_TRACER(trc, cinfo->thisObj, "thisObj");
}

void
CClosure::Finalize(JSContext* cx, JSObject* obj)
{
  // Make sure our ClosureInfo slot is legit. If it's not, bail.
  jsval slot;
  if (!JS_GetReservedSlot(cx, obj, SLOT_CLOSUREINFO, &slot) ||
      JSVAL_IS_VOID(slot))
    return;

  ClosureInfo* cinfo = static_cast<ClosureInfo*>(JSVAL_TO_PRIVATE(slot));
  if (cinfo->closure)
    ffi_closure_free(cinfo->closure);

  delete cinfo;
}

void
CClosure::ClosureStub(ffi_cif* cif, void* result, void** args, void* userData)
{
  JS_ASSERT(cif);
  JS_ASSERT(result);
  JS_ASSERT(args);
  JS_ASSERT(userData);

  // Retrieve the essentials from our closure object.
  ClosureInfo* cinfo = static_cast<ClosureInfo*>(userData);
  JSContext* cx = cinfo->cx;
  JSObject* typeObj = cinfo->typeObj;
  JSObject* thisObj = cinfo->thisObj;
  JSObject* jsfnObj = cinfo->jsfnObj;

  ScopedContextThread scopedThread(cx);

  // Assert that we're on the thread we were created from.
  JS_ASSERT(cinfo->cxThread == JS_GetContextThread(cx));

  JSAutoRequest ar(cx);

  // Assert that our CIFs agree.
  FunctionInfo* fninfo = FunctionType::GetFunctionInfo(cx, typeObj);
  JS_ASSERT(cif == &fninfo->mCIF);

  TypeCode typeCode = CType::GetTypeCode(cx, fninfo->mReturnType);

  // Initialize the result to zero, in case something fails. Small integer types
  // are promoted to a word-sized ffi_arg, so we must be careful to zero the
  // whole word.
  if (cif->rtype != &ffi_type_void) {
    size_t size = cif->rtype->size;
    switch (typeCode) {
#define DEFINE_INT_TYPE(name, type, ffiType)                                   \
    case TYPE_##name:
#define DEFINE_WRAPPED_INT_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_BOOL_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_CHAR_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_JSCHAR_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#include "typedefs.h"
      size = Align(size, sizeof(ffi_arg));
      break;
    default:
      break;
    }
    memset(result, 0, size);
  }

  // Get a death grip on 'closureObj'.
  js::AutoObjectRooter root(cx, cinfo->closureObj);

  // Set up an array for converted arguments.
  Array<jsval, 16> argv;
  if (!argv.resize(cif->nargs)) {
    JS_ReportOutOfMemory(cx);
    return;
  }

  for (JSUint32 i = 0; i < cif->nargs; ++i)
    argv[i] = JSVAL_VOID;

  js::AutoArrayRooter roots(cx, argv.length(), Valueify(argv.begin()));
  for (JSUint32 i = 0; i < cif->nargs; ++i) {
    // Convert each argument, and have any CData objects created depend on
    // the existing buffers.
    if (!ConvertToJS(cx, fninfo->mArgTypes[i], NULL, args[i], false, false,
           &argv[i]))
      return;
  }

  // Call the JS function. 'thisObj' may be NULL, in which case the JS engine
  // will find an appropriate object to use.
  jsval rval;
  if (!JS_CallFunctionValue(cx, thisObj, OBJECT_TO_JSVAL(jsfnObj), cif->nargs,
       argv.begin(), &rval))
    return;

  // Convert the result. Note that we pass 'isArgument = false', such that
  // ImplicitConvert will *not* autoconvert a JS string into a pointer-to-char
  // type, which would require an allocation that we can't track. The JS
  // function must perform this conversion itself and return a PointerType
  // CData; thusly, the burden of freeing the data is left to the user.
  if (!ImplicitConvert(cx, rval, fninfo->mReturnType, result, false, NULL))
    return;

  // Small integer types must be returned as a word-sized ffi_arg. Coerce it
  // back into the size libffi expects.
  switch (typeCode) {
#define DEFINE_INT_TYPE(name, type, ffiType)                                   \
  case TYPE_##name:                                                            \
    if (sizeof(type) < sizeof(ffi_arg)) {                                      \
      ffi_arg data = *static_cast<type*>(result);                              \
      *static_cast<ffi_arg*>(result) = data;                                   \
    }                                                                          \
    break;
#define DEFINE_WRAPPED_INT_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_BOOL_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_CHAR_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_JSCHAR_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#include "typedefs.h"
  default:
    break;
  }
}

/*******************************************************************************
** CData implementation
*******************************************************************************/

// Create a new CData object of type 'typeObj' containing binary data supplied
// in 'source', optionally with a referent object 'refObj'.
//
// * 'typeObj' must be a CType of defined (but possibly zero) size.
//
// * If an object 'refObj' is supplied, the new CData object stores the
//   referent object in a reserved slot for GC safety, such that 'refObj' will
//   be held alive by the resulting CData object. 'refObj' may or may not be
//   a CData object; merely an object we want to keep alive.
//   * If 'refObj' is a CData object, 'ownResult' must be false.
//   * Otherwise, 'refObj' is a Library or CClosure object, and 'ownResult'
//     may be true or false.
// * Otherwise 'refObj' is NULL. In this case, 'ownResult' may be true or false.
//
// * If 'ownResult' is true, the CData object will allocate an appropriately
//   sized buffer, and free it upon finalization. If 'source' data is
//   supplied, the data will be copied from 'source' into the buffer;
//   otherwise, the entirety of the new buffer will be initialized to zero.
// * If 'ownResult' is false, the new CData's buffer refers to a slice of
//   another buffer kept alive by 'refObj'. 'source' data must be provided,
//   and the new CData's buffer will refer to 'source'.
JSObject*
CData::Create(JSContext* cx,
              JSObject* typeObj,
              JSObject* refObj,
              void* source,
              bool ownResult)
{
  JS_ASSERT(typeObj);
  JS_ASSERT(CType::IsCType(cx, typeObj));
  JS_ASSERT(CType::IsSizeDefined(cx, typeObj));
  JS_ASSERT(ownResult || source);
  JS_ASSERT_IF(refObj && CData::IsCData(cx, refObj), !ownResult);

  // Get the 'prototype' property from the type.
  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, typeObj, SLOT_PROTO, &slot));
  JS_ASSERT(!JSVAL_IS_PRIMITIVE(slot));

  JSObject* proto = JSVAL_TO_OBJECT(slot);
  JSObject* parent = JS_GetParent(cx, typeObj);
  JS_ASSERT(parent);

  JSObject* dataObj = JS_NewObject(cx, &sCDataClass, proto, parent);
  if (!dataObj)
    return NULL;
  js::AutoObjectRooter root(cx, dataObj);

  // set the CData's associated type
  if (!JS_SetReservedSlot(cx, dataObj, SLOT_CTYPE, OBJECT_TO_JSVAL(typeObj)))
    return NULL;

  // Stash the referent object, if any, for GC safety.
  if (refObj &&
      !JS_SetReservedSlot(cx, dataObj, SLOT_REFERENT, OBJECT_TO_JSVAL(refObj)))
    return NULL;

  // Set our ownership flag.
  if (!JS_SetReservedSlot(cx, dataObj, SLOT_OWNS, BOOLEAN_TO_JSVAL(ownResult)))
    return NULL;

  // attach the buffer. since it might not be 2-byte aligned, we need to
  // allocate an aligned space for it and store it there. :(
  char** buffer = new char*;
  if (!buffer) {
    JS_ReportOutOfMemory(cx);
    return NULL;
  }

  char* data;
  if (!ownResult) {
    data = static_cast<char*>(source);
  } else {
    // Initialize our own buffer.
    size_t size = CType::GetSize(cx, typeObj);
    data = new char[size];
    if (!data) {
      // Report a catchable allocation error.
      JS_ReportAllocationOverflow(cx);
      delete buffer;
      return NULL;
    }

    if (!source)
      memset(data, 0, size);
    else
      memcpy(data, source, size);
  }

  *buffer = data;
  if (!JS_SetReservedSlot(cx, dataObj, SLOT_DATA, PRIVATE_TO_JSVAL(buffer))) {
    if (ownResult)
      delete[] data;
    delete buffer;
    return NULL;
  }

  return dataObj;
}

void
CData::Finalize(JSContext* cx, JSObject* obj)
{
  // Delete our buffer, and the data it contains if we own it.
  jsval slot;
  if (!JS_GetReservedSlot(cx, obj, SLOT_OWNS, &slot) || JSVAL_IS_VOID(slot))
    return;

  JSBool owns = JSVAL_TO_BOOLEAN(slot);

  if (!JS_GetReservedSlot(cx, obj, SLOT_DATA, &slot) || JSVAL_IS_VOID(slot))
    return;
  char** buffer = static_cast<char**>(JSVAL_TO_PRIVATE(slot));

  if (owns)
    delete[] *buffer;
  delete buffer;
}

JSObject*
CData::GetCType(JSContext* cx, JSObject* dataObj)
{
  JS_ASSERT(CData::IsCData(cx, dataObj));

  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, dataObj, SLOT_CTYPE, &slot));
  JSObject* typeObj = JSVAL_TO_OBJECT(slot);
  JS_ASSERT(CType::IsCType(cx, typeObj));
  return typeObj;
}

void*
CData::GetData(JSContext* cx, JSObject* dataObj)
{
  JS_ASSERT(CData::IsCData(cx, dataObj));

  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, dataObj, SLOT_DATA, &slot));

  void** buffer = static_cast<void**>(JSVAL_TO_PRIVATE(slot));
  JS_ASSERT(buffer);
  JS_ASSERT(*buffer);
  return *buffer;
}

bool
CData::IsCData(JSContext* cx, JSObject* obj)
{
  return JS_GET_CLASS(cx, obj) == &sCDataClass;
}

JSBool
CData::ValueGetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  // Convert the value to a primitive; do not create a new CData object.
  if (!ConvertToJS(cx, GetCType(cx, obj), NULL, GetData(cx, obj), true, false, vp))
    return JS_FALSE;

  return JS_TRUE;
}

JSBool
CData::ValueSetter(JSContext* cx, JSObject* obj, jsid idval, jsval* vp)
{
  if (!IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  return ImplicitConvert(cx, *vp, GetCType(cx, obj), GetData(cx, obj), false, NULL);
}

JSBool
CData::Address(JSContext* cx, uintN argc, jsval* vp)
{
  if (argc != 0) {
    JS_ReportError(cx, "address takes zero arguments");
    return JS_FALSE;
  }

  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj || !IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  JSObject* typeObj = CData::GetCType(cx, obj);
  JSObject* pointerType = PointerType::CreateInternal(cx, typeObj);
  if (!pointerType)
    return JS_FALSE;
  js::AutoObjectRooter root(cx, pointerType);

  // Create a PointerType CData object containing null.
  JSObject* result = CData::Create(cx, pointerType, NULL, NULL, true);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));

  // Manually set the pointer inside the object, so we skip the conversion step.
  void** data = static_cast<void**>(GetData(cx, result));
  *data = GetData(cx, obj);
  return JS_TRUE;
}

JSBool
CData::Cast(JSContext* cx, uintN argc, jsval* vp)
{
  if (argc != 2) {
    JS_ReportError(cx, "cast takes two arguments");
    return JS_FALSE;
  }

  jsval* argv = JS_ARGV(cx, vp);
  if (JSVAL_IS_PRIMITIVE(argv[0]) ||
      !CData::IsCData(cx, JSVAL_TO_OBJECT(argv[0]))) {
    JS_ReportError(cx, "first argument must be a CData");
    return JS_FALSE;
  }
  JSObject* sourceData = JSVAL_TO_OBJECT(argv[0]);
  JSObject* sourceType = CData::GetCType(cx, sourceData);

  if (JSVAL_IS_PRIMITIVE(argv[1]) ||
      !CType::IsCType(cx, JSVAL_TO_OBJECT(argv[1]))) {
    JS_ReportError(cx, "second argument must be a CType");
    return JS_FALSE;
  }

  JSObject* targetType = JSVAL_TO_OBJECT(argv[1]);
  size_t targetSize;
  if (!CType::GetSafeSize(cx, targetType, &targetSize) ||
      targetSize > CType::GetSize(cx, sourceType)) {
    JS_ReportError(cx,
      "target CType has undefined or larger size than source CType");
    return JS_FALSE;
  }

  // Construct a new CData object with a type of 'targetType' and a referent
  // of 'sourceData'.
  void* data = CData::GetData(cx, sourceData);
  JSObject* result = CData::Create(cx, targetType, sourceData, data, false);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
  return JS_TRUE;
}

JSBool
CData::ReadString(JSContext* cx, uintN argc, jsval* vp)
{
  if (argc != 0) {
    JS_ReportError(cx, "readString takes zero arguments");
    return JS_FALSE;
  }

  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj || !IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  // Make sure we are a pointer to, or an array of, an 8-bit or 16-bit
  // character or integer type.
  JSObject* baseType;
  JSObject* typeObj = GetCType(cx, obj);
  TypeCode typeCode = CType::GetTypeCode(cx, typeObj);
  void* data;
  size_t maxLength = -1;
  switch (typeCode) {
  case TYPE_pointer:
    baseType = PointerType::GetBaseType(cx, typeObj);
    data = *static_cast<void**>(GetData(cx, obj));
    if (data == NULL) {
      JS_ReportError(cx, "cannot read contents of null pointer");
      return JS_FALSE;
    }
    break;
  case TYPE_array:
    baseType = ArrayType::GetBaseType(cx, typeObj);
    data = GetData(cx, obj);
    maxLength = ArrayType::GetLength(cx, typeObj);
    break;
  default:
    JS_ReportError(cx, "not a PointerType or ArrayType");
    return JS_FALSE;
  }

  // Convert the string buffer, taking care to determine the correct string
  // length in the case of arrays (which may contain embedded nulls).
  JSString* result;
  switch (CType::GetTypeCode(cx, baseType)) {
  case TYPE_int8_t:
  case TYPE_uint8_t:
  case TYPE_char:
  case TYPE_signed_char:
  case TYPE_unsigned_char: {
    char* bytes = static_cast<char*>(data);
    size_t length = strnlen(bytes, maxLength);

    // Determine the length.
    size_t dstlen;
    if (!js_InflateUTF8StringToBuffer(cx, bytes, length, NULL, &dstlen))
      return JS_FALSE;

    jschar* dst =
      static_cast<jschar*>(JS_malloc(cx, (dstlen + 1) * sizeof(jschar)));
    if (!dst)
      return JS_FALSE;

    ASSERT_OK(js_InflateUTF8StringToBuffer(cx, bytes, length, dst, &dstlen));
    dst[dstlen] = 0;

    result = JS_NewUCString(cx, dst, dstlen);
    break;
  }
  case TYPE_int16_t:
  case TYPE_uint16_t:
  case TYPE_short:
  case TYPE_unsigned_short:
  case TYPE_jschar: {
    jschar* chars = static_cast<jschar*>(data);
    size_t length = strnlen(chars, maxLength);
    result = JS_NewUCStringCopyN(cx, chars, length);
    break;
  }
  default:
    JS_ReportError(cx,
      "base type is not an 8-bit or 16-bit integer or character type");
    return JS_FALSE;
  }

  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(result));
  return JS_TRUE;
}

JSBool
CData::ToSource(JSContext* cx, uintN argc, jsval* vp)
{
  if (argc != 0) {
    JS_ReportError(cx, "toSource takes zero arguments");
    return JS_FALSE;
  }

  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj || !CData::IsCData(cx, obj)) {
    JS_ReportError(cx, "not a CData");
    return JS_FALSE;
  }

  JSObject* typeObj = CData::GetCType(cx, obj);
  void* data = CData::GetData(cx, obj);

  // Walk the types, building up the toSource() string.
  // First, we build up the type expression:
  // 't.ptr' for pointers;
  // 't.array([n])' for arrays;
  // 'n' for structs, where n = t.name, the struct's name. (We assume this is
  // bound to a variable in the current scope.)
  AutoString source;
  BuildTypeSource(cx, typeObj, true, source);
  AppendString(source, "(");
  if (!BuildDataSource(cx, typeObj, data, false, source))
    return JS_FALSE;

  AppendString(source, ")");

  JSString* result = NewUCString(cx, source);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(result));
  return JS_TRUE;
}

/*******************************************************************************
** Int64 and UInt64 implementation
*******************************************************************************/

JSObject*
Int64Base::Construct(JSContext* cx,
                     JSObject* proto,
                     JSUint64 data,
                     bool isUnsigned)
{
  JSClass* clasp = isUnsigned ? &sUInt64Class : &sInt64Class;
  JSObject* result = JS_NewObject(cx, clasp, proto, JS_GetParent(cx, proto));
  if (!result)
    return NULL;
  js::AutoObjectRooter root(cx, result);

  // attach the Int64's data
  JSUint64* buffer = new JSUint64(data);
  if (!buffer) {
    JS_ReportOutOfMemory(cx);
    return NULL;
  }

  if (!JS_SetReservedSlot(cx, result, SLOT_INT64, PRIVATE_TO_JSVAL(buffer))) {
    delete buffer;
    return NULL;
  }

  if (!JS_FreezeObject(cx, result))
    return NULL;

  return result;
}

void
Int64Base::Finalize(JSContext* cx, JSObject* obj)
{
  jsval slot;
  if (!JS_GetReservedSlot(cx, obj, SLOT_INT64, &slot) || JSVAL_IS_VOID(slot))
    return;

  delete static_cast<JSUint64*>(JSVAL_TO_PRIVATE(slot));
}

JSUint64
Int64Base::GetInt(JSContext* cx, JSObject* obj) {
  JS_ASSERT(Int64::IsInt64(cx, obj) || UInt64::IsUInt64(cx, obj));

  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, obj, SLOT_INT64, &slot));
  return *static_cast<JSUint64*>(JSVAL_TO_PRIVATE(slot));
}

JSBool
Int64Base::ToString(JSContext* cx,
                    JSObject* obj,
                    uintN argc,
                    jsval* vp,
                    bool isUnsigned)
{
  if (argc > 1) {
    JS_ReportError(cx, "toString takes zero or one argument");
    return JS_FALSE;
  }

  jsuint radix = 10;
  if (argc == 1) {
    jsval arg = JS_ARGV(cx, vp)[0];
    if (JSVAL_IS_INT(arg))
      radix = JSVAL_TO_INT(arg);
    if (!JSVAL_IS_INT(arg) || radix < 2 || radix > 36) {
      JS_ReportError(cx, "radix argument must be an integer between 2 and 36");
      return JS_FALSE;
    }
  }

  AutoString intString;
  if (isUnsigned) {
    IntegerToString(GetInt(cx, obj), radix, intString);
  } else {
    IntegerToString(static_cast<JSInt64>(GetInt(cx, obj)), radix, intString);
  }

  JSString *result = NewUCString(cx, intString);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(result));
  return JS_TRUE;
}

JSBool
Int64Base::ToSource(JSContext* cx,
                    JSObject* obj,
                    uintN argc,
                    jsval* vp,
                    bool isUnsigned)
{
  if (argc != 0) {
    JS_ReportError(cx, "toSource takes zero arguments");
    return JS_FALSE;
  }

  // Return a decimal string suitable for constructing the number.
  AutoString source;
  if (isUnsigned) {
    AppendString(source, "ctypes.UInt64(\"");
    IntegerToString(GetInt(cx, obj), 10, source);
  } else {
    AppendString(source, "ctypes.Int64(\"");
    IntegerToString(static_cast<JSInt64>(GetInt(cx, obj)), 10, source);
  }
  AppendString(source, "\")");

  JSString *result = NewUCString(cx, source);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(result));
  return JS_TRUE;
}

JSBool
Int64::Construct(JSContext* cx,
                 uintN argc,
                 jsval* vp)
{
  // Construct and return a new Int64 object.
  if (argc != 1) {
    JS_ReportError(cx, "Int64 takes one argument");
    return JS_FALSE;
  }

  jsval* argv = JS_ARGV(cx, vp);
  JSInt64 i = 0;
  if (!jsvalToBigInteger(cx, argv[0], true, &i))
    return TypeError(cx, "int64", argv[0]);

  // Get ctypes.Int64.prototype from the 'prototype' property of the ctor.
  jsval slot;
  ASSERT_OK(JS_GetProperty(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)),
    "prototype", &slot));
  JSObject* proto = JSVAL_TO_OBJECT(slot);
  JS_ASSERT(JS_GET_CLASS(cx, proto) == &sInt64ProtoClass);

  JSObject* result = Int64Base::Construct(cx, proto, i, false);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
  return JS_TRUE;
}

bool
Int64::IsInt64(JSContext* cx, JSObject* obj)
{
  return JS_GET_CLASS(cx, obj) == &sInt64Class;
}

JSBool
Int64::ToString(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj || !Int64::IsInt64(cx, obj)) {
    JS_ReportError(cx, "not an Int64");
    return JS_FALSE;
  }

  return Int64Base::ToString(cx, obj, argc, vp, false);
}

JSBool
Int64::ToSource(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj || !Int64::IsInt64(cx, obj)) {
    JS_ReportError(cx, "not an Int64");
    return JS_FALSE;
  }

  return Int64Base::ToSource(cx, obj, argc, vp, false);
}

JSBool
Int64::Compare(JSContext* cx, uintN argc, jsval* vp)
{
  jsval* argv = JS_ARGV(cx, vp);
  if (argc != 2 ||
      JSVAL_IS_PRIMITIVE(argv[0]) ||
      JSVAL_IS_PRIMITIVE(argv[1]) ||
      !Int64::IsInt64(cx, JSVAL_TO_OBJECT(argv[0])) ||
      !Int64::IsInt64(cx, JSVAL_TO_OBJECT(argv[1]))) {
    JS_ReportError(cx, "compare takes two Int64 arguments");
    return JS_FALSE;
  }

  JSObject* obj1 = JSVAL_TO_OBJECT(argv[0]);
  JSObject* obj2 = JSVAL_TO_OBJECT(argv[1]);

  JSInt64 i1 = Int64Base::GetInt(cx, obj1);
  JSInt64 i2 = Int64Base::GetInt(cx, obj2);

  if (i1 == i2)
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(0));
  else if (i1 < i2)
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(-1));
  else
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(1));

  return JS_TRUE;
}

#define LO_MASK ((JSUint64(1) << 32) - 1)
#define INT64_LO(i) ((i) & LO_MASK)
#define INT64_HI(i) ((i) >> 32)

JSBool
Int64::Lo(JSContext* cx, uintN argc, jsval* vp)
{
  jsval* argv = JS_ARGV(cx, vp);
  if (argc != 1 || JSVAL_IS_PRIMITIVE(argv[0]) ||
      !Int64::IsInt64(cx, JSVAL_TO_OBJECT(argv[0]))) {
    JS_ReportError(cx, "lo takes one Int64 argument");
    return JS_FALSE;
  }

  JSObject* obj = JSVAL_TO_OBJECT(argv[0]);
  JSInt64 u = Int64Base::GetInt(cx, obj);
  jsdouble d = JSUint32(INT64_LO(u));

  jsval result;
  if (!JS_NewNumberValue(cx, d, &result))
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, result);
  return JS_TRUE;
}

JSBool
Int64::Hi(JSContext* cx, uintN argc, jsval* vp)
{
  jsval* argv = JS_ARGV(cx, vp);
  if (argc != 1 || JSVAL_IS_PRIMITIVE(argv[0]) ||
      !Int64::IsInt64(cx, JSVAL_TO_OBJECT(argv[0]))) {
    JS_ReportError(cx, "hi takes one Int64 argument");
    return JS_FALSE;
  }

  JSObject* obj = JSVAL_TO_OBJECT(argv[0]);
  JSInt64 u = Int64Base::GetInt(cx, obj);
  jsdouble d = JSInt32(INT64_HI(u));

  jsval result;
  if (!JS_NewNumberValue(cx, d, &result))
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, result);
  return JS_TRUE;
}

JSBool
Int64::Join(JSContext* cx, uintN argc, jsval* vp)
{
  if (argc != 2) {
    JS_ReportError(cx, "join takes two arguments");
    return JS_FALSE;
  }

  jsval* argv = JS_ARGV(cx, vp);
  JSInt32 hi;
  JSUint32 lo;
  if (!jsvalToInteger(cx, argv[0], &hi))
    return TypeError(cx, "int32", argv[0]);
  if (!jsvalToInteger(cx, argv[1], &lo))
    return TypeError(cx, "uint32", argv[1]);

  JSInt64 i = (JSInt64(hi) << 32) + JSInt64(lo);

  // Get Int64.prototype from the function's reserved slot.
  JSObject* callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));

  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, callee, SLOT_FN_INT64PROTO, &slot));
  JSObject* proto = JSVAL_TO_OBJECT(slot);
  JS_ASSERT(JS_GET_CLASS(cx, proto) == &sInt64ProtoClass);

  JSObject* result = Int64Base::Construct(cx, proto, i, false);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
  return JS_TRUE;
}

JSBool
UInt64::Construct(JSContext* cx,
                  uintN argc,
                  jsval* vp)
{
  // Construct and return a new UInt64 object.
  if (argc != 1) {
    JS_ReportError(cx, "UInt64 takes one argument");
    return JS_FALSE;
  }

  jsval* argv = JS_ARGV(cx, vp);
  JSUint64 u = 0;
  if (!jsvalToBigInteger(cx, argv[0], true, &u))
    return TypeError(cx, "uint64", argv[0]);

  // Get ctypes.UInt64.prototype from the 'prototype' property of the ctor.
  jsval slot;
  ASSERT_OK(JS_GetProperty(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)),
    "prototype", &slot));
  JSObject* proto = JSVAL_TO_OBJECT(slot);
  JS_ASSERT(JS_GET_CLASS(cx, proto) == &sUInt64ProtoClass);

  JSObject* result = Int64Base::Construct(cx, proto, u, true);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
  return JS_TRUE;
}

bool
UInt64::IsUInt64(JSContext* cx, JSObject* obj)
{
  return JS_GET_CLASS(cx, obj) == &sUInt64Class;
}

JSBool
UInt64::ToString(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj || !UInt64::IsUInt64(cx, obj)) {
    JS_ReportError(cx, "not a UInt64");
    return JS_FALSE;
  }

  return Int64Base::ToString(cx, obj, argc, vp, true);
}

JSBool
UInt64::ToSource(JSContext* cx, uintN argc, jsval* vp)
{
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj || !UInt64::IsUInt64(cx, obj)) {
    JS_ReportError(cx, "not a UInt64");
    return JS_FALSE;
  }

  return Int64Base::ToSource(cx, obj, argc, vp, true);
}

JSBool
UInt64::Compare(JSContext* cx, uintN argc, jsval* vp)
{
  jsval* argv = JS_ARGV(cx, vp);
  if (argc != 2 ||
      JSVAL_IS_PRIMITIVE(argv[0]) ||
      JSVAL_IS_PRIMITIVE(argv[1]) ||
      !UInt64::IsUInt64(cx, JSVAL_TO_OBJECT(argv[0])) ||
      !UInt64::IsUInt64(cx, JSVAL_TO_OBJECT(argv[1]))) {
    JS_ReportError(cx, "compare takes two UInt64 arguments");
    return JS_FALSE;
  }

  JSObject* obj1 = JSVAL_TO_OBJECT(argv[0]);
  JSObject* obj2 = JSVAL_TO_OBJECT(argv[1]);

  JSUint64 u1 = Int64Base::GetInt(cx, obj1);
  JSUint64 u2 = Int64Base::GetInt(cx, obj2);

  if (u1 == u2)
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(0));
  else if (u1 < u2)
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(-1));
  else
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(1));

  return JS_TRUE;
}

JSBool
UInt64::Lo(JSContext* cx, uintN argc, jsval* vp)
{
  jsval* argv = JS_ARGV(cx, vp);
  if (argc != 1 || JSVAL_IS_PRIMITIVE(argv[0]) ||
      !UInt64::IsUInt64(cx, JSVAL_TO_OBJECT(argv[0]))) {
    JS_ReportError(cx, "lo takes one UInt64 argument");
    return JS_FALSE;
  }

  JSObject* obj = JSVAL_TO_OBJECT(argv[0]);
  JSUint64 u = Int64Base::GetInt(cx, obj);
  jsdouble d = JSUint32(INT64_LO(u));

  jsval result;
  if (!JS_NewNumberValue(cx, d, &result))
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, result);
  return JS_TRUE;
}

JSBool
UInt64::Hi(JSContext* cx, uintN argc, jsval* vp)
{
  jsval* argv = JS_ARGV(cx, vp);
  if (argc != 1 || JSVAL_IS_PRIMITIVE(argv[0]) ||
      !UInt64::IsUInt64(cx, JSVAL_TO_OBJECT(argv[0]))) {
    JS_ReportError(cx, "hi takes one UInt64 argument");
    return JS_FALSE;
  }

  JSObject* obj = JSVAL_TO_OBJECT(argv[0]);
  JSUint64 u = Int64Base::GetInt(cx, obj);
  jsdouble d = JSUint32(INT64_HI(u));

  jsval result;
  if (!JS_NewNumberValue(cx, d, &result))
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, result);
  return JS_TRUE;
}

JSBool
UInt64::Join(JSContext* cx, uintN argc, jsval* vp)
{
  if (argc != 2) {
    JS_ReportError(cx, "join takes two arguments");
    return JS_FALSE;
  }

  jsval* argv = JS_ARGV(cx, vp);
  JSUint32 hi;
  JSUint32 lo;
  if (!jsvalToInteger(cx, argv[0], &hi))
    return TypeError(cx, "uint32_t", argv[0]);
  if (!jsvalToInteger(cx, argv[1], &lo))
    return TypeError(cx, "uint32_t", argv[1]);

  JSUint64 u = (JSUint64(hi) << 32) + JSUint64(lo);

  // Get UInt64.prototype from the function's reserved slot.
  JSObject* callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));

  jsval slot;
  ASSERT_OK(JS_GetReservedSlot(cx, callee, SLOT_FN_INT64PROTO, &slot));
  JSObject* proto = JSVAL_TO_OBJECT(slot);
  JS_ASSERT(JS_GET_CLASS(cx, proto) == &sUInt64ProtoClass);

  JSObject* result = Int64Base::Construct(cx, proto, u, true);
  if (!result)
    return JS_FALSE;

  JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(result));
  return JS_TRUE;
}

}
}

