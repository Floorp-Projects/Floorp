/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debugger/Script-inl.h"

#include "mozilla/Maybe.h"   // for Some, Maybe
#include "mozilla/Span.h"    // for Span
#include "mozilla/Vector.h"  // for Vector

#include <stddef.h>  // for ptrdiff_t
#include <stdint.h>  // for uint32_t, SIZE_MAX, int32_t

#include "jsapi.h"             // for CallArgs, Rooted, CallArgsFromVp
#include "jsfriendapi.h"       // for GetErrorMessage
#include "jsnum.h"             // for ToNumber
#include "NamespaceImports.h"  // for CallArgs, RootedValue

#include "builtin/Array.h"         // for NewDenseEmptyArray
#include "debugger/Debugger.h"     // for DebuggerScriptReferent, Debugger
#include "debugger/DebugScript.h"  // for DebugScript
#include "debugger/Source.h"       // for DebuggerSource
#include "gc/Barrier.h"            // for ImmutablePropertyNamePtr
#include "gc/GC.h"                 // for MemoryUse, MemoryUse::Breakpoint
#include "gc/Rooting.h"            // for RootedDebuggerScript
#include "gc/Tracer.h"         // for TraceManuallyBarrieredCrossCompartmentEdge
#include "gc/Zone.h"           // for Zone
#include "gc/ZoneAllocator.h"  // for AddCellMemory
#include "js/HeapAPI.h"        // for GCCellPtr
#include "js/Wrapper.h"        // for UncheckedUnwrap
#include "vm/ArrayObject.h"    // for ArrayObject
#include "vm/BytecodeUtil.h"   // for GET_JUMP_OFFSET
#include "vm/GlobalObject.h"   // for GlobalObject
#include "vm/JSContext.h"      // for JSContext, ReportValueError
#include "vm/JSFunction.h"     // for JSFunction
#include "vm/JSObject.h"       // for RequireObject, JSObject
#include "vm/ObjectGroup.h"    // for TenuredObject
#include "vm/ObjectOperations.h"  // for DefineDataProperty, HasOwnProperty
#include "vm/Realm.h"             // for AutoRealm
#include "vm/Runtime.h"           // for JSAtomState, JSRuntime
#include "vm/StringType.h"        // for NameToId, PropertyName, JSAtom
#include "wasm/WasmDebug.h"       // for ExprLoc, DebugState
#include "wasm/WasmInstance.h"    // for Instance
#include "wasm/WasmTypes.h"       // for Bytes

#include "vm/BytecodeUtil-inl.h"      // for BytecodeRangeWithPosition
#include "vm/JSAtom-inl.h"            // for ValueToId
#include "vm/JSObject-inl.h"          // for NewBuiltinClassInstance
#include "vm/JSScript-inl.h"          // for LazyScript::functionDelazifying
#include "vm/ObjectOperations-inl.h"  // for GetProperty
#include "vm/Realm-inl.h"             // for AutoRealm::AutoRealm

using namespace js;

using mozilla::Maybe;
using mozilla::Some;

const ClassOps DebuggerScript::classOps_ = {nullptr, /* addProperty */
                                            nullptr, /* delProperty */
                                            nullptr, /* enumerate   */
                                            nullptr, /* newEnumerate */
                                            nullptr, /* resolve     */
                                            nullptr, /* mayResolve  */
                                            nullptr, /* finalize    */
                                            nullptr, /* call        */
                                            nullptr, /* hasInstance */
                                            nullptr, /* construct   */
                                            trace};

const Class DebuggerScript::class_ = {
    "Script", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS),
    &classOps_};

/* static */
void DebuggerScript::trace(JSTracer* trc, JSObject* obj) {
  DebuggerScript* self = &obj->as<DebuggerScript>();
  // This comes from a private pointer, so no barrier needed.
  gc::Cell* cell = self->getReferentCell();
  if (cell) {
    if (cell->is<JSScript>()) {
      JSScript* script = cell->as<JSScript>();
      TraceManuallyBarrieredCrossCompartmentEdge(
          trc, self, &script, "Debugger.Script script referent");
      self->setPrivateUnbarriered(script);
    } else if (cell->is<LazyScript>()) {
      LazyScript* lazyScript = cell->as<LazyScript>();
      TraceManuallyBarrieredCrossCompartmentEdge(
          trc, self, &lazyScript, "Debugger.Script lazy script referent");
      self->setPrivateUnbarriered(lazyScript);
    } else {
      JSObject* wasm = cell->as<JSObject>();
      TraceManuallyBarrieredCrossCompartmentEdge(
          trc, self, &wasm, "Debugger.Script wasm referent");
      MOZ_ASSERT(wasm->is<WasmInstanceObject>());
      self->setPrivateUnbarriered(wasm);
    }
  }
}

/* static */
NativeObject* DebuggerScript::initClass(JSContext* cx,
                                        Handle<GlobalObject*> global,
                                        HandleObject debugCtor) {
  return InitClass(cx, debugCtor, nullptr, &class_, construct, 0, properties_,
                   methods_, nullptr, nullptr);
}

class DebuggerScript::SetPrivateMatcher {
  DebuggerScript* obj_;

 public:
  explicit SetPrivateMatcher(DebuggerScript* obj) : obj_(obj) {}
  using ReturnType = void;
  ReturnType match(HandleScript script) { obj_->setPrivateGCThing(script); }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    obj_->setPrivateGCThing(lazyScript);
  }
  ReturnType match(Handle<WasmInstanceObject*> instance) {
    obj_->setPrivateGCThing(instance);
  }
};

/* static */
DebuggerScript* DebuggerScript::create(JSContext* cx, HandleObject proto,
                                       Handle<DebuggerScriptReferent> referent,
                                       HandleNativeObject debugger) {
  DebuggerScript* scriptobj =
      NewObjectWithGivenProto<DebuggerScript>(cx, proto, TenuredObject);
  if (!scriptobj) {
    return nullptr;
  }

  scriptobj->setReservedSlot(DebuggerScript::OWNER_SLOT,
                             ObjectValue(*debugger));
  SetPrivateMatcher matcher(scriptobj);
  referent.match(matcher);

  return scriptobj;
}

static JSScript* DelazifyScript(JSContext* cx, Handle<LazyScript*> lazyScript) {
  if (lazyScript->maybeScript()) {
    return lazyScript->maybeScript();
  }

  // JSFunction::getOrCreateScript requires the enclosing script not to be
  // lazified.
  MOZ_ASSERT(lazyScript->hasEnclosingLazyScript() ||
             lazyScript->hasEnclosingScope());
  if (lazyScript->hasEnclosingLazyScript()) {
    Rooted<LazyScript*> enclosingLazyScript(cx,
                                            lazyScript->enclosingLazyScript());
    if (!DelazifyScript(cx, enclosingLazyScript)) {
      return nullptr;
    }

    if (!lazyScript->enclosingScriptHasEverBeenCompiled()) {
      // It didn't work! Delazifying the enclosing script still didn't
      // delazify this script. This happens when the function
      // corresponding to this script was removed by constant folding.
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_OPTIMIZED_OUT_FUN);
      return nullptr;
    }
  }
  MOZ_ASSERT(lazyScript->enclosingScriptHasEverBeenCompiled());

  RootedFunction fun0(cx, lazyScript->functionNonDelazifying());
  AutoRealm ar(cx, fun0);
  RootedFunction fun(cx, LazyScript::functionDelazifying(cx, lazyScript));
  if (!fun) {
    return nullptr;
  }
  return fun->getOrCreateScript(cx, fun);
}

/* static */
DebuggerScript* DebuggerScript::check(JSContext* cx, HandleValue v,
                                      const char* fnname) {
  JSObject* thisobj = RequireObject(cx, v);
  if (!thisobj) {
    return nullptr;
  }
  if (!thisobj->is<DebuggerScript>()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger.Script",
                              fnname, thisobj->getClass()->name);
    return nullptr;
  }

  DebuggerScript& scriptObj = thisobj->as<DebuggerScript>();

  // Check for Debugger.Script.prototype, which is of class
  // DebuggerScript::class but whose script is null.
  if (!scriptObj.getReferentCell()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_INCOMPATIBLE_PROTO, "Debugger.Script",
                              fnname, "prototype object");
    return nullptr;
  }

  return &scriptObj;
}

/* static */
DebuggerScript* DebuggerScript::checkThis(JSContext* cx, const CallArgs& args,
                                          const char* fnname) {
  DebuggerScript* thisobj = DebuggerScript::check(cx, args.thisv(), fnname);
  if (!thisobj) {
    return nullptr;
  }

  if (!thisobj->getReferent().is<JSScript*>() &&
      !thisobj->getReferent().is<LazyScript*>()) {
    ReportValueError(cx, JSMSG_DEBUG_BAD_REFERENT, JSDVG_SEARCH_STACK,
                     args.thisv(), nullptr, "a JS script");
    return nullptr;
  }

  return thisobj;
}

#define THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, fnname, args, obj, referent) \
  CallArgs args = CallArgsFromVp(argc, vp);                                  \
  RootedDebuggerScript obj(cx,                                               \
                           DebuggerScript::check(cx, args.thisv(), fnname)); \
  if (!obj) return false;                                                    \
  Rooted<DebuggerScriptReferent> referent(cx, obj->getReferent())

#define THIS_DEBUGSCRIPT_SCRIPT_MAYBE_LAZY(cx, argc, vp, fnname, args, obj)  \
  CallArgs args = CallArgsFromVp(argc, vp);                                  \
  RootedDebuggerScript obj(cx, DebuggerScript::checkThis(cx, args, fnname)); \
  if (!obj) return false;

#define THIS_DEBUGSCRIPT_SCRIPT_DELAZIFY(cx, argc, vp, fnname, args, obj,     \
                                         script)                              \
  THIS_DEBUGSCRIPT_SCRIPT_MAYBE_LAZY(cx, argc, vp, fnname, args, obj);        \
  RootedScript script(cx);                                                    \
  if (obj->getReferent().is<JSScript*>()) {                                   \
    script = obj->getReferent().as<JSScript*>();                              \
  } else {                                                                    \
    Rooted<LazyScript*> lazyScript(cx, obj->getReferent().as<LazyScript*>()); \
    script = DelazifyScript(cx, lazyScript);                                  \
    if (!script) return false;                                                \
  }

template <typename Result>
Result CallScriptMethod(HandleDebuggerScript obj,
                        Result (JSScript::*ifJSScript)() const,
                        Result (LazyScript::*ifLazyScript)() const) {
  if (obj->getReferent().is<JSScript*>()) {
    JSScript* script = obj->getReferent().as<JSScript*>();
    return (script->*ifJSScript)();
  }

  LazyScript* lazyScript = obj->getReferent().as<LazyScript*>();
  return (lazyScript->*ifLazyScript)();
}

/* static */
bool DebuggerScript::getIsGeneratorFunction(JSContext* cx, unsigned argc,
                                            Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_MAYBE_LAZY(cx, argc, vp, "(get isGeneratorFunction)",
                                     args, obj);
  args.rval().setBoolean(
      CallScriptMethod(obj, &JSScript::isGenerator, &LazyScript::isGenerator));
  return true;
}

/* static */
bool DebuggerScript::getIsAsyncFunction(JSContext* cx, unsigned argc,
                                        Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_MAYBE_LAZY(cx, argc, vp, "(get isAsyncFunction)",
                                     args, obj);
  args.rval().setBoolean(
      CallScriptMethod(obj, &JSScript::isAsync, &LazyScript::isAsync));
  return true;
}

/* static */
bool DebuggerScript::getIsModule(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_MAYBE_LAZY(cx, argc, vp, "(get isModule)", args, obj);
  DebuggerScriptReferent referent = obj->getReferent();
  args.rval().setBoolean(referent.is<JSScript*>() &&
                         referent.as<JSScript*>()->isModule());
  return true;
}

/* static */
bool DebuggerScript::getDisplayName(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_MAYBE_LAZY(cx, argc, vp, "(get displayName)", args,
                                     obj);
  JSFunction* func = CallScriptMethod(obj, &JSScript::functionNonDelazifying,
                                      &LazyScript::functionNonDelazifying);
  Debugger* dbg = Debugger::fromChildJSObject(obj);

  JSString* name = func ? func->displayAtom() : nullptr;
  if (!name) {
    args.rval().setUndefined();
    return true;
  }

  RootedValue namev(cx, StringValue(name));
  if (!dbg->wrapDebuggeeValue(cx, &namev)) {
    return false;
  }
  args.rval().set(namev);
  return true;
}

template <typename T>
/* static */
bool DebuggerScript::getUrlImpl(JSContext* cx, CallArgs& args,
                                Handle<T*> script) {
  if (script->filename()) {
    JSString* str;
    if (script->scriptSource()->introducerFilename()) {
      str = NewStringCopyZ<CanGC>(cx,
                                  script->scriptSource()->introducerFilename());
    } else {
      str = NewStringCopyZ<CanGC>(cx, script->filename());
    }
    if (!str) {
      return false;
    }
    args.rval().setString(str);
  } else {
    args.rval().setNull();
  }
  return true;
}

/* static */
bool DebuggerScript::getUrl(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_MAYBE_LAZY(cx, argc, vp, "(get url)", args, obj);
  if (obj->getReferent().is<JSScript*>()) {
    RootedScript script(cx, obj->getReferent().as<JSScript*>());
    return getUrlImpl<JSScript>(cx, args, script);
  }

  Rooted<LazyScript*> lazyScript(cx, obj->getReferent().as<LazyScript*>());
  return getUrlImpl<LazyScript>(cx, args, lazyScript);
}

struct DebuggerScript::GetStartLineMatcher {
  using ReturnType = uint32_t;

  ReturnType match(HandleScript script) { return script->lineno(); }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    return lazyScript->lineno();
  }
  ReturnType match(Handle<WasmInstanceObject*> wasmInstance) { return 1; }
};

/* static */
bool DebuggerScript::getStartLine(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "(get startLine)", args, obj,
                            referent);
  GetStartLineMatcher matcher;
  args.rval().setNumber(referent.match(matcher));
  return true;
}

struct DebuggerScript::GetStartColumnMatcher {
  using ReturnType = uint32_t;

  ReturnType match(HandleScript script) { return script->column(); }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    return lazyScript->column();
  }
  ReturnType match(Handle<WasmInstanceObject*> wasmInstance) { return 0; }
};

/* static */
bool DebuggerScript::getStartColumn(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "(get startColumn)", args, obj,
                            referent);
  GetStartColumnMatcher matcher;
  args.rval().setNumber(referent.match(matcher));
  return true;
}

struct DebuggerScript::GetLineCountMatcher {
  JSContext* cx_;
  double totalLines;

  explicit GetLineCountMatcher(JSContext* cx) : cx_(cx), totalLines(0.0) {}
  using ReturnType = bool;

  ReturnType match(HandleScript script) {
    totalLines = double(GetScriptLineExtent(script));
    return true;
  }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    RootedScript script(cx_, DelazifyScript(cx_, lazyScript));
    if (!script) {
      return false;
    }
    return match(script);
  }
  ReturnType match(Handle<WasmInstanceObject*> instanceObj) {
    wasm::Instance& instance = instanceObj->instance();
    if (instance.debugEnabled()) {
      totalLines = double(instance.debug().bytecode().length());
    } else {
      totalLines = 0;
    }
    return true;
  }
};

/* static */
bool DebuggerScript::getLineCount(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "(get lineCount)", args, obj,
                            referent);
  GetLineCountMatcher matcher(cx);
  if (!referent.match(matcher)) {
    return false;
  }
  args.rval().setNumber(matcher.totalLines);
  return true;
}

class DebuggerScript::GetSourceMatcher {
  JSContext* cx_;
  Debugger* dbg_;

 public:
  GetSourceMatcher(JSContext* cx, Debugger* dbg) : cx_(cx), dbg_(dbg) {}

  using ReturnType = DebuggerSource*;

  ReturnType match(HandleScript script) {
    // JSScript holds the refefence to possibly wrapped ScriptSourceObject.
    // It's wrapped when the script is cloned.
    // See CreateEmptyScriptForClone for more info.
    RootedScriptSourceObject source(
        cx_,
        &UncheckedUnwrap(script->sourceObject())->as<ScriptSourceObject>());
    return dbg_->wrapSource(cx_, source);
  }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    // LazyScript holds the reference to the unwrapped ScriptSourceObject.
    RootedScriptSourceObject source(cx_, lazyScript->sourceObject());
    return dbg_->wrapSource(cx_, source);
  }
  ReturnType match(Handle<WasmInstanceObject*> wasmInstance) {
    return dbg_->wrapWasmSource(cx_, wasmInstance);
  }
};

/* static */
bool DebuggerScript::getSource(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "(get source)", args, obj, referent);
  Debugger* dbg = Debugger::fromChildJSObject(obj);

  GetSourceMatcher matcher(cx, dbg);
  RootedDebuggerSource sourceObject(cx, referent.match(matcher));
  if (!sourceObject) {
    return false;
  }

  args.rval().setObject(*sourceObject);
  return true;
}

/* static */
bool DebuggerScript::getSourceStart(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_MAYBE_LAZY(cx, argc, vp, "(get sourceStart)", args,
                                     obj);
  args.rval().setNumber(uint32_t(obj->getReferentScript()->sourceStart()));
  return true;
}

/* static */
bool DebuggerScript::getSourceLength(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_MAYBE_LAZY(cx, argc, vp, "(get sourceEnd)", args,
                                     obj);
  args.rval().setNumber(uint32_t(obj->getReferentScript()->sourceLength()));
  return true;
}

/* static */
bool DebuggerScript::getMainOffset(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_DELAZIFY(cx, argc, vp, "(get mainOffset)", args, obj,
                                   script);
  args.rval().setNumber(uint32_t(script->mainOffset()));
  return true;
}

/* static */
bool DebuggerScript::getGlobal(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_DELAZIFY(cx, argc, vp, "(get global)", args, obj,
                                   script);
  Debugger* dbg = Debugger::fromChildJSObject(obj);

  RootedValue v(cx, ObjectValue(script->global()));
  if (!dbg->wrapDebuggeeValue(cx, &v)) {
    return false;
  }
  args.rval().set(v);
  return true;
}

class DebuggerScript::GetFormatMatcher {
  const JSAtomState& names_;

 public:
  explicit GetFormatMatcher(const JSAtomState& names) : names_(names) {}
  using ReturnType = JSAtom*;
  ReturnType match(HandleScript script) { return names_.js; }
  ReturnType match(Handle<LazyScript*> lazyScript) { return names_.js; }
  ReturnType match(Handle<WasmInstanceObject*> wasmInstance) {
    return names_.wasm;
  }
};

/* static */
bool DebuggerScript::getFormat(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "(get format)", args, obj, referent);
  GetFormatMatcher matcher(cx->names());
  args.rval().setString(referent.match(matcher));
  return true;
}

/* static */
bool DebuggerScript::getChildScripts(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_DELAZIFY(cx, argc, vp, "getChildScripts", args, obj,
                                   script);
  Debugger* dbg = Debugger::fromChildJSObject(obj);

  RootedObject result(cx, NewDenseEmptyArray(cx));
  if (!result) {
    return false;
  }

  // Wrap and append scripts for the inner functions in script->gcthings().
  RootedFunction fun(cx);
  RootedScript funScript(cx);
  RootedObject s(cx);
  for (JS::GCCellPtr gcThing : script->gcthings()) {
    if (!gcThing.is<JSObject>()) {
      continue;
    }

    JSObject* obj = &gcThing.as<JSObject>();
    if (obj->is<JSFunction>()) {
      fun = &obj->as<JSFunction>();
      // The inner function could be an asm.js native.
      if (!IsInterpretedNonSelfHostedFunction(fun)) {
        continue;
      }
      funScript = GetOrCreateFunctionScript(cx, fun);
      if (!funScript) {
        return false;
      }
      s = dbg->wrapScript(cx, funScript);
      if (!s || !NewbornArrayPush(cx, result, ObjectValue(*s))) {
        return false;
      }
    }
  }
  args.rval().setObject(*result);
  return true;
}

static bool ScriptOffset(JSContext* cx, const Value& v, size_t* offsetp) {
  double d;
  size_t off;

  bool ok = v.isNumber();
  if (ok) {
    d = v.toNumber();
    off = size_t(d);
  }
  if (!ok || off != d) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEBUG_BAD_OFFSET);
    return false;
  }
  *offsetp = off;
  return true;
}

static bool EnsureScriptOffsetIsValid(JSContext* cx, JSScript* script,
                                      size_t offset) {
  if (IsValidBytecodeOffset(cx, script, offset)) {
    return true;
  }
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_DEBUG_BAD_OFFSET);
  return false;
}

template <bool OnlyOffsets>
class DebuggerScript::GetPossibleBreakpointsMatcher {
  JSContext* cx_;
  MutableHandleObject result_;

  Maybe<size_t> minOffset;
  Maybe<size_t> maxOffset;

  Maybe<size_t> minLine;
  size_t minColumn;
  Maybe<size_t> maxLine;
  size_t maxColumn;

  bool passesQuery(size_t offset, size_t lineno, size_t colno) {
    // [minOffset, maxOffset) - Inclusive minimum and exclusive maximum.
    if ((minOffset && offset < *minOffset) ||
        (maxOffset && offset >= *maxOffset)) {
      return false;
    }

    if (minLine) {
      if (lineno < *minLine || (lineno == *minLine && colno < minColumn)) {
        return false;
      }
    }

    if (maxLine) {
      if (lineno > *maxLine || (lineno == *maxLine && colno >= maxColumn)) {
        return false;
      }
    }

    return true;
  }

  bool maybeAppendEntry(size_t offset, size_t lineno, size_t colno,
                        bool isStepStart) {
    if (!passesQuery(offset, lineno, colno)) {
      return true;
    }

    if (OnlyOffsets) {
      if (!NewbornArrayPush(cx_, result_, NumberValue(offset))) {
        return false;
      }

      return true;
    }

    RootedPlainObject entry(cx_, NewBuiltinClassInstance<PlainObject>(cx_));
    if (!entry) {
      return false;
    }

    RootedValue value(cx_, NumberValue(offset));
    if (!DefineDataProperty(cx_, entry, cx_->names().offset, value)) {
      return false;
    }

    value = NumberValue(lineno);
    if (!DefineDataProperty(cx_, entry, cx_->names().lineNumber, value)) {
      return false;
    }

    value = NumberValue(colno);
    if (!DefineDataProperty(cx_, entry, cx_->names().columnNumber, value)) {
      return false;
    }

    value = BooleanValue(isStepStart);
    if (!DefineDataProperty(cx_, entry, cx_->names().isStepStart, value)) {
      return false;
    }

    if (!NewbornArrayPush(cx_, result_, ObjectValue(*entry))) {
      return false;
    }
    return true;
  }

  bool parseIntValue(HandleValue value, size_t* result) {
    if (!value.isNumber()) {
      return false;
    }

    double doubleOffset = value.toNumber();
    if (doubleOffset < 0 || (unsigned int)doubleOffset != doubleOffset) {
      return false;
    }

    *result = doubleOffset;
    return true;
  }

  bool parseIntValue(HandleValue value, Maybe<size_t>* result) {
    size_t result_;
    if (!parseIntValue(value, &result_)) {
      return false;
    }

    *result = Some(result_);
    return true;
  }

 public:
  explicit GetPossibleBreakpointsMatcher(JSContext* cx,
                                         MutableHandleObject result)
      : cx_(cx),
        result_(result),
        minOffset(),
        maxOffset(),
        minLine(),
        minColumn(0),
        maxLine(),
        maxColumn(0) {}

  bool parseQuery(HandleObject query) {
    RootedValue lineValue(cx_);
    if (!GetProperty(cx_, query, query, cx_->names().line, &lineValue)) {
      return false;
    }

    RootedValue minLineValue(cx_);
    if (!GetProperty(cx_, query, query, cx_->names().minLine, &minLineValue)) {
      return false;
    }

    RootedValue minColumnValue(cx_);
    if (!GetProperty(cx_, query, query, cx_->names().minColumn,
                     &minColumnValue)) {
      return false;
    }

    RootedValue minOffsetValue(cx_);
    if (!GetProperty(cx_, query, query, cx_->names().minOffset,
                     &minOffsetValue)) {
      return false;
    }

    RootedValue maxLineValue(cx_);
    if (!GetProperty(cx_, query, query, cx_->names().maxLine, &maxLineValue)) {
      return false;
    }

    RootedValue maxColumnValue(cx_);
    if (!GetProperty(cx_, query, query, cx_->names().maxColumn,
                     &maxColumnValue)) {
      return false;
    }

    RootedValue maxOffsetValue(cx_);
    if (!GetProperty(cx_, query, query, cx_->names().maxOffset,
                     &maxOffsetValue)) {
      return false;
    }

    if (!minOffsetValue.isUndefined()) {
      if (!parseIntValue(minOffsetValue, &minOffset)) {
        JS_ReportErrorNumberASCII(
            cx_, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
            "getPossibleBreakpoints' 'minOffset'", "not an integer");
        return false;
      }
    }
    if (!maxOffsetValue.isUndefined()) {
      if (!parseIntValue(maxOffsetValue, &maxOffset)) {
        JS_ReportErrorNumberASCII(
            cx_, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
            "getPossibleBreakpoints' 'maxOffset'", "not an integer");
        return false;
      }
    }

    if (!lineValue.isUndefined()) {
      if (!minLineValue.isUndefined() || !maxLineValue.isUndefined()) {
        JS_ReportErrorNumberASCII(cx_, GetErrorMessage, nullptr,
                                  JSMSG_UNEXPECTED_TYPE,
                                  "getPossibleBreakpoints' 'line'",
                                  "not allowed alongside 'minLine'/'maxLine'");
        return false;
      }

      size_t line;
      if (!parseIntValue(lineValue, &line)) {
        JS_ReportErrorNumberASCII(
            cx_, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
            "getPossibleBreakpoints' 'line'", "not an integer");
        return false;
      }

      // If no end column is given, we use the default of 0 and wrap to
      // the next line.
      minLine = Some(line);
      maxLine = Some(line + (maxColumnValue.isUndefined() ? 1 : 0));
    }

    if (!minLineValue.isUndefined()) {
      if (!parseIntValue(minLineValue, &minLine)) {
        JS_ReportErrorNumberASCII(
            cx_, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
            "getPossibleBreakpoints' 'minLine'", "not an integer");
        return false;
      }
    }

    if (!minColumnValue.isUndefined()) {
      if (!minLine) {
        JS_ReportErrorNumberASCII(cx_, GetErrorMessage, nullptr,
                                  JSMSG_UNEXPECTED_TYPE,
                                  "getPossibleBreakpoints' 'minColumn'",
                                  "not allowed without 'line' or 'minLine'");
        return false;
      }

      if (!parseIntValue(minColumnValue, &minColumn)) {
        JS_ReportErrorNumberASCII(
            cx_, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
            "getPossibleBreakpoints' 'minColumn'", "not an integer");
        return false;
      }
    }

    if (!maxLineValue.isUndefined()) {
      if (!parseIntValue(maxLineValue, &maxLine)) {
        JS_ReportErrorNumberASCII(
            cx_, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
            "getPossibleBreakpoints' 'maxLine'", "not an integer");
        return false;
      }
    }

    if (!maxColumnValue.isUndefined()) {
      if (!maxLine) {
        JS_ReportErrorNumberASCII(cx_, GetErrorMessage, nullptr,
                                  JSMSG_UNEXPECTED_TYPE,
                                  "getPossibleBreakpoints' 'maxColumn'",
                                  "not allowed without 'line' or 'maxLine'");
        return false;
      }

      if (!parseIntValue(maxColumnValue, &maxColumn)) {
        JS_ReportErrorNumberASCII(
            cx_, GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
            "getPossibleBreakpoints' 'maxColumn'", "not an integer");
        return false;
      }
    }

    return true;
  }

  using ReturnType = bool;
  ReturnType match(HandleScript script) {
    // Second pass: build the result array.
    result_.set(NewDenseEmptyArray(cx_));
    if (!result_) {
      return false;
    }

    for (BytecodeRangeWithPosition r(cx_, script); !r.empty(); r.popFront()) {
      if (!r.frontIsBreakablePoint()) {
        continue;
      }

      size_t offset = r.frontOffset();
      size_t lineno = r.frontLineNumber();
      size_t colno = r.frontColumnNumber();

      if (!maybeAppendEntry(offset, lineno, colno,
                            r.frontIsBreakableStepPoint())) {
        return false;
      }
    }

    return true;
  }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    RootedScript script(cx_, DelazifyScript(cx_, lazyScript));
    if (!script) {
      return false;
    }
    return match(script);
  }
  ReturnType match(Handle<WasmInstanceObject*> instanceObj) {
    wasm::Instance& instance = instanceObj->instance();

    Vector<wasm::ExprLoc> offsets(cx_);
    if (instance.debugEnabled() &&
        !instance.debug().getAllColumnOffsets(cx_, &offsets)) {
      return false;
    }

    result_.set(NewDenseEmptyArray(cx_));
    if (!result_) {
      return false;
    }

    for (uint32_t i = 0; i < offsets.length(); i++) {
      size_t lineno = offsets[i].lineno;
      size_t column = offsets[i].column;
      size_t offset = offsets[i].offset;
      if (!maybeAppendEntry(offset, lineno, column, true)) {
        return false;
      }
    }
    return true;
  }
};

/* static */
bool DebuggerScript::getPossibleBreakpoints(JSContext* cx, unsigned argc,
                                            Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "getPossibleBreakpoints", args, obj,
                            referent);

  RootedObject result(cx);
  GetPossibleBreakpointsMatcher<false> matcher(cx, &result);
  if (args.length() >= 1 && !args[0].isUndefined()) {
    RootedObject queryObject(cx, RequireObject(cx, args[0]));
    if (!queryObject || !matcher.parseQuery(queryObject)) {
      return false;
    }
  }
  if (!referent.match(matcher)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/* static */
bool DebuggerScript::getPossibleBreakpointOffsets(JSContext* cx, unsigned argc,
                                                  Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "getPossibleBreakpointOffsets", args,
                            obj, referent);

  RootedObject result(cx);
  GetPossibleBreakpointsMatcher<true> matcher(cx, &result);
  if (args.length() >= 1 && !args[0].isUndefined()) {
    RootedObject queryObject(cx, RequireObject(cx, args[0]));
    if (!queryObject || !matcher.parseQuery(queryObject)) {
      return false;
    }
  }
  if (!referent.match(matcher)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

class DebuggerScript::GetOffsetMetadataMatcher {
  JSContext* cx_;
  size_t offset_;
  MutableHandlePlainObject result_;

 public:
  explicit GetOffsetMetadataMatcher(JSContext* cx, size_t offset,
                                    MutableHandlePlainObject result)
      : cx_(cx), offset_(offset), result_(result) {}
  using ReturnType = bool;
  ReturnType match(HandleScript script) {
    if (!EnsureScriptOffsetIsValid(cx_, script, offset_)) {
      return false;
    }

    result_.set(NewBuiltinClassInstance<PlainObject>(cx_));
    if (!result_) {
      return false;
    }

    BytecodeRangeWithPosition r(cx_, script);
    while (!r.empty() && r.frontOffset() < offset_) {
      r.popFront();
    }

    RootedValue value(cx_, NumberValue(r.frontLineNumber()));
    if (!DefineDataProperty(cx_, result_, cx_->names().lineNumber, value)) {
      return false;
    }

    value = NumberValue(r.frontColumnNumber());
    if (!DefineDataProperty(cx_, result_, cx_->names().columnNumber, value)) {
      return false;
    }

    value = BooleanValue(r.frontIsBreakablePoint());
    if (!DefineDataProperty(cx_, result_, cx_->names().isBreakpoint, value)) {
      return false;
    }

    value = BooleanValue(r.frontIsBreakableStepPoint());
    if (!DefineDataProperty(cx_, result_, cx_->names().isStepStart, value)) {
      return false;
    }

    return true;
  }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    RootedScript script(cx_, DelazifyScript(cx_, lazyScript));
    if (!script) {
      return false;
    }
    return match(script);
  }
  ReturnType match(Handle<WasmInstanceObject*> instanceObj) {
    wasm::Instance& instance = instanceObj->instance();
    if (!instance.debugEnabled()) {
      JS_ReportErrorNumberASCII(cx_, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_BAD_OFFSET);
      return false;
    }

    size_t lineno;
    size_t column;
    if (!instance.debug().getOffsetLocation(offset_, &lineno, &column)) {
      JS_ReportErrorNumberASCII(cx_, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_BAD_OFFSET);
      return false;
    }

    result_.set(NewBuiltinClassInstance<PlainObject>(cx_));
    if (!result_) {
      return false;
    }

    RootedValue value(cx_, NumberValue(lineno));
    if (!DefineDataProperty(cx_, result_, cx_->names().lineNumber, value)) {
      return false;
    }

    value = NumberValue(column);
    if (!DefineDataProperty(cx_, result_, cx_->names().columnNumber, value)) {
      return false;
    }

    value.setBoolean(true);
    if (!DefineDataProperty(cx_, result_, cx_->names().isBreakpoint, value)) {
      return false;
    }

    value.setBoolean(true);
    if (!DefineDataProperty(cx_, result_, cx_->names().isStepStart, value)) {
      return false;
    }

    return true;
  }
};

/* static */
bool DebuggerScript::getOffsetMetadata(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "getOffsetMetadata", args, obj,
                            referent);
  if (!args.requireAtLeast(cx, "Debugger.Script.getOffsetMetadata", 1)) {
    return false;
  }
  size_t offset;
  if (!ScriptOffset(cx, args[0], &offset)) {
    return false;
  }

  RootedPlainObject result(cx);
  GetOffsetMetadataMatcher matcher(cx, offset, &result);
  if (!referent.match(matcher)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

namespace {

/*
 * FlowGraphSummary::populate(cx, script) computes a summary of script's
 * control flow graph used by DebuggerScript_{getAllOffsets,getLineOffsets}.
 *
 * An instruction on a given line is an entry point for that line if it can be
 * reached from (an instruction on) a different line. We distinguish between the
 * following cases:
 *   - hasNoEdges:
 *       The instruction cannot be reached, so the instruction is not an entry
 *       point for the line it is on.
 *   - hasSingleEdge:
 *       The instruction can be reached from a single line. If this line is
 *       different from the line the instruction is on, the instruction is an
 *       entry point for that line.
 *
 * Similarly, an instruction on a given position (line/column pair) is an
 * entry point for that position if it can be reached from (an instruction on) a
 * different position. Again, we distinguish between the following cases:
 *   - hasNoEdges:
 *       The instruction cannot be reached, so the instruction is not an entry
 *       point for the position it is on.
 *   - hasSingleEdge:
 *       The instruction can be reached from a single position. If this line is
 *       different from the position the instruction is on, the instruction is
 *       an entry point for that position.
 */
class FlowGraphSummary {
 public:
  class Entry {
   public:
    static Entry createWithSingleEdge(size_t lineno, size_t column) {
      return Entry(lineno, column);
    }

    static Entry createWithMultipleEdgesFromSingleLine(size_t lineno) {
      return Entry(lineno, SIZE_MAX);
    }

    static Entry createWithMultipleEdgesFromMultipleLines() {
      return Entry(SIZE_MAX, SIZE_MAX);
    }

    Entry() : lineno_(SIZE_MAX), column_(0) {}

    bool hasNoEdges() const {
      return lineno_ == SIZE_MAX && column_ != SIZE_MAX;
    }

    bool hasSingleEdge() const {
      return lineno_ != SIZE_MAX && column_ != SIZE_MAX;
    }

    size_t lineno() const { return lineno_; }

    size_t column() const { return column_; }

   private:
    Entry(size_t lineno, size_t column) : lineno_(lineno), column_(column) {}

    size_t lineno_;
    size_t column_;
  };

  explicit FlowGraphSummary(JSContext* cx) : entries_(cx) {}

  Entry& operator[](size_t index) { return entries_[index]; }

  bool populate(JSContext* cx, JSScript* script) {
    if (!entries_.growBy(script->length())) {
      return false;
    }
    unsigned mainOffset = script->pcToOffset(script->main());
    entries_[mainOffset] = Entry::createWithMultipleEdgesFromMultipleLines();

    size_t prevLineno = script->lineno();
    size_t prevColumn = 0;
    JSOp prevOp = JSOP_NOP;
    for (BytecodeRangeWithPosition r(cx, script); !r.empty(); r.popFront()) {
      size_t lineno = prevLineno;
      size_t column = prevColumn;
      JSOp op = r.frontOpcode();

      if (FlowsIntoNext(prevOp)) {
        addEdge(prevLineno, prevColumn, r.frontOffset());
      }

      // If we visit the branch target before we visit the
      // branch op itself, just reuse the previous location.
      // This is reasonable for the time being because this
      // situation can currently only arise from loop heads,
      // where this assumption holds.
      if (BytecodeIsJumpTarget(op) && !entries_[r.frontOffset()].hasNoEdges()) {
        lineno = entries_[r.frontOffset()].lineno();
        column = entries_[r.frontOffset()].column();
      }

      if (r.frontIsEntryPoint()) {
        lineno = r.frontLineNumber();
        column = r.frontColumnNumber();
      }

      if (CodeSpec[op].type() == JOF_JUMP) {
        addEdge(lineno, column, r.frontOffset() + GET_JUMP_OFFSET(r.frontPC()));
      } else if (op == JSOP_TABLESWITCH) {
        jsbytecode* const switchPC = r.frontPC();
        jsbytecode* pc = switchPC;
        size_t offset = r.frontOffset();
        ptrdiff_t step = JUMP_OFFSET_LEN;
        size_t defaultOffset = offset + GET_JUMP_OFFSET(pc);
        pc += step;
        addEdge(lineno, column, defaultOffset);

        int32_t low = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        int ncases = GET_JUMP_OFFSET(pc) - low + 1;
        pc += JUMP_OFFSET_LEN;

        for (int i = 0; i < ncases; i++) {
          size_t target = script->tableSwitchCaseOffset(switchPC, i);
          addEdge(lineno, column, target);
        }
      } else if (op == JSOP_TRY) {
        // As there is no literal incoming edge into the catch block, we
        // make a fake one by copying the JSOP_TRY location, as-if this
        // was an incoming edge of the catch block. This is needed
        // because we only report offsets of entry points which have
        // valid incoming edges.
        for (const JSTryNote& tn : script->trynotes()) {
          if (tn.start == r.frontOffset() + 1) {
            uint32_t catchOffset = tn.start + tn.length;
            if (tn.kind == JSTRY_CATCH || tn.kind == JSTRY_FINALLY) {
              addEdge(lineno, column, catchOffset);
            }
          }
        }
      }

      prevLineno = lineno;
      prevColumn = column;
      prevOp = op;
    }

    return true;
  }

 private:
  void addEdge(size_t sourceLineno, size_t sourceColumn, size_t targetOffset) {
    if (entries_[targetOffset].hasNoEdges()) {
      entries_[targetOffset] =
          Entry::createWithSingleEdge(sourceLineno, sourceColumn);
    } else if (entries_[targetOffset].lineno() != sourceLineno) {
      entries_[targetOffset] =
          Entry::createWithMultipleEdgesFromMultipleLines();
    } else if (entries_[targetOffset].column() != sourceColumn) {
      entries_[targetOffset] =
          Entry::createWithMultipleEdgesFromSingleLine(sourceLineno);
    }
  }

  Vector<Entry> entries_;
};

} /* anonymous namespace */

class DebuggerScript::GetOffsetLocationMatcher {
  JSContext* cx_;
  size_t offset_;
  MutableHandlePlainObject result_;

 public:
  explicit GetOffsetLocationMatcher(JSContext* cx, size_t offset,
                                    MutableHandlePlainObject result)
      : cx_(cx), offset_(offset), result_(result) {}
  using ReturnType = bool;
  ReturnType match(HandleScript script) {
    if (!EnsureScriptOffsetIsValid(cx_, script, offset_)) {
      return false;
    }

    FlowGraphSummary flowData(cx_);
    if (!flowData.populate(cx_, script)) {
      return false;
    }

    result_.set(NewBuiltinClassInstance<PlainObject>(cx_));
    if (!result_) {
      return false;
    }

    BytecodeRangeWithPosition r(cx_, script);
    while (!r.empty() && r.frontOffset() < offset_) {
      r.popFront();
    }

    size_t offset = r.frontOffset();
    bool isEntryPoint = r.frontIsEntryPoint();

    // Line numbers are only correctly defined on entry points. Thus looks
    // either for the next valid offset in the flowData, being the last entry
    // point flowing into the current offset, or for the next valid entry point.
    while (!r.frontIsEntryPoint() &&
           !flowData[r.frontOffset()].hasSingleEdge()) {
      r.popFront();
      MOZ_ASSERT(!r.empty());
    }

    // If this is an entry point, take the line number associated with the entry
    // point, otherwise settle on the next instruction and take the incoming
    // edge position.
    size_t lineno;
    size_t column;
    if (r.frontIsEntryPoint()) {
      lineno = r.frontLineNumber();
      column = r.frontColumnNumber();
    } else {
      MOZ_ASSERT(flowData[r.frontOffset()].hasSingleEdge());
      lineno = flowData[r.frontOffset()].lineno();
      column = flowData[r.frontOffset()].column();
    }

    RootedValue value(cx_, NumberValue(lineno));
    if (!DefineDataProperty(cx_, result_, cx_->names().lineNumber, value)) {
      return false;
    }

    value = NumberValue(column);
    if (!DefineDataProperty(cx_, result_, cx_->names().columnNumber, value)) {
      return false;
    }

    // The same entry point test that is used by getAllColumnOffsets.
    isEntryPoint = (isEntryPoint && !flowData[offset].hasNoEdges() &&
                    (flowData[offset].lineno() != r.frontLineNumber() ||
                     flowData[offset].column() != r.frontColumnNumber()));
    value.setBoolean(isEntryPoint);
    if (!DefineDataProperty(cx_, result_, cx_->names().isEntryPoint, value)) {
      return false;
    }

    return true;
  }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    RootedScript script(cx_, DelazifyScript(cx_, lazyScript));
    if (!script) {
      return false;
    }
    return match(script);
  }
  ReturnType match(Handle<WasmInstanceObject*> instanceObj) {
    wasm::Instance& instance = instanceObj->instance();
    if (!instance.debugEnabled()) {
      JS_ReportErrorNumberASCII(cx_, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_BAD_OFFSET);
      return false;
    }

    size_t lineno;
    size_t column;
    if (!instance.debug().getOffsetLocation(offset_, &lineno, &column)) {
      JS_ReportErrorNumberASCII(cx_, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_BAD_OFFSET);
      return false;
    }

    result_.set(NewBuiltinClassInstance<PlainObject>(cx_));
    if (!result_) {
      return false;
    }

    RootedValue value(cx_, NumberValue(lineno));
    if (!DefineDataProperty(cx_, result_, cx_->names().lineNumber, value)) {
      return false;
    }

    value = NumberValue(column);
    if (!DefineDataProperty(cx_, result_, cx_->names().columnNumber, value)) {
      return false;
    }

    value.setBoolean(true);
    if (!DefineDataProperty(cx_, result_, cx_->names().isEntryPoint, value)) {
      return false;
    }

    return true;
  }
};

/* static */
bool DebuggerScript::getOffsetLocation(JSContext* cx, unsigned argc,
                                       Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "getOffsetLocation", args, obj,
                            referent);
  if (!args.requireAtLeast(cx, "Debugger.Script.getOffsetLocation", 1)) {
    return false;
  }
  size_t offset;
  if (!ScriptOffset(cx, args[0], &offset)) {
    return false;
  }

  RootedPlainObject result(cx);
  GetOffsetLocationMatcher matcher(cx, offset, &result);
  if (!referent.match(matcher)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

class DebuggerScript::GetSuccessorOrPredecessorOffsetsMatcher {
  JSContext* cx_;
  size_t offset_;
  bool successor_;
  MutableHandleObject result_;

 public:
  GetSuccessorOrPredecessorOffsetsMatcher(JSContext* cx, size_t offset,
                                          bool successor,
                                          MutableHandleObject result)
      : cx_(cx), offset_(offset), successor_(successor), result_(result) {}

  using ReturnType = bool;

  ReturnType match(HandleScript script) {
    if (!EnsureScriptOffsetIsValid(cx_, script, offset_)) {
      return false;
    }

    PcVector adjacent;
    if (successor_) {
      if (!GetSuccessorBytecodes(script, script->code() + offset_, adjacent)) {
        ReportOutOfMemory(cx_);
        return false;
      }
    } else {
      if (!GetPredecessorBytecodes(script, script->code() + offset_,
                                   adjacent)) {
        ReportOutOfMemory(cx_);
        return false;
      }
    }

    result_.set(NewDenseEmptyArray(cx_));
    if (!result_) {
      return false;
    }

    for (jsbytecode* pc : adjacent) {
      if (!NewbornArrayPush(cx_, result_, NumberValue(pc - script->code()))) {
        return false;
      }
    }
    return true;
  }

  ReturnType match(Handle<LazyScript*> lazyScript) {
    RootedScript script(cx_, DelazifyScript(cx_, lazyScript));
    if (!script) {
      return false;
    }
    return match(script);
  }

  ReturnType match(Handle<WasmInstanceObject*> instance) {
    JS_ReportErrorASCII(
        cx_, "getSuccessorOrPredecessorOffsets NYI on wasm instances");
    return false;
  }
};

/* static */
bool DebuggerScript::getSuccessorOrPredecessorOffsets(JSContext* cx,
                                                      unsigned argc, Value* vp,
                                                      const char* name,
                                                      bool successor) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, name, args, obj, referent);

  if (!args.requireAtLeast(cx, name, 1)) {
    return false;
  }
  size_t offset;
  if (!ScriptOffset(cx, args[0], &offset)) {
    return false;
  }

  RootedObject result(cx);
  GetSuccessorOrPredecessorOffsetsMatcher matcher(cx, offset, successor,
                                                  &result);
  if (!referent.match(matcher)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

/* static */
bool DebuggerScript::getSuccessorOffsets(JSContext* cx, unsigned argc,
                                         Value* vp) {
  return DebuggerScript::getSuccessorOrPredecessorOffsets(
      cx, argc, vp, "getSuccessorOffsets", true);
}

/* static */
bool DebuggerScript::getPredecessorOffsets(JSContext* cx, unsigned argc,
                                           Value* vp) {
  return DebuggerScript::getSuccessorOrPredecessorOffsets(
      cx, argc, vp, "getPredecessorOffsets", false);
}

/* static */
bool DebuggerScript::getAllOffsets(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_DELAZIFY(cx, argc, vp, "getAllOffsets", args, obj,
                                   script);

  // First pass: determine which offsets in this script are jump targets and
  // which line numbers jump to them.
  FlowGraphSummary flowData(cx);
  if (!flowData.populate(cx, script)) {
    return false;
  }

  // Second pass: build the result array.
  RootedObject result(cx, NewDenseEmptyArray(cx));
  if (!result) {
    return false;
  }
  for (BytecodeRangeWithPosition r(cx, script); !r.empty(); r.popFront()) {
    if (!r.frontIsEntryPoint()) {
      continue;
    }

    size_t offset = r.frontOffset();
    size_t lineno = r.frontLineNumber();

    // Make a note, if the current instruction is an entry point for the current
    // line.
    if (!flowData[offset].hasNoEdges() && flowData[offset].lineno() != lineno) {
      // Get the offsets array for this line.
      RootedObject offsets(cx);
      RootedValue offsetsv(cx);

      RootedId id(cx, INT_TO_JSID(lineno));

      bool found;
      if (!HasOwnProperty(cx, result, id, &found)) {
        return false;
      }
      if (found && !GetProperty(cx, result, result, id, &offsetsv)) {
        return false;
      }

      if (offsetsv.isObject()) {
        offsets = &offsetsv.toObject();
      } else {
        MOZ_ASSERT(offsetsv.isUndefined());

        // Create an empty offsets array for this line.
        // Store it in the result array.
        RootedId id(cx);
        RootedValue v(cx, NumberValue(lineno));
        offsets = NewDenseEmptyArray(cx);
        if (!offsets || !ValueToId<CanGC>(cx, v, &id)) {
          return false;
        }

        RootedValue value(cx, ObjectValue(*offsets));
        if (!DefineDataProperty(cx, result, id, value)) {
          return false;
        }
      }

      // Append the current offset to the offsets array.
      if (!NewbornArrayPush(cx, offsets, NumberValue(offset))) {
        return false;
      }
    }
  }

  args.rval().setObject(*result);
  return true;
}

class DebuggerScript::GetAllColumnOffsetsMatcher {
  JSContext* cx_;
  MutableHandleObject result_;

  bool appendColumnOffsetEntry(size_t lineno, size_t column, size_t offset) {
    RootedPlainObject entry(cx_, NewBuiltinClassInstance<PlainObject>(cx_));
    if (!entry) {
      return false;
    }

    RootedValue value(cx_, NumberValue(lineno));
    if (!DefineDataProperty(cx_, entry, cx_->names().lineNumber, value)) {
      return false;
    }

    value = NumberValue(column);
    if (!DefineDataProperty(cx_, entry, cx_->names().columnNumber, value)) {
      return false;
    }

    value = NumberValue(offset);
    if (!DefineDataProperty(cx_, entry, cx_->names().offset, value)) {
      return false;
    }

    return NewbornArrayPush(cx_, result_, ObjectValue(*entry));
  }

 public:
  explicit GetAllColumnOffsetsMatcher(JSContext* cx, MutableHandleObject result)
      : cx_(cx), result_(result) {}
  using ReturnType = bool;
  ReturnType match(HandleScript script) {
    // First pass: determine which offsets in this script are jump targets
    // and which positions jump to them.
    FlowGraphSummary flowData(cx_);
    if (!flowData.populate(cx_, script)) {
      return false;
    }

    // Second pass: build the result array.
    result_.set(NewDenseEmptyArray(cx_));
    if (!result_) {
      return false;
    }

    for (BytecodeRangeWithPosition r(cx_, script); !r.empty(); r.popFront()) {
      size_t lineno = r.frontLineNumber();
      size_t column = r.frontColumnNumber();
      size_t offset = r.frontOffset();

      // Make a note, if the current instruction is an entry point for
      // the current position.
      if (r.frontIsEntryPoint() && !flowData[offset].hasNoEdges() &&
          (flowData[offset].lineno() != lineno ||
           flowData[offset].column() != column)) {
        if (!appendColumnOffsetEntry(lineno, column, offset)) {
          return false;
        }
      }
    }
    return true;
  }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    RootedScript script(cx_, DelazifyScript(cx_, lazyScript));
    if (!script) {
      return false;
    }
    return match(script);
  }
  ReturnType match(Handle<WasmInstanceObject*> instanceObj) {
    wasm::Instance& instance = instanceObj->instance();

    Vector<wasm::ExprLoc> offsets(cx_);
    if (instance.debugEnabled() &&
        !instance.debug().getAllColumnOffsets(cx_, &offsets)) {
      return false;
    }

    result_.set(NewDenseEmptyArray(cx_));
    if (!result_) {
      return false;
    }

    for (uint32_t i = 0; i < offsets.length(); i++) {
      size_t lineno = offsets[i].lineno;
      size_t column = offsets[i].column;
      size_t offset = offsets[i].offset;
      if (!appendColumnOffsetEntry(lineno, column, offset)) {
        return false;
      }
    }
    return true;
  }
};

/* static */
bool DebuggerScript::getAllColumnOffsets(JSContext* cx, unsigned argc,
                                         Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "getAllColumnOffsets", args, obj,
                            referent);

  RootedObject result(cx);
  GetAllColumnOffsetsMatcher matcher(cx, &result);
  if (!referent.match(matcher)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

class DebuggerScript::GetLineOffsetsMatcher {
  JSContext* cx_;
  size_t lineno_;
  MutableHandleObject result_;

 public:
  explicit GetLineOffsetsMatcher(JSContext* cx, size_t lineno,
                                 MutableHandleObject result)
      : cx_(cx), lineno_(lineno), result_(result) {}
  using ReturnType = bool;
  ReturnType match(HandleScript script) {
    // First pass: determine which offsets in this script are jump targets and
    // which line numbers jump to them.
    FlowGraphSummary flowData(cx_);
    if (!flowData.populate(cx_, script)) {
      return false;
    }

    result_.set(NewDenseEmptyArray(cx_));
    if (!result_) {
      return false;
    }

    // Second pass: build the result array.
    for (BytecodeRangeWithPosition r(cx_, script); !r.empty(); r.popFront()) {
      if (!r.frontIsEntryPoint()) {
        continue;
      }

      size_t offset = r.frontOffset();

      // If the op at offset is an entry point, append offset to result.
      if (r.frontLineNumber() == lineno_ && !flowData[offset].hasNoEdges() &&
          flowData[offset].lineno() != lineno_) {
        if (!NewbornArrayPush(cx_, result_, NumberValue(offset))) {
          return false;
        }
      }
    }

    return true;
  }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    RootedScript script(cx_, DelazifyScript(cx_, lazyScript));
    if (!script) {
      return false;
    }
    return match(script);
  }
  ReturnType match(Handle<WasmInstanceObject*> instanceObj) {
    wasm::Instance& instance = instanceObj->instance();

    Vector<uint32_t> offsets(cx_);
    if (instance.debugEnabled() &&
        !instance.debug().getLineOffsets(cx_, lineno_, &offsets)) {
      return false;
    }

    result_.set(NewDenseEmptyArray(cx_));
    if (!result_) {
      return false;
    }

    for (uint32_t i = 0; i < offsets.length(); i++) {
      if (!NewbornArrayPush(cx_, result_, NumberValue(offsets[i]))) {
        return false;
      }
    }
    return true;
  }
};

/* static */
bool DebuggerScript::getLineOffsets(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "getLineOffsets", args, obj,
                            referent);
  if (!args.requireAtLeast(cx, "Debugger.Script.getLineOffsets", 1)) {
    return false;
  }

  // Parse lineno argument.
  RootedValue linenoValue(cx, args[0]);
  size_t lineno;
  if (!ToNumber(cx, &linenoValue)) {
    return false;
  }
  {
    double d = linenoValue.toNumber();
    lineno = size_t(d);
    if (lineno != d) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_BAD_LINE);
      return false;
    }
  }

  RootedObject result(cx);
  GetLineOffsetsMatcher matcher(cx, lineno, &result);
  if (!referent.match(matcher)) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

struct DebuggerScript::SetBreakpointMatcher {
  JSContext* cx_;
  Debugger* dbg_;
  size_t offset_;
  RootedObject handler_;

 public:
  explicit SetBreakpointMatcher(JSContext* cx, Debugger* dbg, size_t offset,
                                HandleObject handler)
      : cx_(cx), dbg_(dbg), offset_(offset), handler_(cx, handler) {}

  using ReturnType = bool;

  ReturnType match(HandleScript script) {
    if (!dbg_->observesScript(script)) {
      JS_ReportErrorNumberASCII(cx_, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_NOT_DEBUGGING);
      return false;
    }

    if (!EnsureScriptOffsetIsValid(cx_, script, offset_)) {
      return false;
    }

    // Ensure observability *before* setting the breakpoint. If the script is
    // not already a debuggee, trying to ensure observability after setting
    // the breakpoint (and thus marking the script as a debuggee) will skip
    // actually ensuring observability.
    if (!dbg_->ensureExecutionObservabilityOfScript(cx_, script)) {
      return false;
    }

    jsbytecode* pc = script->offsetToPC(offset_);
    BreakpointSite* site =
        DebugScript::getOrCreateBreakpointSite(cx_, script, pc);
    if (!site) {
      return false;
    }
    site->inc(cx_->runtime()->defaultFreeOp());
    if (cx_->zone()->new_<Breakpoint>(dbg_, site, handler_)) {
      AddCellMemory(script, sizeof(Breakpoint), MemoryUse::Breakpoint);
      return true;
    }
    site->dec(cx_->runtime()->defaultFreeOp());
    site->destroyIfEmpty(cx_->runtime()->defaultFreeOp());
    return false;
  }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    RootedScript script(cx_, DelazifyScript(cx_, lazyScript));
    if (!script) {
      return false;
    }
    return match(script);
  }
  ReturnType match(Handle<WasmInstanceObject*> wasmInstance) {
    wasm::Instance& instance = wasmInstance->instance();
    if (!instance.debugEnabled() ||
        !instance.debug().hasBreakpointTrapAtOffset(offset_)) {
      JS_ReportErrorNumberASCII(cx_, GetErrorMessage, nullptr,
                                JSMSG_DEBUG_BAD_OFFSET);
      return false;
    }
    WasmBreakpointSite* site = instance.getOrCreateBreakpointSite(cx_, offset_);
    if (!site) {
      return false;
    }
    site->inc(cx_->runtime()->defaultFreeOp());
    if (cx_->zone()->new_<WasmBreakpoint>(dbg_, site, handler_,
                                          instance.object())) {
      AddCellMemory(wasmInstance, sizeof(WasmBreakpoint),
                    MemoryUse::Breakpoint);
      return true;
    }
    site->dec(cx_->runtime()->defaultFreeOp());
    site->destroyIfEmpty(cx_->runtime()->defaultFreeOp());
    return false;
  }
};

/* static */
bool DebuggerScript::setBreakpoint(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "setBreakpoint", args, obj, referent);
  if (!args.requireAtLeast(cx, "Debugger.Script.setBreakpoint", 2)) {
    return false;
  }
  Debugger* dbg = Debugger::fromChildJSObject(obj);

  size_t offset;
  if (!ScriptOffset(cx, args[0], &offset)) {
    return false;
  }

  RootedObject handler(cx, RequireObject(cx, args[1]));
  if (!handler) {
    return false;
  }

  SetBreakpointMatcher matcher(cx, dbg, offset, handler);
  if (!referent.match(matcher)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

/* static */
bool DebuggerScript::getBreakpoints(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_DELAZIFY(cx, argc, vp, "getBreakpoints", args, obj,
                                   script);
  Debugger* dbg = Debugger::fromChildJSObject(obj);

  jsbytecode* pc;
  if (args.length() > 0) {
    size_t offset;
    if (!ScriptOffset(cx, args[0], &offset) ||
        !EnsureScriptOffsetIsValid(cx, script, offset)) {
      return false;
    }
    pc = script->offsetToPC(offset);
  } else {
    pc = nullptr;
  }

  RootedObject arr(cx, NewDenseEmptyArray(cx));
  if (!arr) {
    return false;
  }

  for (unsigned i = 0; i < script->length(); i++) {
    BreakpointSite* site =
        DebugScript::getBreakpointSite(script, script->offsetToPC(i));
    if (!site) {
      continue;
    }
    MOZ_ASSERT(site->type() == BreakpointSite::Type::JS);
    if (!pc || site->asJS()->pc == pc) {
      for (Breakpoint* bp = site->firstBreakpoint(); bp;
           bp = bp->nextInSite()) {
        if (bp->debugger == dbg &&
            !NewbornArrayPush(cx, arr, ObjectValue(*bp->getHandler()))) {
          return false;
        }
      }
    }
  }
  args.rval().setObject(*arr);
  return true;
}

class DebuggerScript::ClearBreakpointMatcher {
  JSContext* cx_;
  Debugger* dbg_;
  JSObject* handler_;

 public:
  ClearBreakpointMatcher(JSContext* cx, Debugger* dbg, JSObject* handler)
      : cx_(cx), dbg_(dbg), handler_(handler) {}
  using ReturnType = bool;

  ReturnType match(HandleScript script) {
    DebugScript::clearBreakpointsIn(cx_->runtime()->defaultFreeOp(), script,
                                    dbg_, handler_);
    return true;
  }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    RootedScript script(cx_, DelazifyScript(cx_, lazyScript));
    if (!script) {
      return false;
    }
    return match(script);
  }
  ReturnType match(Handle<WasmInstanceObject*> instanceObj) {
    wasm::Instance& instance = instanceObj->instance();
    if (!instance.debugEnabled()) {
      return true;
    }
    instance.debug().clearBreakpointsIn(cx_->runtime()->defaultFreeOp(),
                                        instanceObj, dbg_, handler_);
    return true;
  }
};

/* static */
bool DebuggerScript::clearBreakpoint(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "clearBreakpoint", args, obj,
                            referent);
  if (!args.requireAtLeast(cx, "Debugger.Script.clearBreakpoint", 1)) {
    return false;
  }
  Debugger* dbg = Debugger::fromChildJSObject(obj);

  JSObject* handler = RequireObject(cx, args[0]);
  if (!handler) {
    return false;
  }

  ClearBreakpointMatcher matcher(cx, dbg, handler);
  if (!referent.match(matcher)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/* static */
bool DebuggerScript::clearAllBreakpoints(JSContext* cx, unsigned argc,
                                         Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "clearAllBreakpoints", args, obj,
                            referent);
  Debugger* dbg = Debugger::fromChildJSObject(obj);
  ClearBreakpointMatcher matcher(cx, dbg, nullptr);
  if (!referent.match(matcher)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

class DebuggerScript::IsInCatchScopeMatcher {
  JSContext* cx_;
  size_t offset_;
  bool isInCatch_;

 public:
  explicit IsInCatchScopeMatcher(JSContext* cx, size_t offset)
      : cx_(cx), offset_(offset), isInCatch_(false) {}
  using ReturnType = bool;

  inline bool isInCatch() const { return isInCatch_; }

  ReturnType match(HandleScript script) {
    if (!EnsureScriptOffsetIsValid(cx_, script, offset_)) {
      return false;
    }

    for (const JSTryNote& tn : script->trynotes()) {
      if (tn.start <= offset_ && offset_ < tn.start + tn.length &&
          tn.kind == JSTRY_CATCH) {
        isInCatch_ = true;
        return true;
      }
    }

    isInCatch_ = false;
    return true;
  }
  ReturnType match(Handle<LazyScript*> lazyScript) {
    RootedScript script(cx_, DelazifyScript(cx_, lazyScript));
    if (!script) {
      return false;
    }
    return match(script);
  }
  ReturnType match(Handle<WasmInstanceObject*> instance) {
    isInCatch_ = false;
    return true;
  }
};

/* static */
bool DebuggerScript::isInCatchScope(JSContext* cx, unsigned argc, Value* vp) {
  THIS_DEBUGSCRIPT_REFERENT(cx, argc, vp, "isInCatchScope", args, obj,
                            referent);
  if (!args.requireAtLeast(cx, "Debugger.Script.isInCatchScope", 1)) {
    return false;
  }

  size_t offset;
  if (!ScriptOffset(cx, args[0], &offset)) {
    return false;
  }

  IsInCatchScopeMatcher matcher(cx, offset);
  if (!referent.match(matcher)) {
    return false;
  }
  args.rval().setBoolean(matcher.isInCatch());
  return true;
}

/* static */
bool DebuggerScript::getOffsetsCoverage(JSContext* cx, unsigned argc,
                                        Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_DELAZIFY(cx, argc, vp, "getOffsetsCoverage", args,
                                   obj, script);

  // If the script has no coverage information, then skip this and return null
  // instead.
  if (!script->hasScriptCounts()) {
    args.rval().setNull();
    return true;
  }

  ScriptCounts* sc = &script->getScriptCounts();

  // If the main ever got visited, then assume that any code before main got
  // visited once.
  uint64_t hits = 0;
  const PCCounts* counts =
      sc->maybeGetPCCounts(script->pcToOffset(script->main()));
  if (counts->numExec()) {
    hits = 1;
  }

  // Build an array of objects which are composed of 4 properties:
  //  - offset          PC offset of the current opcode.
  //  - lineNumber      Line of the current opcode.
  //  - columnNumber    Column of the current opcode.
  //  - count           Number of times the instruction got executed.
  RootedObject result(cx, NewDenseEmptyArray(cx));
  if (!result) {
    return false;
  }

  RootedId offsetId(cx, NameToId(cx->names().offset));
  RootedId lineNumberId(cx, NameToId(cx->names().lineNumber));
  RootedId columnNumberId(cx, NameToId(cx->names().columnNumber));
  RootedId countId(cx, NameToId(cx->names().count));

  RootedObject item(cx);
  RootedValue offsetValue(cx);
  RootedValue lineNumberValue(cx);
  RootedValue columnNumberValue(cx);
  RootedValue countValue(cx);

  // Iterate linearly over the bytecode.
  for (BytecodeRangeWithPosition r(cx, script); !r.empty(); r.popFront()) {
    size_t offset = r.frontOffset();

    // The beginning of each non-branching sequences of instruction set the
    // number of execution of the current instruction and any following
    // instruction.
    counts = sc->maybeGetPCCounts(offset);
    if (counts) {
      hits = counts->numExec();
    }

    offsetValue.setNumber(double(offset));
    lineNumberValue.setNumber(double(r.frontLineNumber()));
    columnNumberValue.setNumber(double(r.frontColumnNumber()));
    countValue.setNumber(double(hits));

    // Create a new object with the offset, line number, column number, the
    // number of hit counts, and append it to the array.
    item = NewObjectWithGivenProto<PlainObject>(cx, nullptr);
    if (!item || !DefineDataProperty(cx, item, offsetId, offsetValue) ||
        !DefineDataProperty(cx, item, lineNumberId, lineNumberValue) ||
        !DefineDataProperty(cx, item, columnNumberId, columnNumberValue) ||
        !DefineDataProperty(cx, item, countId, countValue) ||
        !NewbornArrayPush(cx, result, ObjectValue(*item))) {
      return false;
    }

    // If the current instruction has thrown, then decrement the hit counts
    // with the number of throws.
    counts = sc->maybeGetThrowCounts(offset);
    if (counts) {
      hits -= counts->numExec();
    }
  }

  args.rval().setObject(*result);
  return true;
}

/* static */
bool DebuggerScript::setInstrumentationId(JSContext* cx, unsigned argc,
                                          Value* vp) {
  THIS_DEBUGSCRIPT_SCRIPT_MAYBE_LAZY(cx, argc, vp, "setInstrumentationId", args,
                                     obj);

  if (!obj->getInstrumentationId().isUndefined()) {
    JS_ReportErrorASCII(cx, "Script instrumentation ID is already set");
    return false;
  }

  if (!args.get(0).isNumber()) {
    JS_ReportErrorASCII(cx, "Script instrumentation ID must be a number");
    return false;
  }

  obj->setReservedSlot(INSTRUMENTATION_ID_SLOT, args.get(0));

  args.rval().setUndefined();
  return true;
}

/* static */
bool DebuggerScript::construct(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                            "Debugger.Script");
  return false;
}

const JSPropertySpec DebuggerScript::properties_[] = {
    JS_PSG("isGeneratorFunction", getIsGeneratorFunction, 0),
    JS_PSG("isAsyncFunction", getIsAsyncFunction, 0),
    JS_PSG("isModule", getIsModule, 0),
    JS_PSG("displayName", getDisplayName, 0),
    JS_PSG("url", getUrl, 0),
    JS_PSG("startLine", getStartLine, 0),
    JS_PSG("startColumn", getStartColumn, 0),
    JS_PSG("lineCount", getLineCount, 0),
    JS_PSG("source", getSource, 0),
    JS_PSG("sourceStart", getSourceStart, 0),
    JS_PSG("sourceLength", getSourceLength, 0),
    JS_PSG("mainOffset", getMainOffset, 0),
    JS_PSG("global", getGlobal, 0),
    JS_PSG("format", getFormat, 0),
    JS_PS_END};

const JSFunctionSpec DebuggerScript::methods_[] = {
    JS_FN("getChildScripts", getChildScripts, 0, 0),
    JS_FN("getPossibleBreakpoints", getPossibleBreakpoints, 0, 0),
    JS_FN("getPossibleBreakpointOffsets", getPossibleBreakpointOffsets, 0, 0),
    JS_FN("setBreakpoint", setBreakpoint, 2, 0),
    JS_FN("getBreakpoints", getBreakpoints, 1, 0),
    JS_FN("clearBreakpoint", clearBreakpoint, 1, 0),
    JS_FN("clearAllBreakpoints", clearAllBreakpoints, 0, 0),
    JS_FN("isInCatchScope", isInCatchScope, 1, 0),
    JS_FN("getOffsetMetadata", getOffsetMetadata, 1, 0),
    JS_FN("getOffsetsCoverage", getOffsetsCoverage, 0, 0),
    JS_FN("getSuccessorOffsets", getSuccessorOffsets, 1, 0),
    JS_FN("getPredecessorOffsets", getPredecessorOffsets, 1, 0),
    JS_FN("setInstrumentationId", setInstrumentationId, 1, 0),

    // The following APIs are deprecated due to their reliance on the
    // under-defined 'entrypoint' concept. Make use of getPossibleBreakpoints,
    // getPossibleBreakpointOffsets, or getOffsetMetadata instead.
    JS_FN("getAllOffsets", getAllOffsets, 0, 0),
    JS_FN("getAllColumnOffsets", getAllColumnOffsets, 0, 0),
    JS_FN("getLineOffsets", getLineOffsets, 1, 0),
    JS_FN("getOffsetLocation", getOffsetLocation, 0, 0), JS_FS_END};
