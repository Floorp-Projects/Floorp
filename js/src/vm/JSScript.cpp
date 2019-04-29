/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS script operations.
 */

#include "vm/JSScript-inl.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"

#include <algorithm>
#include <new>
#include <string.h>
#include <type_traits>
#include <utility>

#include "jsapi.h"
#include "jstypes.h"
#include "jsutil.h"

#include "frontend/BytecodeCompiler.h"
#include "frontend/BytecodeEmitter.h"
#include "frontend/SharedContext.h"
#include "gc/FreeOp.h"
#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "jit/IonCode.h"
#include "jit/JitRealm.h"
#include "js/CompileOptions.h"
#include "js/MemoryMetrics.h"
#include "js/Printf.h"
#include "js/SourceText.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "js/Wrapper.h"
#include "util/StringBuffer.h"
#include "util/Text.h"
#include "vm/ArgumentsObject.h"
#include "vm/BytecodeIterator.h"
#include "vm/BytecodeLocation.h"
#include "vm/BytecodeUtil.h"
#include "vm/Compression.h"
#include "vm/Debugger.h"
#include "vm/JSAtom.h"
#include "vm/JSContext.h"
#include "vm/JSFunction.h"
#include "vm/JSObject.h"
#include "vm/Opcodes.h"
#include "vm/SelfHosting.h"
#include "vm/Shape.h"
#include "vm/SharedImmutableStringsCache.h"
#include "vm/Xdr.h"
#include "vtune/VTuneWrapper.h"

#include "gc/Marking-inl.h"
#include "vm/BytecodeIterator-inl.h"
#include "vm/BytecodeLocation-inl.h"
#include "vm/Compartment-inl.h"
#include "vm/EnvironmentObject-inl.h"
#include "vm/JSFunction-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/SharedImmutableStringsCache-inl.h"
#include "vm/Stack-inl.h"

using namespace js;

using mozilla::Maybe;
using mozilla::PodCopy;
using mozilla::PointerRangeSize;
using mozilla::Utf8AsUnsignedChars;
using mozilla::Utf8Unit;

using JS::CompileOptions;
using JS::ReadOnlyCompileOptions;
using JS::SourceText;

template <XDRMode mode>
XDRResult js::XDRScriptConst(XDRState<mode>* xdr, MutableHandleValue vp) {
  JSContext* cx = xdr->cx();

  enum ConstTag {
    SCRIPT_INT,
    SCRIPT_DOUBLE,
    SCRIPT_ATOM,
    SCRIPT_TRUE,
    SCRIPT_FALSE,
    SCRIPT_NULL,
    SCRIPT_OBJECT,
    SCRIPT_VOID,
    SCRIPT_HOLE,
    SCRIPT_BIGINT
  };

  ConstTag tag;
  if (mode == XDR_ENCODE) {
    if (vp.isInt32()) {
      tag = SCRIPT_INT;
    } else if (vp.isDouble()) {
      tag = SCRIPT_DOUBLE;
    } else if (vp.isString()) {
      tag = SCRIPT_ATOM;
    } else if (vp.isTrue()) {
      tag = SCRIPT_TRUE;
    } else if (vp.isFalse()) {
      tag = SCRIPT_FALSE;
    } else if (vp.isNull()) {
      tag = SCRIPT_NULL;
    } else if (vp.isObject()) {
      tag = SCRIPT_OBJECT;
    } else if (vp.isMagic(JS_ELEMENTS_HOLE)) {
      tag = SCRIPT_HOLE;
    } else if (vp.isBigInt()) {
      tag = SCRIPT_BIGINT;
    } else {
      MOZ_ASSERT(vp.isUndefined());
      tag = SCRIPT_VOID;
    }
  }

  MOZ_TRY(xdr->codeEnum32(&tag));

  switch (tag) {
    case SCRIPT_INT: {
      uint32_t i;
      if (mode == XDR_ENCODE) {
        i = uint32_t(vp.toInt32());
      }
      MOZ_TRY(xdr->codeUint32(&i));
      if (mode == XDR_DECODE) {
        vp.set(Int32Value(int32_t(i)));
      }
      break;
    }
    case SCRIPT_DOUBLE: {
      double d;
      if (mode == XDR_ENCODE) {
        d = vp.toDouble();
      }
      MOZ_TRY(xdr->codeDouble(&d));
      if (mode == XDR_DECODE) {
        vp.set(DoubleValue(d));
      }
      break;
    }
    case SCRIPT_ATOM: {
      RootedAtom atom(cx);
      if (mode == XDR_ENCODE) {
        atom = &vp.toString()->asAtom();
      }
      MOZ_TRY(XDRAtom(xdr, &atom));
      if (mode == XDR_DECODE) {
        vp.set(StringValue(atom));
      }
      break;
    }
    case SCRIPT_TRUE:
      if (mode == XDR_DECODE) {
        vp.set(BooleanValue(true));
      }
      break;
    case SCRIPT_FALSE:
      if (mode == XDR_DECODE) {
        vp.set(BooleanValue(false));
      }
      break;
    case SCRIPT_NULL:
      if (mode == XDR_DECODE) {
        vp.set(NullValue());
      }
      break;
    case SCRIPT_OBJECT: {
      RootedObject obj(cx);
      if (mode == XDR_ENCODE) {
        obj = &vp.toObject();
      }

      MOZ_TRY(XDRObjectLiteral(xdr, &obj));

      if (mode == XDR_DECODE) {
        vp.setObject(*obj);
      }
      break;
    }
    case SCRIPT_VOID:
      if (mode == XDR_DECODE) {
        vp.set(UndefinedValue());
      }
      break;
    case SCRIPT_HOLE:
      if (mode == XDR_DECODE) {
        vp.setMagic(JS_ELEMENTS_HOLE);
      }
      break;
    case SCRIPT_BIGINT: {
      RootedBigInt bi(cx);
      if (mode == XDR_ENCODE) {
        bi = vp.toBigInt();
      }

      MOZ_TRY(XDRBigInt(xdr, &bi));

      if (mode == XDR_DECODE) {
        vp.setBigInt(bi);
      }
      break;
    }
    default:
      // Fail in debug, but only soft-fail in release
      MOZ_ASSERT(false, "Bad XDR value kind");
      return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
  }
  return Ok();
}

template XDRResult js::XDRScriptConst(XDRState<XDR_ENCODE>*,
                                      MutableHandleValue);

template XDRResult js::XDRScriptConst(XDRState<XDR_DECODE>*,
                                      MutableHandleValue);

// Code LazyScript's closed over bindings.
template <XDRMode mode>
static XDRResult XDRLazyClosedOverBindings(XDRState<mode>* xdr,
                                           MutableHandle<LazyScript*> lazy) {
  JSContext* cx = xdr->cx();
  RootedAtom atom(cx);
  for (size_t i = 0; i < lazy->numClosedOverBindings(); i++) {
    uint8_t endOfScopeSentinel;
    if (mode == XDR_ENCODE) {
      atom = lazy->closedOverBindings()[i];
      endOfScopeSentinel = !atom;
    }

    MOZ_TRY(xdr->codeUint8(&endOfScopeSentinel));

    if (endOfScopeSentinel) {
      atom = nullptr;
    } else {
      MOZ_TRY(XDRAtom(xdr, &atom));
    }

    if (mode == XDR_DECODE) {
      lazy->closedOverBindings()[i] = atom;
    }
  }

  return Ok();
}

// Code the missing part needed to re-create a LazyScript from a JSScript.
template <XDRMode mode>
static XDRResult XDRRelazificationInfo(XDRState<mode>* xdr, HandleFunction fun,
                                       HandleScript script,
                                       HandleScope enclosingScope,
                                       MutableHandle<LazyScript*> lazy) {
  MOZ_ASSERT_IF(mode == XDR_ENCODE, script->isRelazifiableIgnoringJitCode() &&
                                        script->maybeLazyScript());
  MOZ_ASSERT_IF(mode == XDR_ENCODE, !lazy->numInnerFunctions());

  JSContext* cx = xdr->cx();

  uint64_t packedFields;
  {
    uint32_t sourceStart = script->sourceStart();
    uint32_t sourceEnd = script->sourceEnd();
    uint32_t toStringStart = script->toStringStart();
    uint32_t toStringEnd = script->toStringEnd();
    uint32_t lineno = script->lineno();
    uint32_t column = script->column();
    uint32_t numFieldInitializers;

    if (mode == XDR_ENCODE) {
      packedFields = lazy->packedFieldsForXDR();
      MOZ_ASSERT(sourceStart == lazy->sourceStart());
      MOZ_ASSERT(sourceEnd == lazy->sourceEnd());
      MOZ_ASSERT(toStringStart == lazy->toStringStart());
      MOZ_ASSERT(toStringEnd == lazy->toStringEnd());
      MOZ_ASSERT(lineno == lazy->lineno());
      MOZ_ASSERT(column == lazy->column());
      // We can assert we have no inner functions because we don't
      // relazify scripts with inner functions.  See
      // JSFunction::createScriptForLazilyInterpretedFunction.
      MOZ_ASSERT(lazy->numInnerFunctions() == 0);
      if (fun->kind() == JSFunction::FunctionKind::ClassConstructor) {
        numFieldInitializers =
            (uint32_t)lazy->getFieldInitializers().numFieldInitializers;
      } else {
        numFieldInitializers = UINT32_MAX;
      }
    }

    MOZ_TRY(xdr->codeUint64(&packedFields));
    MOZ_TRY(xdr->codeUint32(&numFieldInitializers));

    if (mode == XDR_DECODE) {
      RootedScriptSourceObject sourceObject(cx, script->sourceObject());
      lazy.set(LazyScript::CreateForXDR(
          cx, fun, script, enclosingScope, sourceObject, packedFields,
          sourceStart, sourceEnd, toStringStart, lineno, column));
      if (!lazy) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }

      lazy->setToStringEnd(toStringEnd);
      if (numFieldInitializers != UINT32_MAX) {
        lazy->setFieldInitializers(
            FieldInitializers((size_t)numFieldInitializers));
      }
    }
  }

  // Code binding names.
  MOZ_TRY(XDRLazyClosedOverBindings(xdr, lazy));

  // No need to do anything with inner functions, since we asserted we don't
  // have any.

  return Ok();
}

template <XDRMode mode>
XDRResult JSTryNote::XDR(XDRState<mode>* xdr) {
  MOZ_TRY(xdr->codeUint32(&kind));
  MOZ_TRY(xdr->codeUint32(&stackDepth));
  MOZ_TRY(xdr->codeUint32(&start));
  MOZ_TRY(xdr->codeUint32(&length));

  return Ok();
}

template <XDRMode mode>
XDRResult ScopeNote::XDR(XDRState<mode>* xdr) {
  MOZ_TRY(xdr->codeUint32(&index));
  MOZ_TRY(xdr->codeUint32(&start));
  MOZ_TRY(xdr->codeUint32(&length));
  MOZ_TRY(xdr->codeUint32(&parent));

  return Ok();
}

static inline uint32_t FindScopeIndex(mozilla::Span<const GCPtrScope> scopes,
                                      Scope& scope) {
  unsigned length = scopes.size();
  for (uint32_t i = 0; i < length; ++i) {
    if (scopes[i] == &scope) {
      return i;
    }
  }

  MOZ_CRASH("Scope not found");
}

template <XDRMode mode>
static XDRResult XDRInnerObject(XDRState<mode>* xdr,
                                js::PrivateScriptData* data,
                                HandleScriptSourceObject sourceObject,
                                MutableHandleObject inner) {
  enum class ClassKind { RegexpObject, JSFunction, JSObject, ArrayObject };
  JSContext* cx = xdr->cx();

  ClassKind classk;

  if (mode == XDR_ENCODE) {
    if (inner->is<RegExpObject>()) {
      classk = ClassKind::RegexpObject;
    } else if (inner->is<JSFunction>()) {
      classk = ClassKind::JSFunction;
    } else if (inner->is<PlainObject>()) {
      classk = ClassKind::JSObject;
    } else if (inner->is<ArrayObject>()) {
      classk = ClassKind::ArrayObject;
    } else {
      MOZ_CRASH("Cannot encode this class of object.");
    }
  }

  MOZ_TRY(xdr->codeEnum32(&classk));

  switch (classk) {
    case ClassKind::RegexpObject: {
      Rooted<RegExpObject*> regexp(cx);
      if (mode == XDR_ENCODE) {
        regexp = &inner->as<RegExpObject>();
      }
      MOZ_TRY(XDRScriptRegExpObject(xdr, &regexp));
      if (mode == XDR_DECODE) {
        inner.set(regexp);
      }
      break;
    }

    case ClassKind::JSFunction: {
      /* Code the nested function's enclosing scope. */
      uint32_t funEnclosingScopeIndex = 0;
      RootedScope funEnclosingScope(cx);

      if (mode == XDR_ENCODE) {
        RootedFunction function(cx, &inner->as<JSFunction>());

        if (function->isInterpretedLazy()) {
          funEnclosingScope = function->lazyScript()->enclosingScope();
        } else if (function->isInterpreted()) {
          funEnclosingScope = function->nonLazyScript()->enclosingScope();
        } else {
          MOZ_ASSERT(function->isAsmJSNative());
          return xdr->fail(JS::TranscodeResult_Failure_AsmJSNotSupported);
        }

        funEnclosingScopeIndex =
            FindScopeIndex(data->scopes(), *funEnclosingScope);
      }

      MOZ_TRY(xdr->codeUint32(&funEnclosingScopeIndex));

      if (mode == XDR_DECODE) {
        funEnclosingScope = data->scopes()[funEnclosingScopeIndex];
      }

      // Code nested function and script.
      RootedFunction tmp(cx);
      if (mode == XDR_ENCODE) {
        tmp = &inner->as<JSFunction>();
      }
      MOZ_TRY(
          XDRInterpretedFunction(xdr, funEnclosingScope, sourceObject, &tmp));
      if (mode == XDR_DECODE) {
        inner.set(tmp);
      }
      break;
    }

    case ClassKind::JSObject:
    case ClassKind::ArrayObject: {
      /* Code object literal. */
      RootedObject tmp(cx);
      if (mode == XDR_ENCODE) {
        tmp = inner.get();
      }
      MOZ_TRY(XDRObjectLiteral(xdr, &tmp));
      if (mode == XDR_DECODE) {
        inner.set(tmp);
      }
      break;
    }

    default: {
      // Fail in debug, but only soft-fail in release
      MOZ_ASSERT(false, "Bad XDR class kind");
      return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
    }
  }

  return Ok();
}

template <XDRMode mode>
static XDRResult XDRScope(XDRState<mode>* xdr, js::PrivateScriptData* data,
                          HandleScope scriptEnclosingScope, HandleFunction fun,
                          uint32_t scopeIndex, MutableHandleScope scope) {
  JSContext* cx = xdr->cx();

  ScopeKind scopeKind;
  RootedScope enclosing(cx);
  uint32_t enclosingIndex = 0;

  // The enclosingScope is encoded using an integer index into the scope array.
  // This means that scopes must be topologically sorted.
  if (mode == XDR_ENCODE) {
    scopeKind = scope->kind();

    if (scopeIndex == 0) {
      enclosingIndex = UINT32_MAX;
    } else {
      MOZ_ASSERT(scope->enclosing());
      enclosingIndex = FindScopeIndex(data->scopes(), *scope->enclosing());
    }
  }

  MOZ_TRY(xdr->codeEnum32(&scopeKind));
  MOZ_TRY(xdr->codeUint32(&enclosingIndex));

  if (mode == XDR_DECODE) {
    if (scopeIndex == 0) {
      MOZ_ASSERT(enclosingIndex == UINT32_MAX);
      enclosing = scriptEnclosingScope;
    } else {
      MOZ_ASSERT(enclosingIndex < scopeIndex);
      enclosing = data->scopes()[enclosingIndex];
    }
  }

  switch (scopeKind) {
    case ScopeKind::Function:
      MOZ_TRY(FunctionScope::XDR(xdr, fun, enclosing, scope));
      break;
    case ScopeKind::FunctionBodyVar:
    case ScopeKind::ParameterExpressionVar:
      MOZ_TRY(VarScope::XDR(xdr, scopeKind, enclosing, scope));
      break;
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
      MOZ_TRY(LexicalScope::XDR(xdr, scopeKind, enclosing, scope));
      break;
    case ScopeKind::With:
      MOZ_TRY(WithScope::XDR(xdr, enclosing, scope));
      break;
    case ScopeKind::Eval:
    case ScopeKind::StrictEval:
      MOZ_TRY(EvalScope::XDR(xdr, scopeKind, enclosing, scope));
      break;
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic:
      MOZ_TRY(GlobalScope::XDR(xdr, scopeKind, scope));
      break;
    case ScopeKind::Module:
    case ScopeKind::WasmInstance:
      MOZ_CRASH("NYI");
      break;
    case ScopeKind::WasmFunction:
      MOZ_CRASH("wasm functions cannot be nested in JSScripts");
      break;
    default:
      // Fail in debug, but only soft-fail in release
      MOZ_ASSERT(false, "Bad XDR scope kind");
      return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
  }

  return Ok();
}

template <XDRMode mode>
/* static */
XDRResult js::PrivateScriptData::XDR(XDRState<mode>* xdr, HandleScript script,
                                     HandleScriptSourceObject sourceObject,
                                     HandleScope scriptEnclosingScope,
                                     HandleFunction fun) {
  uint32_t nscopes = 0;
  uint32_t nconsts = 0;
  uint32_t nobjects = 0;
  uint32_t ntrynotes = 0;
  uint32_t nscopenotes = 0;
  uint32_t nresumeoffsets = 0;

  JSContext* cx = xdr->cx();
  PrivateScriptData* data = nullptr;

  if (mode == XDR_ENCODE) {
    data = script->data_;

    nscopes = data->scopes().size();
    if (data->hasConsts()) {
      nconsts = data->consts().size();
    }
    if (data->hasObjects()) {
      nobjects = data->objects().size();
    }
    if (data->hasTryNotes()) {
      ntrynotes = data->tryNotes().size();
    }
    if (data->hasScopeNotes()) {
      nscopenotes = data->scopeNotes().size();
    }
    if (data->hasResumeOffsets()) {
      nresumeoffsets = data->resumeOffsets().size();
    }
  }

  MOZ_TRY(xdr->codeUint32(&nscopes));
  MOZ_TRY(xdr->codeUint32(&nconsts));
  MOZ_TRY(xdr->codeUint32(&nobjects));
  MOZ_TRY(xdr->codeUint32(&ntrynotes));
  MOZ_TRY(xdr->codeUint32(&nscopenotes));
  MOZ_TRY(xdr->codeUint32(&nresumeoffsets));

  if (mode == XDR_DECODE) {
    if (!JSScript::createPrivateScriptData(cx, script, nscopes, nconsts,
                                           nobjects, ntrynotes, nscopenotes,
                                           nresumeoffsets)) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }

    data = script->data_;
  }

  if (nconsts) {
    RootedValue val(cx);
    for (GCPtrValue& elem : data->consts()) {
      if (mode == XDR_ENCODE) {
        val = elem.get();
      }
      MOZ_TRY(XDRScriptConst(xdr, &val));
      if (mode == XDR_DECODE) {
        elem.init(val);
      }
    }
  }

  {
    MOZ_ASSERT(nscopes > 0);
    GCPtrScope* vector = data->scopes().data();
    for (uint32_t i = 0; i < nscopes; ++i) {
      RootedScope scope(cx);
      if (mode == XDR_ENCODE) {
        scope = vector[i];
      }
      MOZ_TRY(XDRScope(xdr, data, scriptEnclosingScope, fun, i, &scope));
      if (mode == XDR_DECODE) {
        vector[i].init(scope);
      }
    }

    // Verify marker to detect data corruption after decoding scope data. A
    // mismatch here indicates we will almost certainly crash in release.
    MOZ_TRY(xdr->codeMarker(0x48922BAB));
  }

  /*
   * Here looping from 0-to-length to xdr objects is essential to ensure that
   * all references to enclosing blocks (via FindScopeIndex below) happen
   * after the enclosing block has been XDR'd.
   */
  if (nobjects) {
    for (GCPtrObject& elem : data->objects()) {
      RootedObject inner(cx);
      if (mode == XDR_ENCODE) {
        inner = elem;
      }
      MOZ_TRY(XDRInnerObject(xdr, data, sourceObject, &inner));
      if (mode == XDR_DECODE) {
        elem.init(inner);
      }
    }
  }

  // Verify marker to detect data corruption after decoding object data. A
  // mismatch here indicates we will almost certainly crash in release.
  MOZ_TRY(xdr->codeMarker(0xF83B989A));

  if (ntrynotes) {
    for (JSTryNote& elem : data->tryNotes()) {
      MOZ_TRY(elem.XDR(xdr));
    }
  }

  if (nscopenotes) {
    for (ScopeNote& elem : data->scopeNotes()) {
      MOZ_TRY(elem.XDR(xdr));
    }
  }

  if (nresumeoffsets) {
    for (uint32_t& elem : data->resumeOffsets()) {
      MOZ_TRY(xdr->codeUint32(&elem));
    }
  }

  return Ok();
}

// Placement-new elements of an array. This should optimize away for types with
// trivial default initiation.
template <typename T>
static void DefaultInitializeElements(void* arrayPtr, size_t length) {
  uintptr_t elem = reinterpret_cast<uintptr_t>(arrayPtr);
  MOZ_ASSERT(elem % alignof(T) == 0);

  for (size_t i = 0; i < length; ++i) {
    new (reinterpret_cast<void*>(elem)) T;
    elem += sizeof(T);
  }
}

/* static */ size_t SharedScriptData::AllocationSize(uint32_t codeLength,
                                                     uint32_t noteLength,
                                                     uint32_t natoms) {
  size_t size = sizeof(SharedScriptData);

  size += natoms * sizeof(GCPtrAtom);
  size += codeLength * sizeof(jsbytecode);
  size += noteLength * sizeof(jssrcnote);

  return size;
}

// Placement-new elements of an array. This should optimize away for types with
// trivial default initiation.
template <typename T>
void SharedScriptData::initElements(size_t offset, size_t length) {
  uintptr_t base = reinterpret_cast<uintptr_t>(this);
  DefaultInitializeElements<T>(reinterpret_cast<void*>(base + offset), length);
}

SharedScriptData::SharedScriptData(uint32_t codeLength, uint32_t noteLength,
                                   uint32_t natoms)
    : codeLength_(codeLength), noteLength_(noteLength), natoms_(natoms) {
  // Variable-length data begins immediately after SharedScriptData itself.
  size_t cursor = sizeof(*this);

  // Default-initialize trailing arrays.

  static_assert(alignof(SharedScriptData) >= alignof(GCPtrAtom),
                "Incompatible alignment");
  initElements<GCPtrAtom>(cursor, natoms);
  cursor += natoms * sizeof(GCPtrAtom);

  static_assert(alignof(GCPtrAtom) >= alignof(jsbytecode),
                "Incompatible alignment");
  initElements<jsbytecode>(cursor, codeLength);
  cursor += codeLength * sizeof(jsbytecode);

  static_assert(alignof(jsbytecode) >= alignof(jssrcnote),
                "Incompatible alignment");
  initElements<jssrcnote>(cursor, noteLength);
  cursor += noteLength * sizeof(jssrcnote);

  // Sanity check
  MOZ_ASSERT(AllocationSize(codeLength, noteLength, natoms) == cursor);
}

template <XDRMode mode>
/* static */
XDRResult SharedScriptData::XDR(XDRState<mode>* xdr, HandleScript script) {
  uint32_t natoms = 0;
  uint32_t codeLength = 0;
  uint32_t noteLength = 0;

  JSContext* cx = xdr->cx();
  SharedScriptData* ssd = nullptr;

  if (mode == XDR_ENCODE) {
    ssd = script->scriptData();

    natoms = ssd->natoms();
    codeLength = ssd->codeLength();
    noteLength = ssd->numNotes();
  }

  MOZ_TRY(xdr->codeUint32(&natoms));
  MOZ_TRY(xdr->codeUint32(&codeLength));
  MOZ_TRY(xdr->codeUint32(&noteLength));

  if (mode == XDR_DECODE) {
    if (!script->createSharedScriptData(cx, codeLength, noteLength, natoms)) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    ssd = script->scriptData();
  }

  JS_STATIC_ASSERT(sizeof(jsbytecode) == 1);
  JS_STATIC_ASSERT(sizeof(jssrcnote) == 1);

  jsbytecode* code = ssd->code();
  jssrcnote* notes = ssd->notes();
  MOZ_TRY(xdr->codeBytes(code, codeLength));
  MOZ_TRY(xdr->codeBytes(notes, noteLength));

  {
    RootedAtom atom(cx);
    GCPtrAtom* vector = ssd->atoms();

    for (uint32_t i = 0; i != natoms; ++i) {
      if (mode == XDR_ENCODE) {
        atom = vector[i];
      }
      MOZ_TRY(XDRAtom(xdr, &atom));
      if (mode == XDR_DECODE) {
        vector[i].init(atom);
      }
    }
  }

  return Ok();
}

template
    /* static */
    XDRResult
    SharedScriptData::XDR(XDRState<XDR_ENCODE>* xdr, HandleScript script);

template
    /* static */
    XDRResult
    SharedScriptData::XDR(XDRState<XDR_DECODE>* xdr, HandleScript script);

template <XDRMode mode>
XDRResult js::XDRScript(XDRState<mode>* xdr, HandleScope scriptEnclosingScope,
                        HandleScriptSourceObject sourceObjectArg,
                        HandleFunction fun, MutableHandleScript scriptp) {
  using ImmutableFlags = JSScript::ImmutableFlags;

  /* NB: Keep this in sync with CopyScript. */

  enum XDRScriptFlags {
    OwnSource,
    HasLazyScript,
  };

  uint8_t xdrScriptFlags = 0;

  uint32_t lineno = 0;
  uint32_t column = 0;
  uint32_t mainOffset = 0;
  uint32_t nfixed = 0;
  uint32_t nslots = 0;
  uint32_t bodyScopeIndex = 0;
  uint32_t sourceStart = 0;
  uint32_t sourceEnd = 0;
  uint32_t toStringStart = 0;
  uint32_t toStringEnd = 0;
  uint32_t immutableFlags = 0;
  uint16_t funLength = 0;
  uint16_t numBytecodeTypeSets = 0;

  // NOTE: |mutableFlags| are not preserved by XDR.

  JSContext* cx = xdr->cx();
  RootedScript script(cx);

  if (mode == XDR_ENCODE) {
    script = scriptp.get();
    MOZ_ASSERT(script->functionNonDelazifying() == fun);

    if (!fun && script->treatAsRunOnce() && script->hasRunOnce()) {
      // This is a toplevel or eval script that's runOnce.  We want to
      // make sure that we're not XDR-saving an object we emitted for
      // JSOP_OBJECT that then got modified.  So throw if we're not
      // cloning in JSOP_OBJECT or if we ever didn't clone in it in the
      // past.
      Realm* realm = cx->realm();
      if (!realm->creationOptions().cloneSingletons() ||
          !realm->behaviors().getSingletonsAsTemplates()) {
        return xdr->fail(JS::TranscodeResult_Failure_RunOnceNotSupported);
      }
    }

    if (!sourceObjectArg) {
      xdrScriptFlags |= (1 << OwnSource);
    }
    if (script->isRelazifiableIgnoringJitCode()) {
      xdrScriptFlags |= (1 << HasLazyScript);
    }
  }

  MOZ_TRY(xdr->codeUint8(&xdrScriptFlags));

  if (mode == XDR_ENCODE) {
    lineno = script->lineno();
    column = script->column();
    mainOffset = script->mainOffset();
    nfixed = script->nfixed();
    nslots = script->nslots();
    bodyScopeIndex = script->bodyScopeIndex();

    sourceStart = script->sourceStart();
    sourceEnd = script->sourceEnd();
    toStringStart = script->toStringStart();
    toStringEnd = script->toStringEnd();

    immutableFlags = script->immutableFlags_;

    funLength = script->funLength();
    numBytecodeTypeSets = script->numBytecodeTypeSets();
  }

  MOZ_TRY(xdr->codeUint32(&lineno));
  MOZ_TRY(xdr->codeUint32(&column));
  MOZ_TRY(xdr->codeUint32(&mainOffset));
  MOZ_TRY(xdr->codeUint32(&nfixed));
  MOZ_TRY(xdr->codeUint32(&nslots));
  MOZ_TRY(xdr->codeUint32(&bodyScopeIndex));
  MOZ_TRY(xdr->codeUint32(&sourceStart));
  MOZ_TRY(xdr->codeUint32(&sourceEnd));
  MOZ_TRY(xdr->codeUint32(&toStringStart));
  MOZ_TRY(xdr->codeUint32(&toStringEnd));
  MOZ_TRY(xdr->codeUint32(&immutableFlags));
  MOZ_TRY(xdr->codeUint16(&funLength));
  MOZ_TRY(xdr->codeUint16(&numBytecodeTypeSets));

  RootedScriptSourceObject sourceObject(cx, sourceObjectArg);
  Maybe<CompileOptions> options;

  if (mode == XDR_DECODE) {
    // When loading from the bytecode cache, we get the CompileOptions from
    // the document. If the noScriptRval or selfHostingMode flag doesn't
    // match, we should fail. This only applies to the top-level and not
    // its inner functions.
    bool noScriptRval =
        !!(immutableFlags & uint32_t(ImmutableFlags::NoScriptRval));
    bool selfHosted = !!(immutableFlags & uint32_t(ImmutableFlags::SelfHosted));
    if (xdr->hasOptions() && (xdrScriptFlags & (1 << OwnSource))) {
      options.emplace(xdr->cx(), xdr->options());
      if (options->noScriptRval != noScriptRval ||
          options->selfHostingMode != selfHosted) {
        return xdr->fail(JS::TranscodeResult_Failure_WrongCompileOption);
      }
    } else {
      options.emplace(xdr->cx());
      (*options).setNoScriptRval(noScriptRval).setSelfHostingMode(selfHosted);
    }
  }

  if (xdrScriptFlags & (1 << OwnSource)) {
    Rooted<ScriptSourceHolder> ssHolder(cx);

    // We are relying on the script's ScriptSource so the caller should not
    // have passed in an explicit one.
    MOZ_ASSERT(sourceObjectArg == nullptr);

    if (mode == XDR_ENCODE) {
      sourceObject = script->sourceObject();
      ssHolder.get().reset(sourceObject->source());
    }

    MOZ_TRY(ScriptSource::XDR(xdr, options, &ssHolder));

    if (mode == XDR_DECODE) {
      sourceObject = ScriptSourceObject::create(cx, ssHolder.get().get());
      if (!sourceObject) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }

      if (xdr->hasScriptSourceObjectOut()) {
        // When the ScriptSourceObjectOut is provided by ParseTask, it
        // is stored in a location which is traced by the GC.
        *xdr->scriptSourceObjectOut() = sourceObject;
      } else if (!ScriptSourceObject::initFromOptions(cx, sourceObject,
                                                      *options)) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
    }
  } else {
    // While encoding, the ScriptSource passed in must match the ScriptSource
    // of the script.
    MOZ_ASSERT_IF(mode == XDR_ENCODE,
                  sourceObjectArg->source() == script->scriptSource());
  }

  if (mode == XDR_DECODE) {
    script = JSScript::Create(cx, *options, sourceObject, sourceStart,
                              sourceEnd, toStringStart, toStringEnd);
    if (!script) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    scriptp.set(script);

    script->lineno_ = lineno;
    script->column_ = column;
    script->mainOffset_ = mainOffset;
    script->nfixed_ = nfixed;
    script->nslots_ = nslots;
    script->bodyScopeIndex_ = bodyScopeIndex;
    script->immutableFlags_ = immutableFlags;
    script->funLength_ = funLength;
    script->numBytecodeTypeSets_ = numBytecodeTypeSets;

    if (script->hasFlag(ImmutableFlags::ArgsHasVarBinding)) {
      // Call setArgumentsHasVarBinding to initialize the
      // NeedsArgsAnalysis flag.
      script->setArgumentsHasVarBinding();
    }

    // Set the script in its function now so that inner scripts to be
    // decoded may iterate the static scope chain.
    if (fun) {
      fun->initScript(script);
    }
  }

  // If XDR operation fails, we must call JSScript::freeScriptData in order
  // to neuter the script. Various things that iterate raw scripts in a GC
  // arena use the presense of this data to detect if initialization is
  // complete.
  auto scriptDataGuard = mozilla::MakeScopeExit([&] {
    if (mode == XDR_DECODE) {
      script->freeScriptData();
    }
  });

  // NOTE: The script data is rooted by the script.
  MOZ_TRY(PrivateScriptData::XDR<mode>(xdr, script, sourceObject,
                                       scriptEnclosingScope, fun));
  MOZ_TRY(SharedScriptData::XDR<mode>(xdr, script));

  if (mode == XDR_DECODE) {
    if (!script->shareScriptData(cx)) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  if (xdrScriptFlags & (1 << HasLazyScript)) {
    Rooted<LazyScript*> lazy(cx);
    if (mode == XDR_ENCODE) {
      lazy = script->maybeLazyScript();
    }

    MOZ_TRY(
        XDRRelazificationInfo(xdr, fun, script, scriptEnclosingScope, &lazy));

    if (mode == XDR_DECODE) {
      script->setLazyScript(lazy);
    }
  }

  if (mode == XDR_DECODE) {
    /* see BytecodeEmitter::tellDebuggerAboutCompiledScript */
    if (!fun && !cx->helperThread()) {
      Debugger::onNewScript(cx, script);
    }
  }

  scriptDataGuard.release();
  return Ok();
}

template XDRResult js::XDRScript(XDRState<XDR_ENCODE>*, HandleScope,
                                 HandleScriptSourceObject, HandleFunction,
                                 MutableHandleScript);

template XDRResult js::XDRScript(XDRState<XDR_DECODE>*, HandleScope,
                                 HandleScriptSourceObject, HandleFunction,
                                 MutableHandleScript);

template <XDRMode mode>
XDRResult js::XDRLazyScript(XDRState<mode>* xdr, HandleScope enclosingScope,
                            HandleScriptSourceObject sourceObject,
                            HandleFunction fun,
                            MutableHandle<LazyScript*> lazy) {
  MOZ_ASSERT_IF(mode == XDR_DECODE, sourceObject);

  JSContext* cx = xdr->cx();

  {
    uint32_t sourceStart;
    uint32_t sourceEnd;
    uint32_t toStringStart;
    uint32_t toStringEnd;
    uint32_t lineno;
    uint32_t column;
    uint64_t packedFields;
    uint32_t numFieldInitializers;

    if (mode == XDR_ENCODE) {
      // Note: it's possible the LazyScript has a non-null script_ pointer
      // to a JSScript. We don't encode it: we can just delazify the
      // lazy script.

      MOZ_ASSERT(fun == lazy->functionNonDelazifying());

      sourceStart = lazy->sourceStart();
      sourceEnd = lazy->sourceEnd();
      toStringStart = lazy->toStringStart();
      toStringEnd = lazy->toStringEnd();
      lineno = lazy->lineno();
      column = lazy->column();
      packedFields = lazy->packedFieldsForXDR();
      if (fun->kind() == JSFunction::FunctionKind::ClassConstructor) {
        numFieldInitializers =
            (uint32_t)lazy->getFieldInitializers().numFieldInitializers;
      } else {
        numFieldInitializers = UINT32_MAX;
      }
    }

    MOZ_TRY(xdr->codeUint32(&sourceStart));
    MOZ_TRY(xdr->codeUint32(&sourceEnd));
    MOZ_TRY(xdr->codeUint32(&toStringStart));
    MOZ_TRY(xdr->codeUint32(&toStringEnd));
    MOZ_TRY(xdr->codeUint32(&lineno));
    MOZ_TRY(xdr->codeUint32(&column));
    MOZ_TRY(xdr->codeUint64(&packedFields));
    MOZ_TRY(xdr->codeUint32(&numFieldInitializers));

    if (mode == XDR_DECODE) {
      lazy.set(LazyScript::CreateForXDR(
          cx, fun, nullptr, enclosingScope, sourceObject, packedFields,
          sourceStart, sourceEnd, toStringStart, lineno, column));
      if (!lazy) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
      lazy->setToStringEnd(toStringEnd);
      if (numFieldInitializers != UINT32_MAX) {
        lazy->setFieldInitializers(
            FieldInitializers((size_t)numFieldInitializers));
      }
      fun->initLazyScript(lazy);
    }
  }

  // Code closed-over bindings.
  MOZ_TRY(XDRLazyClosedOverBindings(xdr, lazy));

  // Code inner functions.
  {
    RootedFunction func(cx);
    GCPtrFunction* innerFunctions = lazy->innerFunctions();
    size_t numInnerFunctions = lazy->numInnerFunctions();
    for (size_t i = 0; i < numInnerFunctions; i++) {
      if (mode == XDR_ENCODE) {
        func = innerFunctions[i];
      }

      MOZ_TRY(XDRInterpretedFunction(xdr, nullptr, sourceObject, &func));

      if (mode == XDR_DECODE) {
        innerFunctions[i] = func;
        if (innerFunctions[i]->isInterpretedLazy()) {
          innerFunctions[i]->lazyScript()->setEnclosingLazyScript(lazy);
        }
      }
    }
  }

  return Ok();
}

template XDRResult js::XDRLazyScript(XDRState<XDR_ENCODE>*, HandleScope,
                                     HandleScriptSourceObject, HandleFunction,
                                     MutableHandle<LazyScript*>);

template XDRResult js::XDRLazyScript(XDRState<XDR_DECODE>*, HandleScope,
                                     HandleScriptSourceObject, HandleFunction,
                                     MutableHandle<LazyScript*>);

void JSScript::setSourceObject(js::ScriptSourceObject* object) {
  MOZ_ASSERT(compartment() == object->compartment());
  sourceObject_ = object;
}

void JSScript::setDefaultClassConstructorSpan(
    js::ScriptSourceObject* sourceObject, uint32_t start, uint32_t end,
    unsigned line, unsigned column) {
  MOZ_ASSERT(isDefaultClassConstructor());
  setSourceObject(sourceObject);
  toStringStart_ = start;
  toStringEnd_ = end;
  sourceStart_ = start;
  sourceEnd_ = end;
  lineno_ = line;
  column_ = column;
  // Since this script has been changed to point into the user's source, we
  // can clear its self-hosted flag, allowing Debugger to see it.
  clearFlag(ImmutableFlags::SelfHosted);
}

js::ScriptSource* JSScript::scriptSource() const {
  return sourceObject()->source();
}

js::ScriptSource* JSScript::maybeForwardedScriptSource() const {
  JSObject* source = MaybeForwarded(sourceObject());
  // This may be called during GC. It's OK not to expose the source object
  // here as it doesn't escape.
  return UncheckedUnwrapWithoutExpose(source)
      ->as<ScriptSourceObject>()
      .source();
}

bool JSScript::initScriptCounts(JSContext* cx) {
  MOZ_ASSERT(!hasScriptCounts());

  // Record all pc which are the first instruction of a basic block.
  mozilla::Vector<jsbytecode*, 16, SystemAllocPolicy> jumpTargets;

  js::BytecodeLocation main = mainLocation();
  AllBytecodesIterable iterable(this);
  for (auto& loc : iterable) {
    if (loc.isJumpTarget() || loc == main) {
      if (!jumpTargets.append(loc.toRawBytecode())) {
        ReportOutOfMemory(cx);
        return false;
      }
    }
  }

  // Initialize all PCCounts counters to 0.
  ScriptCounts::PCCountsVector base;
  if (!base.reserve(jumpTargets.length())) {
    ReportOutOfMemory(cx);
    return false;
  }

  for (size_t i = 0; i < jumpTargets.length(); i++) {
    base.infallibleEmplaceBack(pcToOffset(jumpTargets[i]));
  }

  // Create realm's scriptCountsMap if necessary.
  if (!realm()->scriptCountsMap) {
    auto map = cx->make_unique<ScriptCountsMap>();
    if (!map) {
      return false;
    }

    realm()->scriptCountsMap = std::move(map);
  }

  // Allocate the ScriptCounts.
  UniqueScriptCounts sc = cx->make_unique<ScriptCounts>(std::move(base));
  if (!sc) {
    ReportOutOfMemory(cx);
    return false;
  }

  // Register the current ScriptCounts in the realm's map.
  if (!realm()->scriptCountsMap->putNew(this, std::move(sc))) {
    ReportOutOfMemory(cx);
    return false;
  }

  // safe to set this;  we can't fail after this point.
  setFlag(MutableFlags::HasScriptCounts);

  // Enable interrupts in any interpreter frames running on this script. This
  // is used to let the interpreter increment the PCCounts, if present.
  for (ActivationIterator iter(cx); !iter.done(); ++iter) {
    if (iter->isInterpreter()) {
      iter->asInterpreter()->enableInterruptsIfRunning(this);
    }
  }

  return true;
}

static inline ScriptCountsMap::Ptr GetScriptCountsMapEntry(JSScript* script) {
  MOZ_ASSERT(script->hasScriptCounts());
  ScriptCountsMap::Ptr p = script->realm()->scriptCountsMap->lookup(script);
  MOZ_ASSERT(p);
  return p;
}

static inline ScriptNameMap::Ptr GetScriptNameMapEntry(JSScript* script) {
  auto p = script->realm()->scriptNameMap->lookup(script);
  MOZ_ASSERT(p);
  return p;
}

ScriptCounts& JSScript::getScriptCounts() {
  ScriptCountsMap::Ptr p = GetScriptCountsMapEntry(this);
  return *p->value();
}

const char* JSScript::getScriptName() {
  auto p = GetScriptNameMapEntry(this);
  return p->value().get();
}

js::PCCounts* ScriptCounts::maybeGetPCCounts(size_t offset) {
  PCCounts searched = PCCounts(offset);
  PCCounts* elem =
      std::lower_bound(pcCounts_.begin(), pcCounts_.end(), searched);
  if (elem == pcCounts_.end() || elem->pcOffset() != offset) {
    return nullptr;
  }
  return elem;
}

const js::PCCounts* ScriptCounts::maybeGetPCCounts(size_t offset) const {
  PCCounts searched = PCCounts(offset);
  const PCCounts* elem =
      std::lower_bound(pcCounts_.begin(), pcCounts_.end(), searched);
  if (elem == pcCounts_.end() || elem->pcOffset() != offset) {
    return nullptr;
  }
  return elem;
}

js::PCCounts* ScriptCounts::getImmediatePrecedingPCCounts(size_t offset) {
  PCCounts searched = PCCounts(offset);
  PCCounts* elem =
      std::lower_bound(pcCounts_.begin(), pcCounts_.end(), searched);
  if (elem == pcCounts_.end()) {
    return &pcCounts_.back();
  }
  if (elem->pcOffset() == offset) {
    return elem;
  }
  if (elem != pcCounts_.begin()) {
    return elem - 1;
  }
  return nullptr;
}

const js::PCCounts* ScriptCounts::maybeGetThrowCounts(size_t offset) const {
  PCCounts searched = PCCounts(offset);
  const PCCounts* elem =
      std::lower_bound(throwCounts_.begin(), throwCounts_.end(), searched);
  if (elem == throwCounts_.end() || elem->pcOffset() != offset) {
    return nullptr;
  }
  return elem;
}

const js::PCCounts* ScriptCounts::getImmediatePrecedingThrowCounts(
    size_t offset) const {
  PCCounts searched = PCCounts(offset);
  const PCCounts* elem =
      std::lower_bound(throwCounts_.begin(), throwCounts_.end(), searched);
  if (elem == throwCounts_.end()) {
    if (throwCounts_.begin() == throwCounts_.end()) {
      return nullptr;
    }
    return &throwCounts_.back();
  }
  if (elem->pcOffset() == offset) {
    return elem;
  }
  if (elem != throwCounts_.begin()) {
    return elem - 1;
  }
  return nullptr;
}

js::PCCounts* ScriptCounts::getThrowCounts(size_t offset) {
  PCCounts searched = PCCounts(offset);
  PCCounts* elem =
      std::lower_bound(throwCounts_.begin(), throwCounts_.end(), searched);
  if (elem == throwCounts_.end() || elem->pcOffset() != offset) {
    elem = throwCounts_.insert(elem, searched);
  }
  return elem;
}

size_t ScriptCounts::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) {
  return mallocSizeOf(this) + pcCounts_.sizeOfExcludingThis(mallocSizeOf) +
         throwCounts_.sizeOfExcludingThis(mallocSizeOf) +
         ionCounts_->sizeOfIncludingThis(mallocSizeOf);
}

void JSScript::setIonScript(JSRuntime* rt, js::jit::IonScript* ionScript) {
  MOZ_ASSERT_IF(ionScript != ION_DISABLED_SCRIPT,
                !baselineScript()->hasPendingIonBuilder());
  if (hasIonScript()) {
    js::jit::IonScript::writeBarrierPre(zone(), ion);
  }
  ion = ionScript;
  MOZ_ASSERT_IF(hasIonScript(), hasBaselineScript());
  updateJitCodeRaw(rt);
}

js::PCCounts* JSScript::maybeGetPCCounts(jsbytecode* pc) {
  MOZ_ASSERT(containsPC(pc));
  return getScriptCounts().maybeGetPCCounts(pcToOffset(pc));
}

const js::PCCounts* JSScript::maybeGetThrowCounts(jsbytecode* pc) {
  MOZ_ASSERT(containsPC(pc));
  return getScriptCounts().maybeGetThrowCounts(pcToOffset(pc));
}

js::PCCounts* JSScript::getThrowCounts(jsbytecode* pc) {
  MOZ_ASSERT(containsPC(pc));
  return getScriptCounts().getThrowCounts(pcToOffset(pc));
}

uint64_t JSScript::getHitCount(jsbytecode* pc) {
  MOZ_ASSERT(containsPC(pc));
  if (pc < main()) {
    pc = main();
  }

  ScriptCounts& sc = getScriptCounts();
  size_t targetOffset = pcToOffset(pc);
  const js::PCCounts* baseCount =
      sc.getImmediatePrecedingPCCounts(targetOffset);
  if (!baseCount) {
    return 0;
  }
  if (baseCount->pcOffset() == targetOffset) {
    return baseCount->numExec();
  }
  MOZ_ASSERT(baseCount->pcOffset() < targetOffset);
  uint64_t count = baseCount->numExec();
  do {
    const js::PCCounts* throwCount =
        sc.getImmediatePrecedingThrowCounts(targetOffset);
    if (!throwCount) {
      return count;
    }
    if (throwCount->pcOffset() <= baseCount->pcOffset()) {
      return count;
    }
    count -= throwCount->numExec();
    targetOffset = throwCount->pcOffset() - 1;
  } while (true);
}

void JSScript::incHitCount(jsbytecode* pc) {
  MOZ_ASSERT(containsPC(pc));
  if (pc < main()) {
    pc = main();
  }

  ScriptCounts& sc = getScriptCounts();
  js::PCCounts* baseCount = sc.getImmediatePrecedingPCCounts(pcToOffset(pc));
  if (!baseCount) {
    return;
  }
  baseCount->numExec()++;
}

void JSScript::addIonCounts(jit::IonScriptCounts* ionCounts) {
  ScriptCounts& sc = getScriptCounts();
  if (sc.ionCounts_) {
    ionCounts->setPrevious(sc.ionCounts_);
  }
  sc.ionCounts_ = ionCounts;
}

jit::IonScriptCounts* JSScript::getIonCounts() {
  return getScriptCounts().ionCounts_;
}

void JSScript::clearHasScriptCounts() {
  clearFlag(MutableFlags::HasScriptCounts);
}

void JSScript::releaseScriptCounts(ScriptCounts* counts) {
  ScriptCountsMap::Ptr p = GetScriptCountsMapEntry(this);
  *counts = std::move(*p->value().get());
  realm()->scriptCountsMap->remove(p);
  clearHasScriptCounts();
}

void JSScript::destroyScriptCounts() {
  if (hasScriptCounts()) {
    ScriptCounts scriptCounts;
    releaseScriptCounts(&scriptCounts);
  }
}

void JSScript::destroyScriptName() {
  auto p = GetScriptNameMapEntry(this);
  realm()->scriptNameMap->remove(p);
}

void JSScript::resetScriptCounts() {
  if (!hasScriptCounts()) {
    return;
  }

  ScriptCounts& sc = getScriptCounts();

  for (PCCounts& elem : sc.pcCounts_) {
    elem.numExec() = 0;
  }

  for (PCCounts& elem : sc.throwCounts_) {
    elem.numExec() = 0;
  }
}

bool JSScript::hasScriptName() {
  if (!realm()->scriptNameMap) {
    return false;
  }

  auto p = realm()->scriptNameMap->lookup(this);
  return p.found();
}

void ScriptSourceObject::finalize(FreeOp* fop, JSObject* obj) {
  MOZ_ASSERT(fop->onMainThread());
  ScriptSourceObject* sso = &obj->as<ScriptSourceObject>();
  sso->source()->decref();

  // Clear the private value, calling the release hook if necessary.
  sso->setPrivate(fop->runtime(), UndefinedValue());
}

void ScriptSourceObject::trace(JSTracer* trc, JSObject* obj) {
  // This can be invoked during allocation of the SSO itself, before we've had a
  // chance to initialize things properly. In that case, there's nothing to
  // trace.
  if (obj->as<ScriptSourceObject>().hasSource()) {
    obj->as<ScriptSourceObject>().source()->trace(trc);
  }
}

static const ClassOps ScriptSourceObjectClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* enumerate */
    nullptr, /* newEnumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    ScriptSourceObject::finalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    ScriptSourceObject::trace};

const Class ScriptSourceObject::class_ = {
    "ScriptSource",
    JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS) | JSCLASS_FOREGROUND_FINALIZE,
    &ScriptSourceObjectClassOps};

ScriptSourceObject* ScriptSourceObject::createInternal(JSContext* cx,
                                                       ScriptSource* source,
                                                       HandleObject canonical) {
  ScriptSourceObject* obj =
      NewObjectWithGivenProto<ScriptSourceObject>(cx, nullptr);
  if (!obj) {
    return nullptr;
  }

  source->incref();  // The matching decref is in ScriptSourceObject::finalize.

  obj->initReservedSlot(SOURCE_SLOT, PrivateValue(source));

  if (canonical) {
    obj->initReservedSlot(CANONICAL_SLOT, ObjectValue(*canonical));
  } else {
    obj->initReservedSlot(CANONICAL_SLOT, ObjectValue(*obj));
  }

  // The slots below should either be populated by a call to initFromOptions or,
  // if this is a non-canonical ScriptSourceObject, they are unused. Poison
  // them.
  obj->initReservedSlot(ELEMENT_SLOT, MagicValue(JS_GENERIC_MAGIC));
  obj->initReservedSlot(ELEMENT_PROPERTY_SLOT, MagicValue(JS_GENERIC_MAGIC));
  obj->initReservedSlot(INTRODUCTION_SCRIPT_SLOT, MagicValue(JS_GENERIC_MAGIC));

  return obj;
}

ScriptSourceObject* ScriptSourceObject::create(JSContext* cx,
                                               ScriptSource* source) {
  return createInternal(cx, source, nullptr);
}

ScriptSourceObject* ScriptSourceObject::clone(JSContext* cx,
                                              HandleScriptSourceObject sso) {
  MOZ_ASSERT(cx->compartment() != sso->compartment());

  RootedObject wrapped(cx, sso);
  if (!cx->compartment()->wrap(cx, &wrapped)) {
    return nullptr;
  }

  return createInternal(cx, sso->source(), wrapped);
}

ScriptSourceObject* ScriptSourceObject::unwrappedCanonical() const {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtimeFromAnyThread()));

  JSObject* obj = &getReservedSlot(CANONICAL_SLOT).toObject();
  return &UncheckedUnwrap(obj)->as<ScriptSourceObject>();
}

/* static */
bool ScriptSourceObject::initFromOptions(
    JSContext* cx, HandleScriptSourceObject source,
    const ReadOnlyCompileOptions& options) {
  cx->releaseCheck(source);
  MOZ_ASSERT(source->isCanonical());
  MOZ_ASSERT(source->getReservedSlot(ELEMENT_SLOT).isMagic(JS_GENERIC_MAGIC));
  MOZ_ASSERT(
      source->getReservedSlot(ELEMENT_PROPERTY_SLOT).isMagic(JS_GENERIC_MAGIC));
  MOZ_ASSERT(source->getReservedSlot(INTRODUCTION_SCRIPT_SLOT)
                 .isMagic(JS_GENERIC_MAGIC));

  RootedObject element(cx, options.element());
  RootedString elementAttributeName(cx, options.elementAttributeName());
  if (!initElementProperties(cx, source, element, elementAttributeName)) {
    return false;
  }

  // There is no equivalent of cross-compartment wrappers for scripts. If the
  // introduction script and ScriptSourceObject are in different compartments,
  // we would be creating a cross-compartment script reference, which is
  // forbidden. We can still store a CCW to the script source object though.
  RootedValue introductionScript(cx);
  if (JSScript* script = options.introductionScript()) {
    if (script->compartment() == cx->compartment()) {
      introductionScript.setPrivateGCThing(options.introductionScript());
    }
  }
  source->setReservedSlot(INTRODUCTION_SCRIPT_SLOT, introductionScript);

  // Set the private value to that of the script or module that this source is
  // part of, if any.
  RootedValue privateValue(cx);
  if (JSScript* script = options.scriptOrModule()) {
    privateValue = script->sourceObject()->canonicalPrivate();
    if (!JS_WrapValue(cx, &privateValue)) {
      return false;
    }
  }
  source->setPrivate(cx->runtime(), privateValue);

  return true;
}

/* static */
bool ScriptSourceObject::initElementProperties(JSContext* cx,
                                               HandleScriptSourceObject source,
                                               HandleObject element,
                                               HandleString elementAttrName) {
  MOZ_ASSERT(source->isCanonical());

  RootedValue elementValue(cx, ObjectOrNullValue(element));
  if (!cx->compartment()->wrap(cx, &elementValue)) {
    return false;
  }

  RootedValue nameValue(cx);
  if (elementAttrName) {
    nameValue = StringValue(elementAttrName);
  }
  if (!cx->compartment()->wrap(cx, &nameValue)) {
    return false;
  }

  source->setReservedSlot(ELEMENT_SLOT, elementValue);
  source->setReservedSlot(ELEMENT_PROPERTY_SLOT, nameValue);

  return true;
}

void ScriptSourceObject::setPrivate(JSRuntime* rt, const Value& value) {
  // Update the private value, calling addRef/release hooks if necessary
  // to allow the embedding to maintain a reference count for the
  // private data.
  JS::AutoSuppressGCAnalysis nogc;
  Value prevValue = getReservedSlot(PRIVATE_SLOT);
  rt->releaseScriptPrivate(prevValue);
  setReservedSlot(PRIVATE_SLOT, value);
  rt->addRefScriptPrivate(value);
}

/* static */
bool JSScript::loadSource(JSContext* cx, ScriptSource* ss, bool* worked) {
  MOZ_ASSERT(!ss->hasSourceText());
  *worked = false;
  if (!cx->runtime()->sourceHook.ref() || !ss->sourceRetrievable()) {
    return true;
  }
  char16_t* src = nullptr;
  size_t length;
  if (!cx->runtime()->sourceHook->load(cx, ss->filename(), &src, &length)) {
    return false;
  }
  if (!src) {
    return true;
  }

  // XXX On-demand source is currently only UTF-16.  Perhaps it should be
  //     changed to UTF-8, or UTF-8 be allowed in addition to UTF-16?
  if (!ss->setSource(cx, EntryUnits<char16_t>(src), length)) {
    return false;
  }

  *worked = true;
  return true;
}

/* static */
JSFlatString* JSScript::sourceData(JSContext* cx, HandleScript script) {
  MOZ_ASSERT(script->scriptSource()->hasSourceText());
  return script->scriptSource()->substring(cx, script->sourceStart(),
                                           script->sourceEnd());
}

bool JSScript::appendSourceDataForToString(JSContext* cx, StringBuffer& buf) {
  MOZ_ASSERT(scriptSource()->hasSourceText());
  return scriptSource()->appendSubstring(cx, buf, toStringStart(),
                                         toStringEnd());
}

void UncompressedSourceCache::holdEntry(AutoHoldEntry& holder,
                                        const ScriptSourceChunk& ssc) {
  MOZ_ASSERT(!holder_);
  holder.holdEntry(this, ssc);
  holder_ = &holder;
}

void UncompressedSourceCache::releaseEntry(AutoHoldEntry& holder) {
  MOZ_ASSERT(holder_ == &holder);
  holder_ = nullptr;
}

template <typename Unit>
const Unit* UncompressedSourceCache::lookup(const ScriptSourceChunk& ssc,
                                            AutoHoldEntry& holder) {
  MOZ_ASSERT(!holder_);
  MOZ_ASSERT(ssc.ss->compressedSourceIs<Unit>());

  if (!map_) {
    return nullptr;
  }

  if (Map::Ptr p = map_->lookup(ssc)) {
    holdEntry(holder, ssc);
    return static_cast<const Unit*>(p->value().get());
  }

  return nullptr;
}

bool UncompressedSourceCache::put(const ScriptSourceChunk& ssc, SourceData data,
                                  AutoHoldEntry& holder) {
  MOZ_ASSERT(!holder_);

  if (!map_) {
    map_ = MakeUnique<Map>();
    if (!map_) {
      return false;
    }
  }

  if (!map_->put(ssc, std::move(data))) {
    return false;
  }

  holdEntry(holder, ssc);
  return true;
}

void UncompressedSourceCache::purge() {
  if (!map_) {
    return;
  }

  for (Map::Range r = map_->all(); !r.empty(); r.popFront()) {
    if (holder_ && r.front().key() == holder_->sourceChunk()) {
      holder_->deferDelete(std::move(r.front().value()));
      holder_ = nullptr;
    }
  }

  map_ = nullptr;
}

size_t UncompressedSourceCache::sizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) {
  size_t n = 0;
  if (map_ && !map_->empty()) {
    n += map_->shallowSizeOfIncludingThis(mallocSizeOf);
    for (Map::Range r = map_->all(); !r.empty(); r.popFront()) {
      n += mallocSizeOf(r.front().value().get());
    }
  }
  return n;
}

template <typename Unit>
const Unit* ScriptSource::chunkUnits(
    JSContext* cx, UncompressedSourceCache::AutoHoldEntry& holder,
    size_t chunk) {
  const Compressed<Unit>& c = data.as<Compressed<Unit>>();

  ScriptSourceChunk ssc(this, chunk);
  if (const Unit* decompressed =
          cx->caches().uncompressedSourceCache.lookup<Unit>(ssc, holder)) {
    return decompressed;
  }

  size_t totalLengthInBytes = length() * sizeof(Unit);
  size_t chunkBytes = Compressor::chunkSize(totalLengthInBytes, chunk);

  MOZ_ASSERT((chunkBytes % sizeof(Unit)) == 0);
  const size_t chunkLength = chunkBytes / sizeof(Unit);
  EntryUnits<Unit> decompressed(js_pod_malloc<Unit>(chunkLength));
  if (!decompressed) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  // Compression treats input and output memory as plain ol' bytes. These
  // reinterpret_cast<>s accord exactly with that.
  if (!DecompressStringChunk(
          reinterpret_cast<const unsigned char*>(c.raw.chars()), chunk,
          reinterpret_cast<unsigned char*>(decompressed.get()), chunkBytes)) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  const Unit* ret = decompressed.get();
  if (!cx->caches().uncompressedSourceCache.put(
          ssc, ToSourceData(std::move(decompressed)), holder)) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }
  return ret;
}

template <typename Unit>
void ScriptSource::movePendingCompressedSource() {
  if (pendingCompressed_.empty()) {
    return;
  }

  Compressed<Unit>& pending = pendingCompressed_.ref<Compressed<Unit>>();

  MOZ_ASSERT(!hasCompressedSource());
  MOZ_ASSERT_IF(hasUncompressedSource(),
                pending.uncompressedLength == length());

  data = SourceType(std::move(pending));
  pendingCompressed_.destroy();
}

template <typename Unit>
ScriptSource::PinnedUnits<Unit>::~PinnedUnits() {
  if (units_) {
    MOZ_ASSERT(*stack_ == this);
    *stack_ = prev_;
    if (!prev_) {
      source_->movePendingCompressedSource<Unit>();
    }
  }
}

template <typename Unit>
const Unit* ScriptSource::units(JSContext* cx,
                                UncompressedSourceCache::AutoHoldEntry& holder,
                                size_t begin, size_t len) {
  MOZ_ASSERT(begin <= length());
  MOZ_ASSERT(begin + len <= length());

  if (data.is<Uncompressed<Unit>>()) {
    const Unit* units = data.as<Uncompressed<Unit>>().units();
    if (!units) {
      return nullptr;
    }
    return units + begin;
  }

  if (data.is<Missing>()) {
    MOZ_CRASH(
        "ScriptSource::units() on ScriptSource with SourceType = Missing");
  }

  MOZ_ASSERT(data.is<Compressed<Unit>>());

  // Determine first/last chunks, the offset (in bytes) into the first chunk
  // of the requested units, and the number of bytes in the last chunk.
  //
  // Note that first and last chunk sizes are miscomputed and *must not be
  // used* when the first chunk is the last chunk.
  size_t firstChunk, firstChunkOffset, firstChunkSize;
  size_t lastChunk, lastChunkSize;
  Compressor::rangeToChunkAndOffset(
      begin * sizeof(Unit), (begin + len) * sizeof(Unit), &firstChunk,
      &firstChunkOffset, &firstChunkSize, &lastChunk, &lastChunkSize);
  MOZ_ASSERT(firstChunk <= lastChunk);
  MOZ_ASSERT(firstChunkOffset % sizeof(Unit) == 0);
  MOZ_ASSERT(firstChunkSize % sizeof(Unit) == 0);

  size_t firstUnit = firstChunkOffset / sizeof(Unit);

  // Directly return units within a single chunk.  UncompressedSourceCache
  // and |holder| will hold the units alive past function return.
  if (firstChunk == lastChunk) {
    const Unit* units = chunkUnits<Unit>(cx, holder, firstChunk);
    if (!units) {
      return nullptr;
    }

    return units + firstUnit;
  }

  // Otherwise the units span multiple chunks.  Copy successive chunks'
  // decompressed units into freshly-allocated memory to return.
  EntryUnits<Unit> decompressed(js_pod_malloc<Unit>(len));
  if (!decompressed) {
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  Unit* cursor;

  {
    // |AutoHoldEntry| is single-shot, and a holder successfully filled in
    // by |chunkUnits| must be destroyed before another can be used.  Thus
    // we can't use |holder| with |chunkUnits| when |chunkUnits| is used
    // with multiple chunks, and we must use and destroy distinct, fresh
    // holders for each chunk.
    UncompressedSourceCache::AutoHoldEntry firstHolder;
    const Unit* units = chunkUnits<Unit>(cx, firstHolder, firstChunk);
    if (!units) {
      return nullptr;
    }

    cursor = std::copy_n(units + firstUnit, firstChunkSize / sizeof(Unit),
                         decompressed.get());
  }

  for (size_t i = firstChunk + 1; i < lastChunk; i++) {
    UncompressedSourceCache::AutoHoldEntry chunkHolder;
    const Unit* units = chunkUnits<Unit>(cx, chunkHolder, i);
    if (!units) {
      return nullptr;
    }

    cursor = std::copy_n(units, Compressor::CHUNK_SIZE / sizeof(Unit), cursor);
  }

  {
    UncompressedSourceCache::AutoHoldEntry lastHolder;
    const Unit* units = chunkUnits<Unit>(cx, lastHolder, lastChunk);
    if (!units) {
      return nullptr;
    }

    cursor = std::copy_n(units, lastChunkSize / sizeof(Unit), cursor);
  }

  MOZ_ASSERT(PointerRangeSize(decompressed.get(), cursor) == len);

  // Transfer ownership to |holder|.
  const Unit* ret = decompressed.get();
  holder.holdUnits(std::move(decompressed));
  return ret;
}

template <typename Unit>
ScriptSource::PinnedUnits<Unit>::PinnedUnits(
    JSContext* cx, ScriptSource* source,
    UncompressedSourceCache::AutoHoldEntry& holder, size_t begin, size_t len)
    : PinnedUnitsBase(source) {
  MOZ_ASSERT(source->hasSourceType<Unit>(), "must pin units of source's type");

  units_ = source->units<Unit>(cx, holder, begin, len);
  if (units_) {
    stack_ = &source->pinnedUnitsStack_;
    prev_ = *stack_;
    *stack_ = this;
  }
}

template class ScriptSource::PinnedUnits<Utf8Unit>;
template class ScriptSource::PinnedUnits<char16_t>;

JSFlatString* ScriptSource::substring(JSContext* cx, size_t start,
                                      size_t stop) {
  MOZ_ASSERT(start <= stop);

  size_t len = stop - start;
  UncompressedSourceCache::AutoHoldEntry holder;

  // UTF-8 source text.
  if (hasSourceType<Utf8Unit>()) {
    PinnedUnits<Utf8Unit> units(cx, this, holder, start, len);
    if (!units.asChars()) {
      return nullptr;
    }

    const char* str = units.asChars();
    return NewStringCopyUTF8N<CanGC>(cx, JS::UTF8Chars(str, len));
  }

  // UTF-16 source text.
  PinnedUnits<char16_t> units(cx, this, holder, start, len);
  if (!units.asChars()) {
    return nullptr;
  }

  return NewStringCopyN<CanGC>(cx, units.asChars(), len);
}

JSFlatString* ScriptSource::substringDontDeflate(JSContext* cx, size_t start,
                                                 size_t stop) {
  MOZ_ASSERT(start <= stop);

  size_t len = stop - start;
  UncompressedSourceCache::AutoHoldEntry holder;

  // UTF-8 source text.
  if (hasSourceType<Utf8Unit>()) {
    PinnedUnits<Utf8Unit> units(cx, this, holder, start, len);
    if (!units.asChars()) {
      return nullptr;
    }

    const char* str = units.asChars();

    // There doesn't appear to be a non-deflating UTF-8 string creation
    // function -- but then again, it's not entirely clear how current
    // callers benefit from non-deflation.
    return NewStringCopyUTF8N<CanGC>(cx, JS::UTF8Chars(str, len));
  }

  // UTF-16 source text.
  PinnedUnits<char16_t> units(cx, this, holder, start, len);
  if (!units.asChars()) {
    return nullptr;
  }

  return NewStringCopyNDontDeflate<CanGC>(cx, units.asChars(), len);
}

bool ScriptSource::appendSubstring(JSContext* cx, StringBuffer& buf,
                                   size_t start, size_t stop) {
  MOZ_ASSERT(start <= stop);

  size_t len = stop - start;
  UncompressedSourceCache::AutoHoldEntry holder;

  if (hasSourceType<Utf8Unit>()) {
    PinnedUnits<Utf8Unit> pinned(cx, this, holder, start, len);
    if (!pinned.get()) {
      return false;
    }
    if (len > SourceDeflateLimit && !buf.ensureTwoByteChars()) {
      return false;
    }

    const Utf8Unit* units = pinned.get();
    return buf.append(units, len);
  } else {
    PinnedUnits<char16_t> pinned(cx, this, holder, start, len);
    if (!pinned.get()) {
      return false;
    }
    if (len > SourceDeflateLimit && !buf.ensureTwoByteChars()) {
      return false;
    }

    const char16_t* units = pinned.get();
    return buf.append(units, len);
  }
}

JSFlatString* ScriptSource::functionBodyString(JSContext* cx) {
  MOZ_ASSERT(isFunctionBody());

  size_t start =
      parameterListEnd_ + (sizeof(FunctionConstructorMedialSigils) - 1);
  size_t stop = length() - (sizeof(FunctionConstructorFinalBrace) - 1);
  return substring(cx, start, stop);
}

template <typename Unit>
void ScriptSource::setSource(
    typename SourceTypeTraits<Unit>::SharedImmutableString uncompressed) {
  MOZ_ASSERT(data.is<Missing>());
  data = SourceType(Uncompressed<Unit>(std::move(uncompressed)));
}

template <typename Unit>
MOZ_MUST_USE bool ScriptSource::setSource(JSContext* cx,
                                          EntryUnits<Unit>&& source,
                                          size_t length) {
  auto& cache = cx->zone()->runtimeFromAnyThread()->sharedImmutableStrings();

  auto uniqueChars = SourceTypeTraits<Unit>::toCacheable(std::move(source));
  auto deduped = cache.getOrCreate(std::move(uniqueChars), length);
  if (!deduped) {
    ReportOutOfMemory(cx);
    return false;
  }

  setSource<Unit>(std::move(*deduped));
  return true;
}

#if defined(JS_BUILD_BINAST)

MOZ_MUST_USE bool ScriptSource::setBinASTSourceCopy(JSContext* cx,
                                                    const uint8_t* buf,
                                                    size_t len) {
  auto& cache = cx->zone()->runtimeFromAnyThread()->sharedImmutableStrings();
  auto deduped = cache.getOrCreate(reinterpret_cast<const char*>(buf), len);
  if (!deduped) {
    ReportOutOfMemory(cx);
    return false;
  }
  MOZ_ASSERT(data.is<Missing>());
  data = SourceType(BinAST(std::move(*deduped)));
  return true;
}

MOZ_MUST_USE bool ScriptSource::setBinASTSource(JSContext* cx,
                                                UniqueChars&& buf, size_t len) {
  auto& cache = cx->zone()->runtimeFromAnyThread()->sharedImmutableStrings();
  auto deduped = cache.getOrCreate(std::move(buf), len);
  if (!deduped) {
    ReportOutOfMemory(cx);
    return false;
  }
  MOZ_ASSERT(data.is<Missing>());
  data = SourceType(BinAST(std::move(*deduped)));
  return true;
}

const uint8_t* ScriptSource::binASTSource() {
  MOZ_ASSERT(hasBinASTSource());
  return reinterpret_cast<const uint8_t*>(data.as<BinAST>().string.chars());
}

#endif /* JS_BUILD_BINAST */

bool ScriptSource::tryCompressOffThread(JSContext* cx) {
  if (!hasUncompressedSource()) {
    // This excludes already-compressed, missing, and BinAST source.
    return true;
  }

  // There are several cases where source compression is not a good idea:
  //  - If the script is tiny, then compression will save little or no space.
  //  - If there is only one core, then compression will contend with JS
  //    execution (which hurts benchmarketing).
  //
  // Otherwise, enqueue a compression task to be processed when a major
  // GC is requested.

  bool canCompressOffThread = HelperThreadState().cpuCount > 1 &&
                              HelperThreadState().threadCount >= 2 &&
                              CanUseExtraThreads();
  if (length() < ScriptSource::MinimumCompressibleLength ||
      !canCompressOffThread) {
    return true;
  }

  // The SourceCompressionTask needs to record the major GC number for
  // scheduling. If we're parsing off thread, this number is not safe to
  // access.
  //
  // When parsing on the main thread, the attempts made to compress off
  // thread in BytecodeCompiler will succeed.
  //
  // When parsing off-thread, the above attempts will fail and the attempt
  // made in ParseTask::finish will succeed.
  if (!CurrentThreadCanAccessRuntime(cx->runtime())) {
    return true;
  }

  // Heap allocate the task. It will be freed upon compression
  // completing in AttachFinishedCompressedSources.
  auto task = MakeUnique<SourceCompressionTask>(cx->runtime(), this);
  if (!task) {
    ReportOutOfMemory(cx);
    return false;
  }
  return EnqueueOffThreadCompression(cx, std::move(task));
}

template <typename Unit>
void ScriptSource::setCompressedSource(SharedImmutableString raw,
                                       size_t uncompressedLength) {
  MOZ_ASSERT(data.is<Missing>() || hasUncompressedSource());
  MOZ_ASSERT_IF(hasUncompressedSource(), length() == uncompressedLength);

  if (pinnedUnitsStack_) {
    MOZ_ASSERT(pendingCompressed_.empty());
    pendingCompressed_.construct<Compressed<Unit>>(std::move(raw),
                                                   uncompressedLength);
  } else {
    data = SourceType(Compressed<Unit>(std::move(raw), uncompressedLength));
  }
}

template <typename Unit>
MOZ_MUST_USE bool ScriptSource::setCompressedSource(JSContext* cx,
                                                    UniqueChars&& compressed,
                                                    size_t rawLength,
                                                    size_t sourceLength) {
  MOZ_ASSERT(compressed);

  auto& cache = cx->zone()->runtimeFromAnyThread()->sharedImmutableStrings();
  auto deduped = cache.getOrCreate(std::move(compressed), rawLength);
  if (!deduped) {
    ReportOutOfMemory(cx);
    return false;
  }

  setCompressedSource<Unit>(std::move(*deduped), sourceLength);
  return true;
}

template <typename Unit>
bool ScriptSource::setSourceCopy(JSContext* cx, SourceText<Unit>& srcBuf) {
  MOZ_ASSERT(!hasSourceText());

  JSRuntime* runtime = cx->zone()->runtimeFromAnyThread();
  auto& cache = runtime->sharedImmutableStrings();
  auto deduped = cache.getOrCreate(srcBuf.get(), srcBuf.length(), [&srcBuf]() {
    using CharT = typename SourceTypeTraits<Unit>::CharT;
    return srcBuf.ownsUnits()
               ? UniquePtr<CharT[], JS::FreePolicy>(srcBuf.takeChars())
               : DuplicateString(srcBuf.get(), srcBuf.length());
  });
  if (!deduped) {
    ReportOutOfMemory(cx);
    return false;
  }

  setSource<Unit>(std::move(*deduped));
  return true;
}

template bool ScriptSource::setSourceCopy(JSContext* cx,
                                          SourceText<char16_t>& srcBuf);
template bool ScriptSource::setSourceCopy(JSContext* cx,
                                          SourceText<Utf8Unit>& srcBuf);

void ScriptSource::trace(JSTracer* trc) {
#ifdef JS_BUILD_BINAST
  if (binASTMetadata_) {
    binASTMetadata_->trace(trc);
  }
#else
  MOZ_ASSERT(!binASTMetadata_);
#endif  // JS_BUILD_BINAST
}

static MOZ_MUST_USE bool reallocUniquePtr(UniqueChars& unique, size_t size) {
  auto newPtr = static_cast<char*>(js_realloc(unique.get(), size));
  if (!newPtr) {
    return false;
  }

  // Since the realloc succeeded, unique is now holding a freed pointer.
  mozilla::Unused << unique.release();
  unique.reset(newPtr);
  return true;
}

template <typename Unit>
void SourceCompressionTask::workEncodingSpecific() {
  ScriptSource* source = sourceHolder_.get();
  MOZ_ASSERT(source->data.is<ScriptSource::Uncompressed<Unit>>());

  // Try to keep the maximum memory usage down by only allocating half the
  // size of the string, first.
  size_t inputBytes = source->length() * sizeof(Unit);
  size_t firstSize = inputBytes / 2;
  UniqueChars compressed(js_pod_malloc<char>(firstSize));
  if (!compressed) {
    return;
  }

  const Unit* chars =
      source->data.as<ScriptSource::Uncompressed<Unit>>().units();
  Compressor comp(reinterpret_cast<const unsigned char*>(chars), inputBytes);
  if (!comp.init()) {
    return;
  }

  comp.setOutput(reinterpret_cast<unsigned char*>(compressed.get()), firstSize);
  bool cont = true;
  bool reallocated = false;
  while (cont) {
    if (shouldCancel()) {
      return;
    }

    switch (comp.compressMore()) {
      case Compressor::CONTINUE:
        break;
      case Compressor::MOREOUTPUT: {
        if (reallocated) {
          // The compressed string is longer than the original string.
          return;
        }

        // The compressed output is greater than half the size of the
        // original string. Reallocate to the full size.
        if (!reallocUniquePtr(compressed, inputBytes)) {
          return;
        }

        comp.setOutput(reinterpret_cast<unsigned char*>(compressed.get()),
                       inputBytes);
        reallocated = true;
        break;
      }
      case Compressor::DONE:
        cont = false;
        break;
      case Compressor::OOM:
        return;
    }
  }

  size_t totalBytes = comp.totalBytesNeeded();

  // Shrink the buffer to the size of the compressed data.
  if (!reallocUniquePtr(compressed, totalBytes)) {
    return;
  }

  comp.finish(compressed.get(), totalBytes);

  if (shouldCancel()) {
    return;
  }

  auto& strings = runtime_->sharedImmutableStrings();
  resultString_ = strings.getOrCreate(std::move(compressed), totalBytes);
}

struct SourceCompressionTask::PerformTaskWork {
  SourceCompressionTask* const task_;

  explicit PerformTaskWork(SourceCompressionTask* task) : task_(task) {}

  template <typename Unit>
  void match(const ScriptSource::Uncompressed<Unit>&) {
    task_->workEncodingSpecific<Unit>();
  }

  template <typename T>
  void match(const T&) {
    MOZ_CRASH(
        "why are we compressing missing, already-compressed, or "
        "BinAST source?");
  }
};

void ScriptSource::performTaskWork(SourceCompressionTask* task) {
  MOZ_ASSERT(hasUncompressedSource());
  data.match(SourceCompressionTask::PerformTaskWork(task));
}

void SourceCompressionTask::work() {
  if (shouldCancel()) {
    return;
  }

  ScriptSource* source = sourceHolder_.get();
  MOZ_ASSERT(source->hasUncompressedSource());

  source->performTaskWork(this);
}

void ScriptSource::setCompressedSourceFromTask(
    SharedImmutableString compressed) {
  data.match(SetCompressedSourceFromTask(this, compressed));
}

void SourceCompressionTask::complete() {
  if (!shouldCancel() && resultString_.isSome()) {
    ScriptSource* source = sourceHolder_.get();
    source->setCompressedSourceFromTask(std::move(*resultString_));
  }
}

void ScriptSource::addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                          JS::ScriptSourceInfo* info) const {
  info->misc += mallocSizeOf(this) + mallocSizeOf(filename_.get()) +
                mallocSizeOf(introducerFilename_.get());
  info->numScripts++;
}

bool ScriptSource::xdrEncodeTopLevel(JSContext* cx, HandleScript script) {
  // Encoding failures are reported by the xdrFinalizeEncoder function.
  if (containsAsmJS()) {
    return true;
  }

  xdrEncoder_ = js::MakeUnique<XDRIncrementalEncoder>(cx);
  if (!xdrEncoder_) {
    ReportOutOfMemory(cx);
    return false;
  }

  MOZ_ASSERT(hasEncoder());
  auto failureCase =
      mozilla::MakeScopeExit([&] { xdrEncoder_.reset(nullptr); });

  RootedScript s(cx, script);
  XDRResult res = xdrEncoder_->codeScript(&s);
  if (res.isErr()) {
    // On encoding failure, let failureCase destroy encoder and return true
    // to avoid failing any currently executing script.
    if (res.unwrapErr() & JS::TranscodeResult_Failure) {
      return true;
    }

    return false;
  }

  failureCase.release();
  return true;
}

bool ScriptSource::xdrEncodeFunction(JSContext* cx, HandleFunction fun,
                                     HandleScriptSourceObject sourceObject) {
  MOZ_ASSERT(sourceObject->source() == this);
  MOZ_ASSERT(hasEncoder());
  auto failureCase =
      mozilla::MakeScopeExit([&] { xdrEncoder_.reset(nullptr); });

  RootedFunction f(cx, fun);
  XDRResult res = xdrEncoder_->codeFunction(&f, sourceObject);
  if (res.isErr()) {
    // On encoding failure, let failureCase destroy encoder and return true
    // to avoid failing any currently executing script.
    if (res.unwrapErr() & JS::TranscodeResult_Failure) {
      return true;
    }
    return false;
  }

  failureCase.release();
  return true;
}

bool ScriptSource::xdrFinalizeEncoder(JS::TranscodeBuffer& buffer) {
  if (!hasEncoder()) {
    return false;
  }

  auto cleanup = mozilla::MakeScopeExit([&] { xdrEncoder_.reset(nullptr); });

  XDRResult res = xdrEncoder_->linearize(buffer);
  return res.isOk();
}

template <typename Unit>
struct SourceDecoder {
  XDRState<XDR_DECODE>* const xdr_;
  ScriptSource* const scriptSource_;
  const uint32_t uncompressedLength_;

 public:
  SourceDecoder(XDRState<XDR_DECODE>* xdr, ScriptSource* scriptSource,
                uint32_t uncompressedLength)
      : xdr_(xdr),
        scriptSource_(scriptSource),
        uncompressedLength_(uncompressedLength) {}

  XDRResult decode() {
    auto sourceUnits =
        xdr_->cx()->make_pod_array<Unit>(Max<size_t>(uncompressedLength_, 1));
    if (!sourceUnits) {
      return xdr_->fail(JS::TranscodeResult_Throw);
    }

    MOZ_TRY(xdr_->codeChars(sourceUnits.get(), uncompressedLength_));

    if (!scriptSource_->setSource(xdr_->cx(), std::move(sourceUnits),
                                  uncompressedLength_)) {
      return xdr_->fail(JS::TranscodeResult_Throw);
    }

    return Ok();
  }
};

namespace js {

template <>
XDRResult ScriptSource::xdrUncompressedSource<XDR_DECODE>(
    XDRState<XDR_DECODE>* xdr, uint8_t sourceCharSize,
    uint32_t uncompressedLength) {
  MOZ_ASSERT(sourceCharSize == 1 || sourceCharSize == 2);

  if (sourceCharSize == 1) {
    SourceDecoder<Utf8Unit> decoder(xdr, this, uncompressedLength);
    return decoder.decode();
  }

  SourceDecoder<char16_t> decoder(xdr, this, uncompressedLength);
  return decoder.decode();
}

}  // namespace js

template <typename Unit>
struct SourceEncoder {
  XDRState<XDR_ENCODE>* const xdr_;
  ScriptSource* const source_;
  const uint32_t uncompressedLength_;

  SourceEncoder(XDRState<XDR_ENCODE>* xdr, ScriptSource* source,
                uint32_t uncompressedLength)
      : xdr_(xdr), source_(source), uncompressedLength_(uncompressedLength) {}

  XDRResult encode() {
    Unit* sourceUnits = const_cast<Unit*>(source_->uncompressedData<Unit>());

    return xdr_->codeChars(sourceUnits, uncompressedLength_);
  }
};

namespace js {

template <>
XDRResult ScriptSource::xdrUncompressedSource<XDR_ENCODE>(
    XDRState<XDR_ENCODE>* xdr, uint8_t sourceCharSize,
    uint32_t uncompressedLength) {
  MOZ_ASSERT(sourceCharSize == 1 || sourceCharSize == 2);

  if (sourceCharSize == 1) {
    SourceEncoder<Utf8Unit> encoder(xdr, this, uncompressedLength);
    return encoder.encode();
  }

  SourceEncoder<char16_t> encoder(xdr, this, uncompressedLength);
  return encoder.encode();
}

}  // namespace js

template <XDRMode mode>
/* static */
XDRResult ScriptSource::XDR(XDRState<mode>* xdr,
                            const mozilla::Maybe<JS::CompileOptions>& options,
                            MutableHandle<ScriptSourceHolder> holder) {
  JSContext* cx = xdr->cx();
  ScriptSource* ss = nullptr;

  if (mode == XDR_ENCODE) {
    ss = holder.get().get();
  } else {
    // Allocate a new ScriptSource and root it with the holder.
    ss = cx->new_<ScriptSource>();
    if (!ss) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    holder.get().reset(ss);

    // We use this CompileOptions only to initialize the ScriptSourceObject.
    // Most CompileOptions fields aren't used by ScriptSourceObject, and those
    // that are (element; elementAttributeName) aren't preserved by XDR. So
    // this can be simple.
    if (!ss->initFromOptions(cx, *options)) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  uint8_t hasSource = ss->hasSourceText();
  MOZ_TRY(xdr->codeUint8(&hasSource));

  uint8_t hasBinSource = ss->hasBinASTSource();
  MOZ_TRY(xdr->codeUint8(&hasBinSource));

  uint8_t retrievable = ss->sourceRetrievable_;
  MOZ_TRY(xdr->codeUint8(&retrievable));
  ss->sourceRetrievable_ = retrievable;

  if ((hasSource || hasBinSource) && !retrievable) {
    uint32_t uncompressedLength = 0;
    if (mode == XDR_ENCODE) {
      uncompressedLength = ss->length();
    }
    MOZ_TRY(xdr->codeUint32(&uncompressedLength));

    if (hasBinSource) {
      if (mode == XDR_DECODE) {
#if defined(JS_BUILD_BINAST)
        auto bytes = xdr->cx()->template make_pod_array<char>(
            Max<size_t>(uncompressedLength, 1));
        if (!bytes) {
          return xdr->fail(JS::TranscodeResult_Throw);
        }
        MOZ_TRY(xdr->codeBytes(bytes.get(), uncompressedLength));

        if (!ss->setBinASTSource(xdr->cx(), std::move(bytes),
                                 uncompressedLength)) {
          return xdr->fail(JS::TranscodeResult_Throw);
        }
#else
        MOZ_ASSERT(mode != XDR_ENCODE);
        return xdr->fail(JS::TranscodeResult_Throw);
#endif /* JS_BUILD_BINAST */
      } else {
        void* bytes = ss->binASTData();
        MOZ_TRY(xdr->codeBytes(bytes, uncompressedLength));
      }
    } else {
      // A compressed length of 0 indicates source is uncompressed
      uint32_t compressedLength;
      if (mode == XDR_ENCODE) {
        compressedLength = ss->compressedLengthOrZero();
      }
      MOZ_TRY(xdr->codeUint32(&compressedLength));

      uint8_t srcCharSize;
      if (mode == XDR_ENCODE) {
        srcCharSize = ss->sourceCharSize();
      }
      MOZ_TRY(xdr->codeUint8(&srcCharSize));

      if (srcCharSize != 1 && srcCharSize != 2) {
        // Fail in debug, but only soft-fail in release, if the source-char
        // size is invalid.
        MOZ_ASSERT_UNREACHABLE("bad XDR source chars size");
        return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
      }

      if (compressedLength) {
        if (mode == XDR_DECODE) {
          // Compressed data is always single-byte chars.
          auto bytes =
              xdr->cx()->template make_pod_array<char>(compressedLength);
          if (!bytes) {
            return xdr->fail(JS::TranscodeResult_Throw);
          }
          MOZ_TRY(xdr->codeBytes(bytes.get(), compressedLength));

          if (!(srcCharSize == 1 ? ss->setCompressedSource<Utf8Unit>(
                                       xdr->cx(), std::move(bytes),
                                       compressedLength, uncompressedLength)
                                 : ss->setCompressedSource<char16_t>(
                                       xdr->cx(), std::move(bytes),
                                       compressedLength, uncompressedLength))) {
            return xdr->fail(JS::TranscodeResult_Throw);
          }
        } else {
          void* bytes = srcCharSize == 1 ? ss->compressedData<Utf8Unit>()
                                         : ss->compressedData<char16_t>();
          MOZ_TRY(xdr->codeBytes(bytes, compressedLength));
        }
      } else {
        MOZ_TRY(
            ss->xdrUncompressedSource(xdr, srcCharSize, uncompressedLength));
      }
    }

    uint8_t hasMetadata = !!ss->binASTMetadata_;
    MOZ_TRY(xdr->codeUint8(&hasMetadata));
    if (hasMetadata) {
#if defined(JS_BUILD_BINAST)
      UniquePtr<frontend::BinASTSourceMetadata>& binASTMetadata =
          ss->binASTMetadata_;
      uint32_t numBinASTKinds;
      uint32_t numStrings;
      if (mode == XDR_ENCODE) {
        numBinASTKinds = binASTMetadata->numBinASTKinds();
        numStrings = binASTMetadata->numStrings();
      }
      MOZ_TRY(xdr->codeUint32(&numBinASTKinds));
      MOZ_TRY(xdr->codeUint32(&numStrings));

      if (mode == XDR_DECODE) {
        // Use calloc, since we're storing this immediately, and filling it
        // might GC, to avoid marking bogus atoms.
        auto metadata = static_cast<frontend::BinASTSourceMetadata*>(
            js_calloc(frontend::BinASTSourceMetadata::totalSize(numBinASTKinds,
                                                                numStrings)));
        if (!metadata) {
          return xdr->fail(JS::TranscodeResult_Throw);
        }
        new (metadata)
            frontend::BinASTSourceMetadata(numBinASTKinds, numStrings);
        ss->setBinASTSourceMetadata(metadata);
      }

      for (uint32_t i = 0; i < numBinASTKinds; i++) {
        frontend::BinASTKind* binASTKindBase = binASTMetadata->binASTKindBase();
        MOZ_TRY(xdr->codeEnum32(&binASTKindBase[i]));
      }

      RootedAtom atom(xdr->cx());
      JSAtom** atomsBase = binASTMetadata->atomsBase();
      auto slices = binASTMetadata->sliceBase();
      auto sourceBase = reinterpret_cast<const char*>(ss->binASTSource());

      for (uint32_t i = 0; i < numStrings; i++) {
        uint8_t isNull;
        if (mode == XDR_ENCODE) {
          atom = binASTMetadata->getAtom(i);
          isNull = !atom;
        }
        MOZ_TRY(xdr->codeUint8(&isNull));
        if (isNull) {
          atom = nullptr;
        } else {
          MOZ_TRY(XDRAtom(xdr, &atom));
        }
        if (mode == XDR_DECODE) {
          atomsBase[i] = atom;
        }

        uint64_t sliceOffset;
        uint32_t sliceLen;
        if (mode == XDR_ENCODE) {
          auto& slice = binASTMetadata->getSlice(i);
          sliceOffset = slice.begin() - sourceBase;
          sliceLen = slice.byteLen_;
        }

        MOZ_TRY(xdr->codeUint64(&sliceOffset));
        MOZ_TRY(xdr->codeUint32(&sliceLen));

        if (mode == XDR_DECODE) {
          new (&slices[i]) frontend::BinASTSourceMetadata::CharSlice(
              sourceBase + sliceOffset, sliceLen);
        }
      }
#else
      // No BinAST, no BinASTMetadata
      MOZ_ASSERT(mode != XDR_ENCODE);
      return xdr->fail(JS::TranscodeResult_Throw);
#endif  // JS_BUILD_BINAST
    }
  }

  uint8_t haveSourceMap = ss->hasSourceMapURL();
  MOZ_TRY(xdr->codeUint8(&haveSourceMap));

  if (haveSourceMap) {
    UniqueTwoByteChars& sourceMapURL(ss->sourceMapURL_);
    uint32_t sourceMapURLLen =
        (mode == XDR_DECODE) ? 0 : js_strlen(sourceMapURL.get());
    MOZ_TRY(xdr->codeUint32(&sourceMapURLLen));

    if (mode == XDR_DECODE) {
      sourceMapURL =
          xdr->cx()->template make_pod_array<char16_t>(sourceMapURLLen + 1);
      if (!sourceMapURL) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
    }
    auto guard = mozilla::MakeScopeExit([&] {
      if (mode == XDR_DECODE) {
        sourceMapURL = nullptr;
      }
    });
    MOZ_TRY(xdr->codeChars(sourceMapURL.get(), sourceMapURLLen));
    guard.release();
    sourceMapURL[sourceMapURLLen] = '\0';
  }

  uint8_t haveDisplayURL = ss->hasDisplayURL();
  MOZ_TRY(xdr->codeUint8(&haveDisplayURL));

  if (haveDisplayURL) {
    UniqueTwoByteChars& displayURL(ss->displayURL_);
    uint32_t displayURLLen =
        (mode == XDR_DECODE) ? 0 : js_strlen(displayURL.get());
    MOZ_TRY(xdr->codeUint32(&displayURLLen));

    if (mode == XDR_DECODE) {
      displayURL =
          xdr->cx()->template make_pod_array<char16_t>(displayURLLen + 1);
      if (!displayURL) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
    }
    auto guard = mozilla::MakeScopeExit([&] {
      if (mode == XDR_DECODE) {
        displayURL = nullptr;
      }
    });
    MOZ_TRY(xdr->codeChars(displayURL.get(), displayURLLen));
    guard.release();
    displayURL[displayURLLen] = '\0';
  }

  uint8_t haveFilename = !!ss->filename_;
  MOZ_TRY(xdr->codeUint8(&haveFilename));

  if (haveFilename) {
    const char* fn = ss->filename();
    MOZ_TRY(xdr->codeCString(&fn));
    // Note: If the decoder has an option, then the filename is defined by
    // the CompileOption from the document.
    MOZ_ASSERT_IF(mode == XDR_DECODE && xdr->hasOptions(), ss->filename());
    if (mode == XDR_DECODE && !xdr->hasOptions() &&
        !ss->setFilename(xdr->cx(), fn)) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }

    // Note the content of sources decoded when recording or replaying.
    if (mode == XDR_DECODE && ss->hasSourceText() &&
        mozilla::recordreplay::IsRecordingOrReplaying()) {
      UncompressedSourceCache::AutoHoldEntry holder;

      if (ss->hasSourceType<Utf8Unit>()) {
        // UTF-8 source text.
        ScriptSource::PinnedUnits<Utf8Unit> units(xdr->cx(), ss, holder, 0,
                                                  ss->length());
        if (!units.get()) {
          return xdr->fail(JS::TranscodeResult_Throw);
        }
        mozilla::recordreplay::NoteContentParse(ss, ss->filename(),
                                                "application/javascript",
                                                units.get(), ss->length());
      } else {
        // UTF-16 source text.
        ScriptSource::PinnedUnits<char16_t> units(xdr->cx(), ss, holder, 0,
                                                  ss->length());
        if (!units.get()) {
          return xdr->fail(JS::TranscodeResult_Throw);
        }
        mozilla::recordreplay::NoteContentParse(ss, ss->filename(),
                                                "application/javascript",
                                                units.get(), ss->length());
      }
    }
  }

  return Ok();
}

// Format and return a cx->pod_malloc'ed URL for a generated script like:
//   {filename} line {lineno} > {introducer}
// For example:
//   foo.js line 7 > eval
// indicating code compiled by the call to 'eval' on line 7 of foo.js.
char* js::FormatIntroducedFilename(JSContext* cx, const char* filename,
                                   unsigned lineno, const char* introducer) {
  // Compute the length of the string in advance, so we can allocate a
  // buffer of the right size on the first shot.
  //
  // (JS_smprintf would be perfect, as that allocates the result
  // dynamically as it formats the string, but it won't allocate from cx,
  // and wants us to use a special free function.)
  char linenoBuf[15];
  size_t filenameLen = strlen(filename);
  size_t linenoLen = SprintfLiteral(linenoBuf, "%u", lineno);
  size_t introducerLen = strlen(introducer);
  size_t len = filenameLen + 6 /* == strlen(" line ") */ + linenoLen +
               3 /* == strlen(" > ") */ + introducerLen + 1 /* \0 */;
  char* formatted = cx->pod_malloc<char>(len);
  if (!formatted) {
    return nullptr;
  }

  mozilla::DebugOnly<size_t> checkLen = snprintf(
      formatted, len, "%s line %s > %s", filename, linenoBuf, introducer);
  MOZ_ASSERT(checkLen == len - 1);

  return formatted;
}

bool ScriptSource::initFromOptions(JSContext* cx,
                                   const ReadOnlyCompileOptions& options,
                                   const Maybe<uint32_t>& parameterListEnd) {
  MOZ_ASSERT(!filename_);
  MOZ_ASSERT(!introducerFilename_);

  mutedErrors_ = options.mutedErrors();

  introductionType_ = options.introductionType;
  setIntroductionOffset(options.introductionOffset);
  parameterListEnd_ = parameterListEnd.isSome() ? parameterListEnd.value() : 0;

  if (options.hasIntroductionInfo) {
    MOZ_ASSERT(options.introductionType != nullptr);
    const char* filename =
        options.filename() ? options.filename() : "<unknown>";
    char* formatted = FormatIntroducedFilename(
        cx, filename, options.introductionLineno, options.introductionType);
    if (!formatted) {
      return false;
    }
    filename_.reset(formatted);
  } else if (options.filename()) {
    if (!setFilename(cx, options.filename())) {
      return false;
    }
  }

  if (options.introducerFilename()) {
    introducerFilename_ = DuplicateString(cx, options.introducerFilename());
    if (!introducerFilename_) {
      return false;
    }
  }

  return true;
}

bool ScriptSource::setFilename(JSContext* cx, const char* filename) {
  MOZ_ASSERT(!filename_);
  filename_ = DuplicateString(cx, filename);
  return filename_ != nullptr;
}

bool ScriptSource::setDisplayURL(JSContext* cx, const char16_t* displayURL) {
  MOZ_ASSERT(displayURL);
  if (hasDisplayURL()) {
    // FIXME: filename_.get() should be UTF-8 (bug 987069).
    if (!cx->helperThread() &&
        !JS_ReportErrorFlagsAndNumberLatin1(
            cx, JSREPORT_WARNING, GetErrorMessage, nullptr,
            JSMSG_ALREADY_HAS_PRAGMA, filename_.get(), "//# sourceURL")) {
      return false;
    }
  }
  size_t len = js_strlen(displayURL) + 1;
  if (len == 1) {
    return true;
  }

  displayURL_ = DuplicateString(cx, displayURL);
  return displayURL_ != nullptr;
}

bool ScriptSource::setSourceMapURL(JSContext* cx,
                                   const char16_t* sourceMapURL) {
  MOZ_ASSERT(sourceMapURL);

  size_t len = js_strlen(sourceMapURL) + 1;
  if (len == 1) {
    return true;
  }

  sourceMapURL_ = DuplicateString(cx, sourceMapURL);
  return sourceMapURL_ != nullptr;
}

/* static */ mozilla::Atomic<uint32_t, mozilla::SequentiallyConsistent,
                             mozilla::recordreplay::Behavior::DontPreserve>
    ScriptSource::idCount_;

/*
 * [SMDOC] JSScript data layout (shared)
 *
 * Shared script data management.
 *
 * SharedScriptData::data contains data that can be shared within a
 * runtime. The atoms() data is placed first to simplify its alignment.
 *
 * Array elements   Pointed to by         Length
 * --------------   -------------         ------
 * GCPtrAtom        atoms()               natoms()
 * jsbytecode       code()                codeLength()
 * jsscrnote        notes()               numNotes()
 */

SharedScriptData* js::SharedScriptData::new_(JSContext* cx, uint32_t codeLength,
                                             uint32_t noteLength,
                                             uint32_t natoms) {
  // Compute size including trailing arrays
  size_t size = AllocationSize(codeLength, noteLength, natoms);

  // Allocate contiguous raw buffer
  void* raw = cx->pod_malloc<uint8_t>(size);
  MOZ_ASSERT(uintptr_t(raw) % alignof(SharedScriptData) == 0);
  if (!raw) {
    return nullptr;
  }

  // Constuct the SharedScriptData. Trailing arrays are uninitialized but
  // GCPtrs are put into a safe state.
  return new (raw) SharedScriptData(codeLength, noteLength, natoms);
}

inline js::ScriptBytecodeHasher::Lookup::Lookup(SharedScriptData* data)
    : scriptData(data),
      hash(mozilla::HashBytes(scriptData->data(), scriptData->dataLength())) {}

bool JSScript::createSharedScriptData(JSContext* cx, uint32_t codeLength,
                                      uint32_t noteLength, uint32_t natoms) {
  MOZ_ASSERT(!scriptData_);
  scriptData_ = SharedScriptData::new_(cx, codeLength, noteLength, natoms);
  return !!scriptData_;
}

void JSScript::freeScriptData() { scriptData_ = nullptr; }

/*
 * Takes ownership of its *ssd parameter and either adds it into the runtime's
 * ScriptDataTable or frees it if a matching entry already exists.
 *
 * Sets the |code| and |atoms| fields on the given JSScript.
 */
bool JSScript::shareScriptData(JSContext* cx) {
  SharedScriptData* ssd = scriptData();
  MOZ_ASSERT(ssd);
  MOZ_ASSERT(ssd->refCount() == 1);

  // Calculate the hash before taking the lock. Because the data is reference
  // counted, it also will be freed after releasing the lock if necessary.
  ScriptBytecodeHasher::Lookup lookup(ssd);

  AutoLockScriptData lock(cx->runtime());

  ScriptDataTable::AddPtr p = cx->scriptDataTable(lock).lookupForAdd(lookup);
  if (p) {
    MOZ_ASSERT(ssd != *p);
    scriptData_ = *p;
  } else {
    if (!cx->scriptDataTable(lock).add(p, ssd)) {
      ReportOutOfMemory(cx);
      return false;
    }

    // Being in the table counts as a reference on the script data.
    ssd->AddRef();
  }

  // Refs: JSScript, ScriptDataTable
  MOZ_ASSERT(scriptData()->refCount() >= 2);

  return true;
}

void js::SweepScriptData(JSRuntime* rt) {
  // Entries are removed from the table when their reference count is one,
  // i.e. when the only reference to them is from the table entry.

  AutoLockScriptData lock(rt);
  ScriptDataTable& table = rt->scriptDataTable(lock);

  for (ScriptDataTable::Enum e(table); !e.empty(); e.popFront()) {
    SharedScriptData* scriptData = e.front();
    if (scriptData->refCount() == 1) {
      scriptData->Release();
      e.removeFront();
    }
  }
}

void js::FreeScriptData(JSRuntime* rt) {
  AutoLockScriptData lock(rt);

  ScriptDataTable& table = rt->scriptDataTable(lock);

  // The table should be empty unless the embedding leaked GC things.
  MOZ_ASSERT_IF(rt->gc.shutdownCollectedEverything(), table.empty());

  for (ScriptDataTable::Enum e(table); !e.empty(); e.popFront()) {
#ifdef DEBUG
    SharedScriptData* scriptData = e.front();
    fprintf(stderr,
            "ERROR: GC found live SharedScriptData %p with ref count %d at "
            "shutdown\n",
            scriptData, scriptData->refCount());
#endif
    js_free(e.front());
  }

  table.clear();
}

/* static */
size_t PrivateScriptData::AllocationSize(uint32_t nscopes, uint32_t nconsts,
                                         uint32_t nobjects, uint32_t ntrynotes,
                                         uint32_t nscopenotes,
                                         uint32_t nresumeoffsets) {
  size_t size = sizeof(PrivateScriptData);

  if (nconsts) {
    size += sizeof(PackedSpan);
  }
  if (nobjects) {
    size += sizeof(PackedSpan);
  }
  if (ntrynotes) {
    size += sizeof(PackedSpan);
  }
  if (nscopenotes) {
    size += sizeof(PackedSpan);
  }
  if (nresumeoffsets) {
    size += sizeof(PackedSpan);
  }

  size += nscopes * sizeof(GCPtrScope);

  if (nconsts) {
    // The scope array doesn't maintain Value alignment, so compute the
    // padding needed to remedy this.
    size = JS_ROUNDUP(size, alignof(GCPtrValue));
    size += nconsts * sizeof(GCPtrValue);
  }
  if (nobjects) {
    size += nobjects * sizeof(GCPtrObject);
  }
  if (ntrynotes) {
    size += ntrynotes * sizeof(JSTryNote);
  }
  if (nscopenotes) {
    size += nscopenotes * sizeof(ScopeNote);
  }
  if (nresumeoffsets) {
    size += nresumeoffsets * sizeof(uint32_t);
  }

  return size;
}

// Placement-new elements of an array. This should optimize away for types with
// trivial default initiation.
template <typename T>
void PrivateScriptData::initElements(size_t offset, size_t length) {
  uintptr_t base = reinterpret_cast<uintptr_t>(this);
  DefaultInitializeElements<T>(reinterpret_cast<void*>(base + offset), length);
}

template <typename T>
void PrivateScriptData::initSpan(size_t* cursor, uint32_t scaledSpanOffset,
                                 size_t length) {
  // PackedSpans are elided when arrays are empty
  if (scaledSpanOffset == 0) {
    MOZ_ASSERT(length == 0);
    return;
  }

  // Placement-new the PackedSpan
  PackedSpan* span = packedOffsetToPointer<PackedSpan>(scaledSpanOffset);
  span = new (span) PackedSpan{uint32_t(*cursor), uint32_t(length)};

  // Placement-new the elements
  initElements<T>(*cursor, length);

  // Advance cursor
  (*cursor) += length * sizeof(T);
}

// Initialize PackedSpans and placement-new the trailing arrays.
PrivateScriptData::PrivateScriptData(uint32_t nscopes_, uint32_t nconsts,
                                     uint32_t nobjects, uint32_t ntrynotes,
                                     uint32_t nscopenotes,
                                     uint32_t nresumeoffsets)
    : nscopes(nscopes_) {
  // Convert cursor possition to a packed offset.
  auto ToPackedOffset = [](size_t cursor) {
    MOZ_ASSERT(cursor % PackedOffsets::SCALE == 0);
    return cursor / PackedOffsets::SCALE;
  };

  // Helper to allocate a PackedSpan from the variable length data.
  auto TakeSpan = [=](size_t* cursor) {
    size_t packedOffset = ToPackedOffset(*cursor);
    MOZ_ASSERT(packedOffset <= PackedOffsets::MAX_OFFSET);

    (*cursor) += sizeof(PackedSpan);
    return packedOffset;
  };

  // Variable-length data begins immediately after PrivateScriptData itself.
  // NOTE: Alignment is computed using cursor/offset so the alignment of
  // PrivateScriptData must be stricter than any trailing array type.
  size_t cursor = sizeof(*this);

  // Layout PackedSpan structures and initialize packedOffsets fields.
  static_assert(alignof(PrivateScriptData) >= alignof(PackedSpan),
                "Incompatible alignment");
  if (nconsts) {
    packedOffsets.constsSpanOffset = TakeSpan(&cursor);
  }
  if (nobjects) {
    packedOffsets.objectsSpanOffset = TakeSpan(&cursor);
  }
  if (ntrynotes) {
    packedOffsets.tryNotesSpanOffset = TakeSpan(&cursor);
  }
  if (nscopenotes) {
    packedOffsets.scopeNotesSpanOffset = TakeSpan(&cursor);
  }
  if (nresumeoffsets) {
    packedOffsets.resumeOffsetsSpanOffset = TakeSpan(&cursor);
  }

  // Layout and initialize the scopes array. Manually insert padding so that
  // the subsequent |consts| array is aligned.
  {
    MOZ_ASSERT(nscopes > 0);

    static_assert(alignof(PackedSpan) >= alignof(GCPtrScope),
                  "Incompatible alignment");
    initElements<GCPtrScope>(cursor, nscopes);
    packedOffsets.scopesOffset = ToPackedOffset(cursor);

    cursor += nscopes * sizeof(GCPtrScope);
  }

  if (nconsts) {
    // Pad to required alignment if we are emitting constant array.
    cursor = JS_ROUNDUP(cursor, alignof(GCPtrValue));

    static_assert(alignof(PrivateScriptData) >= alignof(GCPtrValue),
                  "Incompatible alignment");
    initSpan<GCPtrValue>(&cursor, packedOffsets.constsSpanOffset, nconsts);
  }

  // Layout arrays, initialize PackedSpans and placement-new the elements.
  static_assert(alignof(GCPtrValue) >= alignof(GCPtrObject),
                "Incompatible alignment");
  static_assert(alignof(GCPtrScope) >= alignof(GCPtrObject),
                "Incompatible alignment");
  initSpan<GCPtrObject>(&cursor, packedOffsets.objectsSpanOffset, nobjects);
  static_assert(alignof(GCPtrObject) >= alignof(JSTryNote),
                "Incompatible alignment");
  initSpan<JSTryNote>(&cursor, packedOffsets.tryNotesSpanOffset, ntrynotes);
  static_assert(alignof(JSTryNote) >= alignof(ScopeNote),
                "Incompatible alignment");
  initSpan<ScopeNote>(&cursor, packedOffsets.scopeNotesSpanOffset, nscopenotes);
  static_assert(alignof(ScopeNote) >= alignof(uint32_t),
                "Incompatible alignment");
  initSpan<uint32_t>(&cursor, packedOffsets.resumeOffsetsSpanOffset,
                     nresumeoffsets);

  // Sanity check
  MOZ_ASSERT(AllocationSize(nscopes_, nconsts, nobjects, ntrynotes, nscopenotes,
                            nresumeoffsets) == cursor);
}

/* static */
PrivateScriptData* PrivateScriptData::new_(JSContext* cx, uint32_t nscopes,
                                           uint32_t nconsts, uint32_t nobjects,
                                           uint32_t ntrynotes,
                                           uint32_t nscopenotes,
                                           uint32_t nresumeoffsets,
                                           uint32_t* dataSize) {
  // Compute size including trailing arrays
  size_t size = AllocationSize(nscopes, nconsts, nobjects, ntrynotes,
                               nscopenotes, nresumeoffsets);

  // Allocate contiguous raw buffer
  void* raw = cx->pod_malloc<uint8_t>(size);
  MOZ_ASSERT(uintptr_t(raw) % alignof(PrivateScriptData) == 0);
  if (!raw) {
    return nullptr;
  }

  if (dataSize) {
    *dataSize = size;
  }

  // Constuct the PrivateScriptData. Trailing arrays are uninitialized but
  // GCPtrs are put into a safe state.
  return new (raw) PrivateScriptData(nscopes, nconsts, nobjects, ntrynotes,
                                     nscopenotes, nresumeoffsets);
}

/* static */ bool PrivateScriptData::InitFromEmitter(
    JSContext* cx, js::HandleScript script, frontend::BytecodeEmitter* bce) {
  uint32_t nscopes = bce->scopeList.length();
  uint32_t nconsts = bce->numberList.length();
  uint32_t nobjects = bce->objectList.length;
  uint32_t ntrynotes = bce->tryNoteList.length();
  uint32_t nscopenotes = bce->scopeNoteList.length();
  uint32_t nresumeoffsets = bce->resumeOffsetList.length();

  // Create and initialize PrivateScriptData
  if (!JSScript::createPrivateScriptData(cx, script, nscopes, nconsts, nobjects,
                                         ntrynotes, nscopenotes,
                                         nresumeoffsets)) {
    return false;
  }

  js::PrivateScriptData* data = script->data_;
  if (nscopes) {
    bce->scopeList.finish(data->scopes());
  }
  if (nconsts) {
    bce->numberList.finish(data->consts());
  }
  if (nobjects) {
    bce->objectList.finish(data->objects());
  }
  if (ntrynotes) {
    bce->tryNoteList.finish(data->tryNotes());
  }
  if (nscopenotes) {
    bce->scopeNoteList.finish(data->scopeNotes());
  }
  if (nresumeoffsets) {
    bce->resumeOffsetList.finish(data->resumeOffsets());
  }

  return true;
}

void PrivateScriptData::trace(JSTracer* trc) {
  auto scopearray = scopes();
  TraceRange(trc, scopearray.size(), scopearray.data(), "scopes");

  if (hasConsts()) {
    auto constarray = consts();
    TraceRange(trc, constarray.size(), constarray.data(), "consts");
  }

  if (hasObjects()) {
    auto objarray = objects();
    TraceRange(trc, objarray.size(), objarray.data(), "objects");
  }
}

JSScript::JSScript(JS::Realm* realm, uint8_t* stubEntry,
                   HandleScriptSourceObject sourceObject, uint32_t sourceStart,
                   uint32_t sourceEnd, uint32_t toStringStart,
                   uint32_t toStringEnd)
    :
#ifndef JS_CODEGEN_NONE
      jitCodeRaw_(stubEntry),
      jitCodeSkipArgCheck_(stubEntry),
#endif
      realm_(realm),
      sourceStart_(sourceStart),
      sourceEnd_(sourceEnd),
      toStringStart_(toStringStart),
      toStringEnd_(toStringEnd) {
  // See JSScript.h for further details.
  MOZ_ASSERT(toStringStart <= sourceStart);
  MOZ_ASSERT(sourceStart <= sourceEnd);
  MOZ_ASSERT(sourceEnd <= toStringEnd);

  setSourceObject(sourceObject);
}

/* static */
JSScript* JSScript::New(JSContext* cx, HandleScriptSourceObject sourceObject,
                        uint32_t sourceStart, uint32_t sourceEnd,
                        uint32_t toStringStart, uint32_t toStringEnd) {
  void* script = Allocate<JSScript>(cx);
  if (!script) {
    return nullptr;
  }

#ifndef JS_CODEGEN_NONE
  uint8_t* stubEntry = cx->runtime()->jitRuntime()->interpreterStub().value;
#else
  uint8_t* stubEntry = nullptr;
#endif

  return new (script)
      JSScript(cx->realm(), stubEntry, sourceObject, sourceStart, sourceEnd,
               toStringStart, toStringEnd);
}

static bool ShouldTrackRecordReplayProgress(JSScript* script) {
  // Progress is only tracked when recording or replaying, and only for
  // scripts associated with the main thread's runtime. Whether self hosted
  // scripts execute may depend on performed Ion optimizations (for example,
  // self hosted TypedObject logic), so they are ignored.
  return MOZ_UNLIKELY(mozilla::recordreplay::IsRecordingOrReplaying()) &&
         !script->runtimeFromAnyThread()->parentRuntime &&
         !script->selfHosted() &&
         mozilla::recordreplay::ShouldUpdateProgressCounter(script->filename());
}

/* static */
JSScript* JSScript::Create(JSContext* cx, const ReadOnlyCompileOptions& options,
                           HandleScriptSourceObject sourceObject,
                           uint32_t sourceStart, uint32_t sourceEnd,
                           uint32_t toStringStart, uint32_t toStringEnd) {
  RootedScript script(cx, JSScript::New(cx, sourceObject, sourceStart,
                                        sourceEnd, toStringStart, toStringEnd));
  if (!script) {
    return nullptr;
  }

  // Record compile options that get checked at runtime.
  script->setFlag(ImmutableFlags::NoScriptRval, options.noScriptRval);
  script->setFlag(ImmutableFlags::SelfHosted, options.selfHostingMode);
  script->setFlag(ImmutableFlags::TreatAsRunOnce, options.isRunOnce);
  script->setFlag(MutableFlags::HideScriptFromDebugger,
                  options.hideScriptFromDebugger);

  script->setFlag(MutableFlags::TrackRecordReplayProgress,
                  ShouldTrackRecordReplayProgress(script));

  if (cx->runtime()->lcovOutput().isEnabled()) {
    if (!script->initScriptName(cx)) {
      return nullptr;
    }
  }

  return script;
}

#ifdef MOZ_VTUNE
uint32_t JSScript::vtuneMethodID() {
  if (!realm()->scriptVTuneIdMap) {
    auto map = MakeUnique<ScriptVTuneIdMap>();
    if (!map) {
      MOZ_CRASH("Failed to allocate ScriptVTuneIdMap");
    }

    realm()->scriptVTuneIdMap = std::move(map);
  }

  ScriptVTuneIdMap::AddPtr p = realm()->scriptVTuneIdMap->lookupForAdd(this);
  if (p) {
    return p->value();
  }

  uint32_t id = vtune::GenerateUniqueMethodID();
  if (!realm()->scriptVTuneIdMap->add(p, this, id)) {
    MOZ_CRASH("Failed to add vtune method id");
  }

  return id;
}
#endif

bool JSScript::initScriptName(JSContext* cx) {
  MOZ_ASSERT(!hasScriptName());

  if (!filename()) {
    return true;
  }

  // Create realm's scriptNameMap if necessary.
  if (!realm()->scriptNameMap) {
    auto map = cx->make_unique<ScriptNameMap>();
    if (!map) {
      return false;
    }

    realm()->scriptNameMap = std::move(map);
  }

  UniqueChars name = DuplicateString(filename());
  if (!name) {
    ReportOutOfMemory(cx);
    return false;
  }

  // Register the script name in the realm's map.
  if (!realm()->scriptNameMap->putNew(this, std::move(name))) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

/* static */
bool JSScript::createPrivateScriptData(JSContext* cx, HandleScript script,
                                       uint32_t nscopes, uint32_t nconsts,
                                       uint32_t nobjects, uint32_t ntrynotes,
                                       uint32_t nscopenotes,
                                       uint32_t nresumeoffsets) {
  cx->check(script);

  uint32_t dataSize;

  PrivateScriptData* data =
      PrivateScriptData::new_(cx, nscopes, nconsts, nobjects, ntrynotes,
                              nscopenotes, nresumeoffsets, &dataSize);
  if (!data) {
    return false;
  }

  script->data_ = data;
  script->dataSize_ = dataSize;
  return true;
}

/* static */
bool JSScript::initFunctionPrototype(JSContext* cx, HandleScript script,
                                     HandleFunction functionProto) {
  uint32_t numScopes = 1;
  uint32_t numConsts = 0;
  uint32_t numObjects = 0;
  uint32_t numTryNotes = 0;
  uint32_t numScopeNotes = 0;
  uint32_t nresumeoffsets = 0;
  if (!createPrivateScriptData(cx, script, numScopes, numConsts, numObjects,
                               numTryNotes, numScopeNotes, nresumeoffsets)) {
    return false;
  }

  script->numBytecodeTypeSets_ = 0;

  RootedScope enclosing(cx, &cx->global()->emptyGlobalScope());
  Scope* functionProtoScope = FunctionScope::create(cx, nullptr, false, false,
                                                    functionProto, enclosing);
  if (!functionProtoScope) {
    return false;
  }

  mozilla::Span<GCPtrScope> scopes = script->data_->scopes();
  scopes[0].init(functionProtoScope);

  uint32_t codeLength = 1;
  uint32_t noteLength = 1;
  uint32_t numAtoms = 0;
  if (!script->createSharedScriptData(cx, codeLength, noteLength, numAtoms)) {
    return false;
  }

  jsbytecode* code = script->scriptData_->code();
  code[0] = JSOP_RETRVAL;

  jssrcnote* notes = script->scriptData_->notes();
  notes[0] = SRC_NULL;

  return script->shareScriptData(cx);
}

static void InitAtomMap(frontend::AtomIndexMap& indices, GCPtrAtom* atoms) {
  for (frontend::AtomIndexMap::Range r = indices.all(); !r.empty();
       r.popFront()) {
    JSAtom* atom = r.front().key();
    uint32_t index = r.front().value();
    MOZ_ASSERT(index < indices.count());
    atoms[index].init(atom);
  }
}

static bool NeedsFunctionEnvironmentObjects(frontend::BytecodeEmitter* bce) {
  // See JSFunction::needsCallObject()
  js::Scope* bodyScope = bce->bodyScope();
  if (bodyScope->kind() == js::ScopeKind::Function) {
    if (bodyScope->hasEnvironment()) {
      return true;
    }
  }

  // See JSScript::maybeNamedLambdaScope()
  js::Scope* outerScope = bce->outermostScope();
  if (outerScope->kind() == js::ScopeKind::NamedLambda ||
      outerScope->kind() == js::ScopeKind::StrictNamedLambda) {
    MOZ_ASSERT(bce->sc->asFunctionBox()->function()->isNamedLambda());

    if (outerScope->hasEnvironment()) {
      return true;
    }
  }

  return false;
}

void JSScript::initFromFunctionBox(frontend::FunctionBox* funbox) {
  funLength_ = funbox->length;

  setFlag(ImmutableFlags::FunHasExtensibleScope, funbox->hasExtensibleScope());
  setFlag(ImmutableFlags::NeedsHomeObject, funbox->needsHomeObject());
  setFlag(ImmutableFlags::IsDerivedClassConstructor,
          funbox->isDerivedClassConstructor());
  setFlag(ImmutableFlags::HasMappedArgsObj, funbox->hasMappedArgsObj());
  setFlag(ImmutableFlags::FunctionHasThisBinding, funbox->hasThisBinding());
  setFlag(ImmutableFlags::FunctionHasExtraBodyVarScope,
          funbox->hasExtraBodyVarScope());
  setFlag(ImmutableFlags::IsGenerator, funbox->isGenerator());
  setFlag(ImmutableFlags::IsAsync, funbox->isAsync());
  setFlag(ImmutableFlags::HasRest, funbox->hasRest());
  setFlag(ImmutableFlags::HasInnerFunctions, funbox->hasInnerFunctions());

  if (funbox->argumentsHasLocalBinding()) {
    setArgumentsHasVarBinding();
    if (funbox->definitelyNeedsArgsObj()) {
      setNeedsArgsObj(true);
    }
  } else {
    MOZ_ASSERT(!funbox->definitelyNeedsArgsObj());
  }
}

/* static */
bool JSScript::fullyInitFromEmitter(JSContext* cx, HandleScript script,
                                    frontend::BytecodeEmitter* bce) {
  MOZ_ASSERT(!script->data_, "JSScript already initialized");

  // If initialization fails, we must call JSScript::freeScriptData in order to
  // neuter the script. Various things that iterate raw scripts in a GC arena
  // use the presense of this data to detect if initialization is complete.
  auto scriptDataGuard =
      mozilla::MakeScopeExit([&] { script->freeScriptData(); });

  /* The counts of indexed things must be checked during code generation. */
  MOZ_ASSERT(bce->atomIndices->count() <= INDEX_LIMIT);
  MOZ_ASSERT(bce->objectList.length <= INDEX_LIMIT);

  uint64_t nslots =
      bce->maxFixedSlots + static_cast<uint64_t>(bce->maxStackDepth);
  if (nslots > UINT32_MAX) {
    bce->reportError(nullptr, JSMSG_NEED_DIET, js_script_str);
    return false;
  }

  // Initialize POD fields
  script->lineno_ = bce->firstLine;
  script->mainOffset_ = bce->mainOffset();
  script->nfixed_ = bce->maxFixedSlots;
  script->nslots_ = nslots;
  script->bodyScopeIndex_ = bce->bodyScopeIndex;
  script->numBytecodeTypeSets_ = bce->typesetCount;

  // Initialize script flags from BytecodeEmitter
  script->setFlag(ImmutableFlags::Strict, bce->sc->strict());
  script->setFlag(ImmutableFlags::BindingsAccessedDynamically,
                  bce->sc->bindingsAccessedDynamically());
  script->setFlag(ImmutableFlags::HasSingletons, bce->hasSingletons);
  script->setFlag(ImmutableFlags::IsForEval, bce->sc->isEvalContext());
  script->setFlag(ImmutableFlags::IsModule, bce->sc->isModuleContext());
  script->setFlag(ImmutableFlags::HasNonSyntacticScope,
                  bce->outermostScope()->hasOnChain(ScopeKind::NonSyntactic));
  script->setFlag(ImmutableFlags::NeedsFunctionEnvironmentObjects,
                  NeedsFunctionEnvironmentObjects(bce));

  // Initialize script flags from FunctionBox
  if (bce->sc->isFunctionBox()) {
    script->initFromFunctionBox(bce->sc->asFunctionBox());
  }

  // Create and initialize PrivateScriptData
  if (!PrivateScriptData::InitFromEmitter(cx, script, bce)) {
    return false;
  }

  // Create and initialize SharedScriptData
  if (!SharedScriptData::InitFromEmitter(cx, script, bce)) {
    return false;
  }
  if (!script->shareScriptData(cx)) {
    return false;
  }

  // NOTE: JSScript is now constructed and should be linked in.

  // Link JSFunction to this JSScript.
  if (bce->sc->isFunctionBox()) {
    JSFunction* fun = bce->sc->asFunctionBox()->function();
    if (fun->isInterpretedLazy()) {
      fun->setUnlazifiedScript(script);
    } else {
      fun->setScript(script);
    }
  }

  // Part of the parse result – the scope containing each inner function – must
  // be stored in the inner function itself. Do this now that compilation is
  // complete and can no longer fail.
  bce->objectList.finishInnerFunctions();

#ifdef JS_STRUCTURED_SPEW
  // We want this to happen after line number initialization to allow filtering
  // to work.
  script->setSpewEnabled(StructuredSpewer::enabled(script));
#endif

#ifdef DEBUG
  script->assertValidJumpTargets();
#endif

  scriptDataGuard.release();
  return true;
}

#ifdef DEBUG
void JSScript::assertValidJumpTargets() const {
  BytecodeLocation mainLoc = mainLocation();
  BytecodeLocation endLoc = endLocation();
  AllBytecodesIterable iter(this);
  for (BytecodeLocation loc : iter) {
    // Check jump instructions' target.
    if (loc.isJump()) {
      BytecodeLocation target = loc.getJumpTarget();
      MOZ_ASSERT(mainLoc <= target && target < endLoc);
      MOZ_ASSERT(target.isJumpTarget());

      // Check fallthrough of conditional jump instructions.
      if (loc.fallsThrough()) {
        BytecodeLocation fallthrough = loc.next();
        MOZ_ASSERT(mainLoc <= fallthrough && fallthrough < endLoc);
        MOZ_ASSERT(fallthrough.isJumpTarget());
      }
    }

    // Check table switch case labels.
    if (loc.is(JSOP_TABLESWITCH)) {
      BytecodeLocation target = loc.getJumpTarget();

      // Default target.
      MOZ_ASSERT(mainLoc <= target && target < endLoc);
      MOZ_ASSERT(target.isJumpTarget());

      int32_t low = loc.getTableSwitchLow();
      int32_t high = loc.getTableSwitchHigh();

      for (int i = 0; i < high - low + 1; i++) {
        BytecodeLocation switchCase(this,
                                    tableSwitchCasePC(loc.toRawBytecode(), i));
        MOZ_ASSERT(mainLoc <= switchCase && switchCase < endLoc);
        MOZ_ASSERT(switchCase.isJumpTarget());
      }
    }
  }

  // Check catch/finally blocks as jump targets.
  if (hasTrynotes()) {
    for (const JSTryNote& tn : trynotes()) {
      jsbytecode* end = codeEnd();
      jsbytecode* mainEntry = main();

      jsbytecode* tryStart = offsetToPC(tn.start);
      jsbytecode* tryPc = tryStart - 1;
      if (tn.kind != JSTRY_CATCH && tn.kind != JSTRY_FINALLY) {
        continue;
      }

      MOZ_ASSERT(JSOp(*tryPc) == JSOP_TRY);
      jsbytecode* tryTarget = tryStart + tn.length;
      MOZ_ASSERT(mainEntry <= tryTarget && tryTarget < end);
      MOZ_ASSERT(BytecodeIsJumpTarget(JSOp(*tryTarget)));
    }
  }
}
#endif

size_t JSScript::computedSizeOfData() const { return dataSize(); }

size_t JSScript::sizeOfData(mozilla::MallocSizeOf mallocSizeOf) const {
  return mallocSizeOf(data_);
}

size_t JSScript::sizeOfTypeScript(mozilla::MallocSizeOf mallocSizeOf) const {
  return types_ ? types_->sizeOfIncludingThis(mallocSizeOf) : 0;
}

js::GlobalObject& JSScript::uninlinedGlobal() const { return global(); }

void JSScript::finalize(FreeOp* fop) {
  // NOTE: this JSScript may be partially initialized at this point.  E.g. we
  // may have created it and partially initialized it with
  // JSScript::Create(), but not yet finished initializing it with
  // fullyInitFromEmitter() or fullyInitTrivial().

  // Collect code coverage information for this script and all its inner
  // scripts, and store the aggregated information on the realm.
  MOZ_ASSERT_IF(hasScriptName(), fop->runtime()->lcovOutput().isEnabled());
  if (fop->runtime()->lcovOutput().isEnabled() && hasScriptName()) {
    realm()->lcovOutput.collectCodeCoverageInfo(realm(), this, getScriptName());
    destroyScriptName();
  }

  fop->runtime()->geckoProfiler().onScriptFinalized(this);

  if (types_) {
    types_->destroy(zone());
  }

  jit::DestroyJitScripts(fop, this);

  destroyScriptCounts();
  destroyDebugScript(fop);

#ifdef MOZ_VTUNE
  if (realm()->scriptVTuneIdMap) {
    // Note: we should only get here if the VTune JIT profiler is running.
    realm()->scriptVTuneIdMap->remove(this);
  }
#endif

  if (data_) {
    AlwaysPoison(data_, 0xdb, computedSizeOfData(), MemCheckKind::MakeNoAccess);
    fop->free_(data_);
  }

  freeScriptData();

  // In most cases, our LazyScript's script pointer will reference this
  // script, and thus be nulled out by normal weakref processing. However, if
  // we unlazified the LazyScript during incremental sweeping, it will have a
  // completely different JSScript.
  MOZ_ASSERT_IF(
      lazyScript && !IsAboutToBeFinalizedUnbarriered(&lazyScript),
      !lazyScript->hasScript() || lazyScript->maybeScriptUnbarriered() != this);
}

static const uint32_t GSN_CACHE_THRESHOLD = 100;

void GSNCache::purge() {
  code = nullptr;
  map.clearAndCompact();
}

jssrcnote* js::GetSrcNote(GSNCache& cache, JSScript* script, jsbytecode* pc) {
  size_t target = pc - script->code();
  if (target >= script->length()) {
    return nullptr;
  }

  if (cache.code == script->code()) {
    GSNCache::Map::Ptr p = cache.map.lookup(pc);
    return p ? p->value() : nullptr;
  }

  size_t offset = 0;
  jssrcnote* result;
  for (jssrcnote* sn = script->notes();; sn = SN_NEXT(sn)) {
    if (SN_IS_TERMINATOR(sn)) {
      result = nullptr;
      break;
    }
    offset += SN_DELTA(sn);
    if (offset == target && SN_IS_GETTABLE(sn)) {
      result = sn;
      break;
    }
  }

  if (cache.code != script->code() && script->length() >= GSN_CACHE_THRESHOLD) {
    unsigned nsrcnotes = 0;
    for (jssrcnote* sn = script->notes(); !SN_IS_TERMINATOR(sn);
         sn = SN_NEXT(sn)) {
      if (SN_IS_GETTABLE(sn)) {
        ++nsrcnotes;
      }
    }
    if (cache.code) {
      cache.map.clear();
      cache.code = nullptr;
    }
    if (cache.map.reserve(nsrcnotes)) {
      pc = script->code();
      for (jssrcnote* sn = script->notes(); !SN_IS_TERMINATOR(sn);
           sn = SN_NEXT(sn)) {
        pc += SN_DELTA(sn);
        if (SN_IS_GETTABLE(sn)) {
          cache.map.putNewInfallible(pc, sn);
        }
      }
      cache.code = script->code();
    }
  }

  return result;
}

jssrcnote* js::GetSrcNote(JSContext* cx, JSScript* script, jsbytecode* pc) {
  return GetSrcNote(cx->caches().gsnCache, script, pc);
}

unsigned js::PCToLineNumber(unsigned startLine, jssrcnote* notes,
                            jsbytecode* code, jsbytecode* pc,
                            unsigned* columnp) {
  unsigned lineno = startLine;
  unsigned column = 0;

  /*
   * Walk through source notes accumulating their deltas, keeping track of
   * line-number notes, until we pass the note for pc's offset within
   * script->code.
   */
  ptrdiff_t offset = 0;
  ptrdiff_t target = pc - code;
  for (jssrcnote* sn = notes; !SN_IS_TERMINATOR(sn); sn = SN_NEXT(sn)) {
    offset += SN_DELTA(sn);
    if (offset > target) {
      break;
    }

    SrcNoteType type = SN_TYPE(sn);
    if (type == SRC_SETLINE) {
      lineno = unsigned(GetSrcNoteOffset(sn, SrcNote::SetLine::Line));
      column = 0;
    } else if (type == SRC_NEWLINE) {
      lineno++;
      column = 0;
    } else if (type == SRC_COLSPAN) {
      ptrdiff_t colspan =
          SN_OFFSET_TO_COLSPAN(GetSrcNoteOffset(sn, SrcNote::ColSpan::Span));
      MOZ_ASSERT(ptrdiff_t(column) + colspan >= 0);
      column += colspan;
    }
  }

  if (columnp) {
    *columnp = column;
  }

  return lineno;
}

unsigned js::PCToLineNumber(JSScript* script, jsbytecode* pc,
                            unsigned* columnp) {
  /* Cope with InterpreterFrame.pc value prior to entering Interpret. */
  if (!pc) {
    return 0;
  }

  return PCToLineNumber(script->lineno(), script->notes(), script->code(), pc,
                        columnp);
}

jsbytecode* js::LineNumberToPC(JSScript* script, unsigned target) {
  ptrdiff_t offset = 0;
  ptrdiff_t best = -1;
  unsigned lineno = script->lineno();
  unsigned bestdiff = SN_MAX_OFFSET;
  for (jssrcnote* sn = script->notes(); !SN_IS_TERMINATOR(sn);
       sn = SN_NEXT(sn)) {
    /*
     * Exact-match only if offset is not in the prologue; otherwise use
     * nearest greater-or-equal line number match.
     */
    if (lineno == target && offset >= ptrdiff_t(script->mainOffset())) {
      goto out;
    }
    if (lineno >= target) {
      unsigned diff = lineno - target;
      if (diff < bestdiff) {
        bestdiff = diff;
        best = offset;
      }
    }
    offset += SN_DELTA(sn);
    SrcNoteType type = SN_TYPE(sn);
    if (type == SRC_SETLINE) {
      lineno = unsigned(GetSrcNoteOffset(sn, SrcNote::SetLine::Line));
    } else if (type == SRC_NEWLINE) {
      lineno++;
    }
  }
  if (best >= 0) {
    offset = best;
  }
out:
  return script->offsetToPC(offset);
}

JS_FRIEND_API unsigned js::GetScriptLineExtent(JSScript* script) {
  unsigned lineno = script->lineno();
  unsigned maxLineNo = lineno;
  for (jssrcnote* sn = script->notes(); !SN_IS_TERMINATOR(sn);
       sn = SN_NEXT(sn)) {
    SrcNoteType type = SN_TYPE(sn);
    if (type == SRC_SETLINE) {
      lineno = unsigned(GetSrcNoteOffset(sn, SrcNote::SetLine::Line));
    } else if (type == SRC_NEWLINE) {
      lineno++;
    }

    if (maxLineNo < lineno) {
      maxLineNo = lineno;
    }
  }

  return 1 + maxLineNo - script->lineno();
}

void js::DescribeScriptedCallerForDirectEval(JSContext* cx, HandleScript script,
                                             jsbytecode* pc, const char** file,
                                             unsigned* linenop,
                                             uint32_t* pcOffset,
                                             bool* mutedErrors) {
  MOZ_ASSERT(script->containsPC(pc));

  static_assert(JSOP_SPREADEVAL_LENGTH == JSOP_STRICTSPREADEVAL_LENGTH,
                "next op after a spread must be at consistent offset");
  static_assert(JSOP_EVAL_LENGTH == JSOP_STRICTEVAL_LENGTH,
                "next op after a direct eval must be at consistent offset");

  MOZ_ASSERT(JSOp(*pc) == JSOP_EVAL || JSOp(*pc) == JSOP_STRICTEVAL ||
             JSOp(*pc) == JSOP_SPREADEVAL ||
             JSOp(*pc) == JSOP_STRICTSPREADEVAL);

  bool isSpread =
      (JSOp(*pc) == JSOP_SPREADEVAL || JSOp(*pc) == JSOP_STRICTSPREADEVAL);
  jsbytecode* nextpc =
      pc + (isSpread ? JSOP_SPREADEVAL_LENGTH : JSOP_EVAL_LENGTH);
  MOZ_ASSERT(*nextpc == JSOP_LINENO);

  *file = script->filename();
  *linenop = GET_UINT32(nextpc);
  *pcOffset = script->pcToOffset(pc);
  *mutedErrors = script->mutedErrors();
}

void js::DescribeScriptedCallerForCompilation(
    JSContext* cx, MutableHandleScript maybeScript, const char** file,
    unsigned* linenop, uint32_t* pcOffset, bool* mutedErrors) {
  NonBuiltinFrameIter iter(cx, cx->realm()->principals());

  if (iter.done()) {
    maybeScript.set(nullptr);
    *file = nullptr;
    *linenop = 0;
    *pcOffset = 0;
    *mutedErrors = false;
    return;
  }

  *file = iter.filename();
  *linenop = iter.computeLine();
  *mutedErrors = iter.mutedErrors();

  // These values are only used for introducer fields which are debugging
  // information and can be safely left null for wasm frames.
  if (iter.hasScript()) {
    maybeScript.set(iter.script());
    *pcOffset = iter.pc() - maybeScript->code();
  } else {
    maybeScript.set(nullptr);
    *pcOffset = 0;
  }
}

static JSObject* CloneInnerInterpretedFunction(
    JSContext* cx, HandleScope enclosingScope, HandleFunction srcFun,
    Handle<ScriptSourceObject*> sourceObject) {
  /* NB: Keep this in sync with XDRInterpretedFunction. */
  RootedObject cloneProto(cx);
  if (!GetFunctionPrototype(cx, srcFun->generatorKind(), srcFun->asyncKind(),
                            &cloneProto)) {
    return nullptr;
  }

  gc::AllocKind allocKind = srcFun->getAllocKind();
  uint16_t flags = srcFun->flags();
  if (srcFun->isSelfHostedBuiltin()) {
    // Functions in the self-hosting compartment are only extended in
    // debug mode. For top-level functions, FUNCTION_EXTENDED gets used by
    // the cloning algorithm. Do the same for inner functions here.
    allocKind = gc::AllocKind::FUNCTION_EXTENDED;
    flags |= JSFunction::Flags::EXTENDED;
  }
  RootedAtom atom(cx, srcFun->displayAtom());
  if (atom) {
    cx->markAtom(atom);
  }
  RootedFunction clone(
      cx, NewFunctionWithProto(cx, nullptr, srcFun->nargs(),
                               JSFunction::Flags(flags), nullptr, atom,
                               cloneProto, allocKind, TenuredObject));
  if (!clone) {
    return nullptr;
  }

  JSScript::AutoDelazify srcScript(cx, srcFun);
  if (!srcScript) {
    return nullptr;
  }
  JSScript* cloneScript = CloneScriptIntoFunction(cx, enclosingScope, clone,
                                                  srcScript, sourceObject);
  if (!cloneScript) {
    return nullptr;
  }

  if (!JSFunction::setTypeForScriptedFunction(cx, clone)) {
    return nullptr;
  }

  return clone;
}

/* static */
bool PrivateScriptData::Clone(JSContext* cx, HandleScript src, HandleScript dst,
                              MutableHandle<GCVector<Scope*>> scopes) {
  PrivateScriptData* srcData = src->data_;
  uint32_t nscopes = srcData->scopes().size();
  uint32_t nconsts = srcData->hasConsts() ? srcData->consts().size() : 0;
  uint32_t nobjects = srcData->hasObjects() ? srcData->objects().size() : 0;
  uint32_t ntrynotes = srcData->hasTryNotes() ? srcData->tryNotes().size() : 0;
  uint32_t nscopenotes =
      srcData->hasScopeNotes() ? srcData->scopeNotes().size() : 0;
  uint32_t nresumeoffsets =
      srcData->hasResumeOffsets() ? srcData->resumeOffsets().size() : 0;

  /* Scopes */

  // The passed in scopes vector contains body scopes that needed to be
  // cloned especially, depending on whether the script is a function or
  // global scope. Starting at scopes.length() means we only deal with
  // intra-body scopes.
  {
    MOZ_ASSERT(nscopes != 0);
    MOZ_ASSERT(src->bodyScopeIndex() + 1 == scopes.length());
    RootedScope original(cx);
    RootedScope clone(cx);
    for (const GCPtrScope& elem : srcData->scopes().From(scopes.length())) {
      original = elem.get();
      uint32_t scopeIndex =
          FindScopeIndex(srcData->scopes(), *original->enclosing());
      clone = Scope::clone(cx, original, scopes[scopeIndex]);
      if (!clone || !scopes.append(clone)) {
        return false;
      }
    }
  }

  /* Constants */

  AutoValueVector consts(cx);
  if (nconsts != 0) {
    RootedValue val(cx);
    RootedValue clone(cx);
    for (const GCPtrValue& elem : srcData->consts()) {
      val = elem.get();
      if (val.isDouble()) {
        clone = val;
      } else if (val.isBigInt()) {
        if (cx->zone() == val.toBigInt()->zone()) {
          clone.setBigInt(val.toBigInt());
        } else {
          RootedBigInt b(cx, val.toBigInt());
          BigInt* copy = BigInt::copy(cx, b);
          if (!copy) {
            return false;
          }
          clone.setBigInt(copy);
        }
      } else {
        MOZ_ASSERT_UNREACHABLE("bad script consts() element");
      }

      if (!consts.append(clone)) {
        return false;
      }
    }
  }

  /* Objects */

  AutoObjectVector objects(cx);
  if (nobjects != 0) {
    RootedObject obj(cx);
    RootedObject clone(cx);
    Rooted<ScriptSourceObject*> sourceObject(cx, dst->sourceObject());
    for (const GCPtrObject& elem : srcData->objects()) {
      obj = elem.get();
      clone = nullptr;
      if (obj->is<RegExpObject>()) {
        clone = CloneScriptRegExpObject(cx, obj->as<RegExpObject>());
      } else if (obj->is<JSFunction>()) {
        RootedFunction innerFun(cx, &obj->as<JSFunction>());
        if (innerFun->isNative()) {
          if (cx->compartment() != innerFun->compartment()) {
            MOZ_ASSERT(innerFun->isAsmJSNative());
            JS_ReportErrorASCII(cx,
                                "AsmJS modules do not yet support cloning.");
            return false;
          }
          clone = innerFun;
        } else {
          if (innerFun->isInterpretedLazy()) {
            AutoRealm ar(cx, innerFun);
            if (!JSFunction::getOrCreateScript(cx, innerFun)) {
              return false;
            }
          }

          Scope* enclosing = innerFun->nonLazyScript()->enclosingScope();
          uint32_t scopeIndex = FindScopeIndex(srcData->scopes(), *enclosing);
          RootedScope enclosingClone(cx, scopes[scopeIndex]);
          clone = CloneInnerInterpretedFunction(cx, enclosingClone, innerFun,
                                                sourceObject);
        }
      } else {
        clone = DeepCloneObjectLiteral(cx, obj, TenuredObject);
      }

      if (!clone || !objects.append(clone)) {
        return false;
      }
    }
  }

  // Create the new PrivateScriptData on |dst| and fill it in.
  if (!JSScript::createPrivateScriptData(cx, dst, nscopes, nconsts, nobjects,
                                         ntrynotes, nscopenotes,
                                         nresumeoffsets)) {
    return false;
  }

  PrivateScriptData* dstData = dst->data_;
  {
    auto array = dstData->scopes();
    for (uint32_t i = 0; i < nscopes; ++i) {
      array[i].init(scopes[i]);
    }
  }
  if (nconsts) {
    auto array = dstData->consts();
    for (unsigned i = 0; i < nconsts; ++i) {
      array[i].init(consts[i]);
    }
  }
  if (nobjects) {
    auto array = dstData->objects();
    for (unsigned i = 0; i < nobjects; ++i) {
      array[i].init(objects[i]);
    }
  }
  if (ntrynotes) {
    std::copy_n(srcData->tryNotes().begin(), ntrynotes,
                dstData->tryNotes().begin());
  }
  if (nscopenotes) {
    std::copy_n(srcData->scopeNotes().begin(), nscopenotes,
                dstData->scopeNotes().begin());
  }
  if (nresumeoffsets) {
    std::copy_n(srcData->resumeOffsets().begin(), nresumeoffsets,
                dstData->resumeOffsets().begin());
  }

  return true;
}

JSScript* js::detail::CopyScript(JSContext* cx, HandleScript src,
                                 HandleScriptSourceObject sourceObject,
                                 MutableHandle<GCVector<Scope*>> scopes) {
  // We don't copy the HideScriptFromDebugger flag and it's not clear what
  // should happen if it's set on the source script.
  MOZ_ASSERT(!src->hideScriptFromDebugger());

  if (src->treatAsRunOnce() && !src->functionNonDelazifying()) {
    JS_ReportErrorASCII(cx, "No cloning toplevel run-once scripts");
    return nullptr;
  }

  /* NB: Keep this in sync with XDRScript. */

  // Some embeddings are not careful to use ExposeObjectToActiveJS as needed.
  JS::AssertObjectIsNotGray(sourceObject);

  CompileOptions options(cx);
  options.setMutedErrors(src->mutedErrors())
      .setSelfHostingMode(src->selfHosted())
      .setNoScriptRval(src->noScriptRval());

  // Create a new JSScript to fill in
  RootedScript dst(
      cx, JSScript::Create(cx, options, sourceObject, src->sourceStart(),
                           src->sourceEnd(), src->toStringStart(),
                           src->toStringEnd()));
  if (!dst) {
    return nullptr;
  }

  // Copy POD fields
  dst->lineno_ = src->lineno();
  dst->column_ = src->column();
  dst->mainOffset_ = src->mainOffset();
  dst->nfixed_ = src->nfixed();
  dst->nslots_ = src->nslots();
  dst->bodyScopeIndex_ = src->bodyScopeIndex();
  dst->immutableFlags_ = src->immutableFlags_;
  dst->funLength_ = src->funLength();
  dst->numBytecodeTypeSets_ = src->numBytecodeTypeSets();

  dst->setFlag(JSScript::ImmutableFlags::HasNonSyntacticScope,
               scopes[0]->hasOnChain(ScopeKind::NonSyntactic));

  if (src->argumentsHasVarBinding()) {
    dst->setArgumentsHasVarBinding();
  }

  // Clone the PrivateScriptData into dst
  if (!PrivateScriptData::Clone(cx, src, dst, scopes)) {
    return nullptr;
  }

  // The SharedScriptData can be reused by any zone in the Runtime as long as
  // we make sure to mark first (to sync Atom pointers).
  if (cx->zone() != src->zoneFromAnyThread()) {
    src->scriptData()->markForCrossZone(cx);
  }
  dst->scriptData_ = src->scriptData();

  return dst;
}

JSScript* js::CloneGlobalScript(JSContext* cx, ScopeKind scopeKind,
                                HandleScript src) {
  MOZ_ASSERT(scopeKind == ScopeKind::Global ||
             scopeKind == ScopeKind::NonSyntactic);

  Rooted<ScriptSourceObject*> sourceObject(cx, src->sourceObject());
  if (cx->compartment() != sourceObject->compartment()) {
    sourceObject = ScriptSourceObject::clone(cx, sourceObject);
    if (!sourceObject) {
      return nullptr;
    }
  }

  MOZ_ASSERT(src->bodyScopeIndex() == 0);
  Rooted<GCVector<Scope*>> scopes(cx, GCVector<Scope*>(cx));
  Rooted<GlobalScope*> original(cx, &src->bodyScope()->as<GlobalScope>());
  GlobalScope* clone = GlobalScope::clone(cx, original, scopeKind);
  if (!clone || !scopes.append(clone)) {
    return nullptr;
  }

  return detail::CopyScript(cx, src, sourceObject, &scopes);
}

JSScript* js::CloneScriptIntoFunction(
    JSContext* cx, HandleScope enclosingScope, HandleFunction fun,
    HandleScript src, Handle<ScriptSourceObject*> sourceObject) {
  MOZ_ASSERT(fun->isInterpreted());
  MOZ_ASSERT(!fun->hasScript() || fun->hasUncompletedScript());

  // Clone the non-intra-body scopes.
  Rooted<GCVector<Scope*>> scopes(cx, GCVector<Scope*>(cx));
  RootedScope original(cx);
  RootedScope enclosingClone(cx);
  for (uint32_t i = 0; i <= src->bodyScopeIndex(); i++) {
    original = src->getScope(i);

    if (i == 0) {
      enclosingClone = enclosingScope;
    } else {
      MOZ_ASSERT(src->getScope(i - 1) == original->enclosing());
      enclosingClone = scopes[i - 1];
    }

    Scope* clone;
    if (original->is<FunctionScope>()) {
      clone = FunctionScope::clone(cx, original.as<FunctionScope>(), fun,
                                   enclosingClone);
    } else {
      clone = Scope::clone(cx, original, enclosingClone);
    }

    if (!clone || !scopes.append(clone)) {
      return nullptr;
    }
  }

  // Save flags in case we need to undo the early mutations.
  const int preservedFlags = fun->flags();
  RootedScript dst(cx, detail::CopyScript(cx, src, sourceObject, &scopes));
  if (!dst) {
    fun->setFlags(preservedFlags);
    return nullptr;
  }

  // Finally set the script after all the fallible operations.
  if (fun->isInterpretedLazy()) {
    fun->setUnlazifiedScript(dst);
  } else {
    fun->initScript(dst);
  }

  return dst;
}

DebugScript* JSScript::debugScript() {
  MOZ_ASSERT(hasDebugScript());
  DebugScriptMap* map = realm()->debugScriptMap.get();
  MOZ_ASSERT(map);
  DebugScriptMap::Ptr p = map->lookup(this);
  MOZ_ASSERT(p);
  return p->value().get();
}

DebugScript* JSScript::releaseDebugScript() {
  MOZ_ASSERT(hasDebugScript());
  DebugScriptMap* map = realm()->debugScriptMap.get();
  MOZ_ASSERT(map);
  DebugScriptMap::Ptr p = map->lookup(this);
  MOZ_ASSERT(p);
  DebugScript* debug = p->value().release();
  map->remove(p);
  clearFlag(MutableFlags::HasDebugScript);
  return debug;
}

void JSScript::destroyDebugScript(FreeOp* fop) {
  if (hasDebugScript()) {
#ifdef DEBUG
    for (jsbytecode* pc = code(); pc < codeEnd(); pc++) {
      if (BreakpointSite* site = getBreakpointSite(pc)) {
        /* Breakpoints are swept before finalization. */
        MOZ_ASSERT(site->firstBreakpoint() == nullptr);
        MOZ_ASSERT(getBreakpointSite(pc) == nullptr);
      }
    }
#endif
    fop->free_(releaseDebugScript());
  }
}

bool JSScript::ensureHasDebugScript(JSContext* cx) {
  if (hasDebugScript()) {
    return true;
  }

  size_t nbytes =
      offsetof(DebugScript, breakpoints) + length() * sizeof(BreakpointSite*);
  UniqueDebugScript debug(
      reinterpret_cast<DebugScript*>(cx->pod_calloc<uint8_t>(nbytes)));
  if (!debug) {
    return false;
  }

  /* Create realm's debugScriptMap if necessary. */
  if (!realm()->debugScriptMap) {
    auto map = cx->make_unique<DebugScriptMap>();
    if (!map) {
      return false;
    }

    realm()->debugScriptMap = std::move(map);
  }

  if (!realm()->debugScriptMap->putNew(this, std::move(debug))) {
    ReportOutOfMemory(cx);
    return false;
  }

  setFlag(MutableFlags::HasDebugScript);  // safe to set this;  we can't fail
                                          // after this point

  /*
   * Ensure that any Interpret() instances running on this script have
   * interrupts enabled. The interrupts must stay enabled until the
   * debug state is destroyed.
   */
  for (ActivationIterator iter(cx); !iter.done(); ++iter) {
    if (iter->isInterpreter()) {
      iter->asInterpreter()->enableInterruptsIfRunning(this);
    }
  }

  return true;
}

void JSScript::setNewStepMode(FreeOp* fop, uint32_t newValue) {
  DebugScript* debug = debugScript();
  uint32_t prior = debug->stepMode;
  debug->stepMode = newValue;

  if (!prior != !newValue) {
    if (hasBaselineScript()) {
      baseline->toggleDebugTraps(this, nullptr);
    }

    if (!stepModeEnabled() && !debug->numSites) {
      fop->free_(releaseDebugScript());
    }
  }
}

bool JSScript::incrementStepModeCount(JSContext* cx) {
  cx->check(this);
  MOZ_ASSERT(cx->realm()->isDebuggee());

  AutoRealm ar(cx, this);

  if (!ensureHasDebugScript(cx)) {
    return false;
  }

  DebugScript* debug = debugScript();
  uint32_t count = debug->stepMode;
  setNewStepMode(cx->runtime()->defaultFreeOp(), count + 1);
  return true;
}

void JSScript::decrementStepModeCount(FreeOp* fop) {
  DebugScript* debug = debugScript();
  uint32_t count = debug->stepMode;
  MOZ_ASSERT(count > 0);
  setNewStepMode(fop, count - 1);
}

BreakpointSite* JSScript::getOrCreateBreakpointSite(JSContext* cx,
                                                    jsbytecode* pc) {
  AutoRealm ar(cx, this);

  if (!ensureHasDebugScript(cx)) {
    return nullptr;
  }

  DebugScript* debug = debugScript();
  BreakpointSite*& site = debug->breakpoints[pcToOffset(pc)];

  if (!site) {
    site = cx->new_<JSBreakpointSite>(this, pc);
    if (!site) {
      return nullptr;
    }
    debug->numSites++;
  }

  return site;
}

void JSScript::destroyBreakpointSite(FreeOp* fop, jsbytecode* pc) {
  DebugScript* debug = debugScript();
  BreakpointSite*& site = debug->breakpoints[pcToOffset(pc)];
  MOZ_ASSERT(site);

  fop->delete_(site);
  site = nullptr;

  if (--debug->numSites == 0 && !stepModeEnabled()) {
    fop->free_(releaseDebugScript());
  }
}

void JSScript::clearBreakpointsIn(FreeOp* fop, js::Debugger* dbg,
                                  JSObject* handler) {
  if (!hasAnyBreakpointsOrStepMode()) {
    return;
  }

  for (jsbytecode* pc = code(); pc < codeEnd(); pc++) {
    BreakpointSite* site = getBreakpointSite(pc);
    if (site) {
      Breakpoint* nextbp;
      for (Breakpoint* bp = site->firstBreakpoint(); bp; bp = nextbp) {
        nextbp = bp->nextInSite();
        if ((!dbg || bp->debugger == dbg) &&
            (!handler || bp->getHandler() == handler)) {
          bp->destroy(fop);
        }
      }
    }
  }
}

bool JSScript::hasBreakpointsAt(jsbytecode* pc) {
  BreakpointSite* site = getBreakpointSite(pc);
  if (!site) {
    return false;
  }

  return site->enabledCount > 0;
}

/* static */ bool SharedScriptData::InitFromEmitter(
    JSContext* cx, js::HandleScript script, frontend::BytecodeEmitter* bce) {
  uint32_t natoms = bce->atomIndices->count();

  size_t codeLength = bce->code().length();
  MOZ_RELEASE_ASSERT(codeLength <= frontend::MaxBytecodeLength);

  // The + 1 is to account for the final SN_MAKE_TERMINATOR that is appended
  // when the notes are copied to their final destination by copySrcNotes.
  static_assert(frontend::MaxSrcNotesLength < UINT32_MAX,
                "Length + 1 shouldn't overflow UINT32_MAX");
  size_t noteLengthNoTerminator = bce->notes().length();
  size_t noteLength = noteLengthNoTerminator + 1;
  MOZ_RELEASE_ASSERT(noteLengthNoTerminator <= frontend::MaxSrcNotesLength);

  // Create and initialize SharedScriptData
  if (!script->createSharedScriptData(cx, codeLength, noteLength, natoms)) {
    return false;
  }

  js::SharedScriptData* data = script->scriptData_;

  // Initialize trailing arrays
  std::copy_n(bce->code().begin(), codeLength, data->code());
  bce->copySrcNotes(data->notes(), noteLength);
  InitAtomMap(*bce->atomIndices, data->atoms());

  return true;
}

void SharedScriptData::traceChildren(JSTracer* trc) {
  MOZ_ASSERT(refCount() != 0);
  for (uint32_t i = 0; i < natoms(); ++i) {
    TraceNullableEdge(trc, &atoms()[i], "atom");
  }
}

void SharedScriptData::markForCrossZone(JSContext* cx) {
  for (uint32_t i = 0; i < natoms(); ++i) {
    cx->markAtom(atoms()[i]);
  }
}

void JSScript::traceChildren(JSTracer* trc) {
  // NOTE: this JSScript may be partially initialized at this point.  E.g. we
  // may have created it and partially initialized it with
  // JSScript::Create(), but not yet finished initializing it with
  // fullyInitFromEmitter() or fullyInitTrivial().

  MOZ_ASSERT_IF(trc->isMarkingTracer() &&
                    GCMarker::fromTracer(trc)->shouldCheckCompartments(),
                zone()->isCollecting());

  if (data_) {
    data_->trace(trc);
  }

  if (scriptData()) {
    scriptData()->traceChildren(trc);
  }

  MOZ_ASSERT_IF(sourceObject(),
                MaybeForwarded(sourceObject())->compartment() == compartment());
  TraceNullableEdge(trc, &sourceObject_, "sourceObject");

  if (maybeLazyScript()) {
    TraceManuallyBarrieredEdge(trc, &lazyScript, "lazyScript");
  }

  if (trc->isMarkingTracer()) {
    realm()->mark();
  }

  jit::TraceJitScripts(trc, this);

  if (trc->isMarkingTracer()) {
    GCMarker::fromTracer(trc)->markImplicitEdges(this);
  }
}

void LazyScript::finalize(FreeOp* fop) { fop->free_(table_); }

size_t JSScript::calculateLiveFixed(jsbytecode* pc) {
  size_t nlivefixed = numAlwaysLiveFixedSlots();

  if (nfixed() != nlivefixed) {
    Scope* scope = lookupScope(pc);
    if (scope) {
      scope = MaybeForwarded(scope);
    }

    // Find the nearest LexicalScope in the same script.
    while (scope && scope->is<WithScope>()) {
      scope = scope->enclosing();
      if (scope) {
        scope = MaybeForwarded(scope);
      }
    }

    if (scope) {
      if (scope->is<LexicalScope>()) {
        nlivefixed = scope->as<LexicalScope>().nextFrameSlot();
      } else if (scope->is<VarScope>()) {
        nlivefixed = scope->as<VarScope>().nextFrameSlot();
      }
    }
  }

  MOZ_ASSERT(nlivefixed <= nfixed());
  MOZ_ASSERT(nlivefixed >= numAlwaysLiveFixedSlots());

  return nlivefixed;
}

Scope* JSScript::lookupScope(jsbytecode* pc) {
  MOZ_ASSERT(containsPC(pc));

  if (!hasScopeNotes()) {
    return nullptr;
  }

  size_t offset = pc - code();

  auto notes = scopeNotes();
  Scope* scope = nullptr;

  // Find the innermost block chain using a binary search.
  size_t bottom = 0;
  size_t top = notes.size();

  while (bottom < top) {
    size_t mid = bottom + (top - bottom) / 2;
    const ScopeNote* note = &notes[mid];
    if (note->start <= offset) {
      // Block scopes are ordered in the list by their starting offset, and
      // since blocks form a tree ones earlier in the list may cover the pc even
      // if later blocks end before the pc. This only happens when the earlier
      // block is a parent of the later block, so we need to check parents of
      // |mid| in the searched range for coverage.
      size_t check = mid;
      while (check >= bottom) {
        const ScopeNote* checkNote = &notes[check];
        MOZ_ASSERT(checkNote->start <= offset);
        if (offset < checkNote->start + checkNote->length) {
          // We found a matching block chain but there may be inner ones
          // at a higher block chain index than mid. Continue the binary search.
          if (checkNote->index == ScopeNote::NoScopeIndex) {
            scope = nullptr;
          } else {
            scope = getScope(checkNote->index);
          }
          break;
        }
        if (checkNote->parent == UINT32_MAX) {
          break;
        }
        check = checkNote->parent;
      }
      bottom = mid + 1;
    } else {
      top = mid;
    }
  }

  return scope;
}

Scope* JSScript::innermostScope(jsbytecode* pc) {
  if (Scope* scope = lookupScope(pc)) {
    return scope;
  }
  return bodyScope();
}

void JSScript::setArgumentsHasVarBinding() {
  setFlag(ImmutableFlags::ArgsHasVarBinding);
  setFlag(MutableFlags::NeedsArgsAnalysis);
}

void JSScript::setNeedsArgsObj(bool needsArgsObj) {
  MOZ_ASSERT_IF(needsArgsObj, argumentsHasVarBinding());
  clearFlag(MutableFlags::NeedsArgsAnalysis);
  setFlag(MutableFlags::NeedsArgsObj, needsArgsObj);
}

void js::SetFrameArgumentsObject(JSContext* cx, AbstractFramePtr frame,
                                 HandleScript script, JSObject* argsobj) {
  /*
   * Replace any optimized arguments in the frame with an explicit arguments
   * object. Note that 'arguments' may have already been overwritten.
   */

  Rooted<BindingIter> bi(cx, BindingIter(script));
  while (bi && bi.name() != cx->names().arguments) {
    bi++;
  }
  if (!bi) {
    return;
  }

  if (bi.location().kind() == BindingLocation::Kind::Environment) {
    /*
     * Scan the script to find the slot in the call object that 'arguments'
     * is assigned to.
     */
    jsbytecode* pc = script->code();
    while (*pc != JSOP_ARGUMENTS) {
      pc += GetBytecodeLength(pc);
    }
    pc += JSOP_ARGUMENTS_LENGTH;
    MOZ_ASSERT(*pc == JSOP_SETALIASEDVAR);

    // Note that here and below, it is insufficient to only check for
    // JS_OPTIMIZED_ARGUMENTS, as Ion could have optimized out the
    // arguments slot.
    EnvironmentObject& env = frame.callObj().as<EnvironmentObject>();
    if (IsOptimizedPlaceholderMagicValue(env.aliasedBinding(bi))) {
      env.setAliasedBinding(cx, bi, ObjectValue(*argsobj));
    }
  } else {
    MOZ_ASSERT(bi.location().kind() == BindingLocation::Kind::Frame);
    uint32_t frameSlot = bi.location().slot();
    if (IsOptimizedPlaceholderMagicValue(frame.unaliasedLocal(frameSlot))) {
      frame.unaliasedLocal(frameSlot) = ObjectValue(*argsobj);
    }
  }
}

/* static */
bool JSScript::argumentsOptimizationFailed(JSContext* cx, HandleScript script) {
  MOZ_ASSERT(script->functionNonDelazifying());
  MOZ_ASSERT(script->analyzedArgsUsage());
  MOZ_ASSERT(script->argumentsHasVarBinding());

  /*
   * It is possible that the arguments optimization has already failed,
   * everything has been fixed up, but there was an outstanding magic value
   * on the stack that has just now flowed into an apply. In this case, there
   * is nothing to do; GuardFunApplySpeculation will patch in the real
   * argsobj.
   */
  if (script->needsArgsObj()) {
    return true;
  }

  MOZ_ASSERT(!script->isGenerator());
  MOZ_ASSERT(!script->isAsync());

  script->setFlag(MutableFlags::NeedsArgsObj);

  /*
   * By design, the arguments optimization is only made when there are no
   * outstanding cases of MagicValue(JS_OPTIMIZED_ARGUMENTS) at any points
   * where the optimization could fail, other than an active invocation of
   * 'f.apply(x, arguments)'. Thus, there are no outstanding values of
   * MagicValue(JS_OPTIMIZED_ARGUMENTS) on the stack. However, there are
   * three things that need fixup:
   *  - there may be any number of activations of this script that don't have
   *    an argsObj that now need one.
   *  - jit code compiled (and possible active on the stack) with the static
   *    assumption of !script->needsArgsObj();
   *  - type inference data for the script assuming script->needsArgsObj
   */
  for (AllScriptFramesIter i(cx); !i.done(); ++i) {
    /*
     * We cannot reliably create an arguments object for Ion activations of
     * this script.  To maintain the invariant that "script->needsArgsObj
     * implies fp->hasArgsObj", the Ion bail mechanism will create an
     * arguments object right after restoring the BaselineFrame and before
     * entering Baseline code (in jit::FinishBailoutToBaseline).
     */
    if (i.isIon()) {
      continue;
    }
    AbstractFramePtr frame = i.abstractFramePtr();
    if (frame.isFunctionFrame() && frame.script() == script) {
      /* We crash on OOM since cleaning up here would be complicated. */
      AutoEnterOOMUnsafeRegion oomUnsafe;
      ArgumentsObject* argsobj = ArgumentsObject::createExpected(cx, frame);
      if (!argsobj) {
        oomUnsafe.crash("JSScript::argumentsOptimizationFailed");
      }
      SetFrameArgumentsObject(cx, frame, script, argsobj);
    }
  }

  return true;
}

bool JSScript::formalIsAliased(unsigned argSlot) {
  if (functionHasParameterExprs()) {
    return false;
  }

  for (PositionalFormalParameterIter fi(this); fi; fi++) {
    if (fi.argumentSlot() == argSlot) {
      return fi.closedOver();
    }
  }
  MOZ_CRASH("Argument slot not found");
}

bool JSScript::formalLivesInArgumentsObject(unsigned argSlot) {
  return argsObjAliasesFormals() && !formalIsAliased(argSlot);
}

LazyScript::LazyScript(JSFunction* fun, ScriptSourceObject& sourceObject,
                       void* table, uint64_t packedFields, uint32_t sourceStart,
                       uint32_t sourceEnd, uint32_t toStringStart,
                       uint32_t lineno, uint32_t column)
    : script_(nullptr),
      function_(fun),
      sourceObject_(&sourceObject),
      table_(table),
      packedFields_(packedFields),
      fieldInitializers_(FieldInitializers::Invalid()),
      sourceStart_(sourceStart),
      sourceEnd_(sourceEnd),
      toStringStart_(toStringStart),
      toStringEnd_(sourceEnd),
      lineno_(lineno),
      column_(column) {
  MOZ_ASSERT(function_);
  MOZ_ASSERT(sourceObject_);
  MOZ_ASSERT(function_->compartment() == sourceObject_->compartment());
  MOZ_ASSERT(sourceStart <= sourceEnd);
  MOZ_ASSERT(toStringStart <= sourceStart);
}

void LazyScript::initScript(JSScript* script) {
  MOZ_ASSERT(script);
  MOZ_ASSERT(!script_.unbarrieredGet());
  script_.set(script);
}

JS::Compartment* LazyScript::compartment() const {
  return function_->compartment();
}

Realm* LazyScript::realm() const { return function_->realm(); }

void LazyScript::setEnclosingLazyScript(LazyScript* enclosingLazyScript) {
  MOZ_ASSERT(enclosingLazyScript);

  // We never change an existing LazyScript.
  MOZ_ASSERT(!hasEnclosingLazyScript());

  // Enclosing scopes never transition back to enclosing lazy scripts.
  MOZ_ASSERT(!hasEnclosingScope());

  enclosingLazyScriptOrScope_ = enclosingLazyScript;
}

void LazyScript::setEnclosingScope(Scope* enclosingScope) {
  MOZ_ASSERT(enclosingScope);
  MOZ_ASSERT(!hasEnclosingScope());

  enclosingLazyScriptOrScope_ = enclosingScope;
}

ScriptSourceObject& LazyScript::sourceObject() const {
  return sourceObject_->as<ScriptSourceObject>();
}

ScriptSource* LazyScript::maybeForwardedScriptSource() const {
  JSObject* source = MaybeForwarded(&sourceObject());
  return UncheckedUnwrapWithoutExpose(source)
      ->as<ScriptSourceObject>()
      .source();
}

uint64_t LazyScript::packedFieldsForXDR() const {
  union {
    PackedView p;
    uint64_t packedFields;
  };

  packedFields = packedFields_;

  // Reset runtime flags
  p.hasBeenCloned = false;

  return packedFields;
}

/* static */
LazyScript* LazyScript::CreateRaw(JSContext* cx, HandleFunction fun,
                                  HandleScriptSourceObject sourceObject,
                                  uint64_t packedFields, uint32_t sourceStart,
                                  uint32_t sourceEnd, uint32_t toStringStart,
                                  uint32_t lineno, uint32_t column) {
  cx->check(fun);

  MOZ_ASSERT(sourceObject);
  union {
    PackedView p;
    uint64_t packed;
  };

  packed = packedFields;

  // Reset runtime flags to obtain a fresh LazyScript.
  p.hasBeenCloned = false;

  size_t bytes = (p.numClosedOverBindings * sizeof(JSAtom*)) +
                 (p.numInnerFunctions * sizeof(GCPtrFunction));

  UniquePtr<uint8_t, JS::FreePolicy> table;
  if (bytes) {
    table.reset(cx->pod_malloc<uint8_t>(bytes));
    if (!table) {
      return nullptr;
    }
  }

  LazyScript* res = Allocate<LazyScript>(cx);
  if (!res) {
    return nullptr;
  }

  cx->realm()->scheduleDelazificationForDebugger();

  return new (res)
      LazyScript(fun, *sourceObject, table.release(), packed, sourceStart,
                 sourceEnd, toStringStart, lineno, column);
}

/* static */
LazyScript* LazyScript::Create(JSContext* cx, HandleFunction fun,
                               HandleScriptSourceObject sourceObject,
                               const frontend::AtomVector& closedOverBindings,
                               Handle<GCVector<JSFunction*, 8>> innerFunctions,
                               uint32_t sourceStart, uint32_t sourceEnd,
                               uint32_t toStringStart, uint32_t lineno,
                               uint32_t column, frontend::ParseGoal parseGoal) {
  union {
    PackedView p;
    uint64_t packedFields;
  };

  p.shouldDeclareArguments = false;
  p.hasThisBinding = false;
  p.isAsync = false;
  p.hasRest = false;
  p.numClosedOverBindings = closedOverBindings.length();
  p.numInnerFunctions = innerFunctions.length();
  p.isGenerator = false;
  p.strict = false;
  p.bindingsAccessedDynamically = false;
  p.hasDebuggerStatement = false;
  p.hasDirectEval = false;
  p.isLikelyConstructorWrapper = false;
  p.treatAsRunOnce = false;
  p.isDerivedClassConstructor = false;
  p.needsHomeObject = false;
  p.isBinAST = false;
  p.parseGoal = uint32_t(parseGoal);

  LazyScript* res =
      LazyScript::CreateRaw(cx, fun, sourceObject, packedFields, sourceStart,
                            sourceEnd, toStringStart, lineno, column);
  if (!res) {
    return nullptr;
  }

  JSAtom** resClosedOverBindings = res->closedOverBindings();
  for (size_t i = 0; i < res->numClosedOverBindings(); i++) {
    resClosedOverBindings[i] = closedOverBindings[i];
  }

  GCPtrFunction* resInnerFunctions = res->innerFunctions();
  for (size_t i = 0; i < res->numInnerFunctions(); i++) {
    resInnerFunctions[i].init(innerFunctions[i]);
    if (resInnerFunctions[i]->isInterpretedLazy()) {
      resInnerFunctions[i]->lazyScript()->setEnclosingLazyScript(res);
    }
  }

  return res;
}

/* static */
LazyScript* LazyScript::CreateForXDR(
    JSContext* cx, HandleFunction fun, HandleScript script,
    HandleScope enclosingScope, HandleScriptSourceObject sourceObject,
    uint64_t packedFields, uint32_t sourceStart, uint32_t sourceEnd,
    uint32_t toStringStart, uint32_t lineno, uint32_t column) {
  // Dummy atom which is not a valid property name.
  RootedAtom dummyAtom(cx, cx->names().comma);

  // Dummy function which is not a valid function as this is the one which is
  // holding this lazy script.
  HandleFunction dummyFun = fun;

  LazyScript* res =
      LazyScript::CreateRaw(cx, fun, sourceObject, packedFields, sourceStart,
                            sourceEnd, toStringStart, lineno, column);
  if (!res) {
    return nullptr;
  }

  // Fill with dummies, to be GC-safe after the initialization of the free
  // variables and inner functions.
  size_t i, num;
  JSAtom** closedOverBindings = res->closedOverBindings();
  for (i = 0, num = res->numClosedOverBindings(); i < num; i++) {
    closedOverBindings[i] = dummyAtom;
  }

  GCPtrFunction* functions = res->innerFunctions();
  for (i = 0, num = res->numInnerFunctions(); i < num; i++) {
    functions[i].init(dummyFun);
  }

  // Set the enclosing scope of the lazy function. This value should only be
  // set if we have a non-lazy enclosing script at this point.
  // LazyScript::enclosingScriptHasEverBeenCompiled relies on the enclosing
  // scope being non-null if we have ever been nested inside non-lazy
  // function.
  MOZ_ASSERT(!res->hasEnclosingScope());
  if (enclosingScope) {
    res->setEnclosingScope(enclosingScope);
  }

  MOZ_ASSERT(!res->hasScript());
  if (script) {
    res->initScript(script);
  }

  return res;
}

void JSScript::updateJitCodeRaw(JSRuntime* rt) {
  MOZ_ASSERT(rt);
  if (hasBaselineScript() && baseline->hasPendingIonBuilder()) {
    MOZ_ASSERT(!isIonCompilingOffThread());
    jitCodeRaw_ = rt->jitRuntime()->lazyLinkStub().value;
    jitCodeSkipArgCheck_ = jitCodeRaw_;
  } else if (hasIonScript()) {
    jitCodeRaw_ = ion->method()->raw();
    jitCodeSkipArgCheck_ = jitCodeRaw_ + ion->getSkipArgCheckEntryOffset();
  } else if (hasBaselineScript()) {
    jitCodeRaw_ = baseline->method()->raw();
    jitCodeSkipArgCheck_ = jitCodeRaw_;
  } else {
    jitCodeRaw_ = rt->jitRuntime()->interpreterStub().value;
    jitCodeSkipArgCheck_ = jitCodeRaw_;
  }
  MOZ_ASSERT(jitCodeRaw_);
  MOZ_ASSERT(jitCodeSkipArgCheck_);
}

bool JSScript::hasLoops() {
  if (!hasTrynotes()) {
    return false;
  }
  for (const JSTryNote& tn : trynotes()) {
    switch (tn.kind) {
      case JSTRY_FOR_IN:
      case JSTRY_FOR_OF:
      case JSTRY_LOOP:
        return true;
      case JSTRY_CATCH:
      case JSTRY_FINALLY:
      case JSTRY_FOR_OF_ITERCLOSE:
      case JSTRY_DESTRUCTURING:
        break;
      default:
        MOZ_ASSERT(false, "Add new try note type to JSScript::hasLoops");
        break;
    }
  }
  return false;
}

bool JSScript::mayReadFrameArgsDirectly() {
  return argumentsHasVarBinding() || hasRest();
}

void JSScript::AutoDelazify::holdScript(JS::HandleFunction fun) {
  if (fun) {
    if (fun->realm()->isSelfHostingRealm()) {
      // The self-hosting realm is shared across runtimes, so we can't use
      // JSAutoRealm: it could cause races. Functions in the self-hosting
      // realm will never be lazy, so we can safely assume we don't have
      // to delazify.
      script_ = fun->nonLazyScript();
    } else {
      JSAutoRealm ar(cx_, fun);
      script_ = JSFunction::getOrCreateScript(cx_, fun);
      if (script_) {
        oldDoNotRelazify_ = script_->hasFlag(MutableFlags::DoNotRelazify);
        script_->setDoNotRelazify(true);
      }
    }
  }
}

void JSScript::AutoDelazify::dropScript() {
  // Don't touch script_ if it's in the self-hosting realm, see the comment
  // in holdScript.
  if (script_ && !script_->realm()->isSelfHostingRealm()) {
    script_->setDoNotRelazify(oldDoNotRelazify_);
  }
  script_ = nullptr;
}

JS::ubi::Base::Size JS::ubi::Concrete<JSScript>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  Size size = gc::Arena::thingSize(get().asTenured().getAllocKind());

  size += get().sizeOfData(mallocSizeOf);
  size += get().sizeOfTypeScript(mallocSizeOf);

  size_t baselineSize = 0;
  size_t baselineStubsSize = 0;
  jit::AddSizeOfBaselineData(&get(), mallocSizeOf, &baselineSize,
                             &baselineStubsSize);
  size += baselineSize;
  size += baselineStubsSize;

  size += jit::SizeOfIonData(&get(), mallocSizeOf);

  MOZ_ASSERT(size > 0);
  return size;
}

const char* JS::ubi::Concrete<JSScript>::scriptFilename() const {
  return get().filename();
}

JS::ubi::Node::Size JS::ubi::Concrete<js::LazyScript>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  Size size = gc::Arena::thingSize(get().asTenured().getAllocKind());
  size += get().sizeOfExcludingThis(mallocSizeOf);
  return size;
}

const char* JS::ubi::Concrete<js::LazyScript>::scriptFilename() const {
  auto source = get().sourceObject().source();
  if (!source) {
    return nullptr;
  }

  return source->filename();
}
