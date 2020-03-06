/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/Frontend2.h"

#include "mozilla/Span.h"  // mozilla::{Span, MakeSpan}

#include <stddef.h>  // size_t
#include <stdint.h>  // uint8_t, uint32_t

#include "jsapi.h"

#include "frontend/CompilationInfo.h"  // CompilationInfo
#include "frontend/smoosh_generated.h"  // CVec, SmooshResult, SmooshCompileOptions, free_smoosh, run_smoosh
#include "frontend/SourceNotes.h"  // jssrcnote
#include "gc/Rooting.h"            // RootedScriptSourceObject
#include "js/HeapAPI.h"            // JS::GCCellPtr
#include "js/RootingAPI.h"         // JS::Handle
#include "js/TypeDecls.h"          // Rooted{Script,Value,String,Object}
#include "vm/JSAtom.h"             // AtomizeUTF8Chars
#include "vm/JSScript.h"           // JSScript

#include "vm/JSContext-inl.h"  // AutoKeepAtoms (used by BytecodeCompiler)

using mozilla::Utf8Unit;

using namespace js::gc;
using namespace js::frontend;
using namespace js;

namespace js {

namespace frontend {

class SmooshScriptStencil : public ScriptStencil {
  const SmooshResult& result_;

  void init() {
    lineno = result_.lineno;
    column = result_.column;

    natoms = result_.strings.len;

    ngcthings = 1;

    numResumeOffsets = 0;
    numScopeNotes = 0;
    numTryNotes = 0;

    mainOffset = result_.main_offset;
    nfixed = result_.max_fixed_slots;
    nslots = nfixed + result_.maximum_stack_depth;
    bodyScopeIndex = result_.body_scope_index;
    numICEntries = result_.num_ic_entries;
    numBytecodeTypeSets = result_.num_type_sets;

    strict = result_.strict;
    bindingsAccessedDynamically = result_.bindings_accessed_dynamically;
    hasCallSiteObj = result_.has_call_site_obj;
    isForEval = result_.is_for_eval;
    isModule = result_.is_module;
    isFunction = result_.is_function;
    hasNonSyntacticScope = result_.has_non_syntactic_scope;
    needsFunctionEnvironmentObjects =
        result_.needs_function_environment_objects;
    hasModuleGoal = result_.has_module_goal;

    code = mozilla::MakeSpan(result_.bytecode.data, result_.bytecode.len);
    MOZ_ASSERT(notes.IsEmpty());
  }

 public:
  explicit SmooshScriptStencil(const SmooshResult& result) : result_(result) {
    init();
  }

  virtual bool finishGCThings(JSContext* cx,
                              mozilla::Span<JS::GCCellPtr> gcthings) const {
    gcthings[0] = JS::GCCellPtr(&cx->global()->emptyGlobalScope());
    return true;
  }

  virtual bool initAtomMap(JSContext* cx, GCPtrAtom* atoms) const {
    for (uint32_t i = 0; i < natoms; i++) {
      const CVec<uint8_t>& string = result_.strings.data[i];
      JSAtom* atom = AtomizeUTF8Chars(cx, (const char*)string.data, string.len);
      if (!atom) {
        return false;
      }
      atoms[i] = atom;
    }

    return true;
  }

  virtual void finishResumeOffsets(
      mozilla::Span<uint32_t> resumeOffsets) const {}

  virtual void finishScopeNotes(mozilla::Span<ScopeNote> scopeNotes) const {}

  virtual void finishTryNotes(mozilla::Span<JSTryNote> tryNotes) const {}

  virtual void finishInnerFunctions() const {}
};

// Free given SmooshResult on leaving scope.
class AutoFreeSmooshResult {
  SmooshResult* result_;

 public:
  AutoFreeSmooshResult() = delete;

  explicit AutoFreeSmooshResult(SmooshResult* result) : result_(result) {}
  ~AutoFreeSmooshResult() {
    if (result_) {
      free_smoosh(*result_);
    }
  }
};

// Free given SmooshParseResult on leaving scope.
class AutoFreeSmooshParseResult {
  SmooshParseResult* result_;

 public:
  AutoFreeSmooshParseResult() = delete;

  explicit AutoFreeSmooshParseResult(SmooshParseResult* result)
      : result_(result) {}
  ~AutoFreeSmooshParseResult() {
    if (result_) {
      free_smoosh_parse_result(*result_);
    }
  }
};

void ReportSmooshCompileError(JSContext* cx, ErrorMetadata&& metadata,
                              int errorNumber, ...) {
  va_list args;
  va_start(args, errorNumber);
  ReportCompileErrorUTF8(cx, std::move(metadata), /* notes = */ nullptr,
                         JSREPORT_ERROR, errorNumber, &args);
  va_end(args);
}

/* static */
JSScript* Smoosh::compileGlobalScript(CompilationInfo& compilationInfo,
                                      JS::SourceText<Utf8Unit>& srcBuf,
                                      bool* unimplemented) {
  // FIXME: check info members and return with *unimplemented = true
  //        if any field doesn't match to run_smoosh.

  auto bytes = reinterpret_cast<const uint8_t*>(srcBuf.get());
  size_t length = srcBuf.length();

  JSContext* cx = compilationInfo.cx;

  const auto& options = compilationInfo.options;
  SmooshCompileOptions compileOptions;
  compileOptions.no_script_rval = options.noScriptRval;

  SmooshResult smoosh = run_smoosh(bytes, length, &compileOptions);
  AutoFreeSmooshResult afsr(&smoosh);

  if (smoosh.error.data) {
    *unimplemented = false;
    ErrorMetadata metadata;
    metadata.filename = "<unknown>";
    metadata.lineNumber = 1;
    metadata.columnNumber = 0;
    metadata.isMuted = false;
    ReportSmooshCompileError(cx, std::move(metadata),
                             JSMSG_SMOOSH_COMPILE_ERROR,
                             reinterpret_cast<const char*>(smoosh.error.data));
    return nullptr;
  }

  if (smoosh.unimplemented) {
    *unimplemented = true;
    return nullptr;
  }

  *unimplemented = false;

  RootedScriptSourceObject sso(cx,
                               frontend::CreateScriptSourceObject(cx, options));
  if (!sso) {
    return nullptr;
  }

  RootedObject proto(cx);
  if (!GetFunctionPrototype(cx, GeneratorKind::NotGenerator,
                            FunctionAsyncKind::SyncFunction, &proto)) {
    return nullptr;
  }

  SourceExtent extent(/* sourceStart = */ 0,
                      /* sourceEnd = */ length,
                      /* toStringStart = */ 0,
                      /* toStringEnd = */ length,
                      /* lineno = */ 1,
                      /* column = */ 0);
  RootedScript script(cx,
                      JSScript::Create(cx, cx->global(), options, sso, extent));

  SmooshScriptStencil stencil(smoosh);
  if (!JSScript::fullyInitFromStencil(cx, script, stencil)) {
    return nullptr;
  }

#if defined(DEBUG) || defined(JS_JITSPEW)
  Sprinter sprinter(cx);
  if (!sprinter.init()) {
    return nullptr;
  }
  if (!Disassemble(cx, script, true, &sprinter, DisassembleSkeptically::Yes)) {
    return nullptr;
  }
  printf("%s\n", sprinter.string());
  if (!Disassemble(cx, script, true, &sprinter, DisassembleSkeptically::No)) {
    return nullptr;
  }
  // (don't bother printing it)
#endif

  return script;
}

bool SmooshParseScript(JSContext* cx, const uint8_t* bytes, size_t length) {
  SmooshParseResult result = test_parse_script(bytes, length);
  AutoFreeSmooshParseResult afspr(&result);
  if (result.error.data) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             result.unimplemented ? JSMSG_SMOOSH_UNIMPLEMENTED
                                                  : JSMSG_SMOOSH_COMPILE_ERROR,
                             reinterpret_cast<const char*>(result.error.data));
    return false;
  }

  return true;
}

bool SmooshParseModule(JSContext* cx, const uint8_t* bytes, size_t length) {
  SmooshParseResult result = test_parse_module(bytes, length);
  AutoFreeSmooshParseResult afspr(&result);
  if (result.error.data) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             result.unimplemented ? JSMSG_SMOOSH_UNIMPLEMENTED
                                                  : JSMSG_SMOOSH_COMPILE_ERROR,
                             reinterpret_cast<const char*>(result.error.data));
    return false;
  }

  return true;
}

}  // namespace frontend

}  // namespace js
