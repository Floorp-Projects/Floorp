/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/WarpSnapshot.h"

#include "mozilla/IntegerPrintfMacros.h"

#include "vm/GlobalObject.h"
#include "vm/JSContext.h"
#include "vm/Printer.h"

#include "vm/EnvironmentObject-inl.h"

using namespace js;
using namespace js::jit;

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
  switch (environment_.kind()) {
    case WarpEnvironment::Kind::None:
      out.printf("None\n");
      break;
    case WarpEnvironment::Kind::ConstantObject:
      out.printf("Object: 0x%p\n", environment_.constantObject());
      break;
    case WarpEnvironment::Kind::Function:
      out.printf(
          "Function: callobject template 0x%p, named lambda template: 0x%p\n",
          environment_.maybeCallObjectTemplate(),
          environment_.maybeNamedLambdaTemplate());
      break;
  }

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
  switch (kind_) {
#  define DUMP(kind) \
    case Kind::kind: \
      return as<kind>()->dumpData(out);
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
