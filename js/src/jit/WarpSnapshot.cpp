/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/WarpSnapshot.h"

#include "mozilla/IntegerPrintfMacros.h"

#include <type_traits>

#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/Printer.h"

#include "vm/EnvironmentObject-inl.h"

using namespace js;
using namespace js::jit;

static_assert(!std::is_polymorphic<WarpOpSnapshot>::value,
              "WarpOpSnapshot should not have any virtual methods");

WarpSnapshot::WarpSnapshot(JSContext* cx, WarpScriptSnapshot* script)
    : script_(script),
      globalLexicalEnv_(&cx->global()->lexicalEnvironment()),
      globalLexicalEnvThis_(globalLexicalEnv_->thisValue()) {}

WarpScriptSnapshot::WarpScriptSnapshot(
    JSScript* script, const WarpEnvironment& env,
    WarpOpSnapshotList&& opSnapshots, ModuleObject* moduleObject,
    JSObject* instrumentationCallback,
    mozilla::Maybe<int32_t> instrumentationScriptId,
    mozilla::Maybe<bool> instrumentationActive)
    : script_(script),
      environment_(env),
      opSnapshots_(std::move(opSnapshots)),
      moduleObject_(moduleObject),
      instrumentationCallback_(instrumentationCallback),
      instrumentationScriptId_(instrumentationScriptId),
      instrumentationActive_(instrumentationActive),
      isArrowFunction_(script->isFunction() && script->function()->isArrow()) {}

#ifdef JS_JITSPEW
void WarpSnapshot::dump() const {
  Fprinter out(stderr);
  dump(out);
}

void WarpSnapshot::dump(GenericPrinter& out) const {
  out.printf("WarpSnapshot (0x%p)\n", this);
  out.printf("------------------------------\n");
  out.printf("globalLexicalEnv: 0x%p\n", globalLexicalEnv());
  out.printf("globalLexicalEnvThis: 0x%016" PRIx64 "\n",
             globalLexicalEnvThis().asRawBits());
  out.printf("\n");

  script_->dump(out);
}

void WarpScriptSnapshot::dump(GenericPrinter& out) const {
  out.printf("Script: %s:%u:%u (0x%p)\n", script_->filename(),
             script_->lineno(), script_->column(), script_);
  out.printf("  moduleObject: 0x%p\n", moduleObject());
  out.printf("  isArrowFunction: %u\n", isArrowFunction());

  out.printf("  environment: ");
  environment_.match(
      [&](const NoEnvironment&) { out.printf("None\n"); },
      [&](JSObject* obj) { out.printf("Object: 0x%p\n", obj); },
      [&](const FunctionEnvironment& env) {
        out.printf(
            "Function: callobject template 0x%p, named lambda template: 0x%p\n",
            env.callObjectTemplate, env.namedLambdaTemplate);
      });

  out.printf("\n");
  for (const WarpOpSnapshot* snapshot : opSnapshots()) {
    snapshot->dump(out, script_);
    out.printf("\n");
  }
}

static const char* OpSnapshotKindString(WarpOpSnapshot::Kind kind) {
  static const char* const names[] = {
#  define NAME(x) #  x,
      WARP_OP_SNAPSHOT_LIST(NAME)
#  undef NAME
  };
  return names[unsigned(kind)];
}

void WarpOpSnapshot::dump(GenericPrinter& out, JSScript* script) const {
  jsbytecode* pc = script->offsetToPC(offset_);
  out.printf("  %s (offset %u, JSOp::%s)\n", OpSnapshotKindString(kind_),
             offset_, CodeName(JSOp(*pc)));

  // Dispatch to dumpData() methods.
  switch (kind_) {
#  define DUMP(kind)             \
    case Kind::kind:             \
      as<kind>()->dumpData(out); \
      break;
    WARP_OP_SNAPSHOT_LIST(DUMP)
#  undef DUMP
  }
}

void WarpArguments::dumpData(GenericPrinter& out) const {
  out.printf("    template: 0x%p\n", templateObj());
}

void WarpRegExp::dumpData(GenericPrinter& out) const {
  out.printf("    hasShared: %u\n", hasShared());
}

void WarpFunctionProto::dumpData(GenericPrinter& out) const {
  out.printf("    proto: 0x%p\n", proto());
}

void WarpGetIntrinsic::dumpData(GenericPrinter& out) const {
  out.printf("    intrinsic: 0x%016" PRIx64 "\n", intrinsic().asRawBits());
}

void WarpGetImport::dumpData(GenericPrinter& out) const {
  out.printf("    targetEnv: 0x%p\n", targetEnv());
  out.printf("    numFixedSlots: %u\n", numFixedSlots());
  out.printf("    slot: %u\n", slot());
  out.printf("    needsLexicalCheck: %u\n", needsLexicalCheck());
}

void WarpLambda::dumpData(GenericPrinter& out) const {
  out.printf("    baseScript: 0x%p\n", baseScript());
  out.printf("    flags: 0x%x\n", unsigned(flags().toRaw()));
  out.printf("    nargs: %u\n", unsigned(nargs_));
}

void WarpRest::dumpData(GenericPrinter& out) const {
  out.printf("    template: 0x%p\n", templateObject());
}
#endif  // JS_JITSPEW

template <typename T>
static void TraceWarpGCPtr(JSTracer* trc, T& thing, const char* name) {
#ifdef DEBUG
  T thingBefore = thing;
#endif
  TraceManuallyBarrieredEdge(trc, &thing, name);
  MOZ_ASSERT(thingBefore == thing, "Unexpected moving GC!");
}

void WarpSnapshot::trace(JSTracer* trc) {
  script_->trace(trc);
  TraceWarpGCPtr(trc, globalLexicalEnv_, "warp-lexical");
  TraceWarpGCPtr(trc, globalLexicalEnvThis_, "warp-lexicalthis");
}

void WarpScriptSnapshot::trace(JSTracer* trc) {
  TraceWarpGCPtr(trc, script_, "warp-script");

  environment_.match(
      [](const NoEnvironment&) {},
      [trc](JSObject* obj) { TraceWarpGCPtr(trc, obj, "warp-env-object"); },
      [trc](FunctionEnvironment& env) {
        if (env.callObjectTemplate) {
          TraceWarpGCPtr(trc, env.callObjectTemplate, "warp-env-callobject");
        }
        if (env.namedLambdaTemplate) {
          TraceWarpGCPtr(trc, env.namedLambdaTemplate, "warp-env-namedlambda");
        }
      });

  for (WarpOpSnapshot* snapshot : opSnapshots_) {
    snapshot->trace(trc);
  }

  if (moduleObject_) {
    TraceWarpGCPtr(trc, moduleObject_, "warp-module-obj");
  }
  if (instrumentationCallback_) {
    TraceWarpGCPtr(trc, instrumentationCallback_, "warp-instr-callback");
  }
}

void WarpOpSnapshot::trace(JSTracer* trc) {
  // Dispatch to traceData() methods.
  switch (kind_) {
#define TRACE(kind)             \
  case Kind::kind:              \
    as<kind>()->traceData(trc); \
    break;
    WARP_OP_SNAPSHOT_LIST(TRACE)
#undef TRACE
  }
}

void WarpArguments::traceData(JSTracer* trc) {
  if (templateObj_) {
    TraceWarpGCPtr(trc, templateObj_, "warp-args-template");
  }
}

void WarpRegExp::traceData(JSTracer* trc) {
  // No GC pointers.
}

void WarpFunctionProto::traceData(JSTracer* trc) {
  TraceWarpGCPtr(trc, proto_, "warp-function-proto");
}

void WarpGetIntrinsic::traceData(JSTracer* trc) {
  TraceWarpGCPtr(trc, intrinsic_, "warp-intrinsic");
}

void WarpGetImport::traceData(JSTracer* trc) {
  TraceWarpGCPtr(trc, targetEnv_, "warp-import-env");
}

void WarpLambda::traceData(JSTracer* trc) {
  TraceWarpGCPtr(trc, baseScript_, "warp-lambda-basescript");
}

void WarpRest::traceData(JSTracer* trc) {
  TraceWarpGCPtr(trc, templateObject_, "warp-rest-template");
}
