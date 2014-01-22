/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ctypes/CTypes.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/NumericLimits.h"

#include <math.h>
#include <stdint.h>

#if defined(XP_WIN) || defined(XP_OS2)
#include <float.h>
#endif

#if defined(SOLARIS)
#include <ieeefp.h>
#endif

#ifdef HAVE_SSIZE_T
#include <sys/types.h>
#endif

#if defined(XP_UNIX)
#include <errno.h>
#elif defined(XP_WIN)
#include <windows.h>
#endif

#include "jsnum.h"
#include "jsprf.h"

#include "builtin/TypeRepresentation.h"
#include "ctypes/Library.h"

using namespace std;
using mozilla::NumericLimits;

namespace js {
namespace ctypes {

size_t
GetDeflatedUTF8StringLength(JSContext *maybecx, const jschar *chars,
                            size_t nchars)
{
    size_t nbytes;
    const jschar *end;
    unsigned c, c2;
    char buffer[10];

    nbytes = nchars;
    for (end = chars + nchars; chars != end; chars++) {
        c = *chars;
        if (c < 0x80)
            continue;
        if (0xD800 <= c && c <= 0xDFFF) {
            /* Surrogate pair. */
            chars++;

            /* nbytes sets 1 length since this is surrogate pair. */
            nbytes--;
            if (c >= 0xDC00 || chars == end)
                goto bad_surrogate;
            c2 = *chars;
            if (c2 < 0xDC00 || c2 > 0xDFFF)
                goto bad_surrogate;
            c = ((c - 0xD800) << 10) + (c2 - 0xDC00) + 0x10000;
        }
        c >>= 11;
        nbytes++;
        while (c) {
            c >>= 5;
            nbytes++;
        }
    }
    return nbytes;

  bad_surrogate:
    if (maybecx) {
        JS_snprintf(buffer, 10, "0x%x", c);
        JS_ReportErrorFlagsAndNumber(maybecx, JSREPORT_ERROR, js_GetErrorMessage,
                                     nullptr, JSMSG_BAD_SURROGATE_CHAR, buffer);
    }
    return (size_t) -1;
}

bool
DeflateStringToUTF8Buffer(JSContext *maybecx, const jschar *src, size_t srclen,
                              char *dst, size_t *dstlenp)
{
    size_t i, utf8Len;
    jschar c, c2;
    uint32_t v;
    uint8_t utf8buf[6];

    size_t dstlen = *dstlenp;
    size_t origDstlen = dstlen;

    while (srclen) {
        c = *src++;
        srclen--;
        if (c >= 0xDC00 && c <= 0xDFFF)
            goto badSurrogate;
        if (c < 0xD800 || c > 0xDBFF) {
            v = c;
        } else {
            if (srclen < 1)
                goto badSurrogate;
            c2 = *src;
            if ((c2 < 0xDC00) || (c2 > 0xDFFF))
                goto badSurrogate;
            src++;
            srclen--;
            v = ((c - 0xD800) << 10) + (c2 - 0xDC00) + 0x10000;
        }
        if (v < 0x0080) {
            /* no encoding necessary - performance hack */
            if (dstlen == 0)
                goto bufferTooSmall;
            *dst++ = (char) v;
            utf8Len = 1;
        } else {
            utf8Len = js_OneUcs4ToUtf8Char(utf8buf, v);
            if (utf8Len > dstlen)
                goto bufferTooSmall;
            for (i = 0; i < utf8Len; i++)
                *dst++ = (char) utf8buf[i];
        }
        dstlen -= utf8Len;
    }
    *dstlenp = (origDstlen - dstlen);
    return true;

badSurrogate:
    *dstlenp = (origDstlen - dstlen);
    /* Delegate error reporting to the measurement function. */
    if (maybecx)
        GetDeflatedUTF8StringLength(maybecx, src - 1, srclen + 1);
    return false;

bufferTooSmall:
    *dstlenp = (origDstlen - dstlen);
    if (maybecx) {
        JS_ReportErrorNumber(maybecx, js_GetErrorMessage, nullptr,
                             JSMSG_BUFFER_TOO_SMALL);
    }
    return false;
}

/*******************************************************************************
** JSAPI function prototypes
*******************************************************************************/

// We use an enclosing struct here out of paranoia about the ability of gcc 4.4
// (and maybe 4.5) to correctly compile this if it were a template function.
// See also the comments in dom/workers/Events.cpp (and other adjacent files) by
// the |struct Property| there.
template<JS::IsAcceptableThis Test, JS::NativeImpl Impl>
struct Property
{
  static bool
  Fun(JSContext* cx, unsigned argc, JS::Value* vp)
  {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    return JS::CallNonGenericMethod<Test, Impl>(cx, args);
  }
};

static bool ConstructAbstract(JSContext* cx, unsigned argc, jsval* vp);

namespace CType {
  static bool ConstructData(JSContext* cx, unsigned argc, jsval* vp);
  static bool ConstructBasic(JSContext* cx, HandleObject obj, const CallArgs& args);

  static void Trace(JSTracer* trc, JSObject* obj);
  static void Finalize(JSFreeOp *fop, JSObject* obj);

  bool IsCType(HandleValue v);
  bool IsCTypeOrProto(HandleValue v);

  bool PrototypeGetter(JSContext* cx, JS::CallArgs args);
  bool NameGetter(JSContext* cx, JS::CallArgs args);
  bool SizeGetter(JSContext* cx, JS::CallArgs args);
  bool PtrGetter(JSContext* cx, JS::CallArgs args);

  static bool CreateArray(JSContext* cx, unsigned argc, jsval* vp);
  static bool ToString(JSContext* cx, unsigned argc, jsval* vp);
  static bool ToSource(JSContext* cx, unsigned argc, jsval* vp);
  static bool HasInstance(JSContext* cx, HandleObject obj, MutableHandleValue v, bool* bp);


  /*
   * Get the global "ctypes" object.
   *
   * |obj| must be a CType object.
   *
   * This function never returns nullptr.
   */
  static JSObject* GetGlobalCTypes(JSContext* cx, JSObject* obj);

}

namespace ABI {
  bool IsABI(JSObject* obj);
  static bool ToSource(JSContext* cx, unsigned argc, jsval* vp);
}

namespace PointerType {
  static bool Create(JSContext* cx, unsigned argc, jsval* vp);
  static bool ConstructData(JSContext* cx, HandleObject obj, const CallArgs& args);

  bool IsPointerType(HandleValue v);
  bool IsPointer(HandleValue v);

  bool TargetTypeGetter(JSContext* cx, JS::CallArgs args);
  bool ContentsGetter(JSContext* cx, JS::CallArgs args);
  bool ContentsSetter(JSContext* cx, JS::CallArgs args);

  static bool IsNull(JSContext* cx, unsigned argc, jsval* vp);
  static bool Increment(JSContext* cx, unsigned argc, jsval* vp);
  static bool Decrement(JSContext* cx, unsigned argc, jsval* vp);
  // The following is not an instance function, since we don't want to expose arbitrary
  // pointer arithmetic at this moment.
  static bool OffsetBy(JSContext* cx, const CallArgs& args, int offset);
}

namespace ArrayType {
  bool IsArrayType(HandleValue v);
  bool IsArrayOrArrayType(HandleValue v);

  static bool Create(JSContext* cx, unsigned argc, jsval* vp);
  static bool ConstructData(JSContext* cx, HandleObject obj, const CallArgs& args);

  bool ElementTypeGetter(JSContext* cx, JS::CallArgs args);
  bool LengthGetter(JSContext* cx, JS::CallArgs args);

  static bool Getter(JSContext* cx, HandleObject obj, HandleId idval, MutableHandleValue vp);
  static bool Setter(JSContext* cx, HandleObject obj, HandleId idval, bool strict, MutableHandleValue vp);
  static bool AddressOfElement(JSContext* cx, unsigned argc, jsval* vp);
}

namespace StructType {
  bool IsStruct(HandleValue v);

  static bool Create(JSContext* cx, unsigned argc, jsval* vp);
  static bool ConstructData(JSContext* cx, HandleObject obj, const CallArgs& args);

  bool FieldsArrayGetter(JSContext* cx, JS::CallArgs args);

  static bool FieldGetter(JSContext* cx, HandleObject obj, HandleId idval,
    MutableHandleValue vp);
  static bool FieldSetter(JSContext* cx, HandleObject obj, HandleId idval, bool strict,
                            MutableHandleValue vp);
  static bool AddressOfField(JSContext* cx, unsigned argc, jsval* vp);
  static bool Define(JSContext* cx, unsigned argc, jsval* vp);
}

namespace FunctionType {
  static bool Create(JSContext* cx, unsigned argc, jsval* vp);
  static bool ConstructData(JSContext* cx, HandleObject typeObj,
    HandleObject dataObj, HandleObject fnObj, HandleObject thisObj, jsval errVal);

  static bool Call(JSContext* cx, unsigned argc, jsval* vp);

  bool IsFunctionType(HandleValue v);

  bool ArgTypesGetter(JSContext* cx, JS::CallArgs args);
  bool ReturnTypeGetter(JSContext* cx, JS::CallArgs args);
  bool ABIGetter(JSContext* cx, JS::CallArgs args);
  bool IsVariadicGetter(JSContext* cx, JS::CallArgs args);
}

namespace CClosure {
  static void Trace(JSTracer* trc, JSObject* obj);
  static void Finalize(JSFreeOp *fop, JSObject* obj);

  // libffi callback
  static void ClosureStub(ffi_cif* cif, void* result, void** args,
    void* userData);
}

namespace CData {
  static void Finalize(JSFreeOp *fop, JSObject* obj);

  bool ValueGetter(JSContext* cx, JS::CallArgs args);
  bool ValueSetter(JSContext* cx, JS::CallArgs args);

  static bool Address(JSContext* cx, unsigned argc, jsval* vp);
  static bool ReadString(JSContext* cx, unsigned argc, jsval* vp);
  static bool ReadStringReplaceMalformed(JSContext* cx, unsigned argc, jsval* vp);
  static bool ToSource(JSContext* cx, unsigned argc, jsval* vp);
  static JSString *GetSourceString(JSContext *cx, HandleObject typeObj,
                                   void *data);

  bool ErrnoGetter(JSContext* cx, JS::CallArgs args);

#if defined(XP_WIN)
  bool LastErrorGetter(JSContext* cx, JS::CallArgs args);
#endif // defined(XP_WIN)
}

namespace CDataFinalizer {
  /*
   * Attach a C function as a finalizer to a JS object.
   *
   * This function is available from JS as |ctypes.withFinalizer|.
   *
   * JavaScript signature:
   * function(CData, CData):   CDataFinalizer
   *          value  finalizer finalizable
   *
   * Where |finalizer| is a one-argument function taking a value
   * with the same type as |value|.
   */
  static bool Construct(JSContext* cx, unsigned argc, jsval *vp);

  /*
   * Private data held by |CDataFinalizer|.
   *
   * See also |enum CDataFinalizerSlot| for the slots of
   * |CDataFinalizer|.
   *
   * Note: the private data may be nullptr, if |dispose|, |forget| or the
   * finalizer has already been called.
   */
  struct Private {
    /*
     * The C data to pass to the code.
     * Finalization/|dispose|/|forget| release this memory.
     */
    void *cargs;

    /*
     * The total size of the buffer pointed by |cargs|
     */
    size_t cargs_size;

    /*
     * Low-level signature information.
     * Finalization/|dispose|/|forget| release this memory.
     */
    ffi_cif CIF;

    /*
     * The C function to invoke during finalization.
     * Do not deallocate this.
     */
    uintptr_t code;

    /*
     * A buffer for holding the return value.
     * Finalization/|dispose|/|forget| release this memory.
     */
    void *rvalue;
  };

  /*
   * Methods of instances of |CDataFinalizer|
   */
  namespace Methods {
    static bool Dispose(JSContext* cx, unsigned argc, jsval *vp);
    static bool Forget(JSContext* cx, unsigned argc, jsval *vp);
    static bool ToSource(JSContext* cx, unsigned argc, jsval *vp);
    static bool ToString(JSContext* cx, unsigned argc, jsval *vp);
  }

  /*
   * Utility functions
   *
   * @return true if |obj| is a CDataFinalizer, false otherwise.
   */
  static bool IsCDataFinalizer(JSObject *obj);

  /*
   * Clean up the finalization information of a CDataFinalizer.
   *
   * Used by |Finalize|, |Dispose| and |Forget|.
   *
   * @param p The private information of the CDataFinalizer. If nullptr,
   * this function does nothing.
   * @param obj Either nullptr, if the object should not be cleaned up (i.e.
   * during finalization) or a CDataFinalizer JSObject. Always use nullptr
   * if you are calling from a finalizer.
   */
  static void Cleanup(Private *p, JSObject *obj);

  /*
   * Perform the actual call to the finalizer code.
   */
  static void CallFinalizer(CDataFinalizer::Private *p,
                            int* errnoStatus,
                            int32_t* lastErrorStatus);

  /*
   * Return the CType of a CDataFinalizer object, or nullptr if the object
   * has been cleaned-up already.
   */
  static JSObject *GetCType(JSContext *cx, JSObject *obj);

  /*
   * Perform finalization of a |CDataFinalizer|
   */
  static void Finalize(JSFreeOp *fop, JSObject *obj);

  /*
   * Return the jsval contained by this finalizer.
   *
   * Note that the jsval is actually not recorded, but converted back from C.
   */
  static bool GetValue(JSContext *cx, JSObject *obj, jsval *result);

  static JSObject* GetCData(JSContext *cx, JSObject *obj);
 }


// Int64Base provides functions common to Int64 and UInt64.
namespace Int64Base {
  JSObject* Construct(JSContext* cx, HandleObject proto, uint64_t data,
    bool isUnsigned);

  uint64_t GetInt(JSObject* obj);

  bool ToString(JSContext* cx, JSObject* obj, const CallArgs& args,
                bool isUnsigned);

  bool ToSource(JSContext* cx, JSObject* obj, const CallArgs& args,
                bool isUnsigned);

  static void Finalize(JSFreeOp *fop, JSObject* obj);
}

namespace Int64 {
  static bool Construct(JSContext* cx, unsigned argc, jsval* vp);

  static bool ToString(JSContext* cx, unsigned argc, jsval* vp);
  static bool ToSource(JSContext* cx, unsigned argc, jsval* vp);

  static bool Compare(JSContext* cx, unsigned argc, jsval* vp);
  static bool Lo(JSContext* cx, unsigned argc, jsval* vp);
  static bool Hi(JSContext* cx, unsigned argc, jsval* vp);
  static bool Join(JSContext* cx, unsigned argc, jsval* vp);
}

namespace UInt64 {
  static bool Construct(JSContext* cx, unsigned argc, jsval* vp);

  static bool ToString(JSContext* cx, unsigned argc, jsval* vp);
  static bool ToSource(JSContext* cx, unsigned argc, jsval* vp);

  static bool Compare(JSContext* cx, unsigned argc, jsval* vp);
  static bool Lo(JSContext* cx, unsigned argc, jsval* vp);
  static bool Hi(JSContext* cx, unsigned argc, jsval* vp);
  static bool Join(JSContext* cx, unsigned argc, jsval* vp);
}

/*******************************************************************************
** JSClass definitions and initialization functions
*******************************************************************************/

// Class representing the 'ctypes' object itself. This exists to contain the
// JSCTypesCallbacks set of function pointers.
static const JSClass sCTypesGlobalClass = {
  "ctypes",
  JSCLASS_HAS_RESERVED_SLOTS(CTYPESGLOBAL_SLOTS),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

static const JSClass sCABIClass = {
  "CABI",
  JSCLASS_HAS_RESERVED_SLOTS(CABI_SLOTS),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

// Class representing ctypes.{C,Pointer,Array,Struct,Function}Type.prototype.
// This exists to give said prototypes a class of "CType", and to provide
// reserved slots for stashing various other prototype objects.
static const JSClass sCTypeProtoClass = {
  "CType",
  JSCLASS_HAS_RESERVED_SLOTS(CTYPEPROTO_SLOTS),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr,
  nullptr, ConstructAbstract, nullptr, ConstructAbstract
};

// Class representing ctypes.CData.prototype and the 'prototype' properties
// of CTypes. This exists to give said prototypes a class of "CData".
static const JSClass sCDataProtoClass = {
  "CData",
  0,
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

static const JSClass sCTypeClass = {
  "CType",
  JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_HAS_RESERVED_SLOTS(CTYPE_SLOTS),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, CType::Finalize,
  nullptr, CType::ConstructData, CType::HasInstance, CType::ConstructData,
  CType::Trace
};

static const JSClass sCDataClass = {
  "CData",
  JSCLASS_HAS_RESERVED_SLOTS(CDATA_SLOTS),
  JS_PropertyStub, JS_DeletePropertyStub, ArrayType::Getter, ArrayType::Setter,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, CData::Finalize,
  nullptr, FunctionType::Call, nullptr, FunctionType::Call
};

static const JSClass sCClosureClass = {
  "CClosure",
  JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_HAS_RESERVED_SLOTS(CCLOSURE_SLOTS),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, CClosure::Finalize,
  nullptr, nullptr, nullptr, nullptr, CClosure::Trace
};

/*
 * Class representing the prototype of CDataFinalizer.
 */
static const JSClass sCDataFinalizerProtoClass = {
  "CDataFinalizer",
  0,
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

/*
 * Class representing instances of CDataFinalizer.
 *
 * Instances of CDataFinalizer have both private data (with type
 * |CDataFinalizer::Private|) and slots (see |CDataFinalizerSlots|).
 */
static const JSClass sCDataFinalizerClass = {
  "CDataFinalizer",
  JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(CDATAFINALIZER_SLOTS),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, CDataFinalizer::Finalize,
};


#define CTYPESFN_FLAGS \
  (JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)

#define CTYPESCTOR_FLAGS \
  (CTYPESFN_FLAGS | JSFUN_CONSTRUCTOR)

#define CTYPESACC_FLAGS \
  (JSPROP_ENUMERATE | JSPROP_PERMANENT)

#define CABIFN_FLAGS \
  (JSPROP_READONLY | JSPROP_PERMANENT)

#define CDATAFN_FLAGS \
  (JSPROP_READONLY | JSPROP_PERMANENT)

#define CDATAFINALIZERFN_FLAGS \
  (JSPROP_READONLY | JSPROP_PERMANENT)

static const JSPropertySpec sCTypeProps[] = {
  JS_PSG("name",
         (Property<CType::IsCType, CType::NameGetter>::Fun),
         CTYPESACC_FLAGS),
  JS_PSG("size",
         (Property<CType::IsCType, CType::SizeGetter>::Fun),
         CTYPESACC_FLAGS),
  JS_PSG("ptr",
         (Property<CType::IsCType, CType::PtrGetter>::Fun),
         CTYPESACC_FLAGS),
  JS_PSG("prototype",
         (Property<CType::IsCTypeOrProto, CType::PrototypeGetter>::Fun),
         CTYPESACC_FLAGS),
  { 0, 0, 0, JSOP_NULLWRAPPER, JSOP_NULLWRAPPER }
};

static const JSFunctionSpec sCTypeFunctions[] = {
  JS_FN("array", CType::CreateArray, 0, CTYPESFN_FLAGS),
  JS_FN("toString", CType::ToString, 0, CTYPESFN_FLAGS),
  JS_FN("toSource", CType::ToSource, 0, CTYPESFN_FLAGS),
  JS_FS_END
};

static const JSFunctionSpec sCABIFunctions[] = {
  JS_FN("toSource", ABI::ToSource, 0, CABIFN_FLAGS),
  JS_FN("toString", ABI::ToSource, 0, CABIFN_FLAGS),
  JS_FS_END
};

static const JSPropertySpec sCDataProps[] = {
  JS_PSGS("value",
          (Property<CData::IsCData, CData::ValueGetter>::Fun),
          (Property<CData::IsCData, CData::ValueSetter>::Fun),
          JSPROP_PERMANENT),
  JS_PS_END
};

static const JSFunctionSpec sCDataFunctions[] = {
  JS_FN("address", CData::Address, 0, CDATAFN_FLAGS),
  JS_FN("readString", CData::ReadString, 0, CDATAFN_FLAGS),
  JS_FN("readStringReplaceMalformed", CData::ReadStringReplaceMalformed, 0, CDATAFN_FLAGS),
  JS_FN("toSource", CData::ToSource, 0, CDATAFN_FLAGS),
  JS_FN("toString", CData::ToSource, 0, CDATAFN_FLAGS),
  JS_FS_END
};

static const JSPropertySpec sCDataFinalizerProps[] = {
  { 0, 0, 0, JSOP_NULLWRAPPER, JSOP_NULLWRAPPER }
};

static const JSFunctionSpec sCDataFinalizerFunctions[] = {
  JS_FN("dispose",  CDataFinalizer::Methods::Dispose,  0, CDATAFINALIZERFN_FLAGS),
  JS_FN("forget",   CDataFinalizer::Methods::Forget,   0, CDATAFINALIZERFN_FLAGS),
  JS_FN("readString",CData::ReadString, 0, CDATAFINALIZERFN_FLAGS),
  JS_FN("toString", CDataFinalizer::Methods::ToString, 0, CDATAFINALIZERFN_FLAGS),
  JS_FN("toSource", CDataFinalizer::Methods::ToSource, 0, CDATAFINALIZERFN_FLAGS),
  JS_FS_END
};

static const JSFunctionSpec sPointerFunction =
  JS_FN("PointerType", PointerType::Create, 1, CTYPESCTOR_FLAGS);

static const JSPropertySpec sPointerProps[] = {
  JS_PSG("targetType",
         (Property<PointerType::IsPointerType, PointerType::TargetTypeGetter>::Fun),
         CTYPESACC_FLAGS),
  JS_PS_END
};

static const JSFunctionSpec sPointerInstanceFunctions[] = {
  JS_FN("isNull", PointerType::IsNull, 0, CTYPESFN_FLAGS),
  JS_FN("increment", PointerType::Increment, 0, CTYPESFN_FLAGS),
  JS_FN("decrement", PointerType::Decrement, 0, CTYPESFN_FLAGS),
  JS_FS_END
};

static const JSPropertySpec sPointerInstanceProps[] = {
  JS_PSGS("contents",
         (Property<PointerType::IsPointer, PointerType::ContentsGetter>::Fun),
         (Property<PointerType::IsPointer, PointerType::ContentsSetter>::Fun),
          JSPROP_PERMANENT),
  JS_PS_END
};

static const JSFunctionSpec sArrayFunction =
  JS_FN("ArrayType", ArrayType::Create, 1, CTYPESCTOR_FLAGS);

static const JSPropertySpec sArrayProps[] = {
  JS_PSG("elementType",
         (Property<ArrayType::IsArrayType, ArrayType::ElementTypeGetter>::Fun),
         CTYPESACC_FLAGS),
  JS_PSG("length",
         (Property<ArrayType::IsArrayOrArrayType, ArrayType::LengthGetter>::Fun),
         CTYPESACC_FLAGS),
  JS_PS_END
};

static const JSFunctionSpec sArrayInstanceFunctions[] = {
  JS_FN("addressOfElement", ArrayType::AddressOfElement, 1, CDATAFN_FLAGS),
  JS_FS_END
};

static const JSPropertySpec sArrayInstanceProps[] = {
  JS_PSG("length",
         (Property<ArrayType::IsArrayOrArrayType, ArrayType::LengthGetter>::Fun),
         JSPROP_PERMANENT),
  JS_PS_END
};

static const JSFunctionSpec sStructFunction =
  JS_FN("StructType", StructType::Create, 2, CTYPESCTOR_FLAGS);

static const JSPropertySpec sStructProps[] = {
  JS_PSG("fields",
         (Property<StructType::IsStruct, StructType::FieldsArrayGetter>::Fun),
         CTYPESACC_FLAGS),
  JS_PS_END
};

static const JSFunctionSpec sStructFunctions[] = {
  JS_FN("define", StructType::Define, 1, CDATAFN_FLAGS),
  JS_FS_END
};

static const JSFunctionSpec sStructInstanceFunctions[] = {
  JS_FN("addressOfField", StructType::AddressOfField, 1, CDATAFN_FLAGS),
  JS_FS_END
};

static const JSFunctionSpec sFunctionFunction =
  JS_FN("FunctionType", FunctionType::Create, 2, CTYPESCTOR_FLAGS);

static const JSPropertySpec sFunctionProps[] = {
  JS_PSG("argTypes",
         (Property<FunctionType::IsFunctionType, FunctionType::ArgTypesGetter>::Fun),
         CTYPESACC_FLAGS),
  JS_PSG("returnType",
         (Property<FunctionType::IsFunctionType, FunctionType::ReturnTypeGetter>::Fun),
         CTYPESACC_FLAGS),
  JS_PSG("abi",
         (Property<FunctionType::IsFunctionType, FunctionType::ABIGetter>::Fun),
         CTYPESACC_FLAGS),
  JS_PSG("isVariadic",
         (Property<FunctionType::IsFunctionType, FunctionType::IsVariadicGetter>::Fun),
         CTYPESACC_FLAGS),
  JS_PS_END
};

static const JSFunctionSpec sFunctionInstanceFunctions[] = {
  JS_FN("call", js_fun_call, 1, CDATAFN_FLAGS),
  JS_FN("apply", js_fun_apply, 2, CDATAFN_FLAGS),
  JS_FS_END
};

static const JSClass sInt64ProtoClass = {
  "Int64",
  0,
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

static const JSClass sUInt64ProtoClass = {
  "UInt64",
  0,
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub
};

static const JSClass sInt64Class = {
  "Int64",
  JSCLASS_HAS_RESERVED_SLOTS(INT64_SLOTS),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Int64Base::Finalize
};

static const JSClass sUInt64Class = {
  "UInt64",
  JSCLASS_HAS_RESERVED_SLOTS(INT64_SLOTS),
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, Int64Base::Finalize
};

static const JSFunctionSpec sInt64StaticFunctions[] = {
  JS_FN("compare", Int64::Compare, 2, CTYPESFN_FLAGS),
  JS_FN("lo", Int64::Lo, 1, CTYPESFN_FLAGS),
  JS_FN("hi", Int64::Hi, 1, CTYPESFN_FLAGS),
  JS_FN("join", Int64::Join, 2, CTYPESFN_FLAGS),
  JS_FS_END
};

static const JSFunctionSpec sUInt64StaticFunctions[] = {
  JS_FN("compare", UInt64::Compare, 2, CTYPESFN_FLAGS),
  JS_FN("lo", UInt64::Lo, 1, CTYPESFN_FLAGS),
  JS_FN("hi", UInt64::Hi, 1, CTYPESFN_FLAGS),
  JS_FN("join", UInt64::Join, 2, CTYPESFN_FLAGS),
  JS_FS_END
};

static const JSFunctionSpec sInt64Functions[] = {
  JS_FN("toString", Int64::ToString, 0, CTYPESFN_FLAGS),
  JS_FN("toSource", Int64::ToSource, 0, CTYPESFN_FLAGS),
  JS_FS_END
};

static const JSFunctionSpec sUInt64Functions[] = {
  JS_FN("toString", UInt64::ToString, 0, CTYPESFN_FLAGS),
  JS_FN("toSource", UInt64::ToSource, 0, CTYPESFN_FLAGS),
  JS_FS_END
};

static const JSPropertySpec sModuleProps[] = {
  JS_PSG("errno",
         (Property<IsCTypesGlobal, CData::ErrnoGetter>::Fun),
         JSPROP_PERMANENT),
#if defined(XP_WIN)
  JS_PSG("winLastError",
         (Property<IsCTypesGlobal, CData::LastErrorGetter>::Fun),
         JSPROP_PERMANENT),
#endif // defined(XP_WIN)
  JS_PS_END
};

static const JSFunctionSpec sModuleFunctions[] = {
  JS_FN("CDataFinalizer", CDataFinalizer::Construct, 2, CTYPESFN_FLAGS),
  JS_FN("open", Library::Open, 1, CTYPESFN_FLAGS),
  JS_FN("cast", CData::Cast, 2, CTYPESFN_FLAGS),
  JS_FN("getRuntime", CData::GetRuntime, 1, CTYPESFN_FLAGS),
  JS_FN("libraryName", Library::Name, 1, CTYPESFN_FLAGS),
  JS_FS_END
};

static JS_ALWAYS_INLINE JSString*
NewUCString(JSContext* cx, const AutoString& from)
{
  return JS_NewUCStringCopyN(cx, from.begin(), from.length());
}

/*
 * Return a size rounded up to a multiple of a power of two.
 *
 * Note: |align| must be a power of 2.
 */
static JS_ALWAYS_INLINE size_t
Align(size_t val, size_t align)
{
  // Ensure that align is a power of two.
  MOZ_ASSERT(align != 0 && (align & (align - 1)) == 0);
  return ((val - 1) | (align - 1)) + 1;
}

static ABICode
GetABICode(JSObject* obj)
{
  // make sure we have an object representing a CABI class,
  // and extract the enumerated class type from the reserved slot.
  if (JS_GetClass(obj) != &sCABIClass)
    return INVALID_ABI;

  jsval result = JS_GetReservedSlot(obj, SLOT_ABICODE);
  return ABICode(JSVAL_TO_INT(result));
}

static const JSErrorFormatString ErrorFormatString[CTYPESERR_LIMIT] = {
#define MSG_DEF(name, number, count, exception, format) \
  { format, count, exception } ,
#include "ctypes/ctypes.msg"
#undef MSG_DEF
};

static const JSErrorFormatString*
GetErrorMessage(void* userRef, const char* locale, const unsigned errorNumber)
{
  if (0 < errorNumber && errorNumber < CTYPESERR_LIMIT)
    return &ErrorFormatString[errorNumber];
  return nullptr;
}

static bool
TypeError(JSContext* cx, const char* expected, HandleValue actual)
{
  JSString* str = JS_ValueToSource(cx, actual);
  JSAutoByteString bytes;

  const char* src;
  if (str) {
    src = bytes.encodeLatin1(cx, str);
    if (!src)
      return false;
  } else {
    JS_ClearPendingException(cx);
    src = "<<error converting value to string>>";
  }
  JS_ReportErrorNumber(cx, GetErrorMessage, nullptr,
                       CTYPESMSG_TYPE_ERROR, expected, src);
  return false;
}

static JSObject*
InitCTypeClass(JSContext* cx, HandleObject parent)
{
  JSFunction *fun = JS_DefineFunction(cx, parent, "CType", ConstructAbstract, 0,
                                      CTYPESCTOR_FLAGS);
  if (!fun)
    return nullptr;

  RootedObject ctor(cx, JS_GetFunctionObject(fun));
  RootedObject fnproto(cx);
  if (!JS_GetPrototype(cx, ctor, &fnproto))
    return nullptr;
  JS_ASSERT(ctor);
  JS_ASSERT(fnproto);

  // Set up ctypes.CType.prototype.
  RootedObject prototype(cx, JS_NewObject(cx, &sCTypeProtoClass, fnproto, parent));
  if (!prototype)
    return nullptr;

  if (!JS_DefineProperty(cx, ctor, "prototype", OBJECT_TO_JSVAL(prototype),
         nullptr, nullptr, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return nullptr;

  if (!JS_DefineProperty(cx, prototype, "constructor", OBJECT_TO_JSVAL(ctor),
         nullptr, nullptr, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return nullptr;

  // Define properties and functions common to all CTypes.
  if (!JS_DefineProperties(cx, prototype, sCTypeProps) ||
      !JS_DefineFunctions(cx, prototype, sCTypeFunctions))
    return nullptr;

  if (!JS_FreezeObject(cx, ctor) || !JS_FreezeObject(cx, prototype))
    return nullptr;

  return prototype;
}

static JSObject*
InitABIClass(JSContext* cx, JSObject* parent)
{
  RootedObject obj(cx, JS_NewObject(cx, nullptr, NullPtr(), NullPtr()));

  if (!obj)
    return nullptr;

  if (!JS_DefineFunctions(cx, obj, sCABIFunctions))
    return nullptr;

  return obj;
}


static JSObject*
InitCDataClass(JSContext* cx, HandleObject parent, HandleObject CTypeProto)
{
  JSFunction* fun = JS_DefineFunction(cx, parent, "CData", ConstructAbstract, 0,
                      CTYPESCTOR_FLAGS);
  if (!fun)
    return nullptr;

  RootedObject ctor(cx, JS_GetFunctionObject(fun));
  JS_ASSERT(ctor);

  // Set up ctypes.CData.__proto__ === ctypes.CType.prototype.
  // (Note that 'ctypes.CData instanceof Function' is still true, thanks to the
  // prototype chain.)
  if (!JS_SetPrototype(cx, ctor, CTypeProto))
    return nullptr;

  // Set up ctypes.CData.prototype.
  RootedObject prototype(cx, JS_NewObject(cx, &sCDataProtoClass, NullPtr(), parent));
  if (!prototype)
    return nullptr;

  if (!JS_DefineProperty(cx, ctor, "prototype", OBJECT_TO_JSVAL(prototype),
         nullptr, nullptr, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return nullptr;

  if (!JS_DefineProperty(cx, prototype, "constructor", OBJECT_TO_JSVAL(ctor),
         nullptr, nullptr, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return nullptr;

  // Define properties and functions common to all CDatas.
  if (!JS_DefineProperties(cx, prototype, sCDataProps) ||
      !JS_DefineFunctions(cx, prototype, sCDataFunctions))
    return nullptr;

  if (//!JS_FreezeObject(cx, prototype) || // XXX fixme - see bug 541212!
      !JS_FreezeObject(cx, ctor))
    return nullptr;

  return prototype;
}

static bool
DefineABIConstant(JSContext* cx,
                  HandleObject parent,
                  const char* name,
                  ABICode code,
                  HandleObject prototype)
{
  RootedObject obj(cx, JS_DefineObject(cx, parent, name, &sCABIClass, prototype,
                                       JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT));
  if (!obj)
    return false;
  JS_SetReservedSlot(obj, SLOT_ABICODE, INT_TO_JSVAL(code));
  return JS_FreezeObject(cx, obj);
}

// Set up a single type constructor for
// ctypes.{Pointer,Array,Struct,Function}Type.
static bool
InitTypeConstructor(JSContext* cx,
                    HandleObject parent,
                    HandleObject CTypeProto,
                    HandleObject CDataProto,
                    const JSFunctionSpec spec,
                    const JSFunctionSpec* fns,
                    const JSPropertySpec* props,
                    const JSFunctionSpec* instanceFns,
                    const JSPropertySpec* instanceProps,
                    MutableHandleObject typeProto,
                    MutableHandleObject dataProto)
{
  JSFunction* fun = js::DefineFunctionWithReserved(cx, parent, spec.name, spec.call.op,
                      spec.nargs, spec.flags);
  if (!fun)
    return false;

  RootedObject obj(cx, JS_GetFunctionObject(fun));
  if (!obj)
    return false;

  // Set up the .prototype and .prototype.constructor properties.
  typeProto.set(JS_NewObject(cx, &sCTypeProtoClass, CTypeProto, parent));
  if (!typeProto)
    return false;

  // Define property before proceeding, for GC safety.
  if (!JS_DefineProperty(cx, obj, "prototype", OBJECT_TO_JSVAL(typeProto),
         nullptr, nullptr, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  if (fns && !JS_DefineFunctions(cx, typeProto, fns))
    return false;

  if (!JS_DefineProperties(cx, typeProto, props))
    return false;

  if (!JS_DefineProperty(cx, typeProto, "constructor", OBJECT_TO_JSVAL(obj),
         nullptr, nullptr, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  // Stash ctypes.{Pointer,Array,Struct}Type.prototype on a reserved slot of
  // the type constructor, for faster lookup.
  js::SetFunctionNativeReserved(obj, SLOT_FN_CTORPROTO, OBJECT_TO_JSVAL(typeProto));

  // Create an object to serve as the common ancestor for all CData objects
  // created from the given type constructor. This has ctypes.CData.prototype
  // as its prototype, such that it inherits the properties and functions
  // common to all CDatas.
  dataProto.set(JS_NewObject(cx, &sCDataProtoClass, CDataProto, parent));
  if (!dataProto)
    return false;

  // Define functions and properties on the 'dataProto' object that are common
  // to all CData objects created from this type constructor. (These will
  // become functions and properties on CData objects created from this type.)
  if (instanceFns && !JS_DefineFunctions(cx, dataProto, instanceFns))
    return false;

  if (instanceProps && !JS_DefineProperties(cx, dataProto, instanceProps))
    return false;

  // Link the type prototype to the data prototype.
  JS_SetReservedSlot(typeProto, SLOT_OURDATAPROTO, OBJECT_TO_JSVAL(dataProto));

  if (!JS_FreezeObject(cx, obj) ||
      //!JS_FreezeObject(cx, dataProto) || // XXX fixme - see bug 541212!
      !JS_FreezeObject(cx, typeProto))
    return false;

  return true;
}

static JSObject*
InitInt64Class(JSContext* cx,
               HandleObject parent,
               const JSClass* clasp,
               JSNative construct,
               const JSFunctionSpec* fs,
               const JSFunctionSpec* static_fs)
{
  // Init type class and constructor
  RootedObject prototype(cx, JS_InitClass(cx, parent, nullptr, clasp, construct,
                                          0, nullptr, fs, nullptr, static_fs));
  if (!prototype)
    return nullptr;

  RootedObject ctor(cx, JS_GetConstructor(cx, prototype));
  if (!ctor)
    return nullptr;
  if (!JS_FreezeObject(cx, ctor))
    return nullptr;

  // Redefine the 'join' function as an extended native and stash
  // ctypes.{Int64,UInt64}.prototype in a reserved slot of the new function.
  JS_ASSERT(clasp == &sInt64ProtoClass || clasp == &sUInt64ProtoClass);
  JSNative native = (clasp == &sInt64ProtoClass) ? Int64::Join : UInt64::Join;
  JSFunction* fun = js::DefineFunctionWithReserved(cx, ctor, "join", native,
                      2, CTYPESFN_FLAGS);
  if (!fun)
    return nullptr;

  js::SetFunctionNativeReserved(fun, SLOT_FN_INT64PROTO,
    OBJECT_TO_JSVAL(prototype));

  if (!JS_FreezeObject(cx, prototype))
    return nullptr;

  return prototype;
}

static void
AttachProtos(JSObject* proto, const AutoObjectVector& protos)
{
  // For a given 'proto' of [[Class]] "CTypeProto", attach each of the 'protos'
  // to the appropriate CTypeProtoSlot. (SLOT_CTYPES is the last slot
  // of [[Class]] "CTypeProto" that we fill in this automated manner.)
  for (uint32_t i = 0; i <= SLOT_CTYPES; ++i)
    JS_SetReservedSlot(proto, i, OBJECT_TO_JSVAL(protos[i]));
}

static bool
InitTypeClasses(JSContext* cx, HandleObject parent)
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
  RootedObject CTypeProto(cx, InitCTypeClass(cx, parent));
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
  RootedObject CDataProto(cx, InitCDataClass(cx, parent, CTypeProto));
  if (!CDataProto)
    return false;

  // Link CTypeProto to CDataProto.
  JS_SetReservedSlot(CTypeProto, SLOT_OURDATAPROTO, OBJECT_TO_JSVAL(CDataProto));

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
  AutoObjectVector protos(cx);
  protos.resize(CTYPEPROTO_SLOTS);
  if (!InitTypeConstructor(cx, parent, CTypeProto, CDataProto,
         sPointerFunction, nullptr, sPointerProps,
         sPointerInstanceFunctions, sPointerInstanceProps,
         protos.handleAt(SLOT_POINTERPROTO), protos.handleAt(SLOT_POINTERDATAPROTO)))
    return false;

  if (!InitTypeConstructor(cx, parent, CTypeProto, CDataProto,
         sArrayFunction, nullptr, sArrayProps,
         sArrayInstanceFunctions, sArrayInstanceProps,
         protos.handleAt(SLOT_ARRAYPROTO), protos.handleAt(SLOT_ARRAYDATAPROTO)))
    return false;

  if (!InitTypeConstructor(cx, parent, CTypeProto, CDataProto,
         sStructFunction, sStructFunctions, sStructProps,
         sStructInstanceFunctions, nullptr,
         protos.handleAt(SLOT_STRUCTPROTO), protos.handleAt(SLOT_STRUCTDATAPROTO)))
    return false;

  if (!InitTypeConstructor(cx, parent, CTypeProto, protos.handleAt(SLOT_POINTERDATAPROTO),
         sFunctionFunction, nullptr, sFunctionProps, sFunctionInstanceFunctions, nullptr,
         protos.handleAt(SLOT_FUNCTIONPROTO), protos.handleAt(SLOT_FUNCTIONDATAPROTO)))
    return false;

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

  // Finally, store a pointer to the global ctypes object.
  // Note that there is no other reliable manner of locating this object.
  protos[SLOT_CTYPES] = parent;

  // Attach the prototypes just created to each of ctypes.CType.prototype,
  // and the special type constructors, so we can access them when constructing
  // instances of those types. 
  AttachProtos(CTypeProto, protos);
  AttachProtos(protos[SLOT_POINTERPROTO], protos);
  AttachProtos(protos[SLOT_ARRAYPROTO], protos);
  AttachProtos(protos[SLOT_STRUCTPROTO], protos);
  AttachProtos(protos[SLOT_FUNCTIONPROTO], protos);

  RootedObject ABIProto(cx, InitABIClass(cx, parent));
  if (!ABIProto)
    return false;

  // Attach objects representing ABI constants.
  if (!DefineABIConstant(cx, parent, "default_abi", ABI_DEFAULT, ABIProto) ||
      !DefineABIConstant(cx, parent, "stdcall_abi", ABI_STDCALL, ABIProto) ||
      !DefineABIConstant(cx, parent, "winapi_abi", ABI_WINAPI, ABIProto))
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
  RootedObject typeObj_##name(cx,                                              \
    CType::DefineBuiltin(cx, parent, #name, CTypeProto, CDataProto, #name,     \
      TYPE_##name, INT_TO_JSVAL(sizeof(type)),                                 \
      INT_TO_JSVAL(ffiType.alignment), &ffiType));                             \
  if (!typeObj_##name)                                                         \
    return false;
#include "ctypes/typedefs.h"

  // Alias 'ctypes.unsigned' as 'ctypes.unsigned_int', since they represent
  // the same type in C.
  if (!JS_DefineProperty(cx, parent, "unsigned",
         OBJECT_TO_JSVAL(typeObj_unsigned_int), nullptr, nullptr,
         JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  // Create objects representing the special types void_t and voidptr_t.
  RootedObject typeObj(cx,
    CType::DefineBuiltin(cx, parent, "void_t", CTypeProto, CDataProto, "void",
                         TYPE_void_t, JSVAL_VOID, JSVAL_VOID, &ffi_type_void));
  if (!typeObj)
    return false;

  typeObj = PointerType::CreateInternal(cx, typeObj);
  if (!typeObj)
    return false;
  if (!JS_DefineProperty(cx, parent, "voidptr_t", OBJECT_TO_JSVAL(typeObj),
         nullptr, nullptr, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  return true;
}

bool
IsCTypesGlobal(JSObject* obj)
{
  return JS_GetClass(obj) == &sCTypesGlobalClass;
}

bool
IsCTypesGlobal(HandleValue v)
{
  return v.isObject() && IsCTypesGlobal(&v.toObject());
}

// Get the JSCTypesCallbacks struct from the 'ctypes' object 'obj'.
JSCTypesCallbacks*
GetCallbacks(JSObject* obj)
{
  JS_ASSERT(IsCTypesGlobal(obj));

  jsval result = JS_GetReservedSlot(obj, SLOT_CALLBACKS);
  if (JSVAL_IS_VOID(result))
    return nullptr;

  return static_cast<JSCTypesCallbacks*>(JSVAL_TO_PRIVATE(result));
}

// Utility function to access a property of an object as an object
// returns false and sets the error if the property does not exist
// or is not an object
static bool GetObjectProperty(JSContext *cx, HandleObject obj,
                              const char *property, MutableHandleObject result)
{
  RootedValue val(cx);
  if (!JS_GetProperty(cx, obj, property, &val)) {
    return false;
  }

  if (JSVAL_IS_PRIMITIVE(val)) {
    JS_ReportError(cx, "missing or non-object field");
    return false;
  }

  result.set(JSVAL_TO_OBJECT(val));
  return true;
}

} /* namespace ctypes */
} /* namespace js */

using namespace js;
using namespace js::ctypes;

JS_PUBLIC_API(bool)
JS_InitCTypesClass(JSContext* cx, JSObject *globalArg)
{
  RootedObject global(cx, globalArg);

  // attach ctypes property to global object
  RootedObject ctypes(cx, JS_NewObject(cx, &sCTypesGlobalClass, NullPtr(), NullPtr()));
  if (!ctypes)
    return false;

  if (!JS_DefineProperty(cx, global, "ctypes", OBJECT_TO_JSVAL(ctypes),
         JS_PropertyStub, JS_StrictPropertyStub, JSPROP_READONLY | JSPROP_PERMANENT)){
    return false;
  }

  if (!InitTypeClasses(cx, ctypes))
    return false;

  // attach API functions and properties
  if (!JS_DefineFunctions(cx, ctypes, sModuleFunctions) ||
      !JS_DefineProperties(cx, ctypes, sModuleProps))
    return false;

  // Set up ctypes.CDataFinalizer.prototype.
  RootedObject ctor(cx);
  if (!GetObjectProperty(cx, ctypes, "CDataFinalizer", &ctor))
    return false;

  RootedObject prototype(cx, JS_NewObject(cx, &sCDataFinalizerProtoClass, NullPtr(), ctypes));
  if (!prototype)
    return false;

  if (!JS_DefineProperties(cx, prototype, sCDataFinalizerProps) ||
      !JS_DefineFunctions(cx, prototype, sCDataFinalizerFunctions))
    return false;

  if (!JS_DefineProperty(cx, ctor, "prototype", OBJECT_TO_JSVAL(prototype),
                         nullptr, nullptr, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  if (!JS_DefineProperty(cx, prototype, "constructor", OBJECT_TO_JSVAL(ctor),
                         nullptr, nullptr, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;


  // Seal the ctypes object, to prevent modification.
  return JS_FreezeObject(cx, ctypes);
}

JS_PUBLIC_API(void)
JS_SetCTypesCallbacks(JSObject *ctypesObj, JSCTypesCallbacks* callbacks)
{
  JS_ASSERT(callbacks);
  JS_ASSERT(IsCTypesGlobal(ctypesObj));

  // Set the callbacks on a reserved slot.
  JS_SetReservedSlot(ctypesObj, SLOT_CALLBACKS, PRIVATE_TO_JSVAL(callbacks));
}

namespace js {

JS_FRIEND_API(size_t)
SizeOfDataIfCDataObject(mozilla::MallocSizeOf mallocSizeOf, JSObject *obj)
{
    if (!CData::IsCData(obj))
        return 0;

    size_t n = 0;
    jsval slot = JS_GetReservedSlot(obj, ctypes::SLOT_OWNS);
    if (!JSVAL_IS_VOID(slot)) {
        bool owns = JSVAL_TO_BOOLEAN(slot);
        slot = JS_GetReservedSlot(obj, ctypes::SLOT_DATA);
        if (!JSVAL_IS_VOID(slot)) {
            char** buffer = static_cast<char**>(JSVAL_TO_PRIVATE(slot));
            n += mallocSizeOf(buffer);
            if (owns)
                n += mallocSizeOf(*buffer);
        }
    }
    return n;
}

namespace ctypes {

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
JS_STATIC_ASSERT(NumericLimits<double>::is_signed);

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
struct ConvertImpl<uint64_t, double> {
  static JS_ALWAYS_INLINE uint64_t Convert(double d) {
    return d > 0x7fffffffffffffffui64 ?
           uint64_t(d - 0x8000000000000000ui64) + 0x8000000000000000ui64 :
           uint64_t(d);
  }
};
#endif

// C++ doesn't guarantee that exact values are the only ones that will
// round-trip. In fact, on some platforms, including SPARC, there are pairs of
// values, a uint64_t and a double, such that neither value is exactly
// representable in the other type, but they cast to each other.
#ifdef SPARC
// Simulate x86 overflow behavior
template<>
struct ConvertImpl<uint64_t, double> {
  static JS_ALWAYS_INLINE uint64_t Convert(double d) {
    return d >= 0xffffffffffffffff ?
           0x8000000000000000 : uint64_t(d);
  }
};

template<>
struct ConvertImpl<int64_t, double> {
  static JS_ALWAYS_INLINE int64_t Convert(double d) {
    return d >= 0x7fffffffffffffff ?
           0x8000000000000000 : int64_t(d);
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
  if (NumericLimits<TargetType>::digits < NumericLimits<FromType>::digits)
    return false;

  if (NumericLimits<FromType>::is_signed &&
      !NumericLimits<TargetType>::is_signed)
    return false;

  if (!NumericLimits<FromType>::is_exact &&
      NumericLimits<TargetType>::is_exact)
    return false;

  return true;
}

// Templated helper to determine if FromType 'i' converts losslessly to
// TargetType 'j'. Default case where both types are the same signedness.
template<class TargetType, class FromType, bool TargetSigned, bool FromSigned>
struct IsExactImpl {
  static JS_ALWAYS_INLINE bool Test(FromType i, TargetType j) {
    JS_STATIC_ASSERT(NumericLimits<TargetType>::is_exact);
    return FromType(j) == i;
  }
};

// Specialization where TargetType is unsigned, FromType is signed.
template<class TargetType, class FromType>
struct IsExactImpl<TargetType, FromType, false, true> {
  static JS_ALWAYS_INLINE bool Test(FromType i, TargetType j) {
    JS_STATIC_ASSERT(NumericLimits<TargetType>::is_exact);
    return i >= 0 && FromType(j) == i;
  }
};

// Specialization where TargetType is signed, FromType is unsigned.
template<class TargetType, class FromType>
struct IsExactImpl<TargetType, FromType, true, false> {
  static JS_ALWAYS_INLINE bool Test(FromType i, TargetType j) {
    JS_STATIC_ASSERT(NumericLimits<TargetType>::is_exact);
    return TargetType(i) >= 0 && FromType(j) == i;
  }
};

// Convert FromType 'i' to TargetType 'result', returning true iff 'result'
// is an exact representation of 'i'.
template<class TargetType, class FromType>
static JS_ALWAYS_INLINE bool ConvertExact(FromType i, TargetType* result)
{
  // Require that TargetType is integral, to simplify conversion.
  JS_STATIC_ASSERT(NumericLimits<TargetType>::is_exact);

  *result = Convert<TargetType>(i);

  // See if we can avoid a dynamic check.
  if (IsAlwaysExact<TargetType, FromType>())
    return true;

  // Return 'true' if 'i' is exactly representable in 'TargetType'.
  return IsExactImpl<TargetType,
                     FromType,
                     NumericLimits<TargetType>::is_signed,
                     NumericLimits<FromType>::is_signed>::Test(i, *result);
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
  return IsNegativeImpl<Type, NumericLimits<Type>::is_signed>::Test(i);
}

// Implicitly convert val to bool, allowing bool, int, and double
// arguments numerically equal to 0 or 1.
static bool
jsvalToBool(JSContext* cx, jsval val, bool* result)
{
  if (JSVAL_IS_BOOLEAN(val)) {
    *result = JSVAL_TO_BOOLEAN(val);
    return true;
  }
  if (JSVAL_IS_INT(val)) {
    int32_t i = JSVAL_TO_INT(val);
    *result = i != 0;
    return i == 0 || i == 1;
  }
  if (JSVAL_IS_DOUBLE(val)) {
    double d = JSVAL_TO_DOUBLE(val);
    *result = d != 0;
    // Allow -0.
    return d == 1 || d == 0;
  }
  // Don't silently convert null to bool. It's probably a mistake.
  return false;
}

// Implicitly convert val to IntegerType, allowing bool, int, double,
// Int64, UInt64, and CData integer types 't' where all values of 't' are
// representable by IntegerType.
template<class IntegerType>
static bool
jsvalToInteger(JSContext* cx, jsval val, IntegerType* result)
{
  JS_STATIC_ASSERT(NumericLimits<IntegerType>::is_exact);

  if (JSVAL_IS_INT(val)) {
    // Make sure the integer fits in the alotted precision, and has the right
    // sign.
    int32_t i = JSVAL_TO_INT(val);
    return ConvertExact(i, result);
  }
  if (JSVAL_IS_DOUBLE(val)) {
    // Don't silently lose bits here -- check that val really is an
    // integer value, and has the right sign.
    double d = JSVAL_TO_DOUBLE(val);
    return ConvertExact(d, result);
  }
  if (!JSVAL_IS_PRIMITIVE(val)) {
    JSObject* obj = JSVAL_TO_OBJECT(val);
    if (CData::IsCData(obj)) {
      JSObject* typeObj = CData::GetCType(obj);
      void* data = CData::GetData(obj);

      // Check whether the source type is always representable, with exact
      // precision, by the target type. If it is, convert the value.
      switch (CType::GetTypeCode(typeObj)) {
#define DEFINE_INT_TYPE(name, fromType, ffiType)                               \
      case TYPE_##name:                                                        \
        if (!IsAlwaysExact<IntegerType, fromType>())                           \
          return false;                                                        \
        *result = IntegerType(*static_cast<fromType*>(data));                  \
        return true;
#define DEFINE_WRAPPED_INT_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#include "ctypes/typedefs.h"
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

    if (Int64::IsInt64(obj)) {
      // Make sure the integer fits in IntegerType.
      int64_t i = Int64Base::GetInt(obj);
      return ConvertExact(i, result);
    }

    if (UInt64::IsUInt64(obj)) {
      // Make sure the integer fits in IntegerType.
      uint64_t i = Int64Base::GetInt(obj);
      return ConvertExact(i, result);
    }

    if (CDataFinalizer::IsCDataFinalizer(obj)) {
      RootedValue innerData(cx);
      if (!CDataFinalizer::GetValue(cx, obj, innerData.address())) {
        return false; // Nothing to convert
      }
      return jsvalToInteger(cx, innerData, result);
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

// Implicitly convert val to FloatType, allowing int, double,
// Int64, UInt64, and CData numeric types 't' where all values of 't' are
// representable by FloatType.
template<class FloatType>
static bool
jsvalToFloat(JSContext *cx, jsval val, FloatType* result)
{
  JS_STATIC_ASSERT(!NumericLimits<FloatType>::is_exact);

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
    if (CData::IsCData(obj)) {
      JSObject* typeObj = CData::GetCType(obj);
      void* data = CData::GetData(obj);

      // Check whether the source type is always representable, with exact
      // precision, by the target type. If it is, convert the value.
      switch (CType::GetTypeCode(typeObj)) {
#define DEFINE_FLOAT_TYPE(name, fromType, ffiType)                             \
      case TYPE_##name:                                                        \
        if (!IsAlwaysExact<FloatType, fromType>())                             \
          return false;                                                        \
        *result = FloatType(*static_cast<fromType*>(data));                    \
        return true;
#define DEFINE_INT_TYPE(x, y, z) DEFINE_FLOAT_TYPE(x, y, z)
#define DEFINE_WRAPPED_INT_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#include "ctypes/typedefs.h"
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
  JS_STATIC_ASSERT(NumericLimits<IntegerType>::is_exact);

  const jschar* cp = string->getChars(nullptr);
  if (!cp)
    return false;

  const jschar* end = cp + string->length();
  if (cp == end)
    return false;

  IntegerType sign = 1;
  if (cp[0] == '-') {
    if (!NumericLimits<IntegerType>::is_signed)
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

// Implicitly convert val to IntegerType, allowing int, double,
// Int64, UInt64, and optionally a decimal or hexadecimal string argument.
// (This is common code shared by jsvalToSize and the Int64/UInt64 constructors.)
template<class IntegerType>
static bool
jsvalToBigInteger(JSContext* cx,
                  jsval val,
                  bool allowString,
                  IntegerType* result)
{
  JS_STATIC_ASSERT(NumericLimits<IntegerType>::is_exact);

  if (JSVAL_IS_INT(val)) {
    // Make sure the integer fits in the alotted precision, and has the right
    // sign.
    int32_t i = JSVAL_TO_INT(val);
    return ConvertExact(i, result);
  }
  if (JSVAL_IS_DOUBLE(val)) {
    // Don't silently lose bits here -- check that val really is an
    // integer value, and has the right sign.
    double d = JSVAL_TO_DOUBLE(val);
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

    if (UInt64::IsUInt64(obj)) {
      // Make sure the integer fits in IntegerType.
      uint64_t i = Int64Base::GetInt(obj);
      return ConvertExact(i, result);
    }

    if (Int64::IsInt64(obj)) {
      // Make sure the integer fits in IntegerType.
      int64_t i = Int64Base::GetInt(obj);
      return ConvertExact(i, result);
    }

    if (CDataFinalizer::IsCDataFinalizer(obj)) {
      RootedValue innerData(cx);
      if (!CDataFinalizer::GetValue(cx, obj, innerData.address())) {
        return false; // Nothing to convert
      }
      return jsvalToBigInteger(cx, innerData, allowString, result);
    }

  }
  return false;
}

// Implicitly convert val to a size value, where the size value is represented
// by size_t but must also fit in a double.
static bool
jsvalToSize(JSContext* cx, jsval val, bool allowString, size_t* result)
{
  if (!jsvalToBigInteger(cx, val, allowString, result))
    return false;

  // Also check that the result fits in a double.
  return Convert<size_t>(double(*result)) == *result;
}

// Implicitly convert val to IntegerType, allowing int, double,
// Int64, UInt64, and optionally a decimal or hexadecimal string argument.
// (This is common code shared by jsvalToSize and the Int64/UInt64 constructors.)
template<class IntegerType>
static bool
jsidToBigInteger(JSContext* cx,
                  jsid val,
                  bool allowString,
                  IntegerType* result)
{
  JS_STATIC_ASSERT(NumericLimits<IntegerType>::is_exact);

  if (JSID_IS_INT(val)) {
    // Make sure the integer fits in the alotted precision, and has the right
    // sign.
    int32_t i = JSID_TO_INT(val);
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

    if (UInt64::IsUInt64(obj)) {
      // Make sure the integer fits in IntegerType.
      uint64_t i = Int64Base::GetInt(obj);
      return ConvertExact(i, result);
    }

    if (Int64::IsInt64(obj)) {
      // Make sure the integer fits in IntegerType.
      int64_t i = Int64Base::GetInt(obj);
      return ConvertExact(i, result);
    }
  }
  return false;
}

// Implicitly convert val to a size value, where the size value is represented
// by size_t but must also fit in a double.
static bool
jsidToSize(JSContext* cx, jsid val, bool allowString, size_t* result)
{
  if (!jsidToBigInteger(cx, val, allowString, result))
    return false;

  // Also check that the result fits in a double.
  return Convert<size_t>(double(*result)) == *result;
}

// Implicitly convert a size value to a jsval, ensuring that the size_t value
// fits in a double.
static bool
SizeTojsval(JSContext* cx, size_t size, jsval* result)
{
  if (Convert<size_t>(double(size)) != size) {
    JS_ReportError(cx, "size overflow");
    return false;
  }

  *result = JS_NumberValue(double(size));
  return true;
}

// Forcefully convert val to IntegerType when explicitly requested.
template<class IntegerType>
static bool
jsvalToIntegerExplicit(jsval val, IntegerType* result)
{
  JS_STATIC_ASSERT(NumericLimits<IntegerType>::is_exact);

  if (JSVAL_IS_DOUBLE(val)) {
    // Convert -Inf, Inf, and NaN to 0; otherwise, convert by C-style cast.
    double d = JSVAL_TO_DOUBLE(val);
    *result = mozilla::IsFinite(d) ? IntegerType(d) : 0;
    return true;
  }
  if (!JSVAL_IS_PRIMITIVE(val)) {
    // Convert Int64 and UInt64 values by C-style cast.
    JSObject* obj = JSVAL_TO_OBJECT(val);
    if (Int64::IsInt64(obj)) {
      int64_t i = Int64Base::GetInt(obj);
      *result = IntegerType(i);
      return true;
    }
    if (UInt64::IsUInt64(obj)) {
      uint64_t i = Int64Base::GetInt(obj);
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
    // int32_t always fits in intptr_t. If the integer is negative, cast through
    // an intptr_t intermediate to sign-extend.
    int32_t i = JSVAL_TO_INT(val);
    *result = i < 0 ? uintptr_t(intptr_t(i)) : uintptr_t(i);
    return true;
  }
  if (JSVAL_IS_DOUBLE(val)) {
    double d = JSVAL_TO_DOUBLE(val);
    if (d < 0) {
      // Cast through an intptr_t intermediate to sign-extend.
      intptr_t i = Convert<intptr_t>(d);
      if (double(i) != d)
        return false;

      *result = uintptr_t(i);
      return true;
    }

    // Don't silently lose bits here -- check that val really is an
    // integer value, and has the right sign.
    *result = Convert<uintptr_t>(d);
    return double(*result) == d;
  }
  if (!JSVAL_IS_PRIMITIVE(val)) {
    JSObject* obj = JSVAL_TO_OBJECT(val);
    if (Int64::IsInt64(obj)) {
      int64_t i = Int64Base::GetInt(obj);
      intptr_t p = intptr_t(i);

      // Make sure the integer fits in the alotted precision.
      if (int64_t(p) != i)
        return false;
      *result = uintptr_t(p);
      return true;
    }

    if (UInt64::IsUInt64(obj)) {
      uint64_t i = Int64Base::GetInt(obj);

      // Make sure the integer fits in the alotted precision.
      *result = uintptr_t(i);
      return uint64_t(*result) == i;
    }
  }
  return false;
}

template<class IntegerType, class CharType, size_t N, class AP>
void
IntegerToString(IntegerType i, int radix, Vector<CharType, N, AP>& result)
{
  JS_STATIC_ASSERT(NumericLimits<IntegerType>::is_exact);

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
static bool
ConvertToJS(JSContext* cx,
            HandleObject typeObj,
            HandleObject parentObj,
            void* data,
            bool wantPrimitive,
            bool ownResult,
            jsval* result)
{
  JS_ASSERT(!parentObj || CData::IsCData(parentObj));
  JS_ASSERT(!parentObj || !ownResult);
  JS_ASSERT(!wantPrimitive || !ownResult);

  TypeCode typeCode = CType::GetTypeCode(typeObj);

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
      *result = INT_TO_JSVAL(int32_t(value));                                  \
    else                                                                       \
      *result = JS_NumberValue(double(value));                                 \
    break;                                                                     \
  }
#define DEFINE_WRAPPED_INT_TYPE(name, type, ffiType)                           \
  case TYPE_##name: {                                                          \
    /* Return an Int64 or UInt64 object - do not convert to a JS number. */    \
    uint64_t value;                                                            \
    RootedObject proto(cx);                                                    \
    if (!NumericLimits<type>::is_signed) {                                     \
      value = *static_cast<type*>(data);                                       \
      /* Get ctypes.UInt64.prototype from ctypes.CType.prototype. */           \
      proto = CType::GetProtoFromType(cx, typeObj, SLOT_UINT64PROTO);          \
      if (!proto)                                                              \
        return false;                                                          \
    } else {                                                                   \
      value = int64_t(*static_cast<type*>(data));                              \
      /* Get ctypes.Int64.prototype from ctypes.CType.prototype. */            \
      proto = CType::GetProtoFromType(cx, typeObj, SLOT_INT64PROTO);           \
      if (!proto)                                                              \
        return false;                                                          \
    }                                                                          \
                                                                               \
    JSObject* obj = Int64Base::Construct(cx, proto, value,                     \
      !NumericLimits<type>::is_signed);                                        \
    if (!obj)                                                                  \
      return false;                                                            \
    *result = OBJECT_TO_JSVAL(obj);                                            \
    break;                                                                     \
  }
#define DEFINE_FLOAT_TYPE(name, type, ffiType)                                 \
  case TYPE_##name: {                                                          \
    type value = *static_cast<type*>(data);                                    \
    *result = JS_NumberValue(double(value));                                   \
    break;                                                                     \
  }
#define DEFINE_CHAR_TYPE(name, type, ffiType)                                  \
  case TYPE_##name:                                                            \
    /* Convert to an integer. We have no idea what character encoding to */    \
    /* use, if any. */                                                         \
    *result = INT_TO_JSVAL(*static_cast<type*>(data));                         \
    break;
#include "ctypes/typedefs.h"
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
    MOZ_ASSUME_UNREACHABLE("cannot return a FunctionType");
  }

  return true;
}

// Determine if the contents of a typed array can be converted without
// ambiguity to a C type. Elements of a Int8Array are converted to
// ctypes.int8_t, UInt8Array to ctypes.uint8_t, etc.
bool CanConvertTypedArrayItemTo(JSObject *baseType, JSObject *valObj, JSContext *cx) {
  TypeCode baseTypeCode = CType::GetTypeCode(baseType);
  if (baseTypeCode == TYPE_void_t) {
    return true;
  }
  TypeCode elementTypeCode;
  switch (JS_GetArrayBufferViewType(valObj)) {
  case ScalarTypeRepresentation::TYPE_INT8:
    elementTypeCode = TYPE_int8_t;
    break;
  case ScalarTypeRepresentation::TYPE_UINT8:
  case ScalarTypeRepresentation::TYPE_UINT8_CLAMPED:
    elementTypeCode = TYPE_uint8_t;
    break;
  case ScalarTypeRepresentation::TYPE_INT16:
    elementTypeCode = TYPE_int16_t;
    break;
  case ScalarTypeRepresentation::TYPE_UINT16:
    elementTypeCode = TYPE_uint16_t;
    break;
  case ScalarTypeRepresentation::TYPE_INT32:
    elementTypeCode = TYPE_int32_t;
    break;
  case ScalarTypeRepresentation::TYPE_UINT32:
    elementTypeCode = TYPE_uint32_t;
    break;
  case ScalarTypeRepresentation::TYPE_FLOAT32:
    elementTypeCode = TYPE_float32_t;
    break;
  case ScalarTypeRepresentation::TYPE_FLOAT64:
    elementTypeCode = TYPE_float64_t;
    break;
  default:
    return false;
  }
  return elementTypeCode == baseTypeCode;
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
static bool
ImplicitConvert(JSContext* cx,
                HandleValue val,
                JSObject* targetType_,
                void* buffer,
                bool isArgument,
                bool* freePointer)
{
  RootedObject targetType(cx, targetType_);
  JS_ASSERT(CType::IsSizeDefined(targetType));

  // First, check if val is either a CData object or a CDataFinalizer
  // of type targetType.
  JSObject* sourceData = nullptr;
  JSObject* sourceType = nullptr;
  RootedObject valObj(cx, nullptr);
  if (!JSVAL_IS_PRIMITIVE(val)) {
    valObj = JSVAL_TO_OBJECT(val);
    if (CData::IsCData(valObj)) {
      sourceData = valObj;
      sourceType = CData::GetCType(sourceData);

      // If the types are equal, copy the buffer contained within the CData.
      // (Note that the buffers may overlap partially or completely.)
      if (CType::TypesEqual(sourceType, targetType)) {
        size_t size = CType::GetSize(sourceType);
        memmove(buffer, CData::GetData(sourceData), size);
        return true;
      }
    } else if (CDataFinalizer::IsCDataFinalizer(valObj)) {
      sourceData = valObj;
      sourceType = CDataFinalizer::GetCType(cx, sourceData);

      CDataFinalizer::Private *p = (CDataFinalizer::Private *)
        JS_GetPrivate(sourceData);

      if (!p) {
        // We have called |dispose| or |forget| already.
        JS_ReportError(cx, "Attempting to convert an empty CDataFinalizer");
        return false;
      }

      // If the types are equal, copy the buffer contained within the CData.
      if (CType::TypesEqual(sourceType, targetType)) {
        memmove(buffer, p->cargs, p->cargs_size);
        return true;
      }
    }
  }

  TypeCode targetCode = CType::GetTypeCode(targetType);

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
      const jschar *chars = str->getChars(cx);                                 \
      if (!chars)                                                              \
        return false;                                                          \
      result = chars[0];                                                       \
    } else if (!jsvalToInteger(cx, val, &result)) {                            \
      return TypeError(cx, #name, val);                                        \
    }                                                                          \
    *static_cast<type*>(buffer) = result;                                      \
    break;                                                                     \
  }
#include "ctypes/typedefs.h"
  case TYPE_pointer: {
    if (JSVAL_IS_NULL(val)) {
      // Convert to a null pointer.
      *static_cast<void**>(buffer) = nullptr;
      break;
    }

    JS::Rooted<JSObject*> baseType(cx, PointerType::GetBaseType(targetType));
    if (sourceData) {
      // First, determine if the targetType is ctypes.void_t.ptr.
      TypeCode sourceCode = CType::GetTypeCode(sourceType);
      void* sourceBuffer = CData::GetData(sourceData);
      bool voidptrTarget = CType::GetTypeCode(baseType) == TYPE_void_t;

      if (sourceCode == TYPE_pointer && voidptrTarget) {
        // Autoconvert if targetType is ctypes.voidptr_t.
        *static_cast<void**>(buffer) = *static_cast<void**>(sourceBuffer);
        break;
      }
      if (sourceCode == TYPE_array) {
        // Autoconvert an array to a ctypes.void_t.ptr or to
        // sourceType.elementType.ptr, just like C.
        JSObject* elementType = ArrayType::GetBaseType(sourceType);
        if (voidptrTarget || CType::TypesEqual(baseType, elementType)) {
          *static_cast<void**>(buffer) = sourceBuffer;
          break;
        }
      }

    } else if (isArgument && JSVAL_IS_STRING(val)) {
      // Convert the string for the ffi call. This requires allocating space
      // which the caller assumes ownership of.
      // TODO: Extend this so we can safely convert strings at other times also.
      JSString* sourceString = JSVAL_TO_STRING(val);
      size_t sourceLength = sourceString->length();
      const jschar* sourceChars = sourceString->getChars(cx);
      if (!sourceChars)
        return false;

      switch (CType::GetTypeCode(baseType)) {
      case TYPE_char:
      case TYPE_signed_char:
      case TYPE_unsigned_char: {
        // Convert from UTF-16 to UTF-8.
        size_t nbytes =
          GetDeflatedUTF8StringLength(cx, sourceChars, sourceLength);
        if (nbytes == (size_t) -1)
          return false;

        char** charBuffer = static_cast<char**>(buffer);
        *charBuffer = cx->pod_malloc<char>(nbytes + 1);
        if (!*charBuffer) {
          JS_ReportAllocationOverflow(cx);
          return false;
        }

        ASSERT_OK(DeflateStringToUTF8Buffer(cx, sourceChars, sourceLength,
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
        *jscharBuffer = cx->pod_malloc<jschar>(sourceLength + 1);
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
        return TypeError(cx, "string pointer", val);
      }
      break;
    } else if (!JSVAL_IS_PRIMITIVE(val) && JS_IsArrayBufferObject(valObj)) {
      // Convert ArrayBuffer to pointer without any copy.
      // Just as with C arrays, we make no effort to
      // keep the ArrayBuffer alive.
      *static_cast<void**>(buffer) = JS_GetArrayBufferData(valObj);
      break;
    } if (!JSVAL_IS_PRIMITIVE(val) && JS_IsTypedArrayObject(valObj)) {
      if(!CanConvertTypedArrayItemTo(baseType, valObj, cx)) {
        return TypeError(cx, "typed array with the appropriate type", val);
      }

      // Convert TypedArray to pointer without any copy.
      // Just as with C arrays, we make no effort to
      // keep the TypedArray alive.
      *static_cast<void**>(buffer) = JS_GetArrayBufferViewData(valObj);
      break;
    }
    return TypeError(cx, "pointer", val);
  }
  case TYPE_array: {
    RootedObject baseType(cx, ArrayType::GetBaseType(targetType));
    size_t targetLength = ArrayType::GetLength(targetType);

    if (JSVAL_IS_STRING(val)) {
      JSString* sourceString = JSVAL_TO_STRING(val);
      size_t sourceLength = sourceString->length();
      const jschar* sourceChars = sourceString->getChars(cx);
      if (!sourceChars)
        return false;

      switch (CType::GetTypeCode(baseType)) {
      case TYPE_char:
      case TYPE_signed_char:
      case TYPE_unsigned_char: {
        // Convert from UTF-16 to UTF-8.
        size_t nbytes =
          GetDeflatedUTF8StringLength(cx, sourceChars, sourceLength);
        if (nbytes == (size_t) -1)
          return false;

        if (targetLength < nbytes) {
          JS_ReportError(cx, "ArrayType has insufficient length");
          return false;
        }

        char* charBuffer = static_cast<char*>(buffer);
        ASSERT_OK(DeflateStringToUTF8Buffer(cx, sourceChars, sourceLength,
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

    } else if (!JSVAL_IS_PRIMITIVE(val) && JS_IsArrayObject(cx, valObj)) {
      // Convert each element of the array by calling ImplicitConvert.
      uint32_t sourceLength;
      if (!JS_GetArrayLength(cx, valObj, &sourceLength) ||
          targetLength != size_t(sourceLength)) {
        JS_ReportError(cx, "ArrayType length does not match source array length");
        return false;
      }

      // Convert into an intermediate, in case of failure.
      size_t elementSize = CType::GetSize(baseType);
      size_t arraySize = elementSize * targetLength;
      AutoPtr<char> intermediate(cx->pod_malloc<char>(arraySize));
      if (!intermediate) {
        JS_ReportAllocationOverflow(cx);
        return false;
      }

      for (uint32_t i = 0; i < sourceLength; ++i) {
        RootedValue item(cx);
        if (!JS_GetElement(cx, valObj, i, &item))
          return false;

        char* data = intermediate.get() + elementSize * i;
        if (!ImplicitConvert(cx, item, baseType, data, false, nullptr))
          return false;
      }

      memcpy(buffer, intermediate.get(), arraySize);

    } else if (!JSVAL_IS_PRIMITIVE(val) &&
               JS_IsArrayBufferObject(valObj)) {
      // Check that array is consistent with type, then
      // copy the array.
      uint32_t sourceLength = JS_GetArrayBufferByteLength(valObj);
      size_t elementSize = CType::GetSize(baseType);
      size_t arraySize = elementSize * targetLength;
      if (arraySize != size_t(sourceLength)) {
        JS_ReportError(cx, "ArrayType length does not match source ArrayBuffer length");
        return false;
      }
      memcpy(buffer, JS_GetArrayBufferData(valObj), sourceLength);
      break;
    }  else if (!JSVAL_IS_PRIMITIVE(val) &&
               JS_IsTypedArrayObject(valObj)) {
      // Check that array is consistent with type, then
      // copy the array.
      if(!CanConvertTypedArrayItemTo(baseType, valObj, cx)) {
        return TypeError(cx, "typed array with the appropriate type", val);
      }

      uint32_t sourceLength = JS_GetTypedArrayByteLength(valObj);
      size_t elementSize = CType::GetSize(baseType);
      size_t arraySize = elementSize * targetLength;
      if (arraySize != size_t(sourceLength)) {
        JS_ReportError(cx, "typed array length does not match source TypedArray length");
        return false;
      }
      memcpy(buffer, JS_GetArrayBufferViewData(valObj), sourceLength);
      break;
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
      RootedObject iter(cx, JS_NewPropertyIterator(cx, valObj));
      if (!iter)
        return false;

      // Convert into an intermediate, in case of failure.
      size_t structSize = CType::GetSize(targetType);
      AutoPtr<char> intermediate(cx->pod_malloc<char>(structSize));
      if (!intermediate) {
        JS_ReportAllocationOverflow(cx);
        return false;
      }

      RootedId id(cx);
      size_t i = 0;
      while (1) {
        if (!JS_NextProperty(cx, iter, id.address()))
          return false;
        if (JSID_IS_VOID(id))
          break;

        if (!JSID_IS_STRING(id)) {
          JS_ReportError(cx, "property name is not a string");
          return false;
        }

        JSFlatString *name = JSID_TO_FLAT_STRING(id);
        const FieldInfo* field = StructType::LookupField(cx, targetType, name);
        if (!field)
          return false;

        RootedValue prop(cx);
        if (!JS_GetPropertyById(cx, valObj, id, &prop))
          return false;

        // Convert the field via ImplicitConvert().
        char* fieldData = intermediate.get() + field->mOffset;
        if (!ImplicitConvert(cx, prop, field->mType, fieldData, false, nullptr))
          return false;

        ++i;
      }

      const FieldInfoHash* fields = StructType::GetFieldInfo(targetType);
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
    MOZ_ASSUME_UNREACHABLE("invalid type");
  }

  return true;
}

// Convert jsval 'val' to a C binary representation of CType 'targetType',
// storing the result in 'buffer'. This function is more forceful than
// ImplicitConvert.
static bool
ExplicitConvert(JSContext* cx, HandleValue val, HandleObject targetType, void* buffer)
{
  // If ImplicitConvert succeeds, use that result.
  if (ImplicitConvert(cx, val, targetType, buffer, false, nullptr))
    return true;

  // If ImplicitConvert failed, and there is no pending exception, then assume
  // hard failure (out of memory, or some other similarly serious condition).
  // We store any pending exception in case we need to re-throw it.
  RootedValue ex(cx);
  if (!JS_GetPendingException(cx, &ex))
    return false;

  // Otherwise, assume soft failure. Clear the pending exception so that we
  // can throw a different one as required.
  JS_ClearPendingException(cx);

  TypeCode type = CType::GetTypeCode(targetType);

  switch (type) {
  case TYPE_bool: {
    *static_cast<bool*>(buffer) = ToBoolean(val);
    break;
  }
#define DEFINE_INT_TYPE(name, type, ffiType)                                   \
  case TYPE_##name: {                                                          \
    /* Convert numeric values with a C-style cast, and */                      \
    /* allow conversion from a base-10 or base-16 string. */                   \
    type result;                                                               \
    if (!jsvalToIntegerExplicit(val, &result) &&                               \
        (!JSVAL_IS_STRING(val) ||                                              \
         !StringToInteger(cx, JSVAL_TO_STRING(val), &result)))                 \
      return TypeError(cx, #name, val);                                        \
    *static_cast<type*>(buffer) = result;                                      \
    break;                                                                     \
  }
#define DEFINE_WRAPPED_INT_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_CHAR_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_JSCHAR_TYPE(x, y, z) DEFINE_CHAR_TYPE(x, y, z)
#include "ctypes/typedefs.h"
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
    JS_SetPendingException(cx, ex);
    return false;
  case TYPE_void_t:
  case TYPE_function:
    MOZ_ASSUME_UNREACHABLE("invalid type");
  }
  return true;
}

// Given a CType 'typeObj', generate a string describing the C type declaration
// corresponding to 'typeObj'. For instance, the CType constructed from
// 'ctypes.int32_t.ptr.array(4).ptr.ptr' will result in the type string
// 'int32_t*(**)[4]'.
static JSString*
BuildTypeName(JSContext* cx, JSObject* typeObj_)
{
  AutoString result;
  RootedObject typeObj(cx, typeObj_);

  // Walk the hierarchy of types, outermost to innermost, building up the type
  // string. This consists of the base type, which goes on the left.
  // Derived type modifiers (* and []) build from the inside outward, with
  // pointers on the left and arrays on the right. An excellent description
  // of the rules for building C type declarations can be found at:
  // http://unixwiz.net/techtips/reading-cdecl.html
  TypeCode prevGrouping = CType::GetTypeCode(typeObj), currentGrouping;
  while (1) {
    currentGrouping = CType::GetTypeCode(typeObj);
    switch (currentGrouping) {
    case TYPE_pointer: {
      // Pointer types go on the left.
      PrependString(result, "*");

      typeObj = PointerType::GetBaseType(typeObj);
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
      if (ArrayType::GetSafeLength(typeObj, &length))
        IntegerToString(length, 10, result);

      AppendString(result, "]");

      typeObj = ArrayType::GetBaseType(typeObj);
      prevGrouping = currentGrouping;
      continue;
    }
    case TYPE_function: {
      FunctionInfo* fninfo = FunctionType::GetFunctionInfo(typeObj);

      // Add in the calling convention, if it's not cdecl.
      // There's no trailing or leading space needed here, as none of the
      // modifiers can produce a string beginning with an identifier ---
      // except for TYPE_function itself, which is fine because functions
      // can't return functions.
      ABICode abi = GetABICode(fninfo->mABI);
      if (abi == ABI_STDCALL)
        PrependString(result, "__stdcall");
      else if (abi == ABI_WINAPI)
        PrependString(result, "WINAPI");

      // Function application binds more tightly than dereferencing, so
      // wrap pointer types in parens. Functions can't return functions
      // (only pointers to them), and arrays can't hold functions
      // (similarly), so we don't need to address those cases.
      if (prevGrouping == TYPE_pointer) {
        PrependString(result, "(");
        AppendString(result, ")");
      }

      // Argument list goes on the right.
      AppendString(result, "(");
      for (size_t i = 0; i < fninfo->mArgTypes.length(); ++i) {
        RootedObject argType(cx, fninfo->mArgTypes[i]);
        JSString* argName = CType::GetName(cx, argType);
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

  // If prepending the base type name directly would splice two
  // identifiers, insert a space.
  if (('a' <= result[0] && result[0] <= 'z') ||
      ('A' <= result[0] && result[0] <= 'Z') ||
      (result[0] == '_'))
    PrependString(result, " ");

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
                JSObject* typeObj_, 
                bool makeShort, 
                AutoString& result)
{
  RootedObject typeObj(cx, typeObj_);

  // Walk the types, building up the toSource() string.
  switch (CType::GetTypeCode(typeObj)) {
  case TYPE_void_t:
#define DEFINE_TYPE(name, type, ffiType)  \
  case TYPE_##name:
#include "ctypes/typedefs.h"
  {
    AppendString(result, "ctypes.");
    JSString* nameStr = CType::GetName(cx, typeObj);
    AppendString(result, nameStr);
    break;
  }
  case TYPE_pointer: {
    RootedObject baseType(cx, PointerType::GetBaseType(typeObj));

    // Specialcase ctypes.voidptr_t.
    if (CType::GetTypeCode(baseType) == TYPE_void_t) {
      AppendString(result, "ctypes.voidptr_t");
      break;
    }

    // Recursively build the source string, and append '.ptr'.
    BuildTypeSource(cx, baseType, makeShort, result);
    AppendString(result, ".ptr");
    break;
  }
  case TYPE_function: {
    FunctionInfo* fninfo = FunctionType::GetFunctionInfo(typeObj);

    AppendString(result, "ctypes.FunctionType(");

    switch (GetABICode(fninfo->mABI)) {
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
      MOZ_ASSUME_UNREACHABLE("invalid abi");
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
    JSObject* baseType = ArrayType::GetBaseType(typeObj);
    BuildTypeSource(cx, baseType, makeShort, result);
    AppendString(result, ".array(");

    size_t length;
    if (ArrayType::GetSafeLength(typeObj, &length))
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
    if (!CType::IsSizeDefined(typeObj)) {
      AppendString(result, ")");
      break;
    }

    AppendString(result, ", [");

    const FieldInfoHash* fields = StructType::GetFieldInfo(typeObj);
    size_t length = fields->count();
    Array<const FieldInfoHash::Entry*, 64> fieldsArray;
    if (!fieldsArray.resize(length))
      break;

    for (FieldInfoHash::Range r = fields->all(); !r.empty(); r.popFront())
      fieldsArray[r.front().value().mIndex] = &r.front();

    for (size_t i = 0; i < length; ++i) {
      const FieldInfoHash::Entry* entry = fieldsArray[i];
      AppendString(result, "{ \"");
      AppendString(result, entry->key());
      AppendString(result, "\": ");
      BuildTypeSource(cx, entry->value().mType, true, result);
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
static bool
BuildDataSource(JSContext* cx,
                HandleObject typeObj, 
                void* data, 
                bool isImplicit, 
                AutoString& result)
{
  TypeCode type = CType::GetTypeCode(typeObj);
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
    if (!NumericLimits<type>::is_signed)                                       \
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
#include "ctypes/typedefs.h"
  case TYPE_jschar: {
    // Serialize as a 1-character JS string.
    JSString* str = JS_NewUCStringCopyN(cx, static_cast<jschar*>(data), 1);
    if (!str)
      return false;

    // Escape characters, and quote as necessary.
    RootedValue valStr(cx, StringValue(str));
    JSString* src = JS_ValueToSource(cx, valStr);
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
    RootedObject baseType(cx, ArrayType::GetBaseType(typeObj));
    AppendString(result, "[");

    size_t length = ArrayType::GetLength(typeObj);
    size_t elementSize = CType::GetSize(baseType);
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
    const FieldInfoHash* fields = StructType::GetFieldInfo(typeObj);
    size_t length = fields->count();
    Array<const FieldInfoHash::Entry*, 64> fieldsArray;
    if (!fieldsArray.resize(length))
      return false;

    for (FieldInfoHash::Range r = fields->all(); !r.empty(); r.popFront())
      fieldsArray[r.front().value().mIndex] = &r.front();

    for (size_t i = 0; i < length; ++i) {
      const FieldInfoHash::Entry* entry = fieldsArray[i];

      if (isImplicit) {
        AppendString(result, "\"");
        AppendString(result, entry->key());
        AppendString(result, "\": ");
      }

      char* fieldData = static_cast<char*>(data) + entry->value().mOffset;
      RootedObject entryType(cx, entry->value().mType);
      if (!BuildDataSource(cx, entryType, fieldData, true, result))
        return false;

      if (i + 1 != length)
        AppendString(result, ", ");
    }

    if (isImplicit)
      AppendString(result, "}");

    break;
  }
  case TYPE_void_t:
    MOZ_ASSUME_UNREACHABLE("invalid type");
  }

  return true;
}

/*******************************************************************************
** JSAPI callback function implementations
*******************************************************************************/

bool
ConstructAbstract(JSContext* cx,
                  unsigned argc,
                  jsval* vp)
{
  // Calling an abstract base class constructor is disallowed.
  JS_ReportError(cx, "cannot construct from abstract type");
  return false;
}

/*******************************************************************************
** CType implementation
*******************************************************************************/

bool
CType::ConstructData(JSContext* cx,
                     unsigned argc,
                     jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  // get the callee object...
  RootedObject obj(cx, &args.callee());
  if (!CType::IsCType(obj)) {
    JS_ReportError(cx, "not a CType");
    return false;
  }

  // How we construct the CData object depends on what type we represent.
  // An instance 'd' of a CData object of type 't' has:
  //   * [[Class]] "CData"
  //   * __proto__ === t.prototype
  switch (GetTypeCode(obj)) {
  case TYPE_void_t:
    JS_ReportError(cx, "cannot construct from void_t");
    return false;
  case TYPE_function:
    JS_ReportError(cx, "cannot construct from FunctionType; use FunctionType.ptr instead");
    return false;
  case TYPE_pointer:
    return PointerType::ConstructData(cx, obj, args);
  case TYPE_array:
    return ArrayType::ConstructData(cx, obj, args);
  case TYPE_struct:
    return StructType::ConstructData(cx, obj, args);
  default:
    return ConstructBasic(cx, obj, args);
  }
}

bool
CType::ConstructBasic(JSContext* cx,
                      HandleObject obj,
                      const CallArgs& args)
{
  if (args.length() > 1) {
    JS_ReportError(cx, "CType constructor takes zero or one argument");
    return false;
  }

  // construct a CData object
  RootedObject result(cx, CData::Create(cx, obj, NullPtr(), nullptr, true));
  if (!result)
    return false;

  if (args.length() == 1) {
    if (!ExplicitConvert(cx, args[0], obj, CData::GetData(result)))
      return false;
  }

  args.rval().setObject(*result);
  return true;
}

JSObject*
CType::Create(JSContext* cx,
              HandleObject typeProto,
              HandleObject dataProto,
              TypeCode type,
              JSString* name_,
              jsval size_,
              jsval align_,
              ffi_type* ffiType)
{
  RootedString name(cx, name_);
  RootedValue size(cx, size_);
  RootedValue align(cx, align_);
  RootedObject parent(cx, JS_GetParent(typeProto));
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
  RootedObject typeObj(cx, JS_NewObject(cx, &sCTypeClass, typeProto, parent));
  if (!typeObj)
    return nullptr;

  // Set up the reserved slots.
  JS_SetReservedSlot(typeObj, SLOT_TYPECODE, INT_TO_JSVAL(type));
  if (ffiType)
    JS_SetReservedSlot(typeObj, SLOT_FFITYPE, PRIVATE_TO_JSVAL(ffiType));
  if (name)
    JS_SetReservedSlot(typeObj, SLOT_NAME, STRING_TO_JSVAL(name));
  JS_SetReservedSlot(typeObj, SLOT_SIZE, size);
  JS_SetReservedSlot(typeObj, SLOT_ALIGN, align);

  if (dataProto) {
    // Set up the 'prototype' and 'prototype.constructor' properties.
    RootedObject prototype(cx, JS_NewObject(cx, &sCDataProtoClass, dataProto, parent));
    if (!prototype)
      return nullptr;

    if (!JS_DefineProperty(cx, prototype, "constructor", OBJECT_TO_JSVAL(typeObj),
           nullptr, nullptr, JSPROP_READONLY | JSPROP_PERMANENT))
      return nullptr;

    // Set the 'prototype' object.
    //if (!JS_FreezeObject(cx, prototype)) // XXX fixme - see bug 541212!
    //  return nullptr;
    JS_SetReservedSlot(typeObj, SLOT_PROTO, OBJECT_TO_JSVAL(prototype));
  }

  if (!JS_FreezeObject(cx, typeObj))
    return nullptr;

  // Assert a sanity check on size and alignment: size % alignment should always
  // be zero.
  JS_ASSERT_IF(IsSizeDefined(typeObj),
               GetSize(typeObj) % GetAlignment(typeObj) == 0);

  return typeObj;
}

JSObject*
CType::DefineBuiltin(JSContext* cx,
                     JSObject* parent_,
                     const char* propName,
                     JSObject* typeProto_,
                     JSObject* dataProto_,
                     const char* name,
                     TypeCode type,
                     jsval size_,
                     jsval align_,
                     ffi_type* ffiType)
{
  RootedObject parent(cx, parent_);
  RootedObject typeProto(cx, typeProto_);
  RootedObject dataProto(cx, dataProto_);
  RootedValue size(cx, size_);
  RootedValue align(cx, align_);

  RootedString nameStr(cx, JS_NewStringCopyZ(cx, name));
  if (!nameStr)
    return nullptr;

  // Create a new CType object with the common properties and slots.
  RootedObject typeObj(cx, Create(cx, typeProto, dataProto, type, nameStr, size, align, ffiType));
  if (!typeObj)
    return nullptr;

  // Define the CType as a 'propName' property on 'parent'.
  if (!JS_DefineProperty(cx, parent, propName, OBJECT_TO_JSVAL(typeObj),
         nullptr, nullptr, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return nullptr;

  return typeObj;
}

void
CType::Finalize(JSFreeOp *fop, JSObject* obj)
{
  // Make sure our TypeCode slot is legit. If it's not, bail.
  jsval slot = JS_GetReservedSlot(obj, SLOT_TYPECODE);
  if (JSVAL_IS_VOID(slot))
    return;

  // The contents of our slots depends on what kind of type we are.
  switch (TypeCode(JSVAL_TO_INT(slot))) {
  case TYPE_function: {
    // Free the FunctionInfo.
    slot = JS_GetReservedSlot(obj, SLOT_FNINFO);
    if (!JSVAL_IS_VOID(slot))
      FreeOp::get(fop)->delete_(static_cast<FunctionInfo*>(JSVAL_TO_PRIVATE(slot)));
    break;
  }

  case TYPE_struct: {
    // Free the FieldInfoHash table.
    slot = JS_GetReservedSlot(obj, SLOT_FIELDINFO);
    if (!JSVAL_IS_VOID(slot)) {
      void* info = JSVAL_TO_PRIVATE(slot);
      FreeOp::get(fop)->delete_(static_cast<FieldInfoHash*>(info));
    }
  }

    // Fall through.
  case TYPE_array: {
    // Free the ffi_type info.
    slot = JS_GetReservedSlot(obj, SLOT_FFITYPE);
    if (!JSVAL_IS_VOID(slot)) {
      ffi_type* ffiType = static_cast<ffi_type*>(JSVAL_TO_PRIVATE(slot));
      FreeOp::get(fop)->free_(ffiType->elements);
      FreeOp::get(fop)->delete_(ffiType);
    }

    break;
  }
  default:
    // Nothing to do here.
    break;
  }
}

void
CType::Trace(JSTracer* trc, JSObject* obj)
{
  // Make sure our TypeCode slot is legit. If it's not, bail.
  jsval slot = obj->getSlot(SLOT_TYPECODE);
  if (JSVAL_IS_VOID(slot))
    return;

  // The contents of our slots depends on what kind of type we are.
  switch (TypeCode(JSVAL_TO_INT(slot))) {
  case TYPE_struct: {
    slot = obj->getReservedSlot(SLOT_FIELDINFO);
    if (JSVAL_IS_VOID(slot))
      return;

    FieldInfoHash* fields =
      static_cast<FieldInfoHash*>(JSVAL_TO_PRIVATE(slot));
    for (FieldInfoHash::Enum e(*fields); !e.empty(); e.popFront()) {
      JSString *key = e.front().key();
      JS_CallStringTracer(trc, &key, "fieldName");
      if (key != e.front().key())
          e.rekeyFront(JS_ASSERT_STRING_IS_FLAT(key));
      JS_CallHeapObjectTracer(trc, &e.front().value().mType, "fieldType");
    }

    break;
  }
  case TYPE_function: {
    // Check if we have a FunctionInfo.
    slot = obj->getReservedSlot(SLOT_FNINFO);
    if (JSVAL_IS_VOID(slot))
      return;

    FunctionInfo* fninfo = static_cast<FunctionInfo*>(JSVAL_TO_PRIVATE(slot));
    JS_ASSERT(fninfo);

    // Identify our objects to the tracer.
    JS_CallHeapObjectTracer(trc, &fninfo->mABI, "abi");
    JS_CallHeapObjectTracer(trc, &fninfo->mReturnType, "returnType");
    for (size_t i = 0; i < fninfo->mArgTypes.length(); ++i)
      JS_CallHeapObjectTracer(trc, &fninfo->mArgTypes[i], "argType");

    break;
  }
  default:
    // Nothing to do here.
    break;
  }
}

bool
CType::IsCType(JSObject* obj)
{
  return JS_GetClass(obj) == &sCTypeClass;
}

bool
CType::IsCTypeProto(JSObject* obj)
{
  return JS_GetClass(obj) == &sCTypeProtoClass;
}

TypeCode
CType::GetTypeCode(JSObject* typeObj)
{
  JS_ASSERT(IsCType(typeObj));

  jsval result = JS_GetReservedSlot(typeObj, SLOT_TYPECODE);
  return TypeCode(JSVAL_TO_INT(result));
}

bool
CType::TypesEqual(JSObject* t1, JSObject* t2)
{
  JS_ASSERT(IsCType(t1) && IsCType(t2));

  // Fast path: check for object equality.
  if (t1 == t2)
    return true;

  // First, perform shallow comparison.
  TypeCode c1 = GetTypeCode(t1);
  TypeCode c2 = GetTypeCode(t2);
  if (c1 != c2)
    return false;

  // Determine whether the types require shallow or deep comparison.
  switch (c1) {
  case TYPE_pointer: {
    // Compare base types.
    JSObject* b1 = PointerType::GetBaseType(t1);
    JSObject* b2 = PointerType::GetBaseType(t2);
    return TypesEqual(b1, b2);
  }
  case TYPE_function: {
    FunctionInfo* f1 = FunctionType::GetFunctionInfo(t1);
    FunctionInfo* f2 = FunctionType::GetFunctionInfo(t2);

    // Compare abi, return type, and argument types.
    if (f1->mABI != f2->mABI)
      return false;

    if (!TypesEqual(f1->mReturnType, f2->mReturnType))
      return false;

    if (f1->mArgTypes.length() != f2->mArgTypes.length())
      return false;

    if (f1->mIsVariadic != f2->mIsVariadic)
      return false;

    for (size_t i = 0; i < f1->mArgTypes.length(); ++i) {
      if (!TypesEqual(f1->mArgTypes[i], f2->mArgTypes[i]))
        return false;
    }

    return true;
  }
  case TYPE_array: {
    // Compare length, then base types.
    // An undefined length array matches other undefined length arrays.
    size_t s1 = 0, s2 = 0;
    bool d1 = ArrayType::GetSafeLength(t1, &s1);
    bool d2 = ArrayType::GetSafeLength(t2, &s2);
    if (d1 != d2 || (d1 && s1 != s2))
      return false;

    JSObject* b1 = ArrayType::GetBaseType(t1);
    JSObject* b2 = ArrayType::GetBaseType(t2);
    return TypesEqual(b1, b2);
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
CType::GetSafeSize(JSObject* obj, size_t* result)
{
  JS_ASSERT(CType::IsCType(obj));

  jsval size = JS_GetReservedSlot(obj, SLOT_SIZE);

  // The "size" property can be an int, a double, or JSVAL_VOID
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
CType::GetSize(JSObject* obj)
{
  JS_ASSERT(CType::IsCType(obj));

  jsval size = JS_GetReservedSlot(obj, SLOT_SIZE);

  JS_ASSERT(!JSVAL_IS_VOID(size));

  // The "size" property can be an int, a double, or JSVAL_VOID
  // (for arrays of undefined length), and must always fit in a size_t.
  // For callers who know it can never be JSVAL_VOID, return a size_t directly.
  if (JSVAL_IS_INT(size))
    return JSVAL_TO_INT(size);
  return Convert<size_t>(JSVAL_TO_DOUBLE(size));
}

bool
CType::IsSizeDefined(JSObject* obj)
{
  JS_ASSERT(CType::IsCType(obj));

  jsval size = JS_GetReservedSlot(obj, SLOT_SIZE);

  // The "size" property can be an int, a double, or JSVAL_VOID
  // (for arrays of undefined length), and must always fit in a size_t.
  JS_ASSERT(JSVAL_IS_INT(size) || JSVAL_IS_DOUBLE(size) || JSVAL_IS_VOID(size));
  return !JSVAL_IS_VOID(size);
}

size_t
CType::GetAlignment(JSObject* obj)
{
  JS_ASSERT(CType::IsCType(obj));

  jsval slot = JS_GetReservedSlot(obj, SLOT_ALIGN);
  return static_cast<size_t>(JSVAL_TO_INT(slot));
}

ffi_type*
CType::GetFFIType(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(obj));

  jsval slot = JS_GetReservedSlot(obj, SLOT_FFITYPE);

  if (!JSVAL_IS_VOID(slot)) {
    return static_cast<ffi_type*>(JSVAL_TO_PRIVATE(slot));
  }

  AutoPtr<ffi_type> result;
  switch (CType::GetTypeCode(obj)) {
  case TYPE_array:
    result = ArrayType::BuildFFIType(cx, obj);
    break;

  case TYPE_struct:
    result = StructType::BuildFFIType(cx, obj);
    break;

  default:
    MOZ_ASSUME_UNREACHABLE("simple types must have an ffi_type");
  }

  if (!result)
    return nullptr;
  JS_SetReservedSlot(obj, SLOT_FFITYPE, PRIVATE_TO_JSVAL(result.get()));
  return result.forget();
}

JSString*
CType::GetName(JSContext* cx, HandleObject obj)
{
  JS_ASSERT(CType::IsCType(obj));

  jsval string = JS_GetReservedSlot(obj, SLOT_NAME);
  if (!JSVAL_IS_VOID(string))
    return JSVAL_TO_STRING(string);

  // Build the type name lazily.
  JSString* name = BuildTypeName(cx, obj);
  if (!name)
    return nullptr;
  JS_SetReservedSlot(obj, SLOT_NAME, STRING_TO_JSVAL(name));
  return name;
}

JSObject*
CType::GetProtoFromCtor(JSObject* obj, CTypeProtoSlot slot)
{
  // Get ctypes.{Pointer,Array,Struct}Type.prototype from a reserved slot
  // on the type constructor.
  jsval protoslot = js::GetFunctionNativeReserved(obj, SLOT_FN_CTORPROTO);
  JSObject* proto = &protoslot.toObject();
  JS_ASSERT(proto);
  JS_ASSERT(CType::IsCTypeProto(proto));

  // Get the desired prototype.
  jsval result = JS_GetReservedSlot(proto, slot);
  return &result.toObject();
}

JSObject*
CType::GetProtoFromType(JSContext* cx, JSObject* objArg, CTypeProtoSlot slot)
{
  JS_ASSERT(IsCType(objArg));
  RootedObject obj(cx, objArg);

  // Get the prototype of the type object.
  RootedObject proto(cx);
  if (!JS_GetPrototype(cx, obj, &proto))
    return nullptr;
  JS_ASSERT(proto);
  JS_ASSERT(CType::IsCTypeProto(proto));

  // Get the requested ctypes.{Pointer,Array,Struct,Function}Type.prototype.
  jsval result = JS_GetReservedSlot(proto, slot);
  JS_ASSERT(!JSVAL_IS_PRIMITIVE(result));
  return JSVAL_TO_OBJECT(result);
}

bool
CType::IsCTypeOrProto(HandleValue v)
{
  if (!v.isObject())
    return false;
  JSObject* obj = &v.toObject();
  return CType::IsCType(obj) || CType::IsCTypeProto(obj);
}

bool
CType::PrototypeGetter(JSContext* cx, JS::CallArgs args)
{
  RootedObject obj(cx, &args.thisv().toObject());
  unsigned slot = CType::IsCTypeProto(obj) ? (unsigned) SLOT_OURDATAPROTO
                                           : (unsigned) SLOT_PROTO;
  args.rval().set(JS_GetReservedSlot(obj, slot));
  MOZ_ASSERT(args.rval().isObject() || args.rval().isUndefined());
  return true;
}

bool
CType::IsCType(HandleValue v)
{
  return v.isObject() && CType::IsCType(&v.toObject());
}

bool
CType::NameGetter(JSContext* cx, JS::CallArgs args)
{
  RootedObject obj(cx, &args.thisv().toObject());
  JSString* name = CType::GetName(cx, obj);
  if (!name)
    return false;

  args.rval().setString(name);
  return true;
}

bool
CType::SizeGetter(JSContext* cx, JS::CallArgs args)
{
  RootedObject obj(cx, &args.thisv().toObject());
  args.rval().set(JS_GetReservedSlot(obj, SLOT_SIZE));
  MOZ_ASSERT(args.rval().isNumber() || args.rval().isUndefined());
  return true;
}

bool
CType::PtrGetter(JSContext* cx, JS::CallArgs args)
{
  RootedObject obj(cx, &args.thisv().toObject());
  JSObject* pointerType = PointerType::CreateInternal(cx, obj);
  if (!pointerType)
    return false;

  args.rval().setObject(*pointerType);
  return true;
}

bool
CType::CreateArray(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject baseType(cx, JS_THIS_OBJECT(cx, vp));
  if (!baseType)
    return false;
  if (!CType::IsCType(baseType)) {
    JS_ReportError(cx, "not a CType");
    return false;
  }

  // Construct and return a new ArrayType object.
  if (args.length() > 1) {
    JS_ReportError(cx, "array takes zero or one argument");
    return false;
  }

  // Convert the length argument to a size_t.
  size_t length = 0;
  if (args.length() == 1 && !jsvalToSize(cx, args[0], false, &length)) {
    JS_ReportError(cx, "argument must be a nonnegative integer");
    return false;
  }

  JSObject* result = ArrayType::CreateInternal(cx, baseType, length, args.length() == 1);
  if (!result)
    return false;

  args.rval().setObject(*result);
  return true;
}

bool
CType::ToString(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
  if (!obj)
    return false;
  if (!CType::IsCType(obj) && !CType::IsCTypeProto(obj)) {
    JS_ReportError(cx, "not a CType");
    return false;
  }

  // Create the appropriate string depending on whether we're sCTypeClass or
  // sCTypeProtoClass.
  JSString* result;
  if (CType::IsCType(obj)) {
    AutoString type;
    AppendString(type, "type ");
    AppendString(type, GetName(cx, obj));
    result = NewUCString(cx, type);
  }
  else {
    result = JS_NewStringCopyZ(cx, "[CType proto object]");
  }
  if (!result)
    return false;

  args.rval().setString(result);
  return true;
}

bool
CType::ToSource(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj)
    return false;
  if (!CType::IsCType(obj) && !CType::IsCTypeProto(obj))
  {
    JS_ReportError(cx, "not a CType");
    return false;
  }

  // Create the appropriate string depending on whether we're sCTypeClass or
  // sCTypeProtoClass.
  JSString* result;
  if (CType::IsCType(obj)) {
    AutoString source;
    BuildTypeSource(cx, obj, false, source);
    result = NewUCString(cx, source);
  } else {
    result = JS_NewStringCopyZ(cx, "[CType proto object]");
  }
  if (!result)
    return false;

  args.rval().setString(result);
  return true;
}

bool
CType::HasInstance(JSContext* cx, HandleObject obj, MutableHandleValue v, bool* bp)
{
  JS_ASSERT(CType::IsCType(obj));

  jsval slot = JS_GetReservedSlot(obj, SLOT_PROTO);
  JS::Rooted<JSObject*> prototype(cx, &slot.toObject());
  JS_ASSERT(prototype);
  JS_ASSERT(CData::IsCDataProto(prototype));

  *bp = false;
  if (JSVAL_IS_PRIMITIVE(v))
    return true;

  RootedObject proto(cx, &v.toObject());
  for (;;) {
    if (!JS_GetPrototype(cx, proto, &proto))
      return false;
    if (!proto)
      break;
    if (proto == prototype) {
      *bp = true;
      break;
    }
  }
  return true;
}

static JSObject*
CType::GetGlobalCTypes(JSContext* cx, JSObject* objArg)
{
  JS_ASSERT(CType::IsCType(objArg));

  RootedObject obj(cx, objArg);
  RootedObject objTypeProto(cx);
  if (!JS_GetPrototype(cx, obj, &objTypeProto))
    return nullptr;
  JS_ASSERT(objTypeProto);
  JS_ASSERT(CType::IsCTypeProto(objTypeProto));

  jsval valCTypes = JS_GetReservedSlot(objTypeProto, SLOT_CTYPES);
  JS_ASSERT(!JSVAL_IS_PRIMITIVE(valCTypes));

  JS_ASSERT(!JSVAL_IS_PRIMITIVE(valCTypes));
  return &valCTypes.toObject();
}

/*******************************************************************************
** ABI implementation
*******************************************************************************/

bool
ABI::IsABI(JSObject* obj)
{
  return JS_GetClass(obj) == &sCABIClass;
}

bool
ABI::ToSource(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 0) {
    JS_ReportError(cx, "toSource takes zero arguments");
    return false;
  }

  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj)
    return false;
  if (!ABI::IsABI(obj)) {
    JS_ReportError(cx, "not an ABI");
    return false;
  }

  JSString* result;
  switch (GetABICode(obj)) {
    case ABI_DEFAULT:
      result = JS_NewStringCopyZ(cx, "ctypes.default_abi");
      break;
    case ABI_STDCALL:
      result = JS_NewStringCopyZ(cx, "ctypes.stdcall_abi");
      break;
    case ABI_WINAPI:
      result = JS_NewStringCopyZ(cx, "ctypes.winapi_abi");
      break;
    default:
      JS_ReportError(cx, "not a valid ABICode");
      return false;
  }
  if (!result)
    return false;

  args.rval().setString(result);
  return true;
}


/*******************************************************************************
** PointerType implementation
*******************************************************************************/

bool
PointerType::Create(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  // Construct and return a new PointerType object.
  if (args.length() != 1) {
    JS_ReportError(cx, "PointerType takes one argument");
    return false;
  }

  jsval arg = args[0];
  RootedObject obj(cx);
  if (JSVAL_IS_PRIMITIVE(arg) || !CType::IsCType(obj = &arg.toObject())) {
    JS_ReportError(cx, "first argument must be a CType");
    return false;
  }

  JSObject* result = CreateInternal(cx, obj);
  if (!result)
    return false;

  args.rval().setObject(*result);
  return true;
}

JSObject*
PointerType::CreateInternal(JSContext* cx, HandleObject baseType)
{
  // check if we have a cached PointerType on our base CType.
  jsval slot = JS_GetReservedSlot(baseType, SLOT_PTR);
  if (!slot.isUndefined())
    return &slot.toObject();

  // Get ctypes.PointerType.prototype and the common prototype for CData objects
  // of this type, or ctypes.FunctionType.prototype for function pointers.
  CTypeProtoSlot slotId = CType::GetTypeCode(baseType) == TYPE_function ?
    SLOT_FUNCTIONDATAPROTO : SLOT_POINTERDATAPROTO;
  RootedObject dataProto(cx, CType::GetProtoFromType(cx, baseType, slotId));
  if (!dataProto)
    return nullptr;
  RootedObject typeProto(cx, CType::GetProtoFromType(cx, baseType, SLOT_POINTERPROTO));
  if (!typeProto)
    return nullptr;

  // Create a new CType object with the common properties and slots.
  JSObject* typeObj = CType::Create(cx, typeProto, dataProto, TYPE_pointer,
                        nullptr, INT_TO_JSVAL(sizeof(void*)),
                        INT_TO_JSVAL(ffi_type_pointer.alignment),
                        &ffi_type_pointer);
  if (!typeObj)
    return nullptr;

  // Set the target type. (This will be 'null' for an opaque pointer type.)
  JS_SetReservedSlot(typeObj, SLOT_TARGET_T, OBJECT_TO_JSVAL(baseType));

  // Finally, cache our newly-created PointerType on our pointed-to CType.
  JS_SetReservedSlot(baseType, SLOT_PTR, OBJECT_TO_JSVAL(typeObj));

  return typeObj;
}

bool
PointerType::ConstructData(JSContext* cx,
                           HandleObject obj,
                           const CallArgs& args)
{
  if (!CType::IsCType(obj) || CType::GetTypeCode(obj) != TYPE_pointer) {
    JS_ReportError(cx, "not a PointerType");
    return false;
  }

  if (args.length() > 3) {
    JS_ReportError(cx, "constructor takes 0, 1, 2, or 3 arguments");
    return false;
  }

  RootedObject result(cx, CData::Create(cx, obj, NullPtr(), nullptr, true));
  if (!result)
    return false;

  // Set return value early, must not observe *vp after
  args.rval().setObject(*result);

  // There are 3 things that we might be creating here:
  // 1 - A null pointer (no arguments)
  // 2 - An initialized pointer (1 argument)
  // 3 - A closure (1-3 arguments)
  //
  // The API doesn't give us a perfect way to distinguish 2 and 3, but the
  // heuristics we use should be fine.

  //
  // Case 1 - Null pointer
  //
  if (args.length() == 0)
    return true;

  // Analyze the arguments a bit to decide what to do next.
  RootedObject baseObj(cx, PointerType::GetBaseType(obj));
  bool looksLikeClosure = CType::GetTypeCode(baseObj) == TYPE_function &&
                          args[0].isObject() &&
                          JS_ObjectIsCallable(cx, &args[0].toObject());

  //
  // Case 2 - Initialized pointer
  //
  if (!looksLikeClosure) {
    if (args.length() != 1) {
      JS_ReportError(cx, "first argument must be a function");
      return false;
    }
    return ExplicitConvert(cx, args[0], obj, CData::GetData(result));
  }

  //
  // Case 3 - Closure
  //

  // The second argument is an optional 'this' parameter with which to invoke
  // the given js function. Callers may leave this blank, or pass null if they
  // wish to pass the third argument.
  RootedObject thisObj(cx, nullptr);
  if (args.length() >= 2) {
    if (args[1].isNull()) {
      thisObj = nullptr;
    } else if (!JSVAL_IS_PRIMITIVE(args[1])) {
      thisObj = &args[1].toObject();
    } else if (!JS_ValueToObject(cx, args[1], &thisObj)) {
      return false;
    }
  }

  // The third argument is an optional error sentinel that js-ctypes will return
  // if an exception is raised while executing the closure. The type must match
  // the return type of the callback.
  jsval errVal = JSVAL_VOID;
  if (args.length() == 3)
    errVal = args[2];

  RootedObject fnObj(cx, &args[0].toObject());
  return FunctionType::ConstructData(cx, baseObj, result, fnObj, thisObj, errVal);
}

JSObject*
PointerType::GetBaseType(JSObject* obj)
{
  JS_ASSERT(CType::GetTypeCode(obj) == TYPE_pointer);

  jsval type = JS_GetReservedSlot(obj, SLOT_TARGET_T);
  JS_ASSERT(!type.isNull());
  return &type.toObject();
}

bool
PointerType::IsPointerType(HandleValue v)
{
  if (!v.isObject())
    return false;
  JSObject* obj = &v.toObject();
  return CType::IsCType(obj) && CType::GetTypeCode(obj) == TYPE_pointer;
}

bool
PointerType::IsPointer(HandleValue v)
{
  if (!v.isObject())
    return false;
  JSObject* obj = &v.toObject();
  return CData::IsCData(obj) && CType::GetTypeCode(CData::GetCType(obj)) == TYPE_pointer;
}

bool
PointerType::TargetTypeGetter(JSContext* cx, JS::CallArgs args)
{
  RootedObject obj(cx, &args.thisv().toObject());
  args.rval().set(JS_GetReservedSlot(obj, SLOT_TARGET_T));
  MOZ_ASSERT(args.rval().isObject());
  return true;
}

bool
PointerType::IsNull(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj)
    return false;
  if (!CData::IsCData(obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  // Get pointer type and base type.
  JSObject* typeObj = CData::GetCType(obj);
  if (CType::GetTypeCode(typeObj) != TYPE_pointer) {
    JS_ReportError(cx, "not a PointerType");
    return false;
  }

  void* data = *static_cast<void**>(CData::GetData(obj));
  args.rval().setBoolean(data == nullptr);
  return true;
}

bool
PointerType::OffsetBy(JSContext* cx, const CallArgs& args, int offset)
{
  JSObject* obj = JS_THIS_OBJECT(cx, args.base());
  if (!obj)
    return false;
  if (!CData::IsCData(obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  RootedObject typeObj(cx, CData::GetCType(obj));
  if (CType::GetTypeCode(typeObj) != TYPE_pointer) {
    JS_ReportError(cx, "not a PointerType");
    return false;
  }

  RootedObject baseType(cx, PointerType::GetBaseType(typeObj));
  if (!CType::IsSizeDefined(baseType)) {
    JS_ReportError(cx, "cannot modify pointer of undefined size");
    return false;
  }

  size_t elementSize = CType::GetSize(baseType);
  char* data = static_cast<char*>(*static_cast<void**>(CData::GetData(obj)));
  void* address = data + offset * elementSize;

  // Create a PointerType CData object containing the new address.
  JSObject* result = CData::Create(cx, typeObj, NullPtr(), &address, true);
  if (!result)
    return false;

  args.rval().setObject(*result);
  return true;
}

bool
PointerType::Increment(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  return OffsetBy(cx, args, 1);
}

bool
PointerType::Decrement(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  return OffsetBy(cx, args, -1);
}

bool
PointerType::ContentsGetter(JSContext* cx, JS::CallArgs args)
{
  RootedObject obj(cx, &args.thisv().toObject());
  RootedObject baseType(cx, GetBaseType(CData::GetCType(obj)));
  if (!CType::IsSizeDefined(baseType)) {
    JS_ReportError(cx, "cannot get contents of undefined size");
    return false;
  }

  void* data = *static_cast<void**>(CData::GetData(obj));
  if (data == nullptr) {
    JS_ReportError(cx, "cannot read contents of null pointer");
    return false;
  }

  RootedValue result(cx);
  if (!ConvertToJS(cx, baseType, NullPtr(), data, false, false, result.address()))
    return false;

  args.rval().set(result);
  return true;
}

bool
PointerType::ContentsSetter(JSContext* cx, JS::CallArgs args)
{
  RootedObject obj(cx, &args.thisv().toObject());
  RootedObject baseType(cx, GetBaseType(CData::GetCType(obj)));
  if (!CType::IsSizeDefined(baseType)) {
    JS_ReportError(cx, "cannot set contents of undefined size");
    return false;
  }

  void* data = *static_cast<void**>(CData::GetData(obj));
  if (data == nullptr) {
    JS_ReportError(cx, "cannot write contents to null pointer");
    return false;
  }

  args.rval().setUndefined();
  return ImplicitConvert(cx, args.get(0), baseType, data, false, nullptr);
}

/*******************************************************************************
** ArrayType implementation
*******************************************************************************/

bool
ArrayType::Create(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  // Construct and return a new ArrayType object.
  if (args.length() < 1 || args.length() > 2) {
    JS_ReportError(cx, "ArrayType takes one or two arguments");
    return false;
  }

  if (JSVAL_IS_PRIMITIVE(args[0]) ||
      !CType::IsCType(&args[0].toObject())) {
    JS_ReportError(cx, "first argument must be a CType");
    return false;
  }

  // Convert the length argument to a size_t.
  size_t length = 0;
  if (args.length() == 2 && !jsvalToSize(cx, args[1], false, &length)) {
    JS_ReportError(cx, "second argument must be a nonnegative integer");
    return false;
  }

  RootedObject baseType(cx, &args[0].toObject());
  JSObject* result = CreateInternal(cx, baseType, length, args.length() == 2);
  if (!result)
    return false;

  args.rval().setObject(*result);
  return true;
}

JSObject*
ArrayType::CreateInternal(JSContext* cx,
                          HandleObject baseType,
                          size_t length,
                          bool lengthDefined)
{
  // Get ctypes.ArrayType.prototype and the common prototype for CData objects
  // of this type, from ctypes.CType.prototype.
  RootedObject typeProto(cx, CType::GetProtoFromType(cx, baseType, SLOT_ARRAYPROTO));
  if (!typeProto)
    return nullptr;
  RootedObject dataProto(cx, CType::GetProtoFromType(cx, baseType, SLOT_ARRAYDATAPROTO));
  if (!dataProto)
    return nullptr;

  // Determine the size of the array from the base type, if possible.
  // The size of the base type must be defined.
  // If our length is undefined, both our size and length will be undefined.
  size_t baseSize;
  if (!CType::GetSafeSize(baseType, &baseSize)) {
    JS_ReportError(cx, "base size must be defined");
    return nullptr;
  }

  RootedValue sizeVal(cx, JSVAL_VOID);
  RootedValue lengthVal(cx, JSVAL_VOID);
  if (lengthDefined) {
    // Check for overflow, and convert to an int or double as required.
    size_t size = length * baseSize;
    if (length > 0 && size / length != baseSize) {
      JS_ReportError(cx, "size overflow");
      return nullptr;
    }
    if (!SizeTojsval(cx, size, sizeVal.address()) ||
        !SizeTojsval(cx, length, lengthVal.address()))
      return nullptr;
  }

  size_t align = CType::GetAlignment(baseType);

  // Create a new CType object with the common properties and slots.
  JSObject* typeObj = CType::Create(cx, typeProto, dataProto, TYPE_array, nullptr,
                        sizeVal, INT_TO_JSVAL(align), nullptr);
  if (!typeObj)
    return nullptr;

  // Set the element type.
  JS_SetReservedSlot(typeObj, SLOT_ELEMENT_T, OBJECT_TO_JSVAL(baseType));

  // Set the length.
  JS_SetReservedSlot(typeObj, SLOT_LENGTH, lengthVal);

  return typeObj;
}

bool
ArrayType::ConstructData(JSContext* cx,
                         HandleObject obj_,
                         const CallArgs& args)
{
  RootedObject obj(cx, obj_); // Make a mutable version

  if (!CType::IsCType(obj) || CType::GetTypeCode(obj) != TYPE_array) {
    JS_ReportError(cx, "not an ArrayType");
    return false;
  }

  // Decide whether we have an object to initialize from. We'll override this
  // if we get a length argument instead.
  bool convertObject = args.length() == 1;

  // Check if we're an array of undefined length. If we are, allow construction
  // with a length argument, or with an actual JS array.
  if (CType::IsSizeDefined(obj)) {
    if (args.length() > 1) {
      JS_ReportError(cx, "constructor takes zero or one argument");
      return false;
    }

  } else {
    if (args.length() != 1) {
      JS_ReportError(cx, "constructor takes one argument");
      return false;
    }

    RootedObject baseType(cx, GetBaseType(obj));

    size_t length;
    if (jsvalToSize(cx, args[0], false, &length)) {
      // Have a length, rather than an object to initialize from.
      convertObject = false;

    } else if (!JSVAL_IS_PRIMITIVE(args[0])) {
      // We were given an object with a .length property.
      // This could be a JS array, or a CData array.
      RootedObject arg(cx, &args[0].toObject());
      RootedValue lengthVal(cx);
      if (!JS_GetProperty(cx, arg, "length", &lengthVal) ||
          !jsvalToSize(cx, lengthVal, false, &length)) {
        JS_ReportError(cx, "argument must be an array object or length");
        return false;
      }

    } else if (args[0].isString()) {
      // We were given a string. Size the array to the appropriate length,
      // including space for the terminator.
      JSString* sourceString = args[0].toString();
      size_t sourceLength = sourceString->length();
      const jschar* sourceChars = sourceString->getChars(cx);
      if (!sourceChars)
        return false;

      switch (CType::GetTypeCode(baseType)) {
      case TYPE_char:
      case TYPE_signed_char:
      case TYPE_unsigned_char: {
        // Determine the UTF-8 length.
        length = GetDeflatedUTF8StringLength(cx, sourceChars, sourceLength);
        if (length == (size_t) -1)
          return false;

        ++length;
        break;
      }
      case TYPE_jschar:
        length = sourceLength + 1;
        break;
      default:
        return TypeError(cx, "array", args[0]);
      }

    } else {
      JS_ReportError(cx, "argument must be an array object or length");
      return false;
    }

    // Construct a new ArrayType of defined length, for the new CData object.
    obj = CreateInternal(cx, baseType, length, true);
    if (!obj)
      return false;
  }

  JSObject* result = CData::Create(cx, obj, NullPtr(), nullptr, true);
  if (!result)
    return false;

  args.rval().setObject(*result);

  if (convertObject) {
    if (!ExplicitConvert(cx, args[0], obj, CData::GetData(result)))
      return false;
  }

  return true;
}

JSObject*
ArrayType::GetBaseType(JSObject* obj)
{
  JS_ASSERT(CType::IsCType(obj));
  JS_ASSERT(CType::GetTypeCode(obj) == TYPE_array);

  jsval type = JS_GetReservedSlot(obj, SLOT_ELEMENT_T);
  JS_ASSERT(!JSVAL_IS_NULL(type));
  return &type.toObject();
}

bool
ArrayType::GetSafeLength(JSObject* obj, size_t* result)
{
  JS_ASSERT(CType::IsCType(obj));
  JS_ASSERT(CType::GetTypeCode(obj) == TYPE_array);

  jsval length = JS_GetReservedSlot(obj, SLOT_LENGTH);

  // The "length" property can be an int, a double, or JSVAL_VOID
  // (for arrays of undefined length), and must always fit in a size_t.
  if (length.isInt32()) {
    *result = length.toInt32();;
    return true;
  }
  if (length.isDouble()) {
    *result = Convert<size_t>(length.toDouble());
    return true;
  }

  JS_ASSERT(length.isUndefined());
  return false;
}

size_t
ArrayType::GetLength(JSObject* obj)
{
  JS_ASSERT(CType::IsCType(obj));
  JS_ASSERT(CType::GetTypeCode(obj) == TYPE_array);

  jsval length = JS_GetReservedSlot(obj, SLOT_LENGTH);

  JS_ASSERT(!length.isUndefined());

  // The "length" property can be an int, a double, or JSVAL_VOID
  // (for arrays of undefined length), and must always fit in a size_t.
  // For callers who know it can never be JSVAL_VOID, return a size_t directly.
  if (length.isInt32())
    return length.toInt32();;
  return Convert<size_t>(length.toDouble());
}

ffi_type*
ArrayType::BuildFFIType(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(obj));
  JS_ASSERT(CType::GetTypeCode(obj) == TYPE_array);
  JS_ASSERT(CType::IsSizeDefined(obj));

  JSObject* baseType = ArrayType::GetBaseType(obj);
  ffi_type* ffiBaseType = CType::GetFFIType(cx, baseType);
  if (!ffiBaseType)
    return nullptr;

  size_t length = ArrayType::GetLength(obj);

  // Create an ffi_type to represent the array. This is necessary for the case
  // where the array is part of a struct. Since libffi has no intrinsic
  // support for array types, we approximate it by creating a struct type
  // with elements of type 'baseType' and with appropriate size and alignment
  // values. It would be nice to not do all the work of setting up 'elements',
  // but some libffi platforms currently require that it be meaningful. I'm
  // looking at you, x86_64.
  AutoPtr<ffi_type> ffiType(cx->new_<ffi_type>());
  if (!ffiType) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  ffiType->type = FFI_TYPE_STRUCT;
  ffiType->size = CType::GetSize(obj);
  ffiType->alignment = CType::GetAlignment(obj);
  ffiType->elements = cx->pod_malloc<ffi_type*>(length + 1);
  if (!ffiType->elements) {
    JS_ReportAllocationOverflow(cx);
    return nullptr;
  }

  for (size_t i = 0; i < length; ++i)
    ffiType->elements[i] = ffiBaseType;
  ffiType->elements[length] = nullptr;

  return ffiType.forget();
}

bool
ArrayType::IsArrayType(HandleValue v)
{
  if (!v.isObject())
    return false;
  JSObject* obj = &v.toObject();
  return CType::IsCType(obj) && CType::GetTypeCode(obj) == TYPE_array;
}

bool
ArrayType::IsArrayOrArrayType(HandleValue v)
{
  if (!v.isObject())
    return false;
  JSObject* obj = &v.toObject();

   // Allow both CTypes and CDatas of the ArrayType persuasion by extracting the
   // CType if we're dealing with a CData.
  if (CData::IsCData(obj)) {
    obj = CData::GetCType(obj);
  }
  return CType::IsCType(obj) && CType::GetTypeCode(obj) == TYPE_array;
}

bool
ArrayType::ElementTypeGetter(JSContext* cx, JS::CallArgs args)
{
  RootedObject obj(cx, &args.thisv().toObject());
  args.rval().set(JS_GetReservedSlot(obj, SLOT_ELEMENT_T));
  MOZ_ASSERT(args.rval().isObject());
  return true;
}

bool
ArrayType::LengthGetter(JSContext* cx, JS::CallArgs args)
{
  JSObject *obj = &args.thisv().toObject();

  // This getter exists for both CTypes and CDatas of the ArrayType persuasion.
  // If we're dealing with a CData, get the CType from it.
  if (CData::IsCData(obj))
    obj = CData::GetCType(obj);

  args.rval().set(JS_GetReservedSlot(obj, SLOT_LENGTH));
  JS_ASSERT(args.rval().isNumber() || args.rval().isUndefined());
  return true;
}

bool
ArrayType::Getter(JSContext* cx, HandleObject obj, HandleId idval, MutableHandleValue vp)
{
  // This should never happen, but we'll check to be safe.
  if (!CData::IsCData(obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  // Bail early if we're not an ArrayType. (This setter is present for all
  // CData, regardless of CType.)
  JSObject* typeObj = CData::GetCType(obj);
  if (CType::GetTypeCode(typeObj) != TYPE_array)
    return true;

  // Convert the index to a size_t and bounds-check it.
  size_t index;
  size_t length = GetLength(typeObj);
  bool ok = jsidToSize(cx, idval, true, &index);
  int32_t dummy;
  if (!ok && JSID_IS_STRING(idval) && !StringToInteger(cx, JSID_TO_STRING(idval), &dummy)) {
    // String either isn't a number, or doesn't fit in size_t.
    // Chances are it's a regular property lookup, so return.
    return true;
  }
  if (!ok || index >= length) {
    JS_ReportError(cx, "invalid index");
    return false;
  }

  RootedObject baseType(cx, GetBaseType(typeObj));
  size_t elementSize = CType::GetSize(baseType);
  char* data = static_cast<char*>(CData::GetData(obj)) + elementSize * index;
  return ConvertToJS(cx, baseType, obj, data, false, false, vp.address());
}

bool
ArrayType::Setter(JSContext* cx, HandleObject obj, HandleId idval, bool strict, MutableHandleValue vp)
{
  // This should never happen, but we'll check to be safe.
  if (!CData::IsCData(obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  // Bail early if we're not an ArrayType. (This setter is present for all
  // CData, regardless of CType.)
  JSObject* typeObj = CData::GetCType(obj);
  if (CType::GetTypeCode(typeObj) != TYPE_array)
    return true;

  // Convert the index to a size_t and bounds-check it.
  size_t index;
  size_t length = GetLength(typeObj);
  bool ok = jsidToSize(cx, idval, true, &index);
  int32_t dummy;
  if (!ok && JSID_IS_STRING(idval) && !StringToInteger(cx, JSID_TO_STRING(idval), &dummy)) {
    // String either isn't a number, or doesn't fit in size_t.
    // Chances are it's a regular property lookup, so return.
    return true;
  }
  if (!ok || index >= length) {
    JS_ReportError(cx, "invalid index");
    return false;
  }

  JSObject* baseType = GetBaseType(typeObj);
  size_t elementSize = CType::GetSize(baseType);
  char* data = static_cast<char*>(CData::GetData(obj)) + elementSize * index;
  return ImplicitConvert(cx, vp, baseType, data, false, nullptr);
}

bool
ArrayType::AddressOfElement(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
  if (!obj)
    return false;
  if (!CData::IsCData(obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  RootedObject typeObj(cx, CData::GetCType(obj));
  if (CType::GetTypeCode(typeObj) != TYPE_array) {
    JS_ReportError(cx, "not an ArrayType");
    return false;
  }

  if (args.length() != 1) {
    JS_ReportError(cx, "addressOfElement takes one argument");
    return false;
  }

  RootedObject baseType(cx, GetBaseType(typeObj));
  RootedObject pointerType(cx, PointerType::CreateInternal(cx, baseType));
  if (!pointerType)
    return false;

  // Create a PointerType CData object containing null.
  RootedObject result(cx, CData::Create(cx, pointerType, NullPtr(), nullptr, true));
  if (!result)
    return false;

  args.rval().setObject(*result);

  // Convert the index to a size_t and bounds-check it.
  size_t index;
  size_t length = GetLength(typeObj);
  if (!jsvalToSize(cx, args[0], false, &index) ||
      index >= length) {
    JS_ReportError(cx, "invalid index");
    return false;
  }

  // Manually set the pointer inside the object, so we skip the conversion step.
  void** data = static_cast<void**>(CData::GetData(result));
  size_t elementSize = CType::GetSize(baseType);
  *data = static_cast<char*>(CData::GetData(obj)) + elementSize * index;
  return true;
}

/*******************************************************************************
** StructType implementation
*******************************************************************************/

// For a struct field descriptor 'val' of the form { name : type }, extract
// 'name' and 'type'.
static JSFlatString*
ExtractStructField(JSContext* cx, jsval val, JSObject** typeObj)
{
  if (JSVAL_IS_PRIMITIVE(val)) {
    JS_ReportError(cx, "struct field descriptors require a valid name and type");
    return nullptr;
  }

  RootedObject obj(cx, JSVAL_TO_OBJECT(val));
  RootedObject iter(cx, JS_NewPropertyIterator(cx, obj));
  if (!iter)
    return nullptr;

  RootedId nameid(cx);
  if (!JS_NextProperty(cx, iter, nameid.address()))
    return nullptr;
  if (JSID_IS_VOID(nameid)) {
    JS_ReportError(cx, "struct field descriptors require a valid name and type");
    return nullptr;
  }

  if (!JSID_IS_STRING(nameid)) {
    JS_ReportError(cx, "struct field descriptors require a valid name and type");
    return nullptr;
  }

  // make sure we have one, and only one, property
  jsid id;
  if (!JS_NextProperty(cx, iter, &id))
    return nullptr;
  if (!JSID_IS_VOID(id)) {
    JS_ReportError(cx, "struct field descriptors must contain one property");
    return nullptr;
  }

  RootedValue propVal(cx);
  if (!JS_GetPropertyById(cx, obj, nameid, &propVal))
    return nullptr;

  if (propVal.isPrimitive() || !CType::IsCType(&propVal.toObject())) {
    JS_ReportError(cx, "struct field descriptors require a valid name and type");
    return nullptr;
  }

  // Undefined size or zero size struct members are illegal.
  // (Zero-size arrays are legal as struct members in C++, but libffi will
  // choke on a zero-size struct, so we disallow them.)
  *typeObj = &propVal.toObject();
  size_t size;
  if (!CType::GetSafeSize(*typeObj, &size) || size == 0) {
    JS_ReportError(cx, "struct field types must have defined and nonzero size");
    return nullptr;
  }

  return JSID_TO_FLAT_STRING(nameid);
}

// For a struct field with 'name' and 'type', add an element of the form
// { name : type }.
static bool
AddFieldToArray(JSContext* cx,
                jsval* element,
                JSFlatString* name_,
                JSObject* typeObj_)
{
  RootedObject typeObj(cx, typeObj_);
  Rooted<JSFlatString*> name(cx, name_);
  RootedObject fieldObj(cx, JS_NewObject(cx, nullptr, NullPtr(), NullPtr()));
  if (!fieldObj)
    return false;

  *element = OBJECT_TO_JSVAL(fieldObj);

  if (!JS_DefineUCProperty(cx, fieldObj,
         name->chars(), name->length(),
         OBJECT_TO_JSVAL(typeObj), nullptr, nullptr,
         JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  return JS_FreezeObject(cx, fieldObj);
}

bool
StructType::Create(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);

  // Construct and return a new StructType object.
  if (args.length() < 1 || args.length() > 2) {
    JS_ReportError(cx, "StructType takes one or two arguments");
    return false;
  }

  jsval name = args[0];
  if (!name.isString()) {
    JS_ReportError(cx, "first argument must be a string");
    return false;
  }

  // Get ctypes.StructType.prototype from the ctypes.StructType constructor.
  RootedObject typeProto(cx, CType::GetProtoFromCtor(&args.callee(), SLOT_STRUCTPROTO));

  // Create a simple StructType with no defined fields. The result will be
  // non-instantiable as CData, will have no 'prototype' property, and will
  // have undefined size and alignment and no ffi_type.
  RootedObject result(cx, CType::Create(cx, typeProto, NullPtr(), TYPE_struct,
                                        JSVAL_TO_STRING(name), JSVAL_VOID, JSVAL_VOID, nullptr));
  if (!result)
    return false;

  if (args.length() == 2) {
    RootedObject arr(cx, JSVAL_IS_PRIMITIVE(args[1]) ? nullptr : &args[1].toObject());
    if (!arr || !JS_IsArrayObject(cx, arr)) {
      JS_ReportError(cx, "second argument must be an array");
      return false;
    }

    // Define the struct fields.
    if (!DefineInternal(cx, result, arr))
      return false;
  }

  args.rval().setObject(*result);
  return true;
}

static void
PostBarrierCallback(JSTracer *trc, JSString *key, void *data)
{
    typedef HashMap<JSFlatString*,
                    UnbarrieredFieldInfo,
                    FieldHashPolicy,
                    SystemAllocPolicy> UnbarrieredFieldInfoHash;

    UnbarrieredFieldInfoHash *table = reinterpret_cast<UnbarrieredFieldInfoHash*>(data);
    JSString *prior = key;
    JS_CallStringTracer(trc, &key, "CType fieldName");
    table->rekeyIfMoved(JS_ASSERT_STRING_IS_FLAT(prior), JS_ASSERT_STRING_IS_FLAT(key));
}

bool
StructType::DefineInternal(JSContext* cx, JSObject* typeObj_, JSObject* fieldsObj_)
{
  RootedObject typeObj(cx, typeObj_);
  RootedObject fieldsObj(cx, fieldsObj_);

  uint32_t len;
  ASSERT_OK(JS_GetArrayLength(cx, fieldsObj, &len));

  // Get the common prototype for CData objects of this type from
  // ctypes.CType.prototype.
  RootedObject dataProto(cx, CType::GetProtoFromType(cx, typeObj, SLOT_STRUCTDATAPROTO));
  if (!dataProto)
    return false;

  // Set up the 'prototype' and 'prototype.constructor' properties.
  // The prototype will reflect the struct fields as properties on CData objects
  // created from this type.
  RootedObject prototype(cx, JS_NewObject(cx, &sCDataProtoClass, dataProto, NullPtr()));
  if (!prototype)
    return false;

  if (!JS_DefineProperty(cx, prototype, "constructor", OBJECT_TO_JSVAL(typeObj),
         nullptr, nullptr, JSPROP_READONLY | JSPROP_PERMANENT))
    return false;

  // Create a FieldInfoHash to stash on the type object, and an array to root
  // its constituents. (We cannot simply stash the hash in a reserved slot now
  // to get GC safety for free, since if anything in this function fails we
  // do not want to mutate 'typeObj'.)
  AutoPtr<FieldInfoHash> fields(cx->new_<FieldInfoHash>());
  if (!fields || !fields->init(len)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  JS::AutoValueVector fieldRoots(cx);
  if (!fieldRoots.resize(len)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  // Process the field types.
  size_t structSize, structAlign;
  if (len != 0) {
    structSize = 0;
    structAlign = 0;

    for (uint32_t i = 0; i < len; ++i) {
      RootedValue item(cx);
      if (!JS_GetElement(cx, fieldsObj, i, &item))
        return false;

      RootedObject fieldType(cx, nullptr);
      JSFlatString* flat = ExtractStructField(cx, item, fieldType.address());
      if (!flat)
        return false;
      Rooted<JSStableString*> name(cx, flat->ensureStable(cx));
      if (!name)
        return false;
      fieldRoots[i] = JS::ObjectValue(*fieldType);

      // Make sure each field name is unique
      FieldInfoHash::AddPtr entryPtr = fields->lookupForAdd(name);
      if (entryPtr) {
        JS_ReportError(cx, "struct fields must have unique names");
        return false;
      }

      // Add the field to the StructType's 'prototype' property.
      if (!JS_DefineUCProperty(cx, prototype,
             name->chars().get(), name->length(), JSVAL_VOID,
             StructType::FieldGetter, StructType::FieldSetter,
             JSPROP_SHARED | JSPROP_ENUMERATE | JSPROP_PERMANENT))
        return false;

      size_t fieldSize = CType::GetSize(fieldType);
      size_t fieldAlign = CType::GetAlignment(fieldType);
      size_t fieldOffset = Align(structSize, fieldAlign);
      // Check for overflow. Since we hold invariant that fieldSize % fieldAlign
      // be zero, we can safely check fieldOffset + fieldSize without first
      // checking fieldOffset for overflow.
      if (fieldOffset + fieldSize < structSize) {
        JS_ReportError(cx, "size overflow");
        return false;
      }

      // Add field name to the hash
      FieldInfo info;
      info.mType = fieldType;
      info.mIndex = i;
      info.mOffset = fieldOffset;
      ASSERT_OK(fields->add(entryPtr, name, info));
      JS_StoreStringPostBarrierCallback(cx, PostBarrierCallback, name, fields.get());

      structSize = fieldOffset + fieldSize;

      if (fieldAlign > structAlign)
        structAlign = fieldAlign;
    }

    // Pad the struct tail according to struct alignment.
    size_t structTail = Align(structSize, structAlign);
    if (structTail < structSize) {
      JS_ReportError(cx, "size overflow");
      return false;
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

  RootedValue sizeVal(cx);
  if (!SizeTojsval(cx, structSize, sizeVal.address()))
    return false;

  JS_SetReservedSlot(typeObj, SLOT_FIELDINFO, PRIVATE_TO_JSVAL(fields.forget()));

  JS_SetReservedSlot(typeObj, SLOT_SIZE, sizeVal);
  JS_SetReservedSlot(typeObj, SLOT_ALIGN, INT_TO_JSVAL(structAlign));
  //if (!JS_FreezeObject(cx, prototype)0 // XXX fixme - see bug 541212!
  //  return false;
  JS_SetReservedSlot(typeObj, SLOT_PROTO, OBJECT_TO_JSVAL(prototype));
  return true;
}

ffi_type*
StructType::BuildFFIType(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(obj));
  JS_ASSERT(CType::GetTypeCode(obj) == TYPE_struct);
  JS_ASSERT(CType::IsSizeDefined(obj));

  const FieldInfoHash* fields = GetFieldInfo(obj);
  size_t len = fields->count();

  size_t structSize = CType::GetSize(obj);
  size_t structAlign = CType::GetAlignment(obj);

  AutoPtr<ffi_type> ffiType(cx->new_<ffi_type>());
  if (!ffiType) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }
  ffiType->type = FFI_TYPE_STRUCT;

  AutoPtr<ffi_type*> elements;
  if (len != 0) {
    elements = cx->pod_malloc<ffi_type*>(len + 1);
    if (!elements) {
      JS_ReportOutOfMemory(cx);
      return nullptr;
    }
    elements[len] = nullptr;

    for (FieldInfoHash::Range r = fields->all(); !r.empty(); r.popFront()) {
      const FieldInfoHash::Entry& entry = r.front();
      ffi_type* fieldType = CType::GetFFIType(cx, entry.value().mType);
      if (!fieldType)
        return nullptr;
      elements[entry.value().mIndex] = fieldType;
    }

  } else {
    // Represent an empty struct as having a size of 1 byte, just like C++.
    JS_ASSERT(structSize == 1);
    JS_ASSERT(structAlign == 1);
    elements = cx->pod_malloc<ffi_type*>(2);
    if (!elements) {
      JS_ReportOutOfMemory(cx);
      return nullptr;
    }
    elements[0] = &ffi_type_uint8;
    elements[1] = nullptr;
  }

  ffiType->elements = elements.get();

#ifdef DEBUG
  // Perform a sanity check: the result of our struct size and alignment
  // calculations should match libffi's. We force it to do this calculation
  // by calling ffi_prep_cif.
  ffi_cif cif;
  ffiType->size = 0;
  ffiType->alignment = 0;
  ffi_status status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 0, ffiType.get(), nullptr);
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

bool
StructType::Define(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
  if (!obj)
    return false;
  if (!CType::IsCType(obj) ||
      CType::GetTypeCode(obj) != TYPE_struct) {
    JS_ReportError(cx, "not a StructType");
    return false;
  }

  if (CType::IsSizeDefined(obj)) {
    JS_ReportError(cx, "StructType has already been defined");
    return false;
  }

  if (args.length() != 1) {
    JS_ReportError(cx, "define takes one argument");
    return false;
  }

  jsval arg = args[0];
  if (JSVAL_IS_PRIMITIVE(arg)) {
    JS_ReportError(cx, "argument must be an array");
    return false;
  }
  RootedObject arr(cx, JSVAL_TO_OBJECT(arg));
  if (!JS_IsArrayObject(cx, arr)) {
    JS_ReportError(cx, "argument must be an array");
    return false;
  }

  return DefineInternal(cx, obj, arr);
}

bool
StructType::ConstructData(JSContext* cx,
                          HandleObject obj,
                          const CallArgs& args)
{
  if (!CType::IsCType(obj) || CType::GetTypeCode(obj) != TYPE_struct) {
    JS_ReportError(cx, "not a StructType");
    return false;
  }

  if (!CType::IsSizeDefined(obj)) {
    JS_ReportError(cx, "cannot construct an opaque StructType");
    return false;
  }

  JSObject* result = CData::Create(cx, obj, NullPtr(), nullptr, true);
  if (!result)
    return false;

  args.rval().setObject(*result);

  if (args.length() == 0)
    return true;

  char* buffer = static_cast<char*>(CData::GetData(result));
  const FieldInfoHash* fields = GetFieldInfo(obj);

  if (args.length() == 1) {
    // There are two possible interpretations of the argument:
    // 1) It may be an object '{ ... }' with properties representing the
    //    struct fields intended to ExplicitConvert wholesale to our StructType.
    // 2) If the struct contains one field, the arg may be intended to
    //    ImplicitConvert directly to that arg's CType.
    // Thankfully, the conditions for these two possibilities to succeed
    // are mutually exclusive, so we can pick the right one.

    // Try option 1) first.
    if (ExplicitConvert(cx, args[0], obj, buffer))
      return true;

    if (fields->count() != 1)
      return false;

    // If ExplicitConvert failed, and there is no pending exception, then assume
    // hard failure (out of memory, or some other similarly serious condition).
    if (!JS_IsExceptionPending(cx))
      return false;

    // Otherwise, assume soft failure, and clear the pending exception so that we
    // can throw a different one as required.
    JS_ClearPendingException(cx);

    // Fall through to try option 2).
  }

  // We have a type constructor of the form 'ctypes.StructType(a, b, c, ...)'.
  // ImplicitConvert each field.
  if (args.length() == fields->count()) {
    for (FieldInfoHash::Range r = fields->all(); !r.empty(); r.popFront()) {
      const FieldInfo& field = r.front().value();
      STATIC_ASSUME(field.mIndex < fields->count());  /* Quantified invariant */
      if (!ImplicitConvert(cx, args[field.mIndex], field.mType,
             buffer + field.mOffset,
             false, nullptr))
        return false;
    }

    return true;
  }

  JS_ReportError(cx, "constructor takes 0, 1, or %u arguments",
    fields->count());
  return false;
}

const FieldInfoHash*
StructType::GetFieldInfo(JSObject* obj)
{
  JS_ASSERT(CType::IsCType(obj));
  JS_ASSERT(CType::GetTypeCode(obj) == TYPE_struct);

  jsval slot = JS_GetReservedSlot(obj, SLOT_FIELDINFO);
  JS_ASSERT(!JSVAL_IS_VOID(slot) && JSVAL_TO_PRIVATE(slot));

  return static_cast<const FieldInfoHash*>(JSVAL_TO_PRIVATE(slot));
}

const FieldInfo*
StructType::LookupField(JSContext* cx, JSObject* obj, JSFlatString *name)
{
  JS_ASSERT(CType::IsCType(obj));
  JS_ASSERT(CType::GetTypeCode(obj) == TYPE_struct);

  FieldInfoHash::Ptr ptr = GetFieldInfo(obj)->lookup(name);
  if (ptr)
    return &ptr->value();

  JSAutoByteString bytes(cx, name);
  if (!bytes)
    return nullptr;

  JS_ReportError(cx, "%s does not name a field", bytes.ptr());
  return nullptr;
}

JSObject*
StructType::BuildFieldsArray(JSContext* cx, JSObject* obj)
{
  JS_ASSERT(CType::IsCType(obj));
  JS_ASSERT(CType::GetTypeCode(obj) == TYPE_struct);
  JS_ASSERT(CType::IsSizeDefined(obj));

  const FieldInfoHash* fields = GetFieldInfo(obj);
  size_t len = fields->count();

  // Prepare a new array for the 'fields' property of the StructType.
  JS::AutoValueVector fieldsVec(cx);
  if (!fieldsVec.resize(len))
    return nullptr;

  for (FieldInfoHash::Range r = fields->all(); !r.empty(); r.popFront()) {
    const FieldInfoHash::Entry& entry = r.front();
    // Add the field descriptor to the array.
    if (!AddFieldToArray(cx, &fieldsVec[entry.value().mIndex],
                         entry.key(), entry.value().mType))
      return nullptr;
  }

  RootedObject fieldsProp(cx, JS_NewArrayObject(cx, len, fieldsVec.begin()));
  if (!fieldsProp)
    return nullptr;

  // Seal the fields array.
  if (!JS_FreezeObject(cx, fieldsProp))
    return nullptr;

  return fieldsProp;
}

/* static */ bool
StructType::IsStruct(HandleValue v)
{
  if (!v.isObject())
    return false;
  JSObject* obj = &v.toObject();
  return CType::IsCType(obj) && CType::GetTypeCode(obj) == TYPE_struct;
}

bool
StructType::FieldsArrayGetter(JSContext* cx, JS::CallArgs args)
{
  RootedObject obj(cx, &args.thisv().toObject());

  args.rval().set(JS_GetReservedSlot(obj, SLOT_FIELDS));

  if (!CType::IsSizeDefined(obj)) {
    MOZ_ASSERT(args.rval().isUndefined());
    return true;
  }

  if (args.rval().isUndefined()) {
    // Build the 'fields' array lazily.
    JSObject* fields = BuildFieldsArray(cx, obj);
    if (!fields)
      return false;
    JS_SetReservedSlot(obj, SLOT_FIELDS, OBJECT_TO_JSVAL(fields));

    args.rval().setObject(*fields);
  }

  MOZ_ASSERT(args.rval().isObject());
  MOZ_ASSERT(JS_IsArrayObject(cx, &args.rval().toObject()));
  return true;
}

bool
StructType::FieldGetter(JSContext* cx, HandleObject obj, HandleId idval, MutableHandleValue vp)
{
  if (!CData::IsCData(obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  JSObject* typeObj = CData::GetCType(obj);
  if (CType::GetTypeCode(typeObj) != TYPE_struct) {
    JS_ReportError(cx, "not a StructType");
    return false;
  }

  const FieldInfo* field = LookupField(cx, typeObj, JSID_TO_FLAT_STRING(idval));
  if (!field)
    return false;

  char* data = static_cast<char*>(CData::GetData(obj)) + field->mOffset;
  RootedObject fieldType(cx, field->mType);
  return ConvertToJS(cx, fieldType, obj, data, false, false, vp.address());
}

bool
StructType::FieldSetter(JSContext* cx, HandleObject obj, HandleId idval, bool strict, MutableHandleValue vp)
{
  if (!CData::IsCData(obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  JSObject* typeObj = CData::GetCType(obj);
  if (CType::GetTypeCode(typeObj) != TYPE_struct) {
    JS_ReportError(cx, "not a StructType");
    return false;
  }

  const FieldInfo* field = LookupField(cx, typeObj, JSID_TO_FLAT_STRING(idval));
  if (!field)
    return false;

  char* data = static_cast<char*>(CData::GetData(obj)) + field->mOffset;
  return ImplicitConvert(cx, vp, field->mType, data, false, nullptr);
}

bool
StructType::AddressOfField(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
  if (!obj)
    return false;
  if (!CData::IsCData(obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  JSObject* typeObj = CData::GetCType(obj);
  if (CType::GetTypeCode(typeObj) != TYPE_struct) {
    JS_ReportError(cx, "not a StructType");
    return false;
  }

  if (args.length() != 1) {
    JS_ReportError(cx, "addressOfField takes one argument");
    return false;
  }

  JSFlatString *str = JS_FlattenString(cx, args[0].toString());
  if (!str)
    return false;

  const FieldInfo* field = LookupField(cx, typeObj, str);
  if (!field)
    return false;

  RootedObject baseType(cx, field->mType);
  RootedObject pointerType(cx, PointerType::CreateInternal(cx, baseType));
  if (!pointerType)
    return false;

  // Create a PointerType CData object containing null.
  JSObject* result = CData::Create(cx, pointerType, NullPtr(), nullptr, true);
  if (!result)
    return false;

  args.rval().setObject(*result);

  // Manually set the pointer inside the object, so we skip the conversion step.
  void** data = static_cast<void**>(CData::GetData(result));
  *data = static_cast<char*>(CData::GetData(obj)) + field->mOffset;
  return true;
}

/*******************************************************************************
** FunctionType implementation
*******************************************************************************/

// Helper class for handling allocation of function arguments.
struct AutoValue
{
  AutoValue() : mData(nullptr) { }

  ~AutoValue()
  {
    js_free(mData);
  }

  bool SizeToType(JSContext* cx, JSObject* type)
  {
    // Allocate a minimum of sizeof(ffi_arg) to handle small integers.
    size_t size = Align(CType::GetSize(type), sizeof(ffi_arg));
    mData = js_malloc(size);
    if (mData)
      memset(mData, 0, size);
    return mData != nullptr;
  }

  void* mData;
};

static bool
GetABI(JSContext* cx, jsval abiType, ffi_abi* result)
{
  if (JSVAL_IS_PRIMITIVE(abiType))
    return false;

  ABICode abi = GetABICode(JSVAL_TO_OBJECT(abiType));

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
#elif (defined(_WIN64))
    // We'd like the same code to work across Win32 and Win64, so stdcall_api
    // and winapi_abi become aliases to the lone Win64 ABI.
    *result = FFI_WIN64;
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
      !CType::IsCType(JSVAL_TO_OBJECT(type))) {
    JS_ReportError(cx, "not a ctypes type");
    return nullptr;
  }

  JSObject* result = JSVAL_TO_OBJECT(type);
  TypeCode typeCode = CType::GetTypeCode(result);

  if (typeCode == TYPE_array) {
    // convert array argument types to pointers, just like C.
    // ImplicitConvert will do the same, when passing an array as data.
    RootedObject baseType(cx, ArrayType::GetBaseType(result));
    result = PointerType::CreateInternal(cx, baseType);
    if (!result)
      return nullptr;

  } else if (typeCode == TYPE_void_t || typeCode == TYPE_function) {
    // disallow void or function argument types
    JS_ReportError(cx, "Cannot have void or function argument type");
    return nullptr;
  }

  if (!CType::IsSizeDefined(result)) {
    JS_ReportError(cx, "Argument type must have defined size");
    return nullptr;
  }

  // libffi cannot pass types of zero size by value.
  JS_ASSERT(CType::GetSize(result) != 0);

  return result;
}

static JSObject*
PrepareReturnType(JSContext* cx, jsval type)
{
  if (JSVAL_IS_PRIMITIVE(type) ||
      !CType::IsCType(JSVAL_TO_OBJECT(type))) {
    JS_ReportError(cx, "not a ctypes type");
    return nullptr;
  }

  JSObject* result = JSVAL_TO_OBJECT(type);
  TypeCode typeCode = CType::GetTypeCode(result);

  // Arrays and functions can never be return types.
  if (typeCode == TYPE_array || typeCode == TYPE_function) {
    JS_ReportError(cx, "Return type cannot be an array or function");
    return nullptr;
  }

  if (typeCode != TYPE_void_t && !CType::IsSizeDefined(result)) {
    JS_ReportError(cx, "Return type must have defined size");
    return nullptr;
  }

  // libffi cannot pass types of zero size by value.
  JS_ASSERT(typeCode == TYPE_void_t || CType::GetSize(result) != 0);

  return result;
}

static JS_ALWAYS_INLINE bool
IsEllipsis(JSContext* cx, jsval v, bool* isEllipsis)
{
  *isEllipsis = false;
  if (!JSVAL_IS_STRING(v))
    return true;
  JSString* str = JSVAL_TO_STRING(v);
  if (str->length() != 3)
    return true;
  const jschar* chars = str->getChars(cx);
  if (!chars)
    return false;
  jschar dot = '.';
  *isEllipsis = (chars[0] == dot &&
                 chars[1] == dot &&
                 chars[2] == dot);
  return true;
}

static bool
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
FunctionType::BuildSymbolName(JSString* name,
                              JSObject* typeObj,
                              AutoCString& result)
{
  FunctionInfo* fninfo = GetFunctionInfo(typeObj);

  switch (GetABICode(fninfo->mABI)) {
  case ABI_DEFAULT:
  case ABI_WINAPI:
    // For cdecl or WINAPI functions, no mangling is necessary.
    AppendString(result, name);
    break;

  case ABI_STDCALL: {
#if (defined(_WIN32) && !defined(_WIN64)) || defined(_OS2)
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
      size += Align(CType::GetSize(argType), sizeof(ffi_arg));
    }

    IntegerToString(size, 10, result);
#elif defined(_WIN64)
    // On Win64, stdcall is an alias to the default ABI for compatibility, so no
    // mangling is done.
    AppendString(result, name);
#endif
    break;
  }

  case INVALID_ABI:
    MOZ_ASSUME_UNREACHABLE("invalid abi");
  }
}

static FunctionInfo*
NewFunctionInfo(JSContext* cx,
                jsval abiType,
                jsval returnType,
                jsval* argTypes,
                unsigned argLength)
{
  AutoPtr<FunctionInfo> fninfo(cx->new_<FunctionInfo>());
  if (!fninfo) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  ffi_abi abi;
  if (!GetABI(cx, abiType, &abi)) {
    JS_ReportError(cx, "Invalid ABI specification");
    return nullptr;
  }
  fninfo->mABI = JSVAL_TO_OBJECT(abiType);

  // prepare the result type
  fninfo->mReturnType = PrepareReturnType(cx, returnType);
  if (!fninfo->mReturnType)
    return nullptr;

  // prepare the argument types
  if (!fninfo->mArgTypes.reserve(argLength) ||
      !fninfo->mFFITypes.reserve(argLength)) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  fninfo->mIsVariadic = false;

  for (uint32_t i = 0; i < argLength; ++i) {
    bool isEllipsis;
    if (!IsEllipsis(cx, argTypes[i], &isEllipsis))
      return nullptr;
    if (isEllipsis) {
      fninfo->mIsVariadic = true;
      if (i < 1) {
        JS_ReportError(cx, "\"...\" may not be the first and only parameter "
                       "type of a variadic function declaration");
        return nullptr;
      }
      if (i < argLength - 1) {
        JS_ReportError(cx, "\"...\" must be the last parameter type of a "
                       "variadic function declaration");
        return nullptr;
      }
      if (GetABICode(fninfo->mABI) != ABI_DEFAULT) {
        JS_ReportError(cx, "Variadic functions must use the __cdecl calling "
                       "convention");
        return nullptr;
      }
      break;
    }

    JSObject* argType = PrepareType(cx, argTypes[i]);
    if (!argType)
      return nullptr;

    ffi_type* ffiType = CType::GetFFIType(cx, argType);
    if (!ffiType)
      return nullptr;

    fninfo->mArgTypes.infallibleAppend(argType);
    fninfo->mFFITypes.infallibleAppend(ffiType);
  }

  if (fninfo->mIsVariadic)
    // wait to PrepareCIF until function is called
    return fninfo.forget();

  if (!PrepareCIF(cx, fninfo.get()))
    return nullptr;

  return fninfo.forget();
}

bool
FunctionType::Create(JSContext* cx, unsigned argc, jsval* vp)
{
  // Construct and return a new FunctionType object.
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() < 2 || args.length() > 3) {
    JS_ReportError(cx, "FunctionType takes two or three arguments");
    return false;
  }

  AutoValueVector argTypes(cx);
  RootedObject arrayObj(cx, nullptr);

  if (args.length() == 3) {
    // Prepare an array of jsvals for the arguments.
    if (!JSVAL_IS_PRIMITIVE(args[2]))
      arrayObj = &args[2].toObject();
    if (!arrayObj || !JS_IsArrayObject(cx, arrayObj)) {
      JS_ReportError(cx, "third argument must be an array");
      return false;
    }

    uint32_t len;
    ASSERT_OK(JS_GetArrayLength(cx, arrayObj, &len));

    if (!argTypes.resize(len)) {
      JS_ReportOutOfMemory(cx);
      return false;
    }
  }

  // Pull out the argument types from the array, if any.
  JS_ASSERT_IF(argTypes.length(), arrayObj);
  for (uint32_t i = 0; i < argTypes.length(); ++i) {
    if (!JS_GetElement(cx, arrayObj, i, argTypes.handleAt(i)))
      return false;
  }

  JSObject* result = CreateInternal(cx, args[0], args[1],
      argTypes.begin(), argTypes.length());
  if (!result)
    return false;

  args.rval().setObject(*result);
  return true;
}

JSObject*
FunctionType::CreateInternal(JSContext* cx,
                             jsval abi,
                             jsval rtype,
                             jsval* argtypes,
                             unsigned arglen)
{
  // Determine and check the types, and prepare the function CIF.
  AutoPtr<FunctionInfo> fninfo(NewFunctionInfo(cx, abi, rtype, argtypes, arglen));
  if (!fninfo)
    return nullptr;

  // Get ctypes.FunctionType.prototype and the common prototype for CData objects
  // of this type, from ctypes.CType.prototype.
  RootedObject typeProto(cx, CType::GetProtoFromType(cx, fninfo->mReturnType,
                                                     SLOT_FUNCTIONPROTO));
  if (!typeProto)
    return nullptr;
  RootedObject dataProto(cx, CType::GetProtoFromType(cx, fninfo->mReturnType,
                                                     SLOT_FUNCTIONDATAPROTO));
  if (!dataProto)
    return nullptr;

  // Create a new CType object with the common properties and slots.
  JSObject* typeObj = CType::Create(cx, typeProto, dataProto, TYPE_function,
                        nullptr, JSVAL_VOID, JSVAL_VOID, nullptr);
  if (!typeObj)
    return nullptr;

  // Stash the FunctionInfo in a reserved slot.
  JS_SetReservedSlot(typeObj, SLOT_FNINFO, PRIVATE_TO_JSVAL(fninfo.forget()));

  return typeObj;
}

// Construct a function pointer to a JS function (see CClosure::Create()).
// Regular function pointers are constructed directly in
// PointerType::ConstructData().
bool
FunctionType::ConstructData(JSContext* cx,
                            HandleObject typeObj,
                            HandleObject dataObj,
                            HandleObject fnObj,
                            HandleObject thisObj,
                            jsval errVal)
{
  JS_ASSERT(CType::GetTypeCode(typeObj) == TYPE_function);

  PRFuncPtr* data = static_cast<PRFuncPtr*>(CData::GetData(dataObj));

  FunctionInfo* fninfo = FunctionType::GetFunctionInfo(typeObj);
  if (fninfo->mIsVariadic) {
    JS_ReportError(cx, "Can't declare a variadic callback function");
    return false;
  }
  if (GetABICode(fninfo->mABI) == ABI_WINAPI) {
    JS_ReportError(cx, "Can't declare a ctypes.winapi_abi callback function, "
                   "use ctypes.stdcall_abi instead");
    return false;
  }

  RootedObject closureObj(cx, CClosure::Create(cx, typeObj, fnObj, thisObj, errVal, data));
  if (!closureObj)
    return false;

  // Set the closure object as the referent of the new CData object.
  JS_SetReservedSlot(dataObj, SLOT_REFERENT, OBJECT_TO_JSVAL(closureObj));

  // Seal the CData object, to prevent modification of the function pointer.
  // This permanently associates this object with the closure, and avoids
  // having to do things like reset SLOT_REFERENT when someone tries to
  // change the pointer value.
  // XXX This will need to change when bug 541212 is fixed -- CData::ValueSetter
  // could be called on a frozen object.
  return JS_FreezeObject(cx, dataObj);
}

typedef Array<AutoValue, 16> AutoValueAutoArray;

static bool
ConvertArgument(JSContext* cx,
                HandleValue arg,
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

bool
FunctionType::Call(JSContext* cx,
                   unsigned argc,
                   jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  // get the callee object...
  RootedObject obj(cx, &args.callee());
  if (!CData::IsCData(obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  RootedObject typeObj(cx, CData::GetCType(obj));
  if (CType::GetTypeCode(typeObj) != TYPE_pointer) {
    JS_ReportError(cx, "not a FunctionType.ptr");
    return false;
  }

  typeObj = PointerType::GetBaseType(typeObj);
  if (CType::GetTypeCode(typeObj) != TYPE_function) {
    JS_ReportError(cx, "not a FunctionType.ptr");
    return false;
  }

  FunctionInfo* fninfo = GetFunctionInfo(typeObj);
  uint32_t argcFixed = fninfo->mArgTypes.length();

  if ((!fninfo->mIsVariadic && args.length() != argcFixed) ||
      (fninfo->mIsVariadic && args.length() < argcFixed)) {
    JS_ReportError(cx, "Number of arguments does not match declaration");
    return false;
  }

  // Check if we have a Library object. If we do, make sure it's open.
  jsval slot = JS_GetReservedSlot(obj, SLOT_REFERENT);
  if (!slot.isUndefined() && Library::IsLibrary(&slot.toObject())) {
    PRLibrary* library = Library::GetLibrary(&slot.toObject());
    if (!library) {
      JS_ReportError(cx, "library is not open");
      return false;
    }
  }

  // prepare the values for each argument
  AutoValueAutoArray values;
  AutoValueAutoArray strings;
  if (!values.resize(args.length())) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  for (unsigned i = 0; i < argcFixed; ++i)
    if (!ConvertArgument(cx, args[i], fninfo->mArgTypes[i], &values[i], &strings))
      return false;

  if (fninfo->mIsVariadic) {
    if (!fninfo->mFFITypes.resize(args.length())) {
      JS_ReportOutOfMemory(cx);
      return false;
    }

    RootedObject obj(cx);  // Could reuse obj instead of declaring a second
    RootedObject type(cx); // RootedObject, but readability would suffer.

    for (uint32_t i = argcFixed; i < args.length(); ++i) {
      if (JSVAL_IS_PRIMITIVE(args[i]) ||
          !CData::IsCData(obj = &args[i].toObject())) {
        // Since we know nothing about the CTypes of the ... arguments,
        // they absolutely must be CData objects already.
        JS_ReportError(cx, "argument %d of type %s is not a CData object",
                       i, JS_GetTypeName(cx, JS_TypeOfValue(cx, args[i])));
        return false;
      }
      if (!(type = CData::GetCType(obj)) ||
          !(type = PrepareType(cx, OBJECT_TO_JSVAL(type))) ||
          // Relying on ImplicitConvert only for the limited purpose of
          // converting one CType to another (e.g., T[] to T*).
          !ConvertArgument(cx, args[i], type, &values[i], &strings) ||
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
  TypeCode typeCode = CType::GetTypeCode(fninfo->mReturnType);
  if (typeCode != TYPE_void_t &&
      !returnValue.SizeToType(cx, fninfo->mReturnType)) {
    JS_ReportAllocationOverflow(cx);
    return false;
  }

  // Let the runtime callback know that we are about to call into C.
  js::AutoCTypesActivityCallback autoCallback(cx, js::CTYPES_CALL_BEGIN, js::CTYPES_CALL_END);

  uintptr_t fn = *reinterpret_cast<uintptr_t*>(CData::GetData(obj));

#if defined(XP_WIN)
  int32_t lastErrorStatus; // The status as defined by |GetLastError|
  int32_t savedLastError = GetLastError();
  SetLastError(0);
#endif //defined(XP_WIN)
  int errnoStatus;         // The status as defined by |errno|
  int savedErrno = errno;
  errno = 0;

  ffi_call(&fninfo->mCIF, FFI_FN(fn), returnValue.mData,
           reinterpret_cast<void**>(values.begin()));

  // Save error value.
  // We need to save it before leaving the scope of |suspend| as destructing
  // |suspend| has the side-effect of clearing |GetLastError|
  // (see bug 684017).

  errnoStatus = errno;
#if defined(XP_WIN)
  lastErrorStatus = GetLastError();
  SetLastError(savedLastError);
#endif // defined(XP_WIN)

  errno = savedErrno;

  // We're no longer calling into C.
  autoCallback.DoEndCallback();

  // Store the error value for later consultation with |ctypes.getStatus|
  JSObject *objCTypes = CType::GetGlobalCTypes(cx, typeObj);
  if (!objCTypes)
    return false;

  JS_SetReservedSlot(objCTypes, SLOT_ERRNO, INT_TO_JSVAL(errnoStatus));
#if defined(XP_WIN)
  JS_SetReservedSlot(objCTypes, SLOT_LASTERROR, INT_TO_JSVAL(lastErrorStatus));
#endif // defined(XP_WIN)

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
#include "ctypes/typedefs.h"
  default:
    break;
  }

  // prepare a JS object from the result
  RootedObject returnType(cx, fninfo->mReturnType);
  return ConvertToJS(cx, returnType, NullPtr(), returnValue.mData, false, true, vp);
}

FunctionInfo*
FunctionType::GetFunctionInfo(JSObject* obj)
{
  JS_ASSERT(CType::IsCType(obj));
  JS_ASSERT(CType::GetTypeCode(obj) == TYPE_function);

  jsval slot = JS_GetReservedSlot(obj, SLOT_FNINFO);
  JS_ASSERT(!JSVAL_IS_VOID(slot) && JSVAL_TO_PRIVATE(slot));

  return static_cast<FunctionInfo*>(JSVAL_TO_PRIVATE(slot));
}

bool
FunctionType::IsFunctionType(HandleValue v)
{
  if (!v.isObject())
    return false;
  JSObject* obj = &v.toObject();
  return CType::IsCType(obj) && CType::GetTypeCode(obj) == TYPE_function;
}

bool
FunctionType::ArgTypesGetter(JSContext* cx, JS::CallArgs args)
{
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  args.rval().set(JS_GetReservedSlot(obj, SLOT_ARGS_T));
  if (!args.rval().isUndefined())
    return true;

  FunctionInfo* fninfo = GetFunctionInfo(obj);
  size_t len = fninfo->mArgTypes.length();

  // Prepare a new array.
  JS::Rooted<JSObject*> argTypes(cx);
  {
      JS::AutoValueVector vec(cx);
      if (!vec.resize(len))
        return false;

      for (size_t i = 0; i < len; ++i)
        vec[i] = JS::ObjectValue(*fninfo->mArgTypes[i]);

      argTypes = JS_NewArrayObject(cx, len, vec.begin());
      if (!argTypes)
        return false;
  }

  // Seal and cache it.
  if (!JS_FreezeObject(cx, argTypes))
    return false;
  JS_SetReservedSlot(obj, SLOT_ARGS_T, JS::ObjectValue(*argTypes));

  args.rval().setObject(*argTypes);
  return true;
}

bool
FunctionType::ReturnTypeGetter(JSContext* cx, JS::CallArgs args)
{
  // Get the returnType object from the FunctionInfo.
  args.rval().setObject(*GetFunctionInfo(&args.thisv().toObject())->mReturnType);
  return true;
}

bool
FunctionType::ABIGetter(JSContext* cx, JS::CallArgs args)
{
  // Get the abi object from the FunctionInfo.
  args.rval().setObject(*GetFunctionInfo(&args.thisv().toObject())->mABI);
  return true;
}

bool
FunctionType::IsVariadicGetter(JSContext* cx, JS::CallArgs args)
{
  args.rval().setBoolean(GetFunctionInfo(&args.thisv().toObject())->mIsVariadic);
  return true;
}

/*******************************************************************************
** CClosure implementation
*******************************************************************************/

JSObject*
CClosure::Create(JSContext* cx,
                 HandleObject typeObj,
                 HandleObject fnObj,
                 HandleObject thisObj,
                 jsval errVal_,
                 PRFuncPtr* fnptr)
{
  RootedValue errVal(cx, errVal_);
  JS_ASSERT(fnObj);

  RootedObject result(cx, JS_NewObject(cx, &sCClosureClass, NullPtr(), NullPtr()));
  if (!result)
    return nullptr;

  // Get the FunctionInfo from the FunctionType.
  FunctionInfo* fninfo = FunctionType::GetFunctionInfo(typeObj);
  JS_ASSERT(!fninfo->mIsVariadic);
  JS_ASSERT(GetABICode(fninfo->mABI) != ABI_WINAPI);

  AutoPtr<ClosureInfo> cinfo(cx->new_<ClosureInfo>(JS_GetRuntime(cx)));
  if (!cinfo) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  // Get the prototype of the FunctionType object, of class CTypeProto,
  // which stores our JSContext for use with the closure.
  RootedObject proto(cx);
  if (!JS_GetPrototype(cx, typeObj, &proto))
    return nullptr;
  JS_ASSERT(proto);
  JS_ASSERT(CType::IsCTypeProto(proto));

  // Get a JSContext for use with the closure.
  cinfo->cx = js::DefaultJSContext(JS_GetRuntime(cx));

  // Prepare the error sentinel value. It's important to do this now, because
  // we might be unable to convert the value to the proper type. If so, we want
  // the caller to know about it _now_, rather than some uncertain time in the
  // future when the error sentinel is actually needed.
  if (!JSVAL_IS_VOID(errVal)) {

    // Make sure the callback returns something.
    if (CType::GetTypeCode(fninfo->mReturnType) == TYPE_void_t) {
      JS_ReportError(cx, "A void callback can't pass an error sentinel");
      return nullptr;
    }

    // With the exception of void, the FunctionType constructor ensures that
    // the return type has a defined size.
    JS_ASSERT(CType::IsSizeDefined(fninfo->mReturnType));

    // Allocate a buffer for the return value.
    size_t rvSize = CType::GetSize(fninfo->mReturnType);
    cinfo->errResult = cx->malloc_(rvSize);
    if (!cinfo->errResult)
      return nullptr;

    // Do the value conversion. This might fail, in which case we throw.
    if (!ImplicitConvert(cx, errVal, fninfo->mReturnType, cinfo->errResult,
                         false, nullptr))
      return nullptr;
  } else {
    cinfo->errResult = nullptr;
  }

  // Copy the important bits of context into cinfo.
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
    return nullptr;
  }

  ffi_status status = ffi_prep_closure_loc(cinfo->closure, &fninfo->mCIF,
    CClosure::ClosureStub, cinfo.get(), code);
  if (status != FFI_OK) {
    JS_ReportError(cx, "couldn't create closure - libffi error");
    return nullptr;
  }

  // Stash the ClosureInfo struct on our new object.
  JS_SetReservedSlot(result, SLOT_CLOSUREINFO, PRIVATE_TO_JSVAL(cinfo.forget()));

  // Casting between void* and a function pointer is forbidden in C and C++.
  // Do it via an integral type.
  *fnptr = reinterpret_cast<PRFuncPtr>(reinterpret_cast<uintptr_t>(code));
  return result;
}

void
CClosure::Trace(JSTracer* trc, JSObject* obj)
{
  // Make sure our ClosureInfo slot is legit. If it's not, bail.
  jsval slot = JS_GetReservedSlot(obj, SLOT_CLOSUREINFO);
  if (JSVAL_IS_VOID(slot))
    return;

  ClosureInfo* cinfo = static_cast<ClosureInfo*>(JSVAL_TO_PRIVATE(slot));

  // Identify our objects to the tracer. (There's no need to identify
  // 'closureObj', since that's us.)
  JS_CallHeapObjectTracer(trc, &cinfo->typeObj, "typeObj");
  JS_CallHeapObjectTracer(trc, &cinfo->jsfnObj, "jsfnObj");
  if (cinfo->thisObj)
    JS_CallHeapObjectTracer(trc, &cinfo->thisObj, "thisObj");
}

void
CClosure::Finalize(JSFreeOp *fop, JSObject* obj)
{
  // Make sure our ClosureInfo slot is legit. If it's not, bail.
  jsval slot = JS_GetReservedSlot(obj, SLOT_CLOSUREINFO);
  if (JSVAL_IS_VOID(slot))
    return;

  ClosureInfo* cinfo = static_cast<ClosureInfo*>(JSVAL_TO_PRIVATE(slot));
  FreeOp::get(fop)->delete_(cinfo);
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

  // Let the runtime callback know that we are about to call into JS again. The end callback will
  // fire automatically when we exit this function.
  js::AutoCTypesActivityCallback autoCallback(cx, js::CTYPES_CALLBACK_BEGIN,
                                              js::CTYPES_CALLBACK_END);

  RootedObject typeObj(cx, cinfo->typeObj);
  RootedObject thisObj(cx, cinfo->thisObj);
  RootedObject jsfnObj(cx, cinfo->jsfnObj);

  JS_AbortIfWrongThread(JS_GetRuntime(cx));

  JSAutoRequest ar(cx);
  JSAutoCompartment ac(cx, jsfnObj);

  // Assert that our CIFs agree.
  FunctionInfo* fninfo = FunctionType::GetFunctionInfo(typeObj);
  JS_ASSERT(cif == &fninfo->mCIF);

  TypeCode typeCode = CType::GetTypeCode(fninfo->mReturnType);

  // Initialize the result to zero, in case something fails. Small integer types
  // are promoted to a word-sized ffi_arg, so we must be careful to zero the
  // whole word.
  size_t rvSize = 0;
  if (cif->rtype != &ffi_type_void) {
    rvSize = cif->rtype->size;
    switch (typeCode) {
#define DEFINE_INT_TYPE(name, type, ffiType)                                   \
    case TYPE_##name:
#define DEFINE_WRAPPED_INT_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_BOOL_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_CHAR_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#define DEFINE_JSCHAR_TYPE(x, y, z) DEFINE_INT_TYPE(x, y, z)
#include "ctypes/typedefs.h"
      rvSize = Align(rvSize, sizeof(ffi_arg));
      break;
    default:
      break;
    }
    memset(result, 0, rvSize);
  }

  // Set up an array for converted arguments.
  JS::AutoValueVector argv(cx);
  if (!argv.resize(cif->nargs)) {
    JS_ReportOutOfMemory(cx);
    return;
  }

  for (uint32_t i = 0; i < cif->nargs; ++i) {
    // Convert each argument, and have any CData objects created depend on
    // the existing buffers.
    RootedObject argType(cx, fninfo->mArgTypes[i]);
    if (!ConvertToJS(cx, argType, NullPtr(), args[i], false, false, &argv[i]))
      return;
  }

  // Call the JS function. 'thisObj' may be nullptr, in which case the JS
  // engine will find an appropriate object to use.
  RootedValue rval(cx);
  bool success = JS_CallFunctionValue(cx, thisObj, OBJECT_TO_JSVAL(jsfnObj),
                                        cif->nargs, argv.begin(), rval.address());

  // Convert the result. Note that we pass 'isArgument = false', such that
  // ImplicitConvert will *not* autoconvert a JS string into a pointer-to-char
  // type, which would require an allocation that we can't track. The JS
  // function must perform this conversion itself and return a PointerType
  // CData; thusly, the burden of freeing the data is left to the user.
  if (success && cif->rtype != &ffi_type_void)
    success = ImplicitConvert(cx, rval, fninfo->mReturnType, result, false,
                              nullptr);

  if (!success) {
    // Something failed. The callee may have thrown, or it may not have
    // returned a value that ImplicitConvert() was happy with. Depending on how
    // prudent the consumer has been, we may or may not have a recovery plan.

    // In any case, a JS exception cannot be passed to C code, so report the
    // exception if any and clear it from the cx.
    if (JS_IsExceptionPending(cx))
      JS_ReportPendingException(cx);

    if (cinfo->errResult) {
      // Good case: we have a sentinel that we can return. Copy it in place of
      // the actual return value, and then proceed.

      // The buffer we're returning might be larger than the size of the return
      // type, due to libffi alignment issues (see above). But it should never
      // be smaller.
      size_t copySize = CType::GetSize(fninfo->mReturnType);
      JS_ASSERT(copySize <= rvSize);
      memcpy(result, cinfo->errResult, copySize);
    } else {
      // Bad case: not much we can do here. The rv is already zeroed out, so we
      // just report (another) error and hope for the best. JS_ReportError will
      // actually throw an exception here, so then we have to report it. Again.
      // Ugh.
      JS_ReportError(cx, "JavaScript callback failed, and an error sentinel "
                         "was not specified.");
      if (JS_IsExceptionPending(cx))
        JS_ReportPendingException(cx);

      return;
    }
  }

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
#include "ctypes/typedefs.h"
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
// * Otherwise 'refObj' is nullptr. In this case, 'ownResult' may be true or
//   false.
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
              HandleObject typeObj,
              HandleObject refObj,
              void* source,
              bool ownResult)
{
  JS_ASSERT(typeObj);
  JS_ASSERT(CType::IsCType(typeObj));
  JS_ASSERT(CType::IsSizeDefined(typeObj));
  JS_ASSERT(ownResult || source);
  JS_ASSERT_IF(refObj && CData::IsCData(refObj), !ownResult);

  // Get the 'prototype' property from the type.
  jsval slot = JS_GetReservedSlot(typeObj, SLOT_PROTO);
  JS_ASSERT(!JSVAL_IS_PRIMITIVE(slot));

  RootedObject proto(cx, JSVAL_TO_OBJECT(slot));
  RootedObject parent(cx, JS_GetParent(typeObj));
  JS_ASSERT(parent);

  RootedObject dataObj(cx, JS_NewObject(cx, &sCDataClass, proto, parent));
  if (!dataObj)
    return nullptr;

  // set the CData's associated type
  JS_SetReservedSlot(dataObj, SLOT_CTYPE, OBJECT_TO_JSVAL(typeObj));

  // Stash the referent object, if any, for GC safety.
  if (refObj)
    JS_SetReservedSlot(dataObj, SLOT_REFERENT, OBJECT_TO_JSVAL(refObj));

  // Set our ownership flag.
  JS_SetReservedSlot(dataObj, SLOT_OWNS, BOOLEAN_TO_JSVAL(ownResult));

  // attach the buffer. since it might not be 2-byte aligned, we need to
  // allocate an aligned space for it and store it there. :(
  char** buffer = cx->new_<char*>();
  if (!buffer) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  char* data;
  if (!ownResult) {
    data = static_cast<char*>(source);
  } else {
    // Initialize our own buffer.
    size_t size = CType::GetSize(typeObj);
    data = (char*)cx->malloc_(size);
    if (!data) {
      // Report a catchable allocation error.
      JS_ReportAllocationOverflow(cx);
      js_free(buffer);
      return nullptr;
    }

    if (!source)
      memset(data, 0, size);
    else
      memcpy(data, source, size);
  }

  *buffer = data;
  JS_SetReservedSlot(dataObj, SLOT_DATA, PRIVATE_TO_JSVAL(buffer));

  return dataObj;
}

void
CData::Finalize(JSFreeOp *fop, JSObject* obj)
{
  // Delete our buffer, and the data it contains if we own it.
  jsval slot = JS_GetReservedSlot(obj, SLOT_OWNS);
  if (JSVAL_IS_VOID(slot))
    return;

  bool owns = JSVAL_TO_BOOLEAN(slot);

  slot = JS_GetReservedSlot(obj, SLOT_DATA);
  if (JSVAL_IS_VOID(slot))
    return;
  char** buffer = static_cast<char**>(JSVAL_TO_PRIVATE(slot));

  if (owns)
    FreeOp::get(fop)->free_(*buffer);
  FreeOp::get(fop)->delete_(buffer);
}

JSObject*
CData::GetCType(JSObject* dataObj)
{
  JS_ASSERT(CData::IsCData(dataObj));

  jsval slot = JS_GetReservedSlot(dataObj, SLOT_CTYPE);
  JSObject* typeObj = JSVAL_TO_OBJECT(slot);
  JS_ASSERT(CType::IsCType(typeObj));
  return typeObj;
}

void*
CData::GetData(JSObject* dataObj)
{
  JS_ASSERT(CData::IsCData(dataObj));

  jsval slot = JS_GetReservedSlot(dataObj, SLOT_DATA);

  void** buffer = static_cast<void**>(JSVAL_TO_PRIVATE(slot));
  JS_ASSERT(buffer);
  JS_ASSERT(*buffer);
  return *buffer;
}

bool
CData::IsCData(JSObject* obj)
{
  return JS_GetClass(obj) == &sCDataClass;
}

bool
CData::IsCData(HandleValue v)
{
  return v.isObject() && CData::IsCData(&v.toObject());
}

bool
CData::IsCDataProto(JSObject* obj)
{
  return JS_GetClass(obj) == &sCDataProtoClass;
}

bool
CData::ValueGetter(JSContext* cx, JS::CallArgs args)
{
  RootedObject obj(cx, &args.thisv().toObject());

  // Convert the value to a primitive; do not create a new CData object.
  RootedObject ctype(cx, GetCType(obj));
  return ConvertToJS(cx, ctype, NullPtr(), GetData(obj), true, false, args.rval().address());
}

bool
CData::ValueSetter(JSContext* cx, JS::CallArgs args)
{
  RootedObject obj(cx, &args.thisv().toObject());
  args.rval().setUndefined();
  return ImplicitConvert(cx, args.get(0), GetCType(obj), GetData(obj), false, nullptr);
}

bool
CData::Address(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 0) {
    JS_ReportError(cx, "address takes zero arguments");
    return false;
  }

  RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
  if (!obj)
    return false;
  if (!IsCData(obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  RootedObject typeObj(cx, CData::GetCType(obj));
  RootedObject pointerType(cx, PointerType::CreateInternal(cx, typeObj));
  if (!pointerType)
    return false;

  // Create a PointerType CData object containing null.
  JSObject* result = CData::Create(cx, pointerType, NullPtr(), nullptr, true);
  if (!result)
    return false;

  args.rval().setObject(*result);

  // Manually set the pointer inside the object, so we skip the conversion step.
  void** data = static_cast<void**>(GetData(result));
  *data = GetData(obj);
  return true;
}

bool
CData::Cast(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 2) {
    JS_ReportError(cx, "cast takes two arguments");
    return false;
  }

  if (JSVAL_IS_PRIMITIVE(args[0]) ||
      !CData::IsCData(&args[0].toObject())) {
    JS_ReportError(cx, "first argument must be a CData");
    return false;
  }
  RootedObject sourceData(cx, &args[0].toObject());
  JSObject* sourceType = CData::GetCType(sourceData);

  if (JSVAL_IS_PRIMITIVE(args[1]) ||
      !CType::IsCType(&args[1].toObject())) {
    JS_ReportError(cx, "second argument must be a CType");
    return false;
  }

  RootedObject targetType(cx, &args[1].toObject());
  size_t targetSize;
  if (!CType::GetSafeSize(targetType, &targetSize) ||
      targetSize > CType::GetSize(sourceType)) {
    JS_ReportError(cx,
      "target CType has undefined or larger size than source CType");
    return false;
  }

  // Construct a new CData object with a type of 'targetType' and a referent
  // of 'sourceData'.
  void* data = CData::GetData(sourceData);
  JSObject* result = CData::Create(cx, targetType, sourceData, data, false);
  if (!result)
    return false;

  args.rval().setObject(*result);
  return true;
}

bool
CData::GetRuntime(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1) {
    JS_ReportError(cx, "getRuntime takes one argument");
    return false;
  }

  if (JSVAL_IS_PRIMITIVE(args[0]) ||
      !CType::IsCType(&args[0].toObject())) {
    JS_ReportError(cx, "first argument must be a CType");
    return false;
  }

  RootedObject targetType(cx, &args[0].toObject());
  size_t targetSize;
  if (!CType::GetSafeSize(targetType, &targetSize) ||
      targetSize != sizeof(void*)) {
    JS_ReportError(cx, "target CType has non-pointer size");
    return false;
  }

  void* data = static_cast<void*>(cx->runtime());
  JSObject* result = CData::Create(cx, targetType, NullPtr(), &data, true);
  if (!result)
    return false;

  args.rval().setObject(*result);
  return true;
}

typedef JS::TwoByteCharsZ (*InflateUTF8Method)(JSContext *, const JS::UTF8Chars, size_t *);

static bool
ReadStringCommon(JSContext* cx, InflateUTF8Method inflateUTF8, unsigned argc, jsval *vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 0) {
    JS_ReportError(cx, "readString takes zero arguments");
    return false;
  }

  JSObject* obj = CDataFinalizer::GetCData(cx, JS_THIS_OBJECT(cx, vp));
  if (!obj || !CData::IsCData(obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  // Make sure we are a pointer to, or an array of, an 8-bit or 16-bit
  // character or integer type.
  JSObject* baseType;
  JSObject* typeObj = CData::GetCType(obj);
  TypeCode typeCode = CType::GetTypeCode(typeObj);
  void* data;
  size_t maxLength = -1;
  switch (typeCode) {
  case TYPE_pointer:
    baseType = PointerType::GetBaseType(typeObj);
    data = *static_cast<void**>(CData::GetData(obj));
    if (data == nullptr) {
      JS_ReportError(cx, "cannot read contents of null pointer");
      return false;
    }
    break;
  case TYPE_array:
    baseType = ArrayType::GetBaseType(typeObj);
    data = CData::GetData(obj);
    maxLength = ArrayType::GetLength(typeObj);
    break;
  default:
    JS_ReportError(cx, "not a PointerType or ArrayType");
    return false;
  }

  // Convert the string buffer, taking care to determine the correct string
  // length in the case of arrays (which may contain embedded nulls).
  JSString* result;
  switch (CType::GetTypeCode(baseType)) {
  case TYPE_int8_t:
  case TYPE_uint8_t:
  case TYPE_char:
  case TYPE_signed_char:
  case TYPE_unsigned_char: {
    char* bytes = static_cast<char*>(data);
    size_t length = strnlen(bytes, maxLength);

    // Determine the length.
    jschar *dst = inflateUTF8(cx, JS::UTF8Chars(bytes, length), &length).get();
    if (!dst)
      return false;

    result = JS_NewUCString(cx, dst, length);
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
    return false;
  }

  if (!result)
    return false;

  args.rval().setString(result);
  return true;
}

bool
CData::ReadString(JSContext* cx, unsigned argc, jsval* vp)
{
  return ReadStringCommon(cx, JS::UTF8CharsToNewTwoByteCharsZ, argc, vp);
}

bool
CData::ReadStringReplaceMalformed(JSContext* cx, unsigned argc, jsval* vp)
{
  return ReadStringCommon(cx, JS::LossyUTF8CharsToNewTwoByteCharsZ, argc, vp);
}

JSString *
CData::GetSourceString(JSContext *cx, HandleObject typeObj, void *data)
{
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
    return nullptr;

  AppendString(source, ")");

  return NewUCString(cx, source);
}

bool
CData::ToSource(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 0) {
    JS_ReportError(cx, "toSource takes zero arguments");
    return false;
  }

  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj)
    return false;
  if (!CData::IsCData(obj) && !CData::IsCDataProto(obj)) {
    JS_ReportError(cx, "not a CData");
    return false;
  }

  JSString* result;
  if (CData::IsCData(obj)) {
    RootedObject typeObj(cx, CData::GetCType(obj));
    void* data = CData::GetData(obj);

    result = CData::GetSourceString(cx, typeObj, data);
  } else {
    result = JS_NewStringCopyZ(cx, "[CData proto object]");
  }

  if (!result)
    return false;

  args.rval().setString(result);
  return true;
}

bool
CData::ErrnoGetter(JSContext* cx, JS::CallArgs args)
{
  args.rval().set(JS_GetReservedSlot(&args.thisv().toObject(), SLOT_ERRNO));
  return true;
}

#if defined(XP_WIN)
bool
CData::LastErrorGetter(JSContext* cx, JS::CallArgs args)
{
  args.rval().set(JS_GetReservedSlot(&args.thisv().toObject(), SLOT_LASTERROR));
  return true;
}
#endif // defined(XP_WIN)

bool
CDataFinalizer::Methods::ToSource(JSContext *cx, unsigned argc, jsval *vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject objThis(cx, JS_THIS_OBJECT(cx, vp));
  if (!objThis)
    return false;
  if (!CDataFinalizer::IsCDataFinalizer(objThis)) {
    JS_ReportError(cx, "not a CDataFinalizer");
    return false;
  }

  CDataFinalizer::Private *p = (CDataFinalizer::Private *)
    JS_GetPrivate(objThis);

  JSString *strMessage;
  if (!p) {
    strMessage = JS_NewStringCopyZ(cx, "ctypes.CDataFinalizer()");
  } else {
    RootedObject objType(cx, CDataFinalizer::GetCType(cx, objThis));
    if (!objType) {
      JS_ReportError(cx, "CDataFinalizer has no type");
      return false;
    }

    AutoString source;
    AppendString(source, "ctypes.CDataFinalizer(");
    JSString *srcValue = CData::GetSourceString(cx, objType, p->cargs);
    if (!srcValue) {
      return false;
    }
    AppendString(source, srcValue);
    AppendString(source, ", ");
    jsval valCodePtrType = JS_GetReservedSlot(objThis,
                                              SLOT_DATAFINALIZER_CODETYPE);
    if (JSVAL_IS_PRIMITIVE(valCodePtrType)) {
      return false;
    }

    RootedObject typeObj(cx, JSVAL_TO_OBJECT(valCodePtrType));
    JSString *srcDispose = CData::GetSourceString(cx, typeObj, &(p->code));
    if (!srcDispose) {
      return false;
    }

    AppendString(source, srcDispose);
    AppendString(source, ")");
    strMessage = NewUCString(cx, source);
  }

  if (!strMessage) {
    // This is a memory issue, no error message
    return false;
  }

  args.rval().setString(strMessage);
  return true;
}

bool
CDataFinalizer::Methods::ToString(JSContext *cx, unsigned argc, jsval *vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  JSObject* objThis = JS_THIS_OBJECT(cx, vp);
  if (!objThis)
    return false;
  if (!CDataFinalizer::IsCDataFinalizer(objThis)) {
    JS_ReportError(cx, "not a CDataFinalizer");
    return false;
  }

  JSString *strMessage;
  RootedValue value(cx);
  if (!JS_GetPrivate(objThis)) {
    // Pre-check whether CDataFinalizer::GetValue can fail
    // to avoid reporting an error when not appropriate.
    strMessage = JS_NewStringCopyZ(cx, "[CDataFinalizer - empty]");
    if (!strMessage) {
      return false;
    }
  } else if (!CDataFinalizer::GetValue(cx, objThis, value.address())) {
    MOZ_ASSUME_UNREACHABLE("Could not convert an empty CDataFinalizer");
  } else {
    strMessage = ToString(cx, value);
    if (!strMessage) {
      return false;
    }
  }
  args.rval().setString(strMessage);
  return true;
}

bool
CDataFinalizer::IsCDataFinalizer(JSObject *obj)
{
  return JS_GetClass(obj) == &sCDataFinalizerClass;
}


JSObject *
CDataFinalizer::GetCType(JSContext *cx, JSObject *obj)
{
  MOZ_ASSERT(IsCDataFinalizer(obj));

  jsval valData = JS_GetReservedSlot(obj,
                                     SLOT_DATAFINALIZER_VALTYPE);
  if (JSVAL_IS_VOID(valData)) {
    return nullptr;
  }

  return JSVAL_TO_OBJECT(valData);
}

JSObject*
CDataFinalizer::GetCData(JSContext *cx, JSObject *obj)
{
  if (!obj) {
    JS_ReportError(cx, "No C data");
    return nullptr;
  }
  if (CData::IsCData(obj)) {
    return obj;
  }
  if (!CDataFinalizer::IsCDataFinalizer(obj)) {
    JS_ReportError(cx, "Not C data");
    return nullptr;
  }
  RootedValue val(cx);
  if (!CDataFinalizer::GetValue(cx, obj, val.address()) || JSVAL_IS_PRIMITIVE(val)) {
    JS_ReportError(cx, "Empty CDataFinalizer");
    return nullptr;
  }
  return JSVAL_TO_OBJECT(val);
}

bool
CDataFinalizer::GetValue(JSContext *cx, JSObject *obj, jsval *aResult)
{
  MOZ_ASSERT(IsCDataFinalizer(obj));

  CDataFinalizer::Private *p = (CDataFinalizer::Private *)
    JS_GetPrivate(obj);

  if (!p) {
    JS_ReportError(cx, "Attempting to get the value of an empty CDataFinalizer");
    return false;  // We have called |dispose| or |forget| already.
  }

  RootedObject ctype(cx, GetCType(cx, obj));
  return ConvertToJS(cx, ctype, /*parent*/NullPtr(), p -> cargs, false, true, aResult);
}

/*
 * Attach a C function as a finalizer to a JS object.
 *
 * Pseudo-JS signature:
 * function(CData<T>, CData<T -> U>): CDataFinalizer<T>
 *          value,    finalizer
 *
 * This function attaches strong references to the following values:
 * - the CType of |value|
 *
 * Note: This function takes advantage of the fact that non-variadic
 * CData functions are initialized during creation.
 */
bool
CDataFinalizer::Construct(JSContext* cx, unsigned argc, jsval *vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject objSelf(cx, &args.callee());
  RootedObject objProto(cx);
  if (!GetObjectProperty(cx, objSelf, "prototype", &objProto)) {
    JS_ReportError(cx, "CDataFinalizer.prototype does not exist");
    return false;
  }

  // Get arguments
  if (args.length() == 0) { // Special case: the empty (already finalized) object
    JSObject *objResult = JS_NewObject(cx, &sCDataFinalizerClass, objProto, NullPtr());
    args.rval().setObject(*objResult);
    return true;
  }

  if (args.length() != 2) {
    JS_ReportError(cx, "CDataFinalizer takes 2 arguments");
    return false;
  }

  JS::HandleValue valCodePtr = args[1];
  if (!valCodePtr.isObject()) {
    return TypeError(cx, "_a CData object_ of a function pointer type",
                     valCodePtr);
  }
  JSObject *objCodePtr = &valCodePtr.toObject();

  //Note: Using a custom argument formatter here would be awkward (requires
  //a destructor just to uninstall the formatter).

  // 2. Extract argument type of |objCodePtr|
  if (!CData::IsCData(objCodePtr)) {
    return TypeError(cx, "a _CData_ object of a function pointer type",
                     valCodePtr);
  }
  RootedObject objCodePtrType(cx, CData::GetCType(objCodePtr));
  RootedValue valCodePtrType(cx, ObjectValue(*objCodePtrType));
  MOZ_ASSERT(objCodePtrType);

  TypeCode typCodePtr = CType::GetTypeCode(objCodePtrType);
  if (typCodePtr != TYPE_pointer) {
    return TypeError(cx, "a CData object of a function _pointer_ type",
                     valCodePtrType);
  }

  JSObject *objCodeType = PointerType::GetBaseType(objCodePtrType);
  MOZ_ASSERT(objCodeType);

  TypeCode typCode = CType::GetTypeCode(objCodeType);
  if (typCode != TYPE_function) {
    return TypeError(cx, "a CData object of a _function_ pointer type",
                     valCodePtrType);
  }
  uintptr_t code = *reinterpret_cast<uintptr_t*>(CData::GetData(objCodePtr));
  if (!code) {
    return TypeError(cx, "a CData object of a _non-NULL_ function pointer type",
                     valCodePtrType);
  }

  FunctionInfo* funInfoFinalizer =
    FunctionType::GetFunctionInfo(objCodeType);
  MOZ_ASSERT(funInfoFinalizer);

  if ((funInfoFinalizer->mArgTypes.length() != 1)
      || (funInfoFinalizer->mIsVariadic)) {
    RootedValue valCodeType(cx, ObjectValue(*objCodeType));
    return TypeError(cx, "a function accepting exactly one argument",
                     valCodeType);
  }
  RootedObject objArgType(cx, funInfoFinalizer->mArgTypes[0]);
  RootedObject returnType(cx, funInfoFinalizer->mReturnType);

  // Invariant: At this stage, we know that funInfoFinalizer->mIsVariadic
  // is |false|. Therefore, funInfoFinalizer->mCIF has already been initialized.

  bool freePointer = false;

  // 3. Perform dynamic cast of |args[0]| into |objType|, store it in |cargs|

  size_t sizeArg;
  RootedValue valData(cx, args[0]);
  if (!CType::GetSafeSize(objArgType, &sizeArg)) {
    return TypeError(cx, "(an object with known size)", valData);
  }

  ScopedJSFreePtr<void> cargs(malloc(sizeArg));

  if (!ImplicitConvert(cx, valData, objArgType, cargs.get(),
                       false, &freePointer)) {
    RootedValue valArgType(cx, ObjectValue(*objArgType));
    return TypeError(cx, "(an object that can be converted to the following type)",
                     valArgType);
  }
  if (freePointer) {
    // Note: We could handle that case, if necessary.
    JS_ReportError(cx, "Internal Error during CDataFinalizer. Object cannot be represented");
    return false;
  }

  // 4. Prepare buffer for holding return value

  ScopedJSFreePtr<void> rvalue;
  if (CType::GetTypeCode(returnType) != TYPE_void_t) {
    rvalue = malloc(Align(CType::GetSize(returnType),
                          sizeof(ffi_arg)));
  } //Otherwise, simply do not allocate

  // 5. Create |objResult|

  JSObject *objResult = JS_NewObject(cx, &sCDataFinalizerClass, objProto, NullPtr());
  if (!objResult) {
    return false;
  }

  // If our argument is a CData, it holds a type.
  // This is the type that we should capture, not that
  // of the function, which may be less precise.
  JSObject *objBestArgType = objArgType;
  if (!JSVAL_IS_PRIMITIVE(valData)) {
    JSObject *objData = &valData.toObject();
    if (CData::IsCData(objData)) {
      objBestArgType = CData::GetCType(objData);
      size_t sizeBestArg;
      if (!CType::GetSafeSize(objBestArgType, &sizeBestArg)) {
        MOZ_ASSUME_UNREACHABLE("object with unknown size");
      }
      if (sizeBestArg != sizeArg) {
        return TypeError(cx, "(an object with the same size as that expected by the C finalization function)", valData);
      }
    }
  }

  // Used by GetCType
  JS_SetReservedSlot(objResult,
                     SLOT_DATAFINALIZER_VALTYPE,
                     OBJECT_TO_JSVAL(objBestArgType));

  // Used by ToSource
  JS_SetReservedSlot(objResult,
                     SLOT_DATAFINALIZER_CODETYPE,
                     OBJECT_TO_JSVAL(objCodePtrType));

  ffi_abi abi;
  if (!GetABI(cx, OBJECT_TO_JSVAL(funInfoFinalizer->mABI), &abi)) {
    JS_ReportError(cx, "Internal Error: "
                   "Invalid ABI specification in CDataFinalizer");
    return false;
  }

  ffi_type* rtype = CType::GetFFIType(cx, funInfoFinalizer->mReturnType);
  if (!rtype) {
    JS_ReportError(cx, "Internal Error: "
                   "Could not access ffi type of CDataFinalizer");
    return false;
  }

  // 7. Store C information as private
  ScopedJSFreePtr<CDataFinalizer::Private>
    p((CDataFinalizer::Private*)malloc(sizeof(CDataFinalizer::Private)));

  memmove(&p->CIF, &funInfoFinalizer->mCIF, sizeof(ffi_cif));

  p->cargs = cargs.forget();
  p->rvalue = rvalue.forget();
  p->cargs_size = sizeArg;
  p->code = code;


  JS_SetPrivate(objResult, p.forget());
  args.rval().setObject(*objResult);
  return true;
}


/*
 * Actually call the finalizer. Does not perform any cleanup on the object.
 *
 * Preconditions: |this| must be a |CDataFinalizer|, |p| must be non-null.
 * The function fails if |this| has gone through |Forget|/|Dispose|
 * or |Finalize|.
 *
 * This function does not alter the value of |errno|/|GetLastError|.
 *
 * If argument |errnoStatus| is non-nullptr, it receives the value of |errno|
 * immediately after the call. Under Windows, if argument |lastErrorStatus|
 * is non-nullptr, it receives the value of |GetLastError| immediately after
 * the call. On other platforms, |lastErrorStatus| is ignored.
 */
void
CDataFinalizer::CallFinalizer(CDataFinalizer::Private *p,
                              int* errnoStatus,
                              int32_t* lastErrorStatus)
{
  int savedErrno = errno;
  errno = 0;
#if defined(XP_WIN)
  int32_t savedLastError = GetLastError();
  SetLastError(0);
#endif // defined(XP_WIN)

  ffi_call(&p->CIF, FFI_FN(p->code), p->rvalue, &p->cargs);

  if (errnoStatus) {
    *errnoStatus = errno;
  }
  errno = savedErrno;
#if defined(XP_WIN)
  if (lastErrorStatus) {
    *lastErrorStatus = GetLastError();
  }
  SetLastError(savedLastError);
#endif // defined(XP_WIN)
}

/*
 * Forget the value.
 *
 * Preconditions: |this| must be a |CDataFinalizer|.
 * The function fails if |this| has gone through |Forget|/|Dispose|
 * or |Finalize|.
 *
 * Does not call the finalizer. Cleans up the Private memory and releases all
 * strong references.
 */
bool
CDataFinalizer::Methods::Forget(JSContext* cx, unsigned argc, jsval *vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 0) {
    JS_ReportError(cx, "CDataFinalizer.prototype.forget takes no arguments");
    return false;
  }

  JS::Rooted<JSObject*> obj(cx, args.thisv().toObjectOrNull());
  if (!obj)
    return false;
  if (!CDataFinalizer::IsCDataFinalizer(obj)) {
    RootedValue val(cx, ObjectValue(*obj));
    return TypeError(cx, "a CDataFinalizer", val);
  }

  CDataFinalizer::Private *p = (CDataFinalizer::Private *)
    JS_GetPrivate(obj);

  if (!p) {
    JS_ReportError(cx, "forget called on an empty CDataFinalizer");
    return false;
  }

  RootedValue valJSData(cx);
  RootedObject ctype(cx, GetCType(cx, obj));
  if (!ConvertToJS(cx, ctype, NullPtr(), p->cargs, false, true, valJSData.address())) {
    JS_ReportError(cx, "CDataFinalizer value cannot be represented");
    return false;
  }

  CDataFinalizer::Cleanup(p, obj);

  args.rval().set(valJSData);
  return true;
}

/*
 * Clean up the value.
 *
 * Preconditions: |this| must be a |CDataFinalizer|.
 * The function fails if |this| has gone through |Forget|/|Dispose|
 * or |Finalize|.
 *
 * Calls the finalizer, cleans up the Private memory and releases all
 * strong references.
 */
bool
CDataFinalizer::Methods::Dispose(JSContext* cx, unsigned argc, jsval *vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 0) {
    JS_ReportError(cx, "CDataFinalizer.prototype.dispose takes no arguments");
    return false;
  }

  RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));
  if (!obj)
    return false;
  if (!CDataFinalizer::IsCDataFinalizer(obj)) {
    RootedValue val(cx, ObjectValue(*obj));
    return TypeError(cx, "a CDataFinalizer", val);
  }

  CDataFinalizer::Private *p = (CDataFinalizer::Private *)
    JS_GetPrivate(obj);

  if (!p) {
    JS_ReportError(cx, "dispose called on an empty CDataFinalizer.");
    return false;
  }

  jsval valType = JS_GetReservedSlot(obj, SLOT_DATAFINALIZER_VALTYPE);
  JS_ASSERT(!JSVAL_IS_PRIMITIVE(valType));

  JSObject *objCTypes = CType::GetGlobalCTypes(cx, &valType.toObject());
  if (!objCTypes)
    return false;

  jsval valCodePtrType = JS_GetReservedSlot(obj, SLOT_DATAFINALIZER_CODETYPE);
  JS_ASSERT(!JSVAL_IS_PRIMITIVE(valCodePtrType));
  JSObject *objCodePtrType = &valCodePtrType.toObject();

  JSObject *objCodeType = PointerType::GetBaseType(objCodePtrType);
  JS_ASSERT(objCodeType);
  JS_ASSERT(CType::GetTypeCode(objCodeType) == TYPE_function);

  RootedObject resultType(cx, FunctionType::GetFunctionInfo(objCodeType)->mReturnType);
  RootedValue result(cx, JSVAL_VOID);

  int errnoStatus;
#if defined(XP_WIN)
  int32_t lastErrorStatus;
  CDataFinalizer::CallFinalizer(p, &errnoStatus, &lastErrorStatus);
#else
  CDataFinalizer::CallFinalizer(p, &errnoStatus, nullptr);
#endif // defined(XP_WIN)

  JS_SetReservedSlot(objCTypes, SLOT_ERRNO, INT_TO_JSVAL(errnoStatus));
#if defined(XP_WIN)
  JS_SetReservedSlot(objCTypes, SLOT_LASTERROR, INT_TO_JSVAL(lastErrorStatus));
#endif // defined(XP_WIN)

  if (ConvertToJS(cx, resultType, NullPtr(), p->rvalue, false, true, result.address())) {
    CDataFinalizer::Cleanup(p, obj);
    args.rval().set(result);
    return true;
  }
  CDataFinalizer::Cleanup(p, obj);
  return false;
}

/*
 * Perform finalization.
 *
 * Preconditions: |this| must be the result of |CDataFinalizer|.
 * It may have gone through |Forget|/|Dispose|.
 *
 * If |this| has not gone through |Forget|/|Dispose|, calls the
 * finalizer, cleans up the Private memory and releases all
 * strong references.
 */
void
CDataFinalizer::Finalize(JSFreeOp* fop, JSObject* obj)
{
  CDataFinalizer::Private *p = (CDataFinalizer::Private *)
    JS_GetPrivate(obj);

  if (!p) {
    return;
  }

  CDataFinalizer::CallFinalizer(p, nullptr, nullptr);
  CDataFinalizer::Cleanup(p, nullptr);
}

/*
 * Perform cleanup of a CDataFinalizer
 *
 * Release strong references, cleanup |Private|.
 *
 * Argument |p| contains the private information of the CDataFinalizer. If
 * nullptr, this function does nothing.
 * Argument |obj| should contain |nullptr| during finalization (or in any
 * context in which the object itself should not be cleaned up), or a
 * CDataFinalizer object otherwise.
 */
void
CDataFinalizer::Cleanup(CDataFinalizer::Private *p, JSObject *obj)
{
  if (!p) {
    return;  // We have already cleaned up
  }

  free(p->cargs);
  free(p->rvalue);
  free(p);

  if (!obj) {
    return;  // No slots to clean up
  }

  JS_ASSERT(CDataFinalizer::IsCDataFinalizer(obj));

  JS_SetPrivate(obj, nullptr);
  for (int i = 0; i < CDATAFINALIZER_SLOTS; ++i) {
    JS_SetReservedSlot(obj, i, JSVAL_NULL);
  }
}


/*******************************************************************************
** Int64 and UInt64 implementation
*******************************************************************************/

JSObject*
Int64Base::Construct(JSContext* cx,
                     HandleObject proto,
                     uint64_t data,
                     bool isUnsigned)
{
  const JSClass* clasp = isUnsigned ? &sUInt64Class : &sInt64Class;
  RootedObject parent(cx, JS_GetParent(proto));
  RootedObject result(cx, JS_NewObject(cx, clasp, proto, parent));
  if (!result)
    return nullptr;

  // attach the Int64's data
  uint64_t* buffer = cx->new_<uint64_t>(data);
  if (!buffer) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  JS_SetReservedSlot(result, SLOT_INT64, PRIVATE_TO_JSVAL(buffer));

  if (!JS_FreezeObject(cx, result))
    return nullptr;

  return result;
}

void
Int64Base::Finalize(JSFreeOp *fop, JSObject* obj)
{
  jsval slot = JS_GetReservedSlot(obj, SLOT_INT64);
  if (JSVAL_IS_VOID(slot))
    return;

  FreeOp::get(fop)->delete_(static_cast<uint64_t*>(JSVAL_TO_PRIVATE(slot)));
}

uint64_t
Int64Base::GetInt(JSObject* obj) {
  JS_ASSERT(Int64::IsInt64(obj) || UInt64::IsUInt64(obj));

  jsval slot = JS_GetReservedSlot(obj, SLOT_INT64);
  return *static_cast<uint64_t*>(JSVAL_TO_PRIVATE(slot));
}

bool
Int64Base::ToString(JSContext* cx,
                    JSObject* obj,
                    const CallArgs& args,
                    bool isUnsigned)
{
  if (args.length() > 1) {
    JS_ReportError(cx, "toString takes zero or one argument");
    return false;
  }

  int radix = 10;
  if (args.length() == 1) {
    jsval arg = args[0];
    if (arg.isInt32())
      radix = arg.toInt32();
    if (!arg.isInt32() || radix < 2 || radix > 36) {
      JS_ReportError(cx, "radix argument must be an integer between 2 and 36");
      return false;
    }
  }

  AutoString intString;
  if (isUnsigned) {
    IntegerToString(GetInt(obj), radix, intString);
  } else {
    IntegerToString(static_cast<int64_t>(GetInt(obj)), radix, intString);
  }

  JSString *result = NewUCString(cx, intString);
  if (!result)
    return false;

  args.rval().setString(result);
  return true;
}

bool
Int64Base::ToSource(JSContext* cx,
                    JSObject* obj,
                    const CallArgs& args,
                    bool isUnsigned)
{
  if (args.length() != 0) {
    JS_ReportError(cx, "toSource takes zero arguments");
    return false;
  }

  // Return a decimal string suitable for constructing the number.
  AutoString source;
  if (isUnsigned) {
    AppendString(source, "ctypes.UInt64(\"");
    IntegerToString(GetInt(obj), 10, source);
  } else {
    AppendString(source, "ctypes.Int64(\"");
    IntegerToString(static_cast<int64_t>(GetInt(obj)), 10, source);
  }
  AppendString(source, "\")");

  JSString *result = NewUCString(cx, source);
  if (!result)
    return false;

  args.rval().setString(result);
  return true;
}

bool
Int64::Construct(JSContext* cx,
                 unsigned argc,
                 jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);

  // Construct and return a new Int64 object.
  if (args.length() != 1) {
    JS_ReportError(cx, "Int64 takes one argument");
    return false;
  }

  int64_t i = 0;
  if (!jsvalToBigInteger(cx, args[0], true, &i))
    return TypeError(cx, "int64", args[0]);

  // Get ctypes.Int64.prototype from the 'prototype' property of the ctor.
  RootedValue slot(cx);
  RootedObject callee(cx, &args.callee());
  ASSERT_OK(JS_GetProperty(cx, callee, "prototype", &slot));
  RootedObject proto(cx, JSVAL_TO_OBJECT(slot));
  JS_ASSERT(JS_GetClass(proto) == &sInt64ProtoClass);

  JSObject* result = Int64Base::Construct(cx, proto, i, false);
  if (!result)
    return false;

  args.rval().setObject(*result);
  return true;
}

bool
Int64::IsInt64(JSObject* obj)
{
  return JS_GetClass(obj) == &sInt64Class;
}

bool
Int64::ToString(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj)
    return false;
  if (!Int64::IsInt64(obj)) {
    JS_ReportError(cx, "not an Int64");
    return false;
  }

  return Int64Base::ToString(cx, obj, args, false);
}

bool
Int64::ToSource(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj)
    return false;
  if (!Int64::IsInt64(obj)) {
    JS_ReportError(cx, "not an Int64");
    return false;
  }

  return Int64Base::ToSource(cx, obj, args, false);
}

bool
Int64::Compare(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 2 ||
      JSVAL_IS_PRIMITIVE(args[0]) ||
      JSVAL_IS_PRIMITIVE(args[1]) ||
      !Int64::IsInt64(&args[0].toObject()) ||
      !Int64::IsInt64(&args[1].toObject())) {
    JS_ReportError(cx, "compare takes two Int64 arguments");
    return false;
  }

  JSObject* obj1 = &args[0].toObject();
  JSObject* obj2 = &args[1].toObject();

  int64_t i1 = Int64Base::GetInt(obj1);
  int64_t i2 = Int64Base::GetInt(obj2);

  if (i1 == i2)
    args.rval().setInt32(0);
  else if (i1 < i2)
    args.rval().setInt32(-1);
  else
    args.rval().setInt32(1);

  return true;
}

#define LO_MASK ((uint64_t(1) << 32) - 1)
#define INT64_LO(i) ((i) & LO_MASK)
#define INT64_HI(i) ((i) >> 32)

bool
Int64::Lo(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1 || JSVAL_IS_PRIMITIVE(args[0]) ||
      !Int64::IsInt64(&args[0].toObject())) {
    JS_ReportError(cx, "lo takes one Int64 argument");
    return false;
  }

  JSObject* obj = &args[0].toObject();
  int64_t u = Int64Base::GetInt(obj);
  double d = uint32_t(INT64_LO(u));

  args.rval().setNumber(d);
  return true;
}

bool
Int64::Hi(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1 || JSVAL_IS_PRIMITIVE(args[0]) ||
      !Int64::IsInt64(&args[0].toObject())) {
    JS_ReportError(cx, "hi takes one Int64 argument");
    return false;
  }

  JSObject* obj = &args[0].toObject();
  int64_t u = Int64Base::GetInt(obj);
  double d = int32_t(INT64_HI(u));

  args.rval().setDouble(d);
  return true;
}

bool
Int64::Join(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 2) {
    JS_ReportError(cx, "join takes two arguments");
    return false;
  }

  int32_t hi;
  uint32_t lo;
  if (!jsvalToInteger(cx, args[0], &hi))
    return TypeError(cx, "int32", args[0]);
  if (!jsvalToInteger(cx, args[1], &lo))
    return TypeError(cx, "uint32", args[1]);

  int64_t i = (int64_t(hi) << 32) + int64_t(lo);

  // Get Int64.prototype from the function's reserved slot.
  JSObject* callee = &args.callee();

  jsval slot = js::GetFunctionNativeReserved(callee, SLOT_FN_INT64PROTO);
  RootedObject proto(cx, &slot.toObject());
  JS_ASSERT(JS_GetClass(proto) == &sInt64ProtoClass);

  JSObject* result = Int64Base::Construct(cx, proto, i, false);
  if (!result)
    return false;

  args.rval().setObject(*result);
  return true;
}

bool
UInt64::Construct(JSContext* cx,
                  unsigned argc,
                  jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);

  // Construct and return a new UInt64 object.
  if (args.length() != 1) {
    JS_ReportError(cx, "UInt64 takes one argument");
    return false;
  }

  uint64_t u = 0;
  if (!jsvalToBigInteger(cx, args[0], true, &u))
    return TypeError(cx, "uint64", args[0]);

  // Get ctypes.UInt64.prototype from the 'prototype' property of the ctor.
  RootedValue slot(cx);
  RootedObject callee(cx, &args.callee());
  ASSERT_OK(JS_GetProperty(cx, callee, "prototype", &slot));
  RootedObject proto(cx, &slot.toObject());
  JS_ASSERT(JS_GetClass(proto) == &sUInt64ProtoClass);

  JSObject* result = Int64Base::Construct(cx, proto, u, true);
  if (!result)
    return false;

  args.rval().setObject(*result);
  return true;
}

bool
UInt64::IsUInt64(JSObject* obj)
{
  return JS_GetClass(obj) == &sUInt64Class;
}

bool
UInt64::ToString(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj)
    return false;
  if (!UInt64::IsUInt64(obj)) {
    JS_ReportError(cx, "not a UInt64");
    return false;
  }

  return Int64Base::ToString(cx, obj, args, true);
}

bool
UInt64::ToSource(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj)
    return false;
  if (!UInt64::IsUInt64(obj)) {
    JS_ReportError(cx, "not a UInt64");
    return false;
  }

  return Int64Base::ToSource(cx, obj, args, true);
}

bool
UInt64::Compare(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 2 ||
      JSVAL_IS_PRIMITIVE(args[0]) ||
      JSVAL_IS_PRIMITIVE(args[1]) ||
      !UInt64::IsUInt64(&args[0].toObject()) ||
      !UInt64::IsUInt64(&args[1].toObject())) {
    JS_ReportError(cx, "compare takes two UInt64 arguments");
    return false;
  }

  JSObject* obj1 = &args[0].toObject();
  JSObject* obj2 = &args[1].toObject();

  uint64_t u1 = Int64Base::GetInt(obj1);
  uint64_t u2 = Int64Base::GetInt(obj2);

  if (u1 == u2)
    args.rval().setInt32(0);
  else if (u1 < u2)
    args.rval().setInt32(-1);
  else
    args.rval().setInt32(1);

  return true;
}

bool
UInt64::Lo(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1 || JSVAL_IS_PRIMITIVE(args[0]) ||
      !UInt64::IsUInt64(&args[0].toObject())) {
    JS_ReportError(cx, "lo takes one UInt64 argument");
    return false;
  }

  JSObject* obj = &args[0].toObject();
  uint64_t u = Int64Base::GetInt(obj);
  double d = uint32_t(INT64_LO(u));

  args.rval().setDouble(d);
  return true;
}

bool
UInt64::Hi(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1 || JSVAL_IS_PRIMITIVE(args[0]) ||
      !UInt64::IsUInt64(&args[0].toObject())) {
    JS_ReportError(cx, "hi takes one UInt64 argument");
    return false;
  }

  JSObject* obj = &args[0].toObject();
  uint64_t u = Int64Base::GetInt(obj);
  double d = uint32_t(INT64_HI(u));

  args.rval().setDouble(d);
  return true;
}

bool
UInt64::Join(JSContext* cx, unsigned argc, jsval* vp)
{
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 2) {
    JS_ReportError(cx, "join takes two arguments");
    return false;
  }

  uint32_t hi;
  uint32_t lo;
  if (!jsvalToInteger(cx, args[0], &hi))
    return TypeError(cx, "uint32_t", args[0]);
  if (!jsvalToInteger(cx, args[1], &lo))
    return TypeError(cx, "uint32_t", args[1]);

  uint64_t u = (uint64_t(hi) << 32) + uint64_t(lo);

  // Get UInt64.prototype from the function's reserved slot.
  JSObject* callee = &args.callee();

  jsval slot = js::GetFunctionNativeReserved(callee, SLOT_FN_INT64PROTO);
  RootedObject proto(cx, &slot.toObject());
  JS_ASSERT(JS_GetClass(proto) == &sUInt64ProtoClass);

  JSObject* result = Int64Base::Construct(cx, proto, u, true);
  if (!result)
    return false;

  args.rval().setObject(*result);
  return true;
}

}
}
