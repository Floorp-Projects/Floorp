/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/Frontend2.h"

#include "mozilla/Maybe.h"                  // mozilla::Maybe
#include "mozilla/OperatorNewExtensions.h"  // mozilla::KnownNotNull
#include "mozilla/Span.h"                   // mozilla::{Span, MakeSpan}

#include <stddef.h>  // size_t
#include <stdint.h>  // uint8_t, uint32_t

#include "jsapi.h"

#include "frontend/AbstractScopePtr.h"  // ScopeIndex
#include "frontend/CompilationInfo.h"   // CompilationInfo
#include "frontend/smoosh_generated.h"  // CVec, SmooshResult, SmooshCompileOptions, free_smoosh, run_smoosh
#include "frontend/SourceNotes.h"  // jssrcnote
#include "frontend/Stencil.h"      // ScopeCreationData
#include "gc/Rooting.h"            // RootedScriptSourceObject
#include "js/HeapAPI.h"            // JS::GCCellPtr
#include "js/RootingAPI.h"         // JS::Handle, JS::Rooted
#include "js/TypeDecls.h"          // Rooted{Script,Value,String,Object}
#include "vm/JSAtom.h"             // AtomizeUTF8Chars
#include "vm/JSScript.h"           // JSScript
#include "vm/Scope.h"              // BindingName
#include "vm/ScopeKind.h"          // ScopeKind
#include "vm/SharedStencil.h"      // ImmutableScriptData, ScopeNote, TryNote

#include "vm/JSContext-inl.h"  // AutoKeepAtoms (used by BytecodeCompiler)

using mozilla::Utf8Unit;

using namespace js::gc;
using namespace js::frontend;
using namespace js;

namespace js {

namespace frontend {

class SmooshScriptStencil : public ScriptStencil {
  const SmooshResult& result_;
  CompilationInfo& compilationInfo_;
  JSAtom** allAtoms_ = nullptr;

 public:
  SmooshScriptStencil(JSContext* cx, const SmooshResult& result,
                      CompilationInfo& compilationInfo)
      : ScriptStencil(cx), result_(result), compilationInfo_(compilationInfo) {}

  MOZ_MUST_USE bool init(JSContext* cx) {
    lineno = result_.lineno;
    column = result_.column;

    natoms = result_.atoms.len;

    ngcthings = result_.gcthings.len;

    Vector<ScopeNote, 0, SystemAllocPolicy> scopeNotes;
    if (!scopeNotes.resize(result_.scope_notes.len)) {
      return false;
    }
    for (size_t i = 0; i < result_.scope_notes.len; i++) {
      SmooshScopeNote& scopeNote = result_.scope_notes.data[i];
      scopeNotes[i].index = scopeNote.index;
      scopeNotes[i].start = scopeNote.start;
      scopeNotes[i].length = scopeNote.length;
      scopeNotes[i].parent = scopeNote.parent;
    }

    uint32_t nfixed = result_.max_fixed_slots;
    uint64_t nslots64 =
        nfixed + static_cast<uint64_t>(result_.maximum_stack_depth);
    if (nslots64 > UINT32_MAX) {
      JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NEED_DIET,
                                js_script_str);
      return false;
    }
    immutableScriptData = ImmutableScriptData::new_(
        cx, result_.main_offset, nfixed, uint32_t(nslots64),
        result_.body_scope_index, result_.num_ic_entries, result_.num_type_sets,
        result_.is_function, /* funLength = */ 0,
        mozilla::MakeSpan(result_.bytecode.data, result_.bytecode.len),
        mozilla::Span<const jssrcnote>(), mozilla::Span<const uint32_t>(),
        scopeNotes, mozilla::Span<const TryNote>());
    if (!immutableScriptData) {
      return false;
    }

    strict = result_.strict;
    bindingsAccessedDynamically = result_.bindings_accessed_dynamically;
    hasCallSiteObj = result_.has_call_site_obj;
    isForEval = result_.is_for_eval;
    isModule = result_.is_module;

    hasNonSyntacticScope = result_.has_non_syntactic_scope;
    needsFunctionEnvironmentObjects =
        result_.needs_function_environment_objects;
    hasModuleGoal = result_.has_module_goal;

    if (!createAtoms(cx)) {
      return false;
    }

    if (!createScopeCreationData(cx)) {
      return false;
    }

    return true;
  }

  virtual bool finishGCThings(JSContext* cx,
                              mozilla::Span<JS::GCCellPtr> output) const {
    MOZ_ASSERT(output.Length() == ngcthings);

    for (size_t i = 0; i < ngcthings; i++) {
      SmooshGCThing& item = result_.gcthings.data[i];

      switch (item.kind) {
        case SmooshGCThingKind::ScopeIndex: {
          // compilationInfo_.scopeCreationData is filed by
          // `createScopeCreationData`, and i-th item corresponds to
          // the i-th scope.
          MutableHandle<ScopeCreationData> data =
              compilationInfo_.scopeCreationData[item.scope_index];
          Scope* scope = data.get().createScope(cx);
          if (!scope) {
            return false;
          }

          output[i] = JS::GCCellPtr(scope);

          break;
        }
      }
    }

    return true;
  }

  virtual void initAtomMap(GCPtrAtom* atoms) const {
    for (uint32_t i = 0; i < natoms; i++) {
      size_t index = result_.atoms.data[i];
      atoms[i] = allAtoms_[index];
    }
  }

 private:
  bool createAtoms(JSContext* cx) {
    size_t numAtoms = result_.all_atoms_len;

    auto& alloc = compilationInfo_.allocScope.alloc();

    allAtoms_ = alloc.newArray<JSAtom*>(numAtoms);
    if (!allAtoms_) {
      ReportOutOfMemory(cx);
      return false;
    }

    for (size_t i = 0; i < numAtoms; i++) {
      auto s = smoosh_get_atom_at(result_, i);
      auto len = smoosh_get_atom_len_at(result_, i);
      JSAtom* atom = AtomizeUTF8Chars(cx, s, len);
      if (!atom) {
        return false;
      }
      allAtoms_[i] = atom;
    }

    return true;
  }

 public:
  virtual void finishInnerFunctions() const {}

 private:
  // Fill `compilationInfo_.scopeCreationData` with scope data, where
  // i-th item corresponds to i-th scope.
  bool createScopeCreationData(JSContext* cx) {
    auto& alloc = compilationInfo_.allocScope.alloc();

    for (size_t i = 0; i < result_.scopes.len; i++) {
      SmooshScopeData& scopeData = result_.scopes.data[i];
      size_t numBindings = scopeData.bindings.len;
      ScopeIndex index;

      switch (scopeData.kind) {
        case SmooshScopeDataKind::Global: {
          JS::Rooted<GlobalScope::Data*> data(
              cx, NewEmptyGlobalScopeData(cx, alloc, numBindings));
          if (!data) {
            return false;
          }

          copyBindingNames(scopeData.bindings, data->trailingNames.start());

          data->letStart = scopeData.let_start;
          data->constStart = scopeData.const_start;
          data->length = numBindings;

          if (!ScopeCreationData::create(cx, compilationInfo_,
                                         ScopeKind::Global, data, &index)) {
            return false;
          }
          break;
        }
        case SmooshScopeDataKind::Lexical: {
          JS::Rooted<LexicalScope::Data*> data(
              cx, NewEmptyLexicalScopeData(cx, alloc, numBindings));
          if (!data) {
            return false;
          }

          copyBindingNames(scopeData.bindings, data->trailingNames.start());

          // NOTE: data->nextFrameSlot is set in ScopeCreationData::create.

          data->constStart = scopeData.const_start;
          data->length = numBindings;

          uint32_t firstFrameSlot = scopeData.first_frame_slot;
          ScopeIndex enclosingIndex(scopeData.enclosing);
          Rooted<AbstractScopePtr> enclosing(
              cx, AbstractScopePtr(compilationInfo_, enclosingIndex));
          if (!ScopeCreationData::create(cx, compilationInfo_,
                                         ScopeKind::Lexical, data,
                                         firstFrameSlot, enclosing, &index)) {
            return false;
          }
          break;
        }
      }

      // `finishGCThings` depends on this condition.
      MOZ_ASSERT(index == i);
    }

    return true;
  }

  void copyBindingNames(CVec<SmooshBindingName>& from, BindingName* to) {
    size_t numBindings = from.len;
    for (size_t i = 0; i < numBindings; i++) {
      SmooshBindingName& name = from.data[i];
      new (mozilla::KnownNotNull, &to[i])
          BindingName(allAtoms_[name.name], name.is_closed_over,
                      name.is_top_level_function);
    }
  }
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

void InitSmoosh() { init_smoosh(); }

void ReportSmooshCompileError(JSContext* cx, ErrorMetadata&& metadata,
                              int errorNumber, ...) {
  va_list args;
  va_start(args, errorNumber);
  ReportCompileErrorUTF8(cx, std::move(metadata), /* notes = */ nullptr,
                         errorNumber, &args);
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

  SmooshScriptStencil stencil(cx, smoosh, compilationInfo);
  if (!stencil.init(cx)) {
    return nullptr;
  }

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
