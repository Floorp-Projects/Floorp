/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debugger/Source.h"

#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT
#include "mozilla/Maybe.h"       // for Some, Maybe, Nothing
#include "mozilla/Variant.h"     // for AsVariant, Variant

#include <stdint.h>  // for uint32_t
#include <string.h>  // for memcpy
#include <utility>   // for move

#include "jsapi.h"        // for JS_ReportErrorNumberASCII
#include "jsfriendapi.h"  // for GetErrorMessage, JS_NewUint8Array

#include "debugger/Debugger.h"  // for DebuggerSourceReferent, Debugger
#include "debugger/Script.h"    // for DebuggerScript
#include "gc/Tracer.h"  // for TraceManuallyBarrieredCrossCompartmentEdge
#include "js/CompilationAndEvaluation.h" // for Compile
#include "js/StableStringChars.h"  // for AutoStableStringChars
#include "vm/BytecodeUtil.h"       // for JSDVG_SEARCH_STACK
#include "vm/JSContext.h"          // for JSContext (ptr only)
#include "vm/JSObject.h"           // for JSObject, RequireObject
#include "vm/JSScript.h"           // for ScriptSource, ScriptSourceObject
#include "vm/ObjectGroup.h"        // for TenuredObject
#include "vm/StringType.h"         // for NewStringCopyZ, JSString (ptr only)
#include "vm/TypedArrayObject.h"   // for TypedArrayObject, JSObject::is
#include "wasm/WasmCode.h"         // for Metadata
#include "wasm/WasmDebug.h"        // for DebugState
#include "wasm/WasmInstance.h"     // for Instance
#include "wasm/WasmJS.h"           // for WasmInstanceObject
#include "wasm/WasmTypes.h"        // for Bytes, RootedWasmInstanceObject

#include "vm/JSObject-inl.h"      // for InitClass
#include "vm/NativeObject-inl.h"  // for NewNativeObjectWithGivenProto

namespace js {
class GlobalObject;
}

using namespace js;

using JS::AutoStableStringChars;
using mozilla::AsVariant;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

const JSClassOps DebuggerSource::classOps_ = {
    nullptr,                         /* addProperty */
    nullptr,                         /* delProperty */
    nullptr,                         /* enumerate   */
    nullptr,                         /* newEnumerate */
    nullptr,                         /* resolve     */
    nullptr,                         /* mayResolve  */
    nullptr,                         /* finalize    */
    nullptr,                         /* call        */
    nullptr,                         /* hasInstance */
    nullptr,                         /* construct   */
    CallTraceMethod<DebuggerSource>, /* trace */
};

const JSClass DebuggerSource::class_ = {
    "Source", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS),
    &classOps_};

/* static */
NativeObject* DebuggerSource::initClass(JSContext* cx,
                                        Handle<GlobalObject*> global,
                                        HandleObject debugCtor) {
  return InitClass(cx, debugCtor, nullptr, &class_, construct, 0, properties_,
                   methods_, nullptr, nullptr);
}

/* static */
DebuggerSource* DebuggerSource::create(JSContext* cx, HandleObject proto,
                                       Handle<DebuggerSourceReferent> referent,
                                       HandleNativeObject debugger) {
  NativeObject* obj =
      NewNativeObjectWithGivenProto(cx, &class_, proto, TenuredObject);
  if (!obj) {
    return nullptr;
  }
  RootedDebuggerSource sourceObj(cx, &obj->as<DebuggerSource>());
  sourceObj->setReservedSlot(OWNER_SLOT, ObjectValue(*debugger));
  referent.get().match(
      [&](auto sourceHandle) { sourceObj->setPrivateGCThing(sourceHandle); });

  return sourceObj;
}

// For internal use only.
NativeObject* DebuggerSource::getReferentRawObject() const {
  return static_cast<NativeObject*>(getPrivate());
}

DebuggerSourceReferent DebuggerSource::getReferent() const {
  if (NativeObject* referent = getReferentRawObject()) {
    if (referent->is<ScriptSourceObject>()) {
      return AsVariant(&referent->as<ScriptSourceObject>());
    }
    return AsVariant(&referent->as<WasmInstanceObject>());
  }
  return AsVariant(static_cast<ScriptSourceObject*>(nullptr));
}

void DebuggerSource::trace(JSTracer* trc) {
  // There is a barrier on private pointers, so the Unbarriered marking
  // is okay.
  if (JSObject* referent = getReferentRawObject()) {
    TraceManuallyBarrieredCrossCompartmentEdge(
        trc, static_cast<JSObject*>(this), &referent,
        "Debugger.Source referent");
    setPrivateUnbarriered(referent);
  }
}

/* static */
bool DebuggerSource::construct(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                            "Debugger.Source");
  return false;
}

/* static */
DebuggerSource* DebuggerSource::check(JSContext* cx, HandleValue thisv,
                                      const char* fnname) {
  JSObject* thisobj = RequireObject(cx, thisv);
  if (!thisobj) {
    return nullptr;
  }
  if (!thisobj->is<DebuggerSource>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger.Source",
                              fnname, thisobj->getClass()->name);
    return nullptr;
  }

  DebuggerSource* thisSourceObj = &thisobj->as<DebuggerSource>();

  if (!thisSourceObj->getReferentRawObject()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger.Source",
                              fnname, "prototype object");
    return nullptr;
  }

  return thisSourceObj;
}

template <typename ReferentT>
/* static */
DebuggerSource* DebuggerSource::checkThis(JSContext* cx, const CallArgs& args,
                                          const char* fnname,
                                          const char* refname) {
  DebuggerSource* thisobj = check(cx, args.thisv(), fnname);
  if (!thisobj) {
    return nullptr;
  }

  if (!thisobj->getReferent().is<ReferentT>()) {
    ReportValueError(cx, JSMSG_DEBUG_BAD_REFERENT, JSDVG_SEARCH_STACK,
                     args.thisv(), nullptr, refname);
    return nullptr;
  }

  return thisobj;
}

#define THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, fnname, args, obj, referent) \
  CallArgs args = CallArgsFromVp(argc, vp);                                  \
  RootedDebuggerSource obj(cx,                                               \
                           DebuggerSource::check(cx, args.thisv(), fnname)); \
  if (!obj) return false;                                                    \
  Rooted<DebuggerSourceReferent> referent(cx, obj->getReferent())

#define THIS_DEBUGSOURCE_SOURCE(cx, argc, vp, fnname, args, obj, sourceObject) \
  CallArgs args = CallArgsFromVp(argc, vp);                                    \
  RootedDebuggerSource obj(cx, DebuggerSource::checkThis<ScriptSourceObject*>( \
                                   cx, args, fnname, "a JS source"));          \
  if (!obj) return false;                                                      \
  RootedScriptSourceObject sourceObject(                                       \
      cx, obj->getReferent().as<ScriptSourceObject*>())

class DebuggerSourceGetTextMatcher {
  JSContext* cx_;

 public:
  explicit DebuggerSourceGetTextMatcher(JSContext* cx) : cx_(cx) {}

  using ReturnType = JSString*;

  ReturnType match(HandleScriptSourceObject sourceObject) {
    ScriptSource* ss = sourceObject->source();
    bool hasSourceText;
    if (!ScriptSource::loadSource(cx_, ss, &hasSourceText)) {
      return nullptr;
    }
    if (!hasSourceText) {
      return NewStringCopyZ<CanGC>(cx_, "[no source]");
    }

    if (ss->isFunctionBody()) {
      return ss->functionBodyString(cx_);
    }

    return ss->substring(cx_, 0, ss->length());
  }

  ReturnType match(Handle<WasmInstanceObject*> instanceObj) {
    wasm::Instance& instance = instanceObj->instance();
    const char* msg;
    if (!instance.debugEnabled()) {
      msg = "Restart with developer tools open to view WebAssembly source.";
    } else {
      msg = "[debugger missing wasm binary-to-text conversion]";
    }
    return NewStringCopyZ<CanGC>(cx_, msg);
  }
};

/* static */
bool DebuggerSource::getText(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "(get text)", args, obj, referent);
  Value textv = obj->getReservedSlot(TEXT_SLOT);
  if (!textv.isUndefined()) {
    MOZ_ASSERT(textv.isString());
    args.rval().set(textv);
    return true;
  }

  DebuggerSourceGetTextMatcher matcher(cx);
  JSString* str = referent.match(matcher);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  obj->setReservedSlot(TEXT_SLOT, args.rval());
  return true;
}

/* static */
bool DebuggerSource::getBinary(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "(get binary)", args, obj, referent);

  if (!referent.is<WasmInstanceObject*>()) {
    ReportValueError(cx, JSMSG_DEBUG_BAD_REFERENT, JSDVG_SEARCH_STACK,
                     args.thisv(), nullptr, "a wasm source");
    return false;
  }

  RootedWasmInstanceObject instanceObj(cx, referent.as<WasmInstanceObject*>());
  wasm::Instance& instance = instanceObj->instance();

  if (!instance.debugEnabled()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_NO_BINARY_SOURCE);
    return false;
  }

  const wasm::Bytes& bytecode = instance.debug().bytecode();
  RootedObject arr(cx, JS_NewUint8Array(cx, bytecode.length()));
  if (!arr) {
    return false;
  }

  memcpy(arr->as<TypedArrayObject>().dataPointerUnshared(), bytecode.begin(),
         bytecode.length());

  args.rval().setObject(*arr);
  return true;
}

class DebuggerSourceGetURLMatcher {
  JSContext* cx_;

 public:
  explicit DebuggerSourceGetURLMatcher(JSContext* cx) : cx_(cx) {}

  using ReturnType = Maybe<JSString*>;

  ReturnType match(HandleScriptSourceObject sourceObject) {
    ScriptSource* ss = sourceObject->source();
    MOZ_ASSERT(ss);
    if (ss->filename()) {
      JSString* str = NewStringCopyZ<CanGC>(cx_, ss->filename());
      return Some(str);
    }
    return Nothing();
  }
  ReturnType match(Handle<WasmInstanceObject*> instanceObj) {
    return Some(instanceObj->instance().createDisplayURL(cx_));
  }
};

/* static */
bool DebuggerSource::getURL(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "(get url)", args, obj, referent);

  DebuggerSourceGetURLMatcher matcher(cx);
  Maybe<JSString*> str = referent.match(matcher);
  if (str.isSome()) {
    if (!*str) {
      return false;
    }
    args.rval().setString(*str);
  } else {
    args.rval().setNull();
  }
  return true;
}

class DebuggerSourceGetStartLineMatcher {
 public:
  using ReturnType = uint32_t;

  ReturnType match(HandleScriptSourceObject sourceObject) {
    ScriptSource* ss = sourceObject->source();
    return ss->startLine();
  }
  ReturnType match(Handle<WasmInstanceObject*> instanceObj) { return 0; }
};

/* static */
bool DebuggerSource::getStartLine(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "(get startLine)", args, obj,
                            referent);

  DebuggerSourceGetStartLineMatcher matcher;
  uint32_t line = referent.match(matcher);
  args.rval().setNumber(line);
  return true;
}

class DebuggerSourceGetIdMatcher {
 public:
  using ReturnType = uint32_t;

  ReturnType match(HandleScriptSourceObject sourceObject) {
    ScriptSource* ss = sourceObject->source();
    return ss->id();
  }
  ReturnType match(Handle<WasmInstanceObject*> instanceObj) { return 0; }
};

/* static */
bool DebuggerSource::getId(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "(get id)", args, obj, referent);

  DebuggerSourceGetIdMatcher matcher;
  uint32_t id = referent.match(matcher);
  args.rval().setNumber(id);
  return true;
}

struct DebuggerSourceGetDisplayURLMatcher {
  using ReturnType = const char16_t*;
  ReturnType match(HandleScriptSourceObject sourceObject) {
    ScriptSource* ss = sourceObject->source();
    MOZ_ASSERT(ss);
    return ss->hasDisplayURL() ? ss->displayURL() : nullptr;
  }
  ReturnType match(Handle<WasmInstanceObject*> wasmInstance) {
    return wasmInstance->instance().metadata().displayURL();
  }
};

/* static */
bool DebuggerSource::getDisplayURL(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "(get url)", args, obj, referent);

  DebuggerSourceGetDisplayURLMatcher matcher;
  if (const char16_t* displayURL = referent.match(matcher)) {
    JSString* str = JS_NewUCStringCopyZ(cx, displayURL);
    if (!str) {
      return false;
    }
    args.rval().setString(str);
  } else {
    args.rval().setNull();
  }
  return true;
}

struct DebuggerSourceGetElementMatcher {
  using ReturnType = JSObject*;
  ReturnType match(HandleScriptSourceObject sourceObject) {
    return sourceObject->unwrappedElement();
  }
  ReturnType match(Handle<WasmInstanceObject*> wasmInstance) { return nullptr; }
};

/* static */
bool DebuggerSource::getElement(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "(get element)", args, obj, referent);

  DebuggerSourceGetElementMatcher matcher;
  if (JSObject* element = referent.match(matcher)) {
    args.rval().setObjectOrNull(element);
    if (!Debugger::fromChildJSObject(obj)->wrapDebuggeeValue(cx, args.rval())) {
      return false;
    }
  } else {
    args.rval().setUndefined();
  }
  return true;
}

struct DebuggerSourceGetElementPropertyMatcher {
  using ReturnType = Value;
  ReturnType match(HandleScriptSourceObject sourceObject) {
    return sourceObject->unwrappedElementAttributeName();
  }
  ReturnType match(Handle<WasmInstanceObject*> wasmInstance) {
    return UndefinedValue();
  }
};

/* static */
bool DebuggerSource::getElementProperty(JSContext* cx, unsigned argc,
                                        Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "(get elementAttributeName)", args,
                            obj, referent);
  DebuggerSourceGetElementPropertyMatcher matcher;
  args.rval().set(referent.match(matcher));
  return Debugger::fromChildJSObject(obj)->wrapDebuggeeValue(cx, args.rval());
}

class DebuggerSourceGetIntroductionScriptMatcher {
  JSContext* cx_;
  Debugger* dbg_;
  MutableHandleValue rval_;

 public:
  DebuggerSourceGetIntroductionScriptMatcher(JSContext* cx, Debugger* dbg,
                                             MutableHandleValue rval)
      : cx_(cx), dbg_(dbg), rval_(rval) {}

  using ReturnType = bool;

  ReturnType match(HandleScriptSourceObject sourceObject) {
    RootedScript script(cx_, sourceObject->unwrappedIntroductionScript());
    if (script) {
      RootedObject scriptDO(cx_, dbg_->wrapScript(cx_, script));
      if (!scriptDO) {
        return false;
      }
      rval_.setObject(*scriptDO);
    } else {
      rval_.setUndefined();
    }
    return true;
  }

  ReturnType match(Handle<WasmInstanceObject*> wasmInstance) {
    RootedObject ds(cx_, dbg_->wrapWasmScript(cx_, wasmInstance));
    if (!ds) {
      return false;
    }
    rval_.setObject(*ds);
    return true;
  }
};

/* static */
bool DebuggerSource::getIntroductionScript(JSContext* cx, unsigned argc,
                                           Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "(get introductionScript)", args, obj,
                            referent);
  Debugger* dbg = Debugger::fromChildJSObject(obj);
  DebuggerSourceGetIntroductionScriptMatcher matcher(cx, dbg, args.rval());
  return referent.match(matcher);
}

struct DebuggerGetIntroductionOffsetMatcher {
  using ReturnType = Value;
  ReturnType match(HandleScriptSourceObject sourceObject) {
    // Regardless of what's recorded in the ScriptSourceObject and
    // ScriptSource, only hand out the introduction offset if we also have
    // the script within which it applies.
    ScriptSource* ss = sourceObject->source();
    if (ss->hasIntroductionOffset() &&
        sourceObject->unwrappedIntroductionScript()) {
      return Int32Value(ss->introductionOffset());
    }
    return UndefinedValue();
  }
  ReturnType match(Handle<WasmInstanceObject*> wasmInstance) {
    return UndefinedValue();
  }
};

/* static */
bool DebuggerSource::getIntroductionOffset(JSContext* cx, unsigned argc,
                                           Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "(get introductionOffset)", args, obj,
                            referent);
  DebuggerGetIntroductionOffsetMatcher matcher;
  args.rval().set(referent.match(matcher));
  return true;
}

struct DebuggerSourceGetIntroductionTypeMatcher {
  using ReturnType = const char*;
  ReturnType match(HandleScriptSourceObject sourceObject) {
    ScriptSource* ss = sourceObject->source();
    MOZ_ASSERT(ss);
    return ss->hasIntroductionType() ? ss->introductionType() : nullptr;
  }
  ReturnType match(Handle<WasmInstanceObject*> wasmInstance) { return "wasm"; }
};

/* static */
bool DebuggerSource::getIntroductionType(JSContext* cx, unsigned argc,
                                         Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "(get introductionType)", args, obj,
                            referent);

  DebuggerSourceGetIntroductionTypeMatcher matcher;
  if (const char* introductionType = referent.match(matcher)) {
    JSString* str = NewStringCopyZ<CanGC>(cx, introductionType);
    if (!str) {
      return false;
    }
    args.rval().setString(str);
  } else {
    args.rval().setUndefined();
  }

  return true;
}

/* static */
bool DebuggerSource::setSourceMapURL(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSOURCE_SOURCE(cx, argc, vp, "set sourceMapURL", args, obj,
                          sourceObject);
  ScriptSource* ss = sourceObject->source();
  MOZ_ASSERT(ss);
  if (!args.requireAtLeast(cx, "set sourceMapURL", 1)) {
    return false;
  }

  JSString* str = ToString<CanGC>(cx, args[0]);
  if (!str) {
    return false;
  }

  AutoStableStringChars stableChars(cx);
  if (!stableChars.initTwoByte(cx, str)) {
    return false;
  }

  if (!ss->setSourceMapURL(cx, stableChars.twoByteChars())) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

class DebuggerSourceGetSourceMapURLMatcher {
  JSContext* cx_;
  MutableHandleString result_;

 public:
  explicit DebuggerSourceGetSourceMapURLMatcher(JSContext* cx,
                                                MutableHandleString result)
      : cx_(cx), result_(result) {}

  using ReturnType = bool;
  ReturnType match(HandleScriptSourceObject sourceObject) {
    ScriptSource* ss = sourceObject->source();
    MOZ_ASSERT(ss);
    if (!ss->hasSourceMapURL()) {
      result_.set(nullptr);
      return true;
    }
    JSString* str = JS_NewUCStringCopyZ(cx_, ss->sourceMapURL());
    if (!str) {
      return false;
    }
    result_.set(str);
    return true;
  }
  ReturnType match(Handle<WasmInstanceObject*> instanceObj) {
    wasm::Instance& instance = instanceObj->instance();
    if (!instance.debugEnabled()) {
      result_.set(nullptr);
      return true;
    }

    RootedString str(cx_);
    if (!instance.debug().getSourceMappingURL(cx_, &str)) {
      return false;
    }

    result_.set(str);
    return true;
  }
};

/* static */
bool DebuggerSource::getSourceMapURL(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "(get sourceMapURL)", args, obj,
                            referent);

  RootedString result(cx);
  DebuggerSourceGetSourceMapURLMatcher matcher(cx, &result);
  if (!referent.match(matcher)) {
    return false;
  }
  if (result) {
    args.rval().setString(result);
  } else {
    args.rval().setNull();
  }
  return true;
}

template <typename Unit>
static JSScript* ReparseSource(JSContext* cx, HandleScriptSourceObject sso) {
  AutoRealm ar(cx, sso);
  ScriptSource* ss = sso->source();

  JS::CompileOptions options(cx);
  options.hideScriptFromDebugger = true;
  options.setFileAndLine(ss->filename(), ss->startLine());

  UncompressedSourceCache::AutoHoldEntry holder;

  ScriptSource::PinnedUnits<Unit> units(cx, ss, holder, 0, ss->length());
  if (!units.get()) {
    return nullptr;
  }

  JS::SourceText<Unit> srcBuf;
  if (!srcBuf.init(cx, units.get(), ss->length(),
                   JS::SourceOwnership::Borrowed)) {
    return nullptr;
  }

  return JS::Compile(cx, options, srcBuf);
}

/* static */
bool DebuggerSource::reparse(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSOURCE_REFERENT(cx, argc, vp, "reparse", args, obj,
                            referent);
  if (!referent.is<ScriptSourceObject*>()) {
    JS_ReportErrorASCII(cx, "Source object required");
    return false;
  }

  RootedScriptSourceObject sso(cx, referent.as<ScriptSourceObject*>());

  if (!sso->source()->hasSourceText()) {
    JS_ReportErrorASCII(cx, "Source object missing text");
    return false;
  }

  RootedScript script(cx);
  if (sso->source()->hasSourceType<mozilla::Utf8Unit>()) {
    script = ReparseSource<mozilla::Utf8Unit>(cx, sso);
  } else {
    script = ReparseSource<char16_t>(cx, sso);
  }

  if (!script) {
    return false;
  }

  Debugger* dbg = Debugger::fromChildJSObject(obj);
  RootedObject scriptDO(cx, dbg->wrapScript(cx, script));
  if (!scriptDO) {
    return false;
  }

  args.rval().setObject(*scriptDO);
  return true;
}

const JSPropertySpec DebuggerSource::properties_[] = {
    JS_PSG("text", getText, 0),
    JS_PSG("binary", getBinary, 0),
    JS_PSG("url", getURL, 0),
    JS_PSG("startLine", getStartLine, 0),
    JS_PSG("id", getId, 0),
    JS_PSG("element", getElement, 0),
    JS_PSG("displayURL", getDisplayURL, 0),
    JS_PSG("introductionScript", getIntroductionScript, 0),
    JS_PSG("introductionOffset", getIntroductionOffset, 0),
    JS_PSG("introductionType", getIntroductionType, 0),
    JS_PSG("elementAttributeName", getElementProperty, 0),
    JS_PSGS("sourceMapURL", getSourceMapURL, setSourceMapURL, 0),
    JS_PS_END};

const JSFunctionSpec DebuggerSource::methods_[] = {
    JS_FN("reparse", reparse, 0, 0),
    JS_FS_END};
