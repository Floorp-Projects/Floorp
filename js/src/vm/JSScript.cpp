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
#include "jit/JitOptions.h"
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
#include "vm/HelperThreads.h"  // js::RunPendingSourceCompressions
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

#include "debugger/DebugAPI-inl.h"
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
  for (GCPtrAtom& elem : lazy->closedOverBindings()) {
    uint8_t endOfScopeSentinel;
    if (mode == XDR_ENCODE) {
      atom = elem.get();
      endOfScopeSentinel = !atom;
    }

    MOZ_TRY(xdr->codeUint8(&endOfScopeSentinel));

    if (endOfScopeSentinel) {
      atom = nullptr;
    } else {
      MOZ_TRY(XDRAtom(xdr, &atom));
    }

    if (mode == XDR_DECODE) {
      elem.init(atom);
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
  MOZ_ASSERT_IF(mode == XDR_ENCODE, script->maybeLazyScript());
  MOZ_ASSERT_IF(mode == XDR_ENCODE, !lazy->numInnerFunctions());

  JSContext* cx = xdr->cx();

  uint32_t immutableFlags;
  uint32_t numClosedOverBindings;
  {
    uint32_t sourceStart = script->sourceStart();
    uint32_t sourceEnd = script->sourceEnd();
    uint32_t toStringStart = script->toStringStart();
    uint32_t toStringEnd = script->toStringEnd();
    uint32_t lineno = script->lineno();
    uint32_t column = script->column();
    uint32_t numFieldInitializers;

    if (mode == XDR_ENCODE) {
      immutableFlags = lazy->immutableFlags();
      numClosedOverBindings = lazy->numClosedOverBindings();
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
      if (fun->kind() == FunctionFlags::FunctionKind::ClassConstructor) {
        numFieldInitializers =
            (uint32_t)lazy->getFieldInitializers().numFieldInitializers;
      } else {
        numFieldInitializers = UINT32_MAX;
      }
    }

    MOZ_TRY(xdr->codeUint32(&immutableFlags));
    MOZ_TRY(xdr->codeUint32(&numFieldInitializers));
    MOZ_TRY(xdr->codeUint32(&numClosedOverBindings));

    if (mode == XDR_DECODE) {
      RootedScriptSourceObject sourceObject(cx, script->sourceObject());
      lazy.set(LazyScript::CreateForXDR(
          cx, numClosedOverBindings, /* numInnerFunctions = */ 0, fun, script,
          enclosingScope, sourceObject, immutableFlags, sourceStart, sourceEnd,
          toStringStart, toStringEnd, lineno, column));
      if (!lazy) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }

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

static inline uint32_t FindScopeIndex(mozilla::Span<const JS::GCCellPtr> scopes,
                                      Scope& scope) {
  unsigned length = scopes.size();
  for (uint32_t i = 0; i < length; ++i) {
    if (scopes[i].asCell() == &scope) {
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
            FindScopeIndex(data->gcthings(), *funEnclosingScope);
      }

      MOZ_TRY(xdr->codeUint32(&funEnclosingScopeIndex));

      if (mode == XDR_DECODE) {
        funEnclosingScope =
            &data->gcthings()[funEnclosingScopeIndex].as<Scope>();
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
                          bool isFirstScope, MutableHandleScope scope) {
  JSContext* cx = xdr->cx();

  ScopeKind scopeKind;
  RootedScope enclosing(cx);
  uint32_t enclosingIndex = 0;

  // The enclosingScope is encoded using an integer index into the scope array.
  // This means that scopes must be topologically sorted.
  if (mode == XDR_ENCODE) {
    scopeKind = scope->kind();

    if (isFirstScope) {
      enclosingIndex = UINT32_MAX;
    } else {
      MOZ_ASSERT(scope->enclosing());
      enclosingIndex = FindScopeIndex(data->gcthings(), *scope->enclosing());
    }
  }

  MOZ_TRY(xdr->codeEnum32(&scopeKind));
  MOZ_TRY(xdr->codeUint32(&enclosingIndex));

  if (mode == XDR_DECODE) {
    if (isFirstScope) {
      MOZ_ASSERT(enclosingIndex == UINT32_MAX);
      enclosing = scriptEnclosingScope;
    } else {
      enclosing = &data->gcthings()[enclosingIndex].as<Scope>();
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
    case ScopeKind::FunctionLexical:
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
static XDRResult XDRScriptGCThing(XDRState<mode>* xdr, PrivateScriptData* data,
                                  HandleScriptSourceObject sourceObject,
                                  HandleScope scriptEnclosingScope,
                                  HandleFunction fun, bool* isFirstScope,
                                  JS::GCCellPtr* thingp) {
  JSContext* cx = xdr->cx();

  enum class GCThingTag { Object, Scope, BigInt };

  JS::GCCellPtr thing;

  GCThingTag tag;
  if (mode == XDR_ENCODE) {
    thing = *thingp;
    if (thing.is<JSObject>()) {
      tag = GCThingTag::Object;
    } else if (thing.is<Scope>()) {
      tag = GCThingTag::Scope;
    } else {
      MOZ_ASSERT(thing.is<BigInt>());
      tag = GCThingTag::BigInt;
    }
  }

  MOZ_TRY(xdr->codeEnum32(&tag));

  switch (tag) {
    case GCThingTag::Object: {
      RootedObject obj(cx);
      if (mode == XDR_ENCODE) {
        obj = &thing.as<JSObject>();
      }
      MOZ_TRY(XDRInnerObject(xdr, data, sourceObject, &obj));
      if (mode == XDR_DECODE) {
        *thingp = JS::GCCellPtr(obj.get());
      }
      break;
    }
    case GCThingTag::Scope: {
      RootedScope scope(cx);
      if (mode == XDR_ENCODE) {
        scope = &thing.as<Scope>();
      }
      MOZ_TRY(XDRScope(xdr, data, scriptEnclosingScope, fun, *isFirstScope,
                       &scope));
      if (mode == XDR_DECODE) {
        *thingp = JS::GCCellPtr(scope.get());
      }
      *isFirstScope = false;
      break;
    }
    case GCThingTag::BigInt: {
      RootedBigInt bi(cx);
      if (mode == XDR_ENCODE) {
        bi = &thing.as<BigInt>();
      }
      MOZ_TRY(XDRBigInt(xdr, &bi));
      if (mode == XDR_DECODE) {
        *thingp = JS::GCCellPtr(bi.get());
      }
      break;
    }
    default:
      // Fail in debug, but only soft-fail in release.
      MOZ_ASSERT(false, "Bad XDR GCThingTag");
      return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
  }
  return Ok();
}

js::ScriptSource* js::BaseScript::maybeForwardedScriptSource() const {
  JSObject* source = MaybeForwarded(sourceObject());

  // This may be called during GC. It's OK not to expose the source object
  // here as it doesn't escape.
  return UncheckedUnwrapWithoutExpose(source)
      ->as<ScriptSourceObject>()
      .source();
}

template <XDRMode mode>
/* static */
XDRResult js::PrivateScriptData::XDR(XDRState<mode>* xdr, HandleScript script,
                                     HandleScriptSourceObject sourceObject,
                                     HandleScope scriptEnclosingScope,
                                     HandleFunction fun) {
  uint32_t ngcthings = 0;

  JSContext* cx = xdr->cx();
  PrivateScriptData* data = nullptr;

  if (mode == XDR_ENCODE) {
    data = script->data_;

    ngcthings = data->gcthings().size();
  }

  MOZ_TRY(xdr->codeUint32(&ngcthings));

  if (mode == XDR_DECODE) {
    if (!JSScript::createPrivateScriptData(cx, script, ngcthings)) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }

    data = script->data_;
  }

  bool isFirstScope = true;
  for (JS::GCCellPtr& gcThing : data->gcthings()) {
    MOZ_TRY(XDRScriptGCThing(xdr, data, sourceObject, scriptEnclosingScope, fun,
                             &isFirstScope, &gcThing));
  }

  // Verify marker to detect data corruption after decoding GC things. A
  // mismatch here indicates we will almost certainly crash in release.
  MOZ_TRY(xdr->codeMarker(0xF83B989A));

  return Ok();
}

/* static */ size_t ImmutableScriptData::AllocationSize(
    uint32_t codeLength, uint32_t noteLength, uint32_t numResumeOffsets,
    uint32_t numScopeNotes, uint32_t numTryNotes) {
  size_t size = sizeof(ImmutableScriptData);

  size += sizeof(Flags);
  size += codeLength * sizeof(jsbytecode);
  size += noteLength * sizeof(jssrcnote);

  unsigned numOptionalArrays = unsigned(numResumeOffsets > 0) +
                               unsigned(numScopeNotes > 0) +
                               unsigned(numTryNotes > 0);
  size += numOptionalArrays * sizeof(Offset);

  size += numResumeOffsets * sizeof(uint32_t);
  size += numScopeNotes * sizeof(ScopeNote);
  size += numTryNotes * sizeof(JSTryNote);

  return size;
}

// Placement-new elements of an array. This should optimize away for types with
// trivial default initiation.
template <typename T>
void ImmutableScriptData::initElements(size_t offset, size_t length) {
  uintptr_t base = reinterpret_cast<uintptr_t>(this);
  DefaultInitializeElements<T>(reinterpret_cast<void*>(base + offset), length);
}

// Initialize the optional arrays in the trailing allocation. This is a set of
// offsets that delimit each optional array followed by the arrays themselves.
// See comment before 'ImmutableScriptData' for more details.
void ImmutableScriptData::initOptionalArrays(size_t* pcursor,
                                             ImmutableScriptData::Flags* flags,
                                             uint32_t numResumeOffsets,
                                             uint32_t numScopeNotes,
                                             uint32_t numTryNotes) {
  size_t cursor = (*pcursor);

  // The byte arrays must have already been padded.
  MOZ_ASSERT(cursor % sizeof(uint32_t) == 0);

  // Each non-empty optional array needs will need an offset to its end.
  unsigned numOptionalArrays = unsigned(numResumeOffsets > 0) +
                               unsigned(numScopeNotes > 0) +
                               unsigned(numTryNotes > 0);

  // Default-initialize the optional-offsets.
  static_assert(alignof(ImmutableScriptData) >= alignof(Offset),
                "Incompatible alignment");
  initElements<Offset>(cursor, numOptionalArrays);
  cursor += numOptionalArrays * sizeof(Offset);

  // Offset between optional-offsets table and the optional arrays. This is
  // later used to access the optional-offsets table as well as first optional
  // array.
  optArrayOffset_ = cursor;

  // Each optional array that follows must store an end-offset in the offset
  // table. Assign table entries by using this 'offsetIndex'. The index 0 is
  // reserved for implicit value 'optArrayOffset'.
  int offsetIndex = 0;

  // Default-initialize optional 'resumeOffsets'.
  MOZ_ASSERT(resumeOffsetsOffset() == cursor);
  if (numResumeOffsets > 0) {
    static_assert(sizeof(Offset) >= alignof(uint32_t),
                  "Incompatible alignment");
    initElements<uint32_t>(cursor, numResumeOffsets);
    cursor += numResumeOffsets * sizeof(uint32_t);
    setOptionalOffset(++offsetIndex, cursor);
  }
  flags->resumeOffsetsEndIndex = offsetIndex;

  // Default-initialize optional 'scopeNotes'.
  MOZ_ASSERT(scopeNotesOffset() == cursor);
  if (numScopeNotes > 0) {
    static_assert(sizeof(uint32_t) >= alignof(ScopeNote),
                  "Incompatible alignment");
    initElements<ScopeNote>(cursor, numScopeNotes);
    cursor += numScopeNotes * sizeof(ScopeNote);
    setOptionalOffset(++offsetIndex, cursor);
  }
  flags->scopeNotesEndIndex = offsetIndex;

  // Default-initialize optional 'tryNotes'
  MOZ_ASSERT(tryNotesOffset() == cursor);
  if (numTryNotes > 0) {
    static_assert(sizeof(ScopeNote) >= alignof(JSTryNote),
                  "Incompatible alignment");
    initElements<JSTryNote>(cursor, numTryNotes);
    cursor += numTryNotes * sizeof(JSTryNote);
    setOptionalOffset(++offsetIndex, cursor);
  }
  flags->tryNotesEndIndex = offsetIndex;

  MOZ_ASSERT(endOffset() == cursor);
  (*pcursor) = cursor;
}

ImmutableScriptData::ImmutableScriptData(uint32_t codeLength,
                                         uint32_t noteLength,
                                         uint32_t numResumeOffsets,
                                         uint32_t numScopeNotes,
                                         uint32_t numTryNotes)
    : codeLength_(codeLength) {
  // Variable-length data begins immediately after ImmutableScriptData itself.
  size_t cursor = sizeof(*this);

  // The following arrays are byte-aligned with additional padding to ensure
  // that together they maintain uint32_t-alignment.
  {
    MOZ_ASSERT(cursor % CodeNoteAlign == 0);

    // Zero-initialize 'flags'
    static_assert(CodeNoteAlign >= alignof(Flags), "Incompatible alignment");
    new (offsetToPointer<void>(cursor)) Flags{};
    cursor += sizeof(Flags);

    static_assert(alignof(Flags) >= alignof(jsbytecode),
                  "Incompatible alignment");
    initElements<jsbytecode>(cursor, codeLength);
    cursor += codeLength * sizeof(jsbytecode);

    static_assert(alignof(jsbytecode) >= alignof(jssrcnote),
                  "Incompatible alignment");
    initElements<jssrcnote>(cursor, noteLength);
    cursor += noteLength * sizeof(jssrcnote);

    MOZ_ASSERT(cursor % CodeNoteAlign == 0);
  }

  // Initialization for remaining arrays.
  initOptionalArrays(&cursor, &flagsRef(), numResumeOffsets, numScopeNotes,
                     numTryNotes);

  // Check that we correctly recompute the expected values.
  MOZ_ASSERT(this->codeLength() == codeLength);
  MOZ_ASSERT(this->noteLength() == noteLength);

  // Sanity check
  MOZ_ASSERT(AllocationSize(codeLength, noteLength, numResumeOffsets,
                            numScopeNotes, numTryNotes) == cursor);
}

template <XDRMode mode>
/* static */
XDRResult ImmutableScriptData::XDR(XDRState<mode>* xdr, HandleScript script) {
  uint32_t codeLength = 0;
  uint32_t noteLength = 0;
  uint32_t numResumeOffsets = 0;
  uint32_t numScopeNotes = 0;
  uint32_t numTryNotes = 0;

  JSContext* cx = xdr->cx();
  ImmutableScriptData* isd = nullptr;

  if (mode == XDR_ENCODE) {
    isd = script->immutableScriptData();

    codeLength = isd->codeLength();
    noteLength = isd->noteLength();

    numResumeOffsets = isd->resumeOffsets().size();
    numScopeNotes = isd->scopeNotes().size();
    numTryNotes = isd->tryNotes().size();
  }

  MOZ_TRY(xdr->codeUint32(&codeLength));
  MOZ_TRY(xdr->codeUint32(&noteLength));
  MOZ_TRY(xdr->codeUint32(&numResumeOffsets));
  MOZ_TRY(xdr->codeUint32(&numScopeNotes));
  MOZ_TRY(xdr->codeUint32(&numTryNotes));

  if (mode == XDR_DECODE) {
    if (!script->createImmutableScriptData(cx, codeLength, noteLength,
                                           numResumeOffsets, numScopeNotes,
                                           numTryNotes)) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }

    isd = script->immutableScriptData();
  }

  MOZ_TRY(xdr->codeUint32(&isd->mainOffset));
  MOZ_TRY(xdr->codeUint32(&isd->nfixed));
  MOZ_TRY(xdr->codeUint32(&isd->nslots));
  MOZ_TRY(xdr->codeUint32(&isd->bodyScopeIndex));
  MOZ_TRY(xdr->codeUint32(&isd->numICEntries));
  MOZ_TRY(xdr->codeUint16(&isd->funLength));
  MOZ_TRY(xdr->codeUint16(&isd->numBytecodeTypeSets));

  JS_STATIC_ASSERT(sizeof(jsbytecode) == 1);
  JS_STATIC_ASSERT(sizeof(jssrcnote) == 1);

  jsbytecode* code = isd->code();
  jssrcnote* notes = isd->notes();
  MOZ_TRY(xdr->codeBytes(code, codeLength));
  MOZ_TRY(xdr->codeBytes(notes, noteLength));

  for (uint32_t& elem : isd->resumeOffsets()) {
    MOZ_TRY(xdr->codeUint32(&elem));
  }

  for (ScopeNote& elem : isd->scopeNotes()) {
    MOZ_TRY(elem.XDR(xdr));
  }

  for (JSTryNote& elem : isd->tryNotes()) {
    MOZ_TRY(elem.XDR(xdr));
  }

  return Ok();
}

template
    /* static */
    XDRResult
    ImmutableScriptData::XDR(XDRState<XDR_ENCODE>* xdr, HandleScript script);

template
    /* static */
    XDRResult
    ImmutableScriptData::XDR(XDRState<XDR_DECODE>* xdr, HandleScript script);

/* static */ size_t RuntimeScriptData::AllocationSize(uint32_t natoms) {
  size_t size = sizeof(RuntimeScriptData);

  size += natoms * sizeof(GCPtrAtom);

  return size;
}

// Placement-new elements of an array. This should optimize away for types with
// trivial default initiation.
template <typename T>
void RuntimeScriptData::initElements(size_t offset, size_t length) {
  uintptr_t base = reinterpret_cast<uintptr_t>(this);
  DefaultInitializeElements<T>(reinterpret_cast<void*>(base + offset), length);
}

RuntimeScriptData::RuntimeScriptData(uint32_t natoms) : natoms_(natoms) {
  // Variable-length data begins immediately after RuntimeScriptData itself.
  size_t cursor = sizeof(*this);

  // Default-initialize trailing arrays.

  static_assert(alignof(RuntimeScriptData) >= alignof(GCPtrAtom),
                "Incompatible alignment");
  initElements<GCPtrAtom>(cursor, natoms);
  cursor += natoms * sizeof(GCPtrAtom);

  // Check that we correctly recompute the expected values.
  MOZ_ASSERT(this->natoms() == natoms);

  // Sanity check
  MOZ_ASSERT(AllocationSize(natoms) == cursor);
}

template <XDRMode mode>
/* static */
XDRResult RuntimeScriptData::XDR(XDRState<mode>* xdr, HandleScript script) {
  uint32_t natoms = 0;

  JSContext* cx = xdr->cx();
  RuntimeScriptData* rsd = nullptr;

  if (mode == XDR_ENCODE) {
    rsd = script->scriptData();

    natoms = rsd->natoms();
  }

  MOZ_TRY(xdr->codeUint32(&natoms));

  if (mode == XDR_DECODE) {
    if (!script->createScriptData(cx, natoms)) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }

    rsd = script->scriptData();
  }

  {
    RootedAtom atom(cx);
    GCPtrAtom* vector = rsd->atoms();

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

  MOZ_TRY(ImmutableScriptData::XDR<mode>(xdr, script));

  return Ok();
}

template
    /* static */
    XDRResult
    RuntimeScriptData::XDR(XDRState<XDR_ENCODE>* xdr, HandleScript script);

template
    /* static */
    XDRResult
    RuntimeScriptData::XDR(XDRState<XDR_DECODE>* xdr, HandleScript script);

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

  // NOTE: |mutableFlags| are not preserved by XDR.

  JSContext* cx = xdr->cx();
  RootedScript script(cx);

  // Instrumented scripts cannot be encoded, as they have extra instructions
  // which are not normally present. Globals with instrumentation enabled must
  // compile scripts via the bytecode emitter, which will insert these
  // instructions.
  if (xdr->hasOptions() ? !!xdr->options().instrumentationKinds
                        : !!cx->global()->getInstrumentationHolder()) {
    return xdr->fail(JS::TranscodeResult_Failure);
  }

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
    if (script->maybeLazyScript()) {
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

    immutableFlags = script->immutableFlags();
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
    script->immutableFlags_ = immutableFlags;

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
  MOZ_TRY(RuntimeScriptData::XDR<mode>(xdr, script));

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
    if (!fun && !cx->isHelperThreadContext()) {
      DebugAPI::onNewScript(cx, script);
    }
  }

  MOZ_ASSERT(script->code(), "Where's our bytecode?");
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
    uint32_t immutableFlags;
    uint32_t numFieldInitializers;
    uint32_t numClosedOverBindings;
    uint32_t numInnerFunctions;

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
      immutableFlags = lazy->immutableFlags();
      if (fun->kind() == FunctionFlags::FunctionKind::ClassConstructor) {
        numFieldInitializers =
            (uint32_t)lazy->getFieldInitializers().numFieldInitializers;
      } else {
        numFieldInitializers = UINT32_MAX;
      }
      numClosedOverBindings = lazy->numClosedOverBindings();
      numInnerFunctions = lazy->numInnerFunctions();
    }

    MOZ_TRY(xdr->codeUint32(&sourceStart));
    MOZ_TRY(xdr->codeUint32(&sourceEnd));
    MOZ_TRY(xdr->codeUint32(&toStringStart));
    MOZ_TRY(xdr->codeUint32(&toStringEnd));
    MOZ_TRY(xdr->codeUint32(&lineno));
    MOZ_TRY(xdr->codeUint32(&column));
    MOZ_TRY(xdr->codeUint32(&immutableFlags));
    MOZ_TRY(xdr->codeUint32(&numFieldInitializers));
    MOZ_TRY(xdr->codeUint32(&numClosedOverBindings));
    MOZ_TRY(xdr->codeUint32(&numInnerFunctions));

    if (mode == XDR_DECODE) {
      lazy.set(LazyScript::CreateForXDR(
          cx, numClosedOverBindings, numInnerFunctions, fun, nullptr,
          enclosingScope, sourceObject, immutableFlags, sourceStart, sourceEnd,
          toStringStart, toStringEnd, lineno, column));
      if (!lazy) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }

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
    for (GCPtrFunction& elem : lazy->innerFunctions()) {
      if (mode == XDR_ENCODE) {
        func = elem.get();
      }

      MOZ_TRY(XDRInterpretedFunction(xdr, nullptr, sourceObject, &func));

      if (mode == XDR_DECODE) {
        elem.init(func);
        if (elem->isInterpretedLazy()) {
          elem->lazyScript()->setEnclosingLazyScript(lazy);
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

void JSScript::setDefaultClassConstructorSpan(
    js::ScriptSourceObject* sourceObject, uint32_t start, uint32_t end,
    unsigned line, unsigned column) {
  MOZ_ASSERT(compartment() == sourceObject->compartment());
  MOZ_ASSERT(isDefaultClassConstructor());
  sourceObject_ = sourceObject;
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
  setIonScript(rt->defaultFreeOp(), ionScript);
}

void JSScript::setIonScript(JSFreeOp* fop, js::jit::IonScript* ionScript) {
  MOZ_ASSERT_IF(ionScript != ION_DISABLED_SCRIPT,
                !baselineScript()->hasPendingIonBuilder());
  if (hasIonScript()) {
    js::jit::IonScript::writeBarrierPre(zone(), ion);
    clearIonScript(fop);
  }
  ion = ionScript;
  MOZ_ASSERT_IF(hasIonScript(), hasBaselineScript());
  if (hasIonScript()) {
    AddCellMemory(this, ion->allocBytes(), js::MemoryUse::IonScript);
  }
  updateJitCodeRaw(fop->runtime());
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

void ScriptSourceObject::finalize(JSFreeOp* fop, JSObject* obj) {
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

class ScriptSource::LoadSourceMatcher {
  JSContext* const cx_;
  ScriptSource* const ss_;
  bool* const loaded_;

 public:
  explicit LoadSourceMatcher(JSContext* cx, ScriptSource* ss, bool* loaded)
      : cx_(cx), ss_(ss), loaded_(loaded) {}

  template <typename Unit, SourceRetrievable CanRetrieve>
  bool operator()(const Compressed<Unit, CanRetrieve>&) const {
    *loaded_ = true;
    return true;
  }

  template <typename Unit, SourceRetrievable CanRetrieve>
  bool operator()(const Uncompressed<Unit, CanRetrieve>&) const {
    *loaded_ = true;
    return true;
  }

  template <typename Unit>
  bool operator()(const Retrievable<Unit>&) {
    if (!cx_->runtime()->sourceHook.ref()) {
      *loaded_ = false;
      return true;
    }

    size_t length;

    // The first argument is just for overloading -- its value doesn't matter.
    if (!tryLoadAndSetSource(Unit('0'), &length)) {
      return false;
    }

    return true;
  }

  bool operator()(const Missing&) const {
    *loaded_ = false;
    return true;
  }

  bool operator()(const BinAST&) const {
    *loaded_ = false;
    return true;
  }

 private:
  bool tryLoadAndSetSource(const Utf8Unit&, size_t* length) const {
    char* utf8Source;
    if (!cx_->runtime()->sourceHook->load(cx_, ss_->filename(), nullptr,
                                          &utf8Source, length)) {
      return false;
    }

    if (!utf8Source) {
      *loaded_ = false;
      return true;
    }

    if (!ss_->setRetrievedSource(
            cx_, EntryUnits<Utf8Unit>(reinterpret_cast<Utf8Unit*>(utf8Source)),
            *length)) {
      return false;
    }

    *loaded_ = true;
    return true;
  }

  bool tryLoadAndSetSource(const char16_t&, size_t* length) const {
    char16_t* utf16Source;
    if (!cx_->runtime()->sourceHook->load(cx_, ss_->filename(), &utf16Source,
                                          nullptr, length)) {
      return false;
    }

    if (!utf16Source) {
      *loaded_ = false;
      return true;
    }

    if (!ss_->setRetrievedSource(cx_, EntryUnits<char16_t>(utf16Source),
                                 *length)) {
      return false;
    }

    *loaded_ = true;
    return true;
  }
};

/* static */
bool ScriptSource::loadSource(JSContext* cx, ScriptSource* ss, bool* loaded) {
  return ss->data.match(LoadSourceMatcher(cx, ss, loaded));
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
  MOZ_ASSERT(ssc.ss->isCompressed<Unit>());

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
  const CompressedData<Unit>& c = *compressedData<Unit>();

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
void ScriptSource::convertToCompressedSource(SharedImmutableString compressed,
                                             size_t uncompressedLength) {
  MOZ_ASSERT(isUncompressed<Unit>());
  MOZ_ASSERT(uncompressedData<Unit>()->length() == uncompressedLength);

  if (data.is<Uncompressed<Unit, SourceRetrievable::Yes>>()) {
    data = SourceType(Compressed<Unit, SourceRetrievable::Yes>(
        std::move(compressed), uncompressedLength));
  } else {
    data = SourceType(Compressed<Unit, SourceRetrievable::No>(
        std::move(compressed), uncompressedLength));
  }
}

template <typename Unit>
void ScriptSource::performDelayedConvertToCompressedSource() {
  // There might not be a conversion to compressed source happening at all.
  if (pendingCompressed_.empty()) {
    return;
  }

  CompressedData<Unit>& pending =
      pendingCompressed_.ref<CompressedData<Unit>>();

  convertToCompressedSource<Unit>(std::move(pending.raw),
                                  pending.uncompressedLength);

  pendingCompressed_.destroy();
}

template <typename Unit>
ScriptSource::PinnedUnits<Unit>::~PinnedUnits() {
  if (units_) {
    MOZ_ASSERT(*stack_ == this);
    *stack_ = prev_;
    if (!prev_) {
      source_->performDelayedConvertToCompressedSource<Unit>();
    }
  }
}

template <typename Unit>
const Unit* ScriptSource::units(JSContext* cx,
                                UncompressedSourceCache::AutoHoldEntry& holder,
                                size_t begin, size_t len) {
  MOZ_ASSERT(begin <= length());
  MOZ_ASSERT(begin + len <= length());

  if (isUncompressed<Unit>()) {
    const Unit* units = uncompressedData<Unit>()->units();
    if (!units) {
      return nullptr;
    }
    return units + begin;
  }

  if (data.is<Missing>()) {
    MOZ_CRASH("ScriptSource::units() on ScriptSource with missing source");
  }

  if (data.is<Retrievable<Unit>>()) {
    MOZ_CRASH("ScriptSource::units() on ScriptSource with retrievable source");
  }

  MOZ_ASSERT(isCompressed<Unit>());

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
MOZ_MUST_USE bool ScriptSource::setUncompressedSourceHelper(
    JSContext* cx, EntryUnits<Unit>&& source, size_t length,
    SourceRetrievable retrievable) {
  auto& cache = cx->zone()->runtimeFromAnyThread()->sharedImmutableStrings();

  auto uniqueChars = SourceTypeTraits<Unit>::toCacheable(std::move(source));
  auto deduped = cache.getOrCreate(std::move(uniqueChars), length);
  if (!deduped) {
    ReportOutOfMemory(cx);
    return false;
  }

  if (retrievable == SourceRetrievable::Yes) {
    data = SourceType(
        Uncompressed<Unit, SourceRetrievable::Yes>(std::move(*deduped)));
  } else {
    data = SourceType(
        Uncompressed<Unit, SourceRetrievable::No>(std::move(*deduped)));
  }
  return true;
}

template <typename Unit>
MOZ_MUST_USE bool ScriptSource::setRetrievedSource(JSContext* cx,
                                                   EntryUnits<Unit>&& source,
                                                   size_t length) {
  MOZ_ASSERT(data.is<Retrievable<Unit>>(),
             "retrieved source can only overwrite the corresponding "
             "retrievable source");
  return setUncompressedSourceHelper(cx, std::move(source), length,
                                     SourceRetrievable::Yes);
}

#if defined(JS_BUILD_BINAST)

MOZ_MUST_USE bool ScriptSource::setBinASTSourceCopy(JSContext* cx,
                                                    const uint8_t* buf,
                                                    size_t len) {
  MOZ_ASSERT(data.is<Missing>());

  auto& cache = cx->zone()->runtimeFromAnyThread()->sharedImmutableStrings();
  auto deduped = cache.getOrCreate(reinterpret_cast<const char*>(buf), len);
  if (!deduped) {
    ReportOutOfMemory(cx);
    return false;
  }

  data = SourceType(BinAST(std::move(*deduped), nullptr));
  return true;
}

const uint8_t* ScriptSource::binASTSource() {
  MOZ_ASSERT(hasBinASTSource());
  return reinterpret_cast<const uint8_t*>(data.as<BinAST>().string.chars());
}

#endif /* JS_BUILD_BINAST */

bool ScriptSource::tryCompressOffThread(JSContext* cx) {
  // Beware: |js::SynchronouslyCompressSource| assumes that this function is
  // only called once, just after a script has been compiled, and it's never
  // called at some random time after that.  If multiple calls of this can ever
  // occur, that function may require changes.

  if (!hasUncompressedSource()) {
    // This excludes compressed, missing, retrievable, and BinAST source.
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
void ScriptSource::triggerConvertToCompressedSource(
    SharedImmutableString compressed, size_t uncompressedLength) {
  MOZ_ASSERT(isUncompressed<Unit>(),
             "should only be triggering compressed source installation to "
             "overwrite identically-encoded uncompressed source");
  MOZ_ASSERT(uncompressedData<Unit>()->length() == uncompressedLength);

  // If units aren't pinned -- and they probably won't be, we'd have to have a
  // GC in the small window of time where a |PinnedUnits| was live -- then we
  // can immediately convert.
  if (MOZ_LIKELY(!pinnedUnitsStack_)) {
    convertToCompressedSource<Unit>(std::move(compressed), uncompressedLength);
    return;
  }

  // Otherwise, set aside the compressed-data info.  The conversion is performed
  // when the last |PinnedUnits| dies.
  MOZ_ASSERT(pendingCompressed_.empty(),
             "shouldn't be multiple conversions happening");
  pendingCompressed_.construct<CompressedData<Unit>>(std::move(compressed),
                                                     uncompressedLength);
}

template <typename Unit>
MOZ_MUST_USE bool ScriptSource::initializeWithUnretrievableCompressedSource(
    JSContext* cx, UniqueChars&& compressed, size_t rawLength,
    size_t sourceLength) {
  MOZ_ASSERT(data.is<Missing>(), "shouldn't be double-initializing");
  MOZ_ASSERT(compressed != nullptr);

  auto& cache = cx->zone()->runtimeFromAnyThread()->sharedImmutableStrings();
  auto deduped = cache.getOrCreate(std::move(compressed), rawLength);
  if (!deduped) {
    ReportOutOfMemory(cx);
    return false;
  }

  MOZ_ASSERT(pinnedUnitsStack_ == nullptr,
             "shouldn't be initializing a ScriptSource while its characters "
             "are pinned -- that only makes sense with a ScriptSource actively "
             "being inspected");

  data = SourceType(Compressed<Unit, SourceRetrievable::No>(std::move(*deduped),
                                                            sourceLength));

  return true;
}

template <typename Unit>
bool ScriptSource::assignSource(JSContext* cx,
                                const ReadOnlyCompileOptions& options,
                                SourceText<Unit>& srcBuf) {
  MOZ_ASSERT(data.is<Missing>(),
             "source assignment should only occur on fresh ScriptSources");

  if (options.discardSource) {
    return true;
  }

  if (options.sourceIsLazy) {
    data = SourceType(Retrievable<Unit>());
    return true;
  }

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

  data = SourceType(
      Uncompressed<Unit, SourceRetrievable::No>(std::move(*deduped)));
  return true;
}

template bool ScriptSource::assignSource(JSContext* cx,
                                         const ReadOnlyCompileOptions& options,
                                         SourceText<char16_t>& srcBuf);
template bool ScriptSource::assignSource(JSContext* cx,
                                         const ReadOnlyCompileOptions& options,
                                         SourceText<Utf8Unit>& srcBuf);

void ScriptSource::trace(JSTracer* trc) {
#ifdef JS_BUILD_BINAST
  if (data.is<BinAST>()) {
    if (auto& metadata = data.as<BinAST>().metadata) {
      metadata->trace(trc);
    }
  }
#else
  MOZ_ASSERT(!data.is<BinAST>());
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
  MOZ_ASSERT(source->isUncompressed<Unit>());

  // Try to keep the maximum memory usage down by only allocating half the
  // size of the string, first.
  size_t inputBytes = source->length() * sizeof(Unit);
  size_t firstSize = inputBytes / 2;
  UniqueChars compressed(js_pod_malloc<char>(firstSize));
  if (!compressed) {
    return;
  }

  const Unit* chars = source->uncompressedData<Unit>()->units();
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

  template <typename Unit, SourceRetrievable CanRetrieve>
  void operator()(const ScriptSource::Uncompressed<Unit, CanRetrieve>&) {
    task_->workEncodingSpecific<Unit>();
  }

  template <typename T>
  void operator()(const T&) {
    MOZ_CRASH(
        "why are we compressing missing, missing-but-retrievable, "
        "already-compressed, or BinAST source?");
  }
};

void ScriptSource::performTaskWork(SourceCompressionTask* task) {
  MOZ_ASSERT(hasUncompressedSource());
  data.match(SourceCompressionTask::PerformTaskWork(task));
}

void SourceCompressionTask::runTask() {
  if (shouldCancel()) {
    return;
  }

  ScriptSource* source = sourceHolder_.get();
  MOZ_ASSERT(source->hasUncompressedSource());

  source->performTaskWork(this);
}

void ScriptSource::triggerConvertToCompressedSourceFromTask(
    SharedImmutableString compressed) {
  data.match(TriggerConvertToCompressedSourceFromTask(this, compressed));
}

void SourceCompressionTask::complete() {
  if (!shouldCancel() && resultString_.isSome()) {
    ScriptSource* source = sourceHolder_.get();
    source->triggerConvertToCompressedSourceFromTask(std::move(*resultString_));
  }
}

bool js::SynchronouslyCompressSource(JSContext* cx,
                                     JS::Handle<JSScript*> script) {
  MOZ_ASSERT(!cx->isHelperThreadContext(),
             "should only sync-compress on the main thread");

  // Finish all pending source compressions, including the single compression
  // task that may have been created (by |ScriptSource::tryCompressOffThread|)
  // just after the script was compiled.  Because we have flushed this queue,
  // no code below needs to synchronize with an off-thread parse task that
  // assumes the immutability of a |ScriptSource|'s data.
  //
  // This *may* end up compressing |script|'s source.  If it does -- we test
  // this below -- that takes care of things.  But if it doesn't, we will
  // synchronously compress ourselves (and as noted above, this won't race
  // anything).
  RunPendingSourceCompressions(cx->runtime());

  ScriptSource* ss = script->scriptSource();
  MOZ_ASSERT(!ss->pinnedUnitsStack_,
             "can't synchronously compress while source units are in use");

  // In principle a previously-triggered compression on a helper thread could
  // have already completed.  If that happens, there's nothing more to do.
  if (ss->hasCompressedSource()) {
    return true;
  }

  MOZ_ASSERT(ss->hasUncompressedSource(),
             "shouldn't be compressing uncompressible source");

  // Use an explicit scope to delineate the lifetime of |task|, for simplicity.
  {
#ifdef DEBUG
    uint32_t sourceRefs = ss->refs;
#endif
    MOZ_ASSERT(sourceRefs > 0, "at least |script| here should have a ref");

    // |SourceCompressionTask::shouldCancel| can periodically result in source
    // compression being canceled if we're not careful.  Guarantee that two refs
    // to |ss| are always live in this function (at least one preexisting and
    // one held by the task) so that compression is never canceled.
    auto task = MakeUnique<SourceCompressionTask>(cx->runtime(), ss);
    if (!task) {
      ReportOutOfMemory(cx);
      return false;
    }

    MOZ_ASSERT(ss->refs > sourceRefs, "must have at least two refs now");

    // Attempt to compress.  This may not succeed if OOM happens, but (because
    // it ordinarily happens on a helper thread) no error will ever be set here.
    MOZ_ASSERT(!cx->isExceptionPending());
    task->runTask();
    MOZ_ASSERT(!cx->isExceptionPending());

    // Convert |ss| from uncompressed to compressed data.
    task->complete();

    MOZ_ASSERT(!cx->isExceptionPending());
  }

  // The only way source won't be compressed here is if OOM happened.
  return ss->hasCompressedSource();
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
MOZ_MUST_USE bool ScriptSource::initializeUnretrievableUncompressedSource(
    JSContext* cx, EntryUnits<Unit>&& source, size_t length) {
  MOZ_ASSERT(data.is<Missing>(), "must be initializing a fresh ScriptSource");
  return setUncompressedSourceHelper(cx, std::move(source), length,
                                     SourceRetrievable::No);
}

template <typename Unit>
struct UnretrievableSourceDecoder {
  XDRState<XDR_DECODE>* const xdr_;
  ScriptSource* const scriptSource_;
  const uint32_t uncompressedLength_;

 public:
  UnretrievableSourceDecoder(XDRState<XDR_DECODE>* xdr,
                             ScriptSource* scriptSource,
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

    if (!scriptSource_->initializeUnretrievableUncompressedSource(
            xdr_->cx(), std::move(sourceUnits), uncompressedLength_)) {
      return xdr_->fail(JS::TranscodeResult_Throw);
    }

    return Ok();
  }
};

namespace js {

template <>
XDRResult ScriptSource::xdrUnretrievableUncompressedSource<XDR_DECODE>(
    XDRState<XDR_DECODE>* xdr, uint8_t sourceCharSize,
    uint32_t uncompressedLength) {
  MOZ_ASSERT(sourceCharSize == 1 || sourceCharSize == 2);

  if (sourceCharSize == 1) {
    UnretrievableSourceDecoder<Utf8Unit> decoder(xdr, this, uncompressedLength);
    return decoder.decode();
  }

  UnretrievableSourceDecoder<char16_t> decoder(xdr, this, uncompressedLength);
  return decoder.decode();
}

}  // namespace js

template <typename Unit>
struct UnretrievableSourceEncoder {
  XDRState<XDR_ENCODE>* const xdr_;
  ScriptSource* const source_;
  const uint32_t uncompressedLength_;

  UnretrievableSourceEncoder(XDRState<XDR_ENCODE>* xdr, ScriptSource* source,
                             uint32_t uncompressedLength)
      : xdr_(xdr), source_(source), uncompressedLength_(uncompressedLength) {}

  XDRResult encode() {
    Unit* sourceUnits =
        const_cast<Unit*>(source_->uncompressedData<Unit>()->units());

    return xdr_->codeChars(sourceUnits, uncompressedLength_);
  }
};

namespace js {

template <>
XDRResult ScriptSource::xdrUnretrievableUncompressedSource<XDR_ENCODE>(
    XDRState<XDR_ENCODE>* xdr, uint8_t sourceCharSize,
    uint32_t uncompressedLength) {
  MOZ_ASSERT(sourceCharSize == 1 || sourceCharSize == 2);

  if (sourceCharSize == 1) {
    UnretrievableSourceEncoder<Utf8Unit> encoder(xdr, this, uncompressedLength);
    return encoder.encode();
  }

  UnretrievableSourceEncoder<char16_t> encoder(xdr, this, uncompressedLength);
  return encoder.encode();
}

}  // namespace js

template <typename Unit, XDRMode mode>
/* static */
XDRResult ScriptSource::codeUncompressedData(XDRState<mode>* const xdr,
                                             ScriptSource* const ss) {
  static_assert(std::is_same<Unit, Utf8Unit>::value ||
                    std::is_same<Unit, char16_t>::value,
                "should handle UTF-8 and UTF-16");

  if (mode == XDR_ENCODE) {
    MOZ_ASSERT(ss->isUncompressed<Unit>());
  } else {
    MOZ_ASSERT(ss->data.is<Missing>());
  }

  uint32_t uncompressedLength;
  if (mode == XDR_ENCODE) {
    uncompressedLength = ss->uncompressedData<Unit>()->length();
  }
  MOZ_TRY(xdr->codeUint32(&uncompressedLength));

  return ss->xdrUnretrievableUncompressedSource(xdr, sizeof(Unit),
                                                uncompressedLength);
}

template <typename Unit, XDRMode mode>
/* static */
XDRResult ScriptSource::codeCompressedData(XDRState<mode>* const xdr,
                                           ScriptSource* const ss) {
  static_assert(std::is_same<Unit, Utf8Unit>::value ||
                    std::is_same<Unit, char16_t>::value,
                "should handle UTF-8 and UTF-16");

  if (mode == XDR_ENCODE) {
    MOZ_ASSERT(ss->isCompressed<Unit>());
  } else {
    MOZ_ASSERT(ss->data.is<Missing>());
  }

  uint32_t uncompressedLength;
  if (mode == XDR_ENCODE) {
    uncompressedLength = ss->data.as<Compressed<Unit, SourceRetrievable::No>>()
                             .uncompressedLength;
  }
  MOZ_TRY(xdr->codeUint32(&uncompressedLength));

  uint32_t compressedLength;
  if (mode == XDR_ENCODE) {
    compressedLength =
        ss->data.as<Compressed<Unit, SourceRetrievable::No>>().raw.length();
  }
  MOZ_TRY(xdr->codeUint32(&compressedLength));

  if (mode == XDR_DECODE) {
    // Compressed data is always single-byte chars.
    auto bytes = xdr->cx()->template make_pod_array<char>(compressedLength);
    if (!bytes) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    MOZ_TRY(xdr->codeBytes(bytes.get(), compressedLength));

    if (!ss->initializeWithUnretrievableCompressedSource<Unit>(
            xdr->cx(), std::move(bytes), compressedLength,
            uncompressedLength)) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  } else {
    void* bytes = const_cast<char*>(ss->compressedData<Unit>()->raw.chars());
    MOZ_TRY(xdr->codeBytes(bytes, compressedLength));
  }

  return Ok();
}

template <typename Unit,
          template <typename U, SourceRetrievable CanRetrieve> class Data,
          XDRMode mode>
/* static */
void ScriptSource::codeRetrievable(ScriptSource* const ss) {
  static_assert(std::is_same<Unit, Utf8Unit>::value ||
                    std::is_same<Unit, char16_t>::value,
                "should handle UTF-8 and UTF-16");

  if (mode == XDR_ENCODE) {
    MOZ_ASSERT((ss->data.is<Data<Unit, SourceRetrievable::Yes>>()));
  } else {
    MOZ_ASSERT(ss->data.is<Missing>());
    ss->data = SourceType(Retrievable<Unit>());
  }
}

template <XDRMode mode>
/* static */
XDRResult ScriptSource::codeBinASTData(XDRState<mode>* const xdr,
                                       ScriptSource* const ss) {
#if !defined(JS_BUILD_BINAST)
  return xdr->fail(JS::TranscodeResult_Throw);
#else
  if (mode == XDR_ENCODE) {
    MOZ_ASSERT(ss->data.is<BinAST>());
  } else {
    MOZ_ASSERT(ss->data.is<Missing>());
  }

  // XDR the length of the BinAST data.
  uint32_t binASTLength;
  if (mode == XDR_ENCODE) {
    binASTLength = ss->data.as<BinAST>().string.length();
  }
  MOZ_TRY(xdr->codeUint32(&binASTLength));

  // XDR the BinAST data.
  Maybe<SharedImmutableString> binASTData;
  if (mode == XDR_DECODE) {
    auto bytes =
        xdr->cx()->template make_pod_array<char>(Max<size_t>(binASTLength, 1));
    if (!bytes) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    MOZ_TRY(xdr->codeBytes(bytes.get(), binASTLength));

    auto& cache =
        xdr->cx()->zone()->runtimeFromAnyThread()->sharedImmutableStrings();
    binASTData = cache.getOrCreate(std::move(bytes), binASTLength);
    if (!binASTData) {
      ReportOutOfMemory(xdr->cx());
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  } else {
    void* bytes = ss->binASTData();
    MOZ_TRY(xdr->codeBytes(bytes, binASTLength));
  }

  // XDR any BinAST metadata.
  uint8_t hasMetadata;
  if (mode == XDR_ENCODE) {
    hasMetadata = ss->data.as<BinAST>().metadata != nullptr;
  }
  MOZ_TRY(xdr->codeUint8(&hasMetadata));

  Rooted<UniquePtr<frontend::BinASTSourceMetadata>> freshMetadata(xdr->cx());
  if (hasMetadata) {
    // If we're decoding, this is a *mutable borrowed* reference to the
    // |UniquePtr| stored in the |Rooted| above, and the |UniquePtr| will be
    // filled with freshly allocated metadata.
    //
    // If we're encoding, this is an *immutable borrowed* reference to the
    // |UniquePtr| stored in |ss|.  (Immutable up to GCs transparently moving
    // things around, that is.)
    UniquePtr<frontend::BinASTSourceMetadata>& binASTMetadata =
        mode == XDR_DECODE ? freshMetadata.get()
                           : ss->data.as<BinAST>().metadata;

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
      void* mem = js_calloc(frontend::BinASTSourceMetadata::totalSize(
          numBinASTKinds, numStrings));
      if (!mem) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }

      auto metadata =
          new (mem) frontend::BinASTSourceMetadata(numBinASTKinds, numStrings);
      binASTMetadata.reset(metadata);
    }

    MOZ_ASSERT(binASTMetadata != nullptr);

    frontend::BinASTKind* binASTKindBase = binASTMetadata->binASTKindBase();
    for (uint32_t i = 0; i < numBinASTKinds; i++) {
      MOZ_TRY(xdr->codeEnum32(&binASTKindBase[i]));
    }

    RootedAtom atom(xdr->cx());
    JSAtom** atomsBase = binASTMetadata->atomsBase();
    auto slices = binASTMetadata->sliceBase();
    const char* sourceBase =
        (mode == XDR_ENCODE ? ss->data.as<BinAST>().string : *binASTData)
            .chars();

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
  }

  if (mode == XDR_DECODE) {
    MOZ_ASSERT(binASTData.isSome());
    MOZ_ASSERT(freshMetadata != nullptr);

    MOZ_ASSERT(ss->data.is<Missing>(),
               "should only be initializing a fresh ScriptSource");

    ss->data = SourceType(
        BinAST(std::move(*binASTData), std::move(freshMetadata.get())));
  }

  MOZ_ASSERT(binASTData.isNothing());
  MOZ_ASSERT(freshMetadata == nullptr);
  MOZ_ASSERT(ss->data.is<BinAST>());

  return Ok();
#endif  // !defined(JS_BUILD_BINAST)
}

template <typename Unit, XDRMode mode>
/* static */
void ScriptSource::codeRetrievableData(ScriptSource* ss) {
  // There's nothing to code for retrievable data.  Just be sure to set
  // retrievable data when decoding.
  if (mode == XDR_ENCODE) {
    MOZ_ASSERT(ss->data.is<Retrievable<Unit>>());
  } else {
    MOZ_ASSERT(ss->data.is<Missing>());
    ss->data = SourceType(Retrievable<Unit>());
  }
}

template <XDRMode mode>
/* static */
XDRResult ScriptSource::xdrData(XDRState<mode>* const xdr,
                                ScriptSource* const ss) {
  // The order here corresponds to the type order in |ScriptSource::SourceType|
  // so number->internal Variant tag is a no-op.
  enum class DataType {
    CompressedUtf8Retrievable,
    UncompressedUtf8Retrievable,
    CompressedUtf8NotRetrievable,
    UncompressedUtf8NotRetrievable,
    CompressedUtf16Retrievable,
    UncompressedUtf16Retrievable,
    CompressedUtf16NotRetrievable,
    UncompressedUtf16NotRetrievable,
    RetrievableUtf8,
    RetrievableUtf16,
    Missing,
    BinAST,
  };

  DataType tag;
  {
    // This is terrible, but we can't do better.  When |mode == XDR_DECODE| we
    // don't have a |ScriptSource::data| |Variant| to match -- the entire XDR
    // idiom for tagged unions depends on coding a tag-number, then the
    // corresponding tagged data.  So we must manually define a tag-enum, code
    // it, then switch on it (and ignore the |Variant::match| API).
    class XDRDataTag {
     public:
      DataType operator()(const Compressed<Utf8Unit, SourceRetrievable::Yes>&) {
        return DataType::CompressedUtf8Retrievable;
      }
      DataType operator()(
          const Uncompressed<Utf8Unit, SourceRetrievable::Yes>&) {
        return DataType::UncompressedUtf8Retrievable;
      }
      DataType operator()(const Compressed<Utf8Unit, SourceRetrievable::No>&) {
        return DataType::CompressedUtf8NotRetrievable;
      }
      DataType operator()(
          const Uncompressed<Utf8Unit, SourceRetrievable::No>&) {
        return DataType::UncompressedUtf8NotRetrievable;
      }
      DataType operator()(const Compressed<char16_t, SourceRetrievable::Yes>&) {
        return DataType::CompressedUtf16Retrievable;
      }
      DataType operator()(
          const Uncompressed<char16_t, SourceRetrievable::Yes>&) {
        return DataType::UncompressedUtf16Retrievable;
      }
      DataType operator()(const Compressed<char16_t, SourceRetrievable::No>&) {
        return DataType::CompressedUtf16NotRetrievable;
      }
      DataType operator()(
          const Uncompressed<char16_t, SourceRetrievable::No>&) {
        return DataType::UncompressedUtf16NotRetrievable;
      }
      DataType operator()(const Retrievable<Utf8Unit>&) {
        return DataType::RetrievableUtf8;
      }
      DataType operator()(const Retrievable<char16_t>&) {
        return DataType::RetrievableUtf16;
      }
      DataType operator()(const Missing&) { return DataType::Missing; }
      DataType operator()(const BinAST&) { return DataType::BinAST; }
    };

    uint8_t type;
    if (mode == XDR_ENCODE) {
      type = static_cast<uint8_t>(ss->data.match(XDRDataTag()));
    }
    MOZ_TRY(xdr->codeUint8(&type));

    if (type > static_cast<uint8_t>(DataType::BinAST)) {
      // Fail in debug, but only soft-fail in release, if the type is invalid.
      MOZ_ASSERT_UNREACHABLE("bad tag");
      return xdr->fail(JS::TranscodeResult_Failure_BadDecode);
    }

    tag = static_cast<DataType>(type);
  }

  switch (tag) {
    case DataType::CompressedUtf8Retrievable:
      ScriptSource::codeRetrievable<Utf8Unit, Compressed, mode>(ss);
      return Ok();

    case DataType::CompressedUtf8NotRetrievable:
      return ScriptSource::codeCompressedData<Utf8Unit>(xdr, ss);

    case DataType::UncompressedUtf8Retrievable:
      ScriptSource::codeRetrievable<Utf8Unit, Uncompressed, mode>(ss);
      return Ok();

    case DataType::UncompressedUtf8NotRetrievable:
      return ScriptSource::codeUncompressedData<Utf8Unit>(xdr, ss);

    case DataType::CompressedUtf16Retrievable:
      ScriptSource::codeRetrievable<char16_t, Compressed, mode>(ss);
      return Ok();

    case DataType::CompressedUtf16NotRetrievable:
      return ScriptSource::codeCompressedData<char16_t>(xdr, ss);

    case DataType::UncompressedUtf16Retrievable:
      ScriptSource::codeRetrievable<char16_t, Uncompressed, mode>(ss);
      return Ok();

    case DataType::UncompressedUtf16NotRetrievable:
      return ScriptSource::codeUncompressedData<char16_t>(xdr, ss);

    case DataType::Missing: {
      MOZ_ASSERT(ss->data.is<Missing>(),
                 "ScriptSource::data is initialized as missing, so neither "
                 "encoding nor decoding has to change anything");

      // There's no data to XDR for missing source.
      break;
    }

    case DataType::RetrievableUtf8:
      ScriptSource::codeRetrievableData<Utf8Unit, mode>(ss);
      return Ok();

    case DataType::RetrievableUtf16:
      ScriptSource::codeRetrievableData<char16_t, mode>(ss);
      return Ok();

    case DataType::BinAST:
      return codeBinASTData(xdr, ss);
  }

  // The range-check on |type| far above ought ensure the above |switch| is
  // exhaustive and all cases will return, but not all compilers understand
  // this.  Make the Missing case break to here so control obviously never flows
  // off the end.
  MOZ_ASSERT(tag == DataType::Missing);
  return Ok();
}

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

  MOZ_TRY(xdrData(xdr, ss));

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
    if (!cx->isHelperThreadContext() &&
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
 * [SMDOC] JSScript data layout (immutable)
 *
 * Script data that shareable across processes. There are no pointers (GC or
 * otherwise) and the data is relocatable.
 *
 * Array elements   Pointed to by         Length
 * --------------   -------------         ------
 * jsbytecode       code()                codeLength()
 * jsscrnote        notes()               noteLength()
 * uint32_t         resumeOffsets()
 * ScopeNote        scopeNotes()
 * JSTryNote        tryNotes()
 */

ImmutableScriptData* js::ImmutableScriptData::new_(
    JSContext* cx, uint32_t codeLength, uint32_t noteLength,
    uint32_t numResumeOffsets, uint32_t numScopeNotes, uint32_t numTryNotes) {
  // Compute size including trailing arrays
  size_t size = AllocationSize(codeLength, noteLength, numResumeOffsets,
                               numScopeNotes, numTryNotes);

  // Allocate contiguous raw buffer
  void* raw = cx->pod_malloc<uint8_t>(size);
  MOZ_ASSERT(uintptr_t(raw) % alignof(ImmutableScriptData) == 0);
  if (!raw) {
    return nullptr;
  }

  // Constuct the ImmutableScriptData. Trailing arrays are uninitialized but
  // GCPtrs are put into a safe state.
  return new (raw) ImmutableScriptData(codeLength, noteLength, numResumeOffsets,
                                       numScopeNotes, numTryNotes);
}

RuntimeScriptData* js::RuntimeScriptData::new_(JSContext* cx, uint32_t natoms) {
  // Compute size including trailing arrays
  size_t size = AllocationSize(natoms);

  // Allocate contiguous raw buffer
  void* raw = cx->pod_malloc<uint8_t>(size);
  MOZ_ASSERT(uintptr_t(raw) % alignof(RuntimeScriptData) == 0);
  if (!raw) {
    return nullptr;
  }

  // Constuct the RuntimeScriptData. Trailing arrays are uninitialized but
  // GCPtrs are put into a safe state.
  return new (raw) RuntimeScriptData(natoms);
}

bool JSScript::createScriptData(JSContext* cx, uint32_t natoms) {
  MOZ_ASSERT(!scriptData_);

  RefPtr<RuntimeScriptData> rsd(RuntimeScriptData::new_(cx, natoms));
  if (!rsd) {
    return false;
  }

  scriptData_ = std::move(rsd);
  return true;
}

bool JSScript::createImmutableScriptData(JSContext* cx, uint32_t codeLength,
                                         uint32_t noteLength,
                                         uint32_t numResumeOffsets,
                                         uint32_t numScopeNotes,
                                         uint32_t numTryNotes) {
#ifdef DEBUG
  // The compact arrays need to maintain uint32_t alignment. This should have
  // been done by padding out source notes.
  size_t byteArrayLength =
      sizeof(ImmutableScriptData::Flags) + codeLength + noteLength;
  MOZ_ASSERT(byteArrayLength % sizeof(uint32_t) == 0,
             "Source notes should have been padded already");
#endif

  MOZ_ASSERT(!scriptData_->isd_);

  js::UniquePtr<ImmutableScriptData> isd(
      ImmutableScriptData::new_(cx, codeLength, noteLength, numResumeOffsets,
                                numScopeNotes, numTryNotes));
  if (!isd) {
    return false;
  }

  scriptData_->isd_ = std::move(isd);
  return true;
}

void JSScript::freeScriptData() { scriptData_ = nullptr; }

// Takes owndership of the script's scriptData_ and either adds it into the
// runtime's RuntimeScriptDataTable or frees it if a matching entry already
// exists.
bool JSScript::shareScriptData(JSContext* cx) {
  RuntimeScriptData* rsd = scriptData();
  MOZ_ASSERT(rsd);
  MOZ_ASSERT(rsd->refCount() == 1);

  // Calculate the hash before taking the lock. Because the data is reference
  // counted, it also will be freed after releasing the lock if necessary.
  RuntimeScriptDataHasher::Lookup lookup(rsd);

  AutoLockScriptData lock(cx->runtime());

  RuntimeScriptDataTable::AddPtr p =
      cx->scriptDataTable(lock).lookupForAdd(lookup);
  if (p) {
    MOZ_ASSERT(rsd != *p);
    scriptData_ = *p;
  } else {
    if (!cx->scriptDataTable(lock).add(p, rsd)) {
      ReportOutOfMemory(cx);
      return false;
    }

    // Being in the table counts as a reference on the script data.
    rsd->AddRef();
  }

  // Refs: JSScript, RuntimeScriptDataTable
  MOZ_ASSERT(scriptData()->refCount() >= 2);

  return true;
}

void js::SweepScriptData(JSRuntime* rt) {
  // Entries are removed from the table when their reference count is one,
  // i.e. when the only reference to them is from the table entry.

  AutoLockScriptData lock(rt);
  RuntimeScriptDataTable& table = rt->scriptDataTable(lock);

  for (RuntimeScriptDataTable::Enum e(table); !e.empty(); e.popFront()) {
    RuntimeScriptData* scriptData = e.front();
    if (scriptData->refCount() == 1) {
      scriptData->Release();
      e.removeFront();
    }
  }
}

void js::FreeScriptData(JSRuntime* rt) {
  AutoLockScriptData lock(rt);

  RuntimeScriptDataTable& table = rt->scriptDataTable(lock);

  // The table should be empty unless the embedding leaked GC things.
  MOZ_ASSERT_IF(rt->gc.shutdownCollectedEverything(), table.empty());

#ifdef DEBUG
  size_t numLive = 0;
  size_t maxCells = 5;
  char* env = getenv("JS_GC_MAX_LIVE_CELLS");
  if (env && *env) {
    maxCells = atol(env);
  }
#endif

  for (RuntimeScriptDataTable::Enum e(table); !e.empty(); e.popFront()) {
#ifdef DEBUG
    if (++numLive <= maxCells) {
      RuntimeScriptData* scriptData = e.front();
      fprintf(stderr,
              "ERROR: GC found live RuntimeScriptData %p with ref count %d at "
              "shutdown\n",
              scriptData, scriptData->refCount());
    }
#endif
    js_free(e.front());
  }

#ifdef DEBUG
  if (numLive > 0) {
    fprintf(stderr, "ERROR: GC found %zu live RuntimeScriptData at shutdown\n",
            numLive);
  }
#endif

  table.clear();
}

/* static */
size_t PrivateScriptData::AllocationSize(uint32_t ngcthings) {
  size_t size = sizeof(PrivateScriptData);

  size += ngcthings * sizeof(JS::GCCellPtr);

  return size;
}

inline size_t PrivateScriptData::allocationSize() const {
  return AllocationSize(ngcthings);
}

// Placement-new elements of an array. This should optimize away for types with
// trivial default initiation.
template <typename T>
void PrivateScriptData::initElements(size_t offset, size_t length) {
  void* raw = offsetToPointer<void>(offset);
  DefaultInitializeElements<T>(raw, length);
}

// Initialize and placement-new the trailing arrays.
PrivateScriptData::PrivateScriptData(uint32_t ngcthings)
    : ngcthings(ngcthings) {
  // Variable-length data begins immediately after PrivateScriptData itself.
  // NOTE: Alignment is computed using cursor/offset so the alignment of
  // PrivateScriptData must be stricter than any trailing array type.
  size_t cursor = sizeof(*this);

  // Layout and initialize the gcthings array.
  {
    MOZ_ASSERT(ngcthings > 0);

    static_assert(alignof(PrivateScriptData) >= alignof(JS::GCCellPtr),
                  "Incompatible alignment");
    initElements<JS::GCCellPtr>(cursor, ngcthings);

    cursor += ngcthings * sizeof(JS::GCCellPtr);
  }

  // Sanity check
  MOZ_ASSERT(AllocationSize(ngcthings) == cursor);
}

/* static */
PrivateScriptData* PrivateScriptData::new_(JSContext* cx, uint32_t ngcthings) {
  // Allocate contiguous raw buffer for the trailing arrays.
  void* raw = cx->pod_malloc<uint8_t>(AllocationSize(ngcthings));
  MOZ_ASSERT(uintptr_t(raw) % alignof(PrivateScriptData) == 0);
  if (!raw) {
    return nullptr;
  }

  // Constuct the PrivateScriptData. Trailing arrays are uninitialized but
  // GCPtrs are put into a safe state.
  return new (raw) PrivateScriptData(ngcthings);
}

/* static */ bool PrivateScriptData::InitFromEmitter(
    JSContext* cx, js::HandleScript script, frontend::BytecodeEmitter* bce) {
  uint32_t ngcthings = bce->perScriptData().gcThingList().length();

  // Create and initialize PrivateScriptData
  if (!JSScript::createPrivateScriptData(cx, script, ngcthings)) {
    return false;
  }

  js::PrivateScriptData* data = script->data_;
  if (ngcthings) {
    bce->perScriptData().gcThingList().finish(data->gcthings());
  }

  return true;
}

void PrivateScriptData::trace(JSTracer* trc) {
  for (JS::GCCellPtr& elem : gcthings()) {
    gc::Cell* thing = elem.asCell();
    TraceManuallyBarrieredGenericPointerEdge(trc, &thing, "script-gcthing");
    if (MOZ_UNLIKELY(!thing)) {
      // NOTE: If we are clearing edges, also erase the type. This can happen
      // due to OOM triggering the ClearEdgesTracer.
      elem = JS::GCCellPtr();
    } else if (thing != elem.asCell()) {
      elem = JS::GCCellPtr(thing, elem.kind());
    }
  }
}

JSScript::JSScript(JS::Realm* realm, uint8_t* stubEntry,
                   HandleScriptSourceObject sourceObject, uint32_t sourceStart,
                   uint32_t sourceEnd, uint32_t toStringStart,
                   uint32_t toStringEnd)
    : js::BaseScript(stubEntry, sourceObject, sourceStart, sourceEnd,
                     toStringStart, toStringEnd),
      realm_(realm) {
  MOZ_ASSERT(JS::GetCompartmentForRealm(realm) == sourceObject->compartment());
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

  if (coverage::IsLCovEnabled()) {
    if (!script->initScriptName(cx)) {
      return nullptr;
    }
  }

  return script;
}

/* static */ JSScript* JSScript::CreateFromLazy(JSContext* cx,
                                                Handle<LazyScript*> lazy) {
  RootedScriptSourceObject sourceObject(cx, lazy->sourceObject());
  RootedScript script(
      cx,
      JSScript::New(cx, sourceObject, lazy->sourceStart(), lazy->sourceEnd(),
                    lazy->toStringStart(), lazy->toStringEnd()));
  if (!script) {
    return nullptr;
  }

  script->setFlag(MutableFlags::TrackRecordReplayProgress,
                  ShouldTrackRecordReplayProgress(script));

  if (coverage::IsLCovEnabled()) {
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
                                       uint32_t ngcthings) {
  cx->check(script);
  MOZ_ASSERT(!script->data_);

  PrivateScriptData* data = PrivateScriptData::new_(cx, ngcthings);
  if (!data) {
    return false;
  }

  script->data_ = data;
  AddCellMemory(script, data->allocationSize(), MemoryUse::ScriptPrivateData);

  return true;
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
    MOZ_ASSERT(bce->sc->asFunctionBox()->isNamedLambda());

    if (outerScope->hasEnvironment()) {
      return true;
    }
  }

  return false;
}

void JSScript::initFromFunctionBox(frontend::FunctionBox* funbox) {
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
  setFlag(ImmutableFlags::HasDirectEval, funbox->hasDirectEval());
  setFlag(ImmutableFlags::ShouldDeclareArguments, funbox->declaredArguments);

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
  MOZ_ASSERT(bce->perScriptData().atomIndices()->count() <= INDEX_LIMIT);
  MOZ_ASSERT(bce->perScriptData().gcThingList().length() <= INDEX_LIMIT);

  uint64_t nslots =
      bce->maxFixedSlots +
      static_cast<uint64_t>(bce->bytecodeSection().maxStackDepth());
  if (nslots > UINT32_MAX) {
    bce->reportError(nullptr, JSMSG_NEED_DIET, js_script_str);
    return false;
  }

  // Initialize POD fields
  script->lineno_ = bce->firstLine;
  script->column_ = bce->firstColumn;

  // Initialize script flags from BytecodeEmitter
  script->setFlag(ImmutableFlags::Strict, bce->sc->strict());
  script->setFlag(ImmutableFlags::BindingsAccessedDynamically,
                  bce->sc->bindingsAccessedDynamically());
  script->setFlag(ImmutableFlags::HasCallSiteObj, bce->hasCallSiteObj);
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

  // Create and initialize RuntimeScriptData/ImmutableScriptData
  if (!RuntimeScriptData::InitFromEmitter(cx, script, bce, nslots)) {
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
  bce->perScriptData().gcThingList().finishInnerFunctions();

#ifdef JS_STRUCTURED_SPEW
  // We want this to happen after line number initialization to allow filtering
  // to work.
  script->setSpewEnabled(cx->spewer().enabled(script));
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
#endif

size_t JSScript::computedSizeOfData() const { return data_->allocationSize(); }

size_t JSScript::sizeOfData(mozilla::MallocSizeOf mallocSizeOf) const {
  return mallocSizeOf(data_);
}

void JSScript::addSizeOfJitScript(mozilla::MallocSizeOf mallocSizeOf,
                                  size_t* sizeOfJitScript,
                                  size_t* sizeOfBaselineFallbackStubs) const {
  if (!jitScript_) {
    return;
  }

  jitScript_->addSizeOfIncludingThis(mallocSizeOf, sizeOfJitScript,
                                     sizeOfBaselineFallbackStubs);
}

js::GlobalObject& JSScript::uninlinedGlobal() const { return global(); }

void JSScript::finalize(JSFreeOp* fop) {
  // NOTE: this JSScript may be partially initialized at this point.  E.g. we
  // may have created it and partially initialized it with
  // JSScript::Create(), but not yet finished initializing it with
  // fullyInitFromEmitter().

  // Collect code coverage information for this script and all its inner
  // scripts, and store the aggregated information on the realm.
  MOZ_ASSERT_IF(hasScriptName(), coverage::IsLCovEnabled());
  if (coverage::IsLCovEnabled() && hasScriptName()) {
    realm()->lcovOutput.collectCodeCoverageInfo(realm(), this, getScriptName());
    destroyScriptName();
  }

  fop->runtime()->geckoProfiler().onScriptFinalized(this);

  jit::DestroyJitScripts(fop, this);

  destroyScriptCounts();
  DebugAPI::destroyDebugScript(fop, this);

#ifdef MOZ_VTUNE
  if (realm()->scriptVTuneIdMap) {
    // Note: we should only get here if the VTune JIT profiler is running.
    realm()->scriptVTuneIdMap->remove(this);
  }
#endif

  if (data_) {
    size_t size = computedSizeOfData();
    AlwaysPoison(data_, JS_POISONED_JSSCRIPT_DATA_PATTERN, size,
                 MemCheckKind::MakeNoAccess);
    fop->free_(this, data_, size, MemoryUse::ScriptPrivateData);
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
  FunctionFlags flags = srcFun->flags();
  if (srcFun->isSelfHostedBuiltin()) {
    // Functions in the self-hosting compartment are only extended in
    // debug mode. For top-level functions, FUNCTION_EXTENDED gets used by
    // the cloning algorithm. Do the same for inner functions here.
    allocKind = gc::AllocKind::FUNCTION_EXTENDED;
    flags.setIsExtended();
  }
  RootedAtom atom(cx, srcFun->displayAtom());
  if (atom) {
    cx->markAtom(atom);
  }
  RootedFunction clone(
      cx, NewFunctionWithProto(cx, nullptr, srcFun->nargs(), flags, nullptr,
                               atom, cloneProto, allocKind, TenuredObject));
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

static JSObject* CloneScriptObject(JSContext* cx, PrivateScriptData* srcData,
                                   HandleObject obj,
                                   Handle<ScriptSourceObject*> sourceObject,
                                   JS::HandleVector<StackGCCellPtr> gcThings) {
  if (obj->is<RegExpObject>()) {
    return CloneScriptRegExpObject(cx, obj->as<RegExpObject>());
  }

  if (obj->is<JSFunction>()) {
    HandleFunction innerFun = obj.as<JSFunction>();
    if (innerFun->isNative()) {
      if (cx->realm() != innerFun->realm()) {
        MOZ_ASSERT(innerFun->isAsmJSNative());
        JS_ReportErrorASCII(cx, "AsmJS modules do not yet support cloning.");
        return nullptr;
      }
      return innerFun;
    }

    if (innerFun->isInterpretedLazy()) {
      AutoRealm ar(cx, innerFun);
      if (!JSFunction::getOrCreateScript(cx, innerFun)) {
        return nullptr;
      }
    }

    Scope* enclosing = innerFun->nonLazyScript()->enclosingScope();
    uint32_t scopeIndex = FindScopeIndex(srcData->gcthings(), *enclosing);
    RootedScope enclosingClone(cx,
                               &gcThings[scopeIndex].get().get().as<Scope>());
    return CloneInnerInterpretedFunction(cx, enclosingClone, innerFun,
                                         sourceObject);
  }

  return DeepCloneObjectLiteral(cx, obj, TenuredObject);
}

/* static */
bool PrivateScriptData::Clone(JSContext* cx, HandleScript src, HandleScript dst,
                              MutableHandle<GCVector<Scope*>> scopes) {
  PrivateScriptData* srcData = src->data_;
  uint32_t ngcthings = srcData->gcthings().size();

  // Clone GC things.
  JS::RootedVector<StackGCCellPtr> gcThings(cx);
  size_t scopeIndex = 0;
  Rooted<ScriptSourceObject*> sourceObject(cx, dst->sourceObject());
  RootedObject obj(cx);
  RootedScope scope(cx);
  RootedScope enclosingScope(cx);
  RootedBigInt bigint(cx);
  for (JS::GCCellPtr gcThing : srcData->gcthings()) {
    if (gcThing.is<JSObject>()) {
      obj = &gcThing.as<JSObject>();
      JSObject* clone =
          CloneScriptObject(cx, srcData, obj, sourceObject, gcThings);
      if (!clone || !gcThings.append(JS::GCCellPtr(clone))) {
        return false;
      }
    } else if (gcThing.is<Scope>()) {
      // The passed in scopes vector contains body scopes that needed to be
      // cloned especially, depending on whether the script is a function or
      // global scope. Clone all other scopes.
      if (scopeIndex < scopes.length()) {
        if (!gcThings.append(JS::GCCellPtr(scopes[scopeIndex].get()))) {
          return false;
        }
      } else {
        scope = &gcThing.as<Scope>();
        uint32_t enclosingScopeIndex =
            FindScopeIndex(srcData->gcthings(), *scope->enclosing());
        enclosingScope = &gcThings[enclosingScopeIndex].get().get().as<Scope>();
        Scope* clone = Scope::clone(cx, scope, enclosingScope);
        if (!clone || !gcThings.append(JS::GCCellPtr(clone))) {
          return false;
        }
      }
      scopeIndex++;
    } else {
      bigint = &gcThing.as<BigInt>();
      BigInt* clone = bigint;
      if (cx->zone() != bigint->zone()) {
        clone = BigInt::copy(cx, bigint);
        if (!clone) {
          return false;
        }
      }
      if (!gcThings.append(JS::GCCellPtr(clone))) {
        return false;
      }
    }
  }

  // Create the new PrivateScriptData on |dst| and fill it in.
  if (!JSScript::createPrivateScriptData(cx, dst, ngcthings)) {
    return false;
  }

  PrivateScriptData* dstData = dst->data_;
  {
    auto array = dstData->gcthings();
    for (uint32_t i = 0; i < ngcthings; ++i) {
      array[i] = gcThings[i].get().get();
    }
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
  dst->immutableFlags_ = src->immutableFlags();

  dst->setFlag(JSScript::ImmutableFlags::HasNonSyntacticScope,
               scopes[0]->hasOnChain(ScopeKind::NonSyntactic));

  if (src->argumentsHasVarBinding()) {
    dst->setArgumentsHasVarBinding();
  }

  // Clone the PrivateScriptData into dst
  if (!PrivateScriptData::Clone(cx, src, dst, scopes)) {
    return nullptr;
  }

  // The RuntimeScriptData can be reused by any zone in the Runtime as long as
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
  const FunctionFlags preservedFlags = fun->flags();
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

/* static */ bool ImmutableScriptData::InitFromEmitter(
    JSContext* cx, js::HandleScript script, frontend::BytecodeEmitter* bce,
    uint32_t nslots) {
  size_t codeLength = bce->bytecodeSection().code().length();
  MOZ_RELEASE_ASSERT(codeLength <= frontend::MaxBytecodeLength);

  // There are 1-4 copies of SN_MAKE_TERMINATOR appended after the source
  // notes. These are a combination of sentinel and padding values.
  static_assert(frontend::MaxSrcNotesLength <= UINT32_MAX - CodeNoteAlign,
                "Length + CodeNoteAlign shouldn't overflow UINT32_MAX");
  size_t noteLength = bce->bytecodeSection().notes().length();
  MOZ_RELEASE_ASSERT(noteLength <= frontend::MaxSrcNotesLength);

  size_t nullLength = ComputeNotePadding(codeLength, noteLength);

  uint32_t numResumeOffsets =
      bce->bytecodeSection().resumeOffsetList().length();
  uint32_t numScopeNotes = bce->bytecodeSection().scopeNoteList().length();
  uint32_t numTryNotes = bce->bytecodeSection().tryNoteList().length();

  // Allocate ImmutableScriptData
  if (!script->createImmutableScriptData(
          cx, codeLength, noteLength + nullLength, numResumeOffsets,
          numScopeNotes, numTryNotes)) {
    return false;
  }
  js::ImmutableScriptData* data = script->immutableScriptData();

  // Initialize POD fields
  data->mainOffset = bce->mainOffset();
  data->nfixed = bce->maxFixedSlots;
  data->nslots = nslots;
  data->bodyScopeIndex = bce->bodyScopeIndex;
  data->numICEntries = bce->bytecodeSection().numICEntries();
  data->numBytecodeTypeSets =
      std::min<uint32_t>(uint32_t(JSScript::MaxBytecodeTypeSets),
                         bce->bytecodeSection().numTypeSets());

  if (bce->sc->isFunctionBox()) {
    data->funLength = bce->sc->asFunctionBox()->length;
  }

  // Initialize trailing arrays
  std::copy_n(bce->bytecodeSection().code().begin(), codeLength, data->code());
  std::copy_n(bce->bytecodeSection().notes().begin(), noteLength,
              data->notes());
  std::fill_n(data->notes() + noteLength, nullLength, SRC_NULL);

  bce->bytecodeSection().resumeOffsetList().finish(data->resumeOffsets());
  bce->bytecodeSection().scopeNoteList().finish(data->scopeNotes());
  bce->bytecodeSection().tryNoteList().finish(data->tryNotes());

  return true;
}

/* static */ bool RuntimeScriptData::InitFromEmitter(
    JSContext* cx, js::HandleScript script, frontend::BytecodeEmitter* bce,
    uint32_t nslots) {
  uint32_t natoms = bce->perScriptData().atomIndices()->count();

  // Allocate RuntimeScriptData
  if (!script->createScriptData(cx, natoms)) {
    return false;
  }
  js::RuntimeScriptData* data = script->scriptData();

  // Initialize trailing arrays
  InitAtomMap(*bce->perScriptData().atomIndices(), data->atoms());

  return ImmutableScriptData::InitFromEmitter(cx, script, bce, nslots);
}

void RuntimeScriptData::traceChildren(JSTracer* trc) {
  MOZ_ASSERT(refCount() != 0);

  for (uint32_t i = 0; i < natoms(); ++i) {
    TraceNullableEdge(trc, &atoms()[i], "atom");
  }
}

void RuntimeScriptData::markForCrossZone(JSContext* cx) {
  for (uint32_t i = 0; i < natoms(); ++i) {
    cx->markAtom(atoms()[i]);
  }
}

void JSScript::traceChildren(JSTracer* trc) {
  // NOTE: this JSScript may be partially initialized at this point.  E.g. we
  // may have created it and partially initialized it with
  // JSScript::Create(), but not yet finished initializing it with
  // fullyInitFromEmitter().

  // Trace base class fields.
  BaseScript::traceChildren(trc);

  MOZ_ASSERT_IF(trc->isMarkingTracer() &&
                    GCMarker::fromTracer(trc)->shouldCheckCompartments(),
                zone()->isCollecting());

  if (data_) {
    data_->trace(trc);
  }

  if (scriptData()) {
    scriptData()->traceChildren(trc);
  }

  if (maybeLazyScript()) {
    TraceManuallyBarrieredEdge(trc, &lazyScript, "lazyScript");
  }

  JSObject* global = realm()->unsafeUnbarrieredMaybeGlobal();
  MOZ_ASSERT(global);
  TraceManuallyBarrieredEdge(trc, &global, "script_global");

  jit::TraceJitScripts(trc, this);

  if (trc->isMarkingTracer()) {
    GCMarker::fromTracer(trc)->markImplicitEdges(this);
  }
}

void LazyScript::finalize(JSFreeOp* fop) {
  if (lazyData_) {
    fop->free_(this, lazyData_, lazyData_->allocationSize(),
               MemoryUse::LazyScriptData);
  }
}

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
void JSScript::argumentsOptimizationFailed(JSContext* cx, HandleScript script) {
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
    return;
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

/* static */ size_t LazyScriptData::AllocationSize(
    uint32_t numClosedOverBindings, uint32_t numInnerFunctions) {
  size_t size = sizeof(LazyScriptData);

  size += numClosedOverBindings * sizeof(GCPtrAtom);
  size += numInnerFunctions * sizeof(GCPtrFunction);

  return size;
}

inline size_t LazyScriptData::allocationSize() const {
  return AllocationSize(numClosedOverBindings_, numInnerFunctions_);
}

// Placement-new elements of an array. This should optimize away for types with
// trivial default initiation.
template <typename T>
void LazyScriptData::initElements(size_t offset, size_t length) {
  void* raw = offsetToPointer<void>(offset);
  DefaultInitializeElements<T>(raw, length);
}

LazyScriptData::LazyScriptData(uint32_t numClosedOverBindings,
                               uint32_t numInnerFunctions)
    : numClosedOverBindings_(numClosedOverBindings),
      numInnerFunctions_(numInnerFunctions) {
  // Variable-length data begins immediately after LazyScriptData itself.
  size_t cursor = sizeof(*this);

  // Default-initialize trailing arrays.

  static_assert(alignof(LazyScriptData) >= alignof(GCPtrAtom),
                "Incompatible alignment");
  initElements<GCPtrAtom>(cursor, numClosedOverBindings);
  cursor += numClosedOverBindings * sizeof(GCPtrAtom);

  static_assert(alignof(GCPtrAtom) >= alignof(GCPtrFunction),
                "Incompatible alignment");
  initElements<GCPtrFunction>(cursor, numInnerFunctions);
  cursor += numInnerFunctions * sizeof(GCPtrFunction);

  // Sanity check
  MOZ_ASSERT(AllocationSize(numClosedOverBindings, numInnerFunctions) ==
             cursor);
}

/* static */ LazyScriptData* LazyScriptData::new_(
    JSContext* cx, uint32_t numClosedOverBindings, uint32_t numInnerFunctions) {
  // Compute size including trailing arrays
  size_t size = AllocationSize(numClosedOverBindings, numInnerFunctions);

  // Allocate contiguous raw buffer
  void* raw = cx->pod_malloc<uint8_t>(size);
  MOZ_ASSERT(uintptr_t(raw) % alignof(LazyScriptData) == 0);
  if (!raw) {
    return nullptr;
  }

  // Constuct the LazyScriptData. Trailing arrays are uninitialized but
  // GCPtrs are put into a safe state.
  return new (raw) LazyScriptData(numClosedOverBindings, numInnerFunctions);
}

mozilla::Span<GCPtrAtom> LazyScriptData::closedOverBindings() {
  size_t offset = sizeof(LazyScriptData);
  return mozilla::MakeSpan(offsetToPointer<GCPtrAtom>(offset),
                           numClosedOverBindings_);
}

mozilla::Span<GCPtrFunction> LazyScriptData::innerFunctions() {
  size_t offset =
      sizeof(LazyScriptData) + sizeof(GCPtrAtom) * numClosedOverBindings_;
  return mozilla::MakeSpan(offsetToPointer<GCPtrFunction>(offset),
                           numInnerFunctions_);
}

void LazyScriptData::trace(JSTracer* trc) {
  if (numClosedOverBindings_) {
    auto array = closedOverBindings();
    TraceRange(trc, array.size(), array.data(), "closedOverBindings");
  }

  if (numInnerFunctions_) {
    auto array = innerFunctions();
    TraceRange(trc, array.size(), array.data(), "innerFunctions");
  }
}

LazyScript::LazyScript(JSFunction* fun, uint8_t* stubEntry,
                       ScriptSourceObject& sourceObject, LazyScriptData* data,
                       uint32_t immutableFlags, uint32_t sourceStart,
                       uint32_t sourceEnd, uint32_t toStringStart,
                       uint32_t toStringEnd, uint32_t lineno, uint32_t column)
    : BaseScript(stubEntry, &sourceObject, sourceStart, sourceEnd,
                 toStringStart, toStringEnd),
      script_(nullptr),
      function_(fun),
      lazyData_(data) {
  MOZ_ASSERT(function_);
  MOZ_ASSERT(sourceObject_);
  MOZ_ASSERT(function_->compartment() == sourceObject_->compartment());

  lineno_ = lineno;
  column_ = column;

  immutableFlags_ = immutableFlags;

  if (data) {
    AddCellMemory(this, data->allocationSize(), MemoryUse::LazyScriptData);
  }
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

/* static */
LazyScript* LazyScript::CreateRaw(JSContext* cx, uint32_t numClosedOverBindings,
                                  uint32_t numInnerFunctions,
                                  HandleFunction fun,
                                  HandleScriptSourceObject sourceObject,
                                  uint32_t immutableFlags, uint32_t sourceStart,
                                  uint32_t sourceEnd, uint32_t toStringStart,
                                  uint32_t toStringEnd, uint32_t lineno,
                                  uint32_t column) {
  cx->check(fun);

  MOZ_ASSERT(sourceObject);

  // Allocate a LazyScriptData if it will not be empty. Lazy class constructors
  // also need LazyScriptData for field lists.
  Rooted<UniquePtr<LazyScriptData>> data(cx);
  if (numClosedOverBindings || numInnerFunctions || fun->isClassConstructor()) {
    data.reset(
        LazyScriptData::new_(cx, numClosedOverBindings, numInnerFunctions));
    if (!data) {
      return nullptr;
    }
  }

  LazyScript* res = Allocate<LazyScript>(cx);
  if (!res) {
    return nullptr;
  }

  cx->realm()->scheduleDelazificationForDebugger();

#ifndef JS_CODEGEN_NONE
  uint8_t* stubEntry = cx->runtime()->jitRuntime()->interpreterStub().value;
#else
  uint8_t* stubEntry = nullptr;
#endif

  return new (res) LazyScript(fun, stubEntry, *sourceObject, data.release(),
                              immutableFlags, sourceStart, sourceEnd,
                              toStringStart, toStringEnd, lineno, column);
}

/* static */
LazyScript* LazyScript::Create(
    JSContext* cx, HandleFunction fun, HandleScriptSourceObject sourceObject,
    const frontend::AtomVector& closedOverBindings,
    const frontend::FunctionBoxVector& innerFunctionBoxes, uint32_t sourceStart,
    uint32_t sourceEnd, uint32_t toStringStart, uint32_t toStringEnd,
    uint32_t lineno, uint32_t column, frontend::ParseGoal parseGoal) {
  uint32_t immutableFlags = 0;
  if (parseGoal == frontend::ParseGoal::Module) {
    immutableFlags |= uint32_t(ImmutableFlags::IsModule);
  }

  LazyScript* res = LazyScript::CreateRaw(
      cx, closedOverBindings.length(), innerFunctionBoxes.length(), fun,
      sourceObject, immutableFlags, sourceStart, sourceEnd, toStringStart,
      toStringEnd, lineno, column);
  if (!res) {
    return nullptr;
  }

  mozilla::Span<GCPtrAtom> resClosedOverBindings = res->closedOverBindings();
  for (size_t i = 0; i < res->numClosedOverBindings(); i++) {
    resClosedOverBindings[i].init(closedOverBindings[i]);
  }

  mozilla::Span<GCPtrFunction> resInnerFunctions = res->innerFunctions();
  for (size_t i = 0; i < res->numInnerFunctions(); i++) {
    resInnerFunctions[i].init(innerFunctionBoxes[i]->function());
    if (resInnerFunctions[i]->isInterpretedLazy()) {
      resInnerFunctions[i]->lazyScript()->setEnclosingLazyScript(res);
    }
  }

  return res;
}

/* static */
LazyScript* LazyScript::CreateForXDR(
    JSContext* cx, uint32_t numClosedOverBindings, uint32_t numInnerFunctions,
    HandleFunction fun, HandleScript script, HandleScope enclosingScope,
    HandleScriptSourceObject sourceObject, uint32_t immutableFlags,
    uint32_t sourceStart, uint32_t sourceEnd, uint32_t toStringStart,
    uint32_t toStringEnd, uint32_t lineno, uint32_t column) {
  LazyScript* res = LazyScript::CreateRaw(
      cx, numClosedOverBindings, numInnerFunctions, fun, sourceObject,
      immutableFlags, sourceStart, sourceEnd, toStringStart, toStringEnd,
      lineno, column);
  if (!res) {
    return nullptr;
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
  uint8_t* jitCodeSkipArgCheck;
  if (hasBaselineScript() && baseline->hasPendingIonBuilder()) {
    MOZ_ASSERT(!isIonCompilingOffThread());
    jitCodeRaw_ = rt->jitRuntime()->lazyLinkStub().value;
    jitCodeSkipArgCheck = jitCodeRaw_;
  } else if (hasIonScript()) {
    jitCodeRaw_ = ion->method()->raw();
    jitCodeSkipArgCheck = jitCodeRaw_ + ion->getSkipArgCheckEntryOffset();
  } else if (hasBaselineScript()) {
    jitCodeRaw_ = baseline->method()->raw();
    jitCodeSkipArgCheck = jitCodeRaw_;
  } else if (jitScript() && js::jit::IsBaselineInterpreterEnabled()) {
    jitCodeRaw_ = rt->jitRuntime()->baselineInterpreter().codeRaw();
    jitCodeSkipArgCheck = jitCodeRaw_;
  } else {
    jitCodeRaw_ = rt->jitRuntime()->interpreterStub().value;
    jitCodeSkipArgCheck = jitCodeRaw_;
  }
  MOZ_ASSERT(jitCodeRaw_);
  MOZ_ASSERT(jitCodeSkipArgCheck);

  if (jitScript_) {
    jitScript_->jitCodeSkipArgCheck_ = jitCodeSkipArgCheck;
  }
}

bool JSScript::hasLoops() {
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

void JSScript::resetWarmUpCounterToDelayIonCompilation() {
  // Reset the warm-up count only if it's greater than the BaselineCompiler
  // threshold. We do this to ensure this has no effect on Baseline compilation
  // because we don't want scripts to get stuck in the (Baseline) interpreter in
  // pathological cases.

  if (warmUpCount > jit::JitOptions.baselineJitWarmUpThreshold) {
    incWarmUpResetCounter();
    warmUpCount = jit::JitOptions.baselineJitWarmUpThreshold;
  }
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

  size_t jitScriptSize = 0;
  size_t fallbackStubSize = 0;
  get().addSizeOfJitScript(mallocSizeOf, &jitScriptSize, &fallbackStubSize);
  size += jitScriptSize;
  size += fallbackStubSize;

  size_t baselineSize = 0;
  jit::AddSizeOfBaselineData(&get(), mallocSizeOf, &baselineSize);
  size += baselineSize;

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
  auto source = get().scriptSource();
  if (!source) {
    return nullptr;
  }

  return source->filename();
}
