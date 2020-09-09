/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/Frontend2.h"

#include "mozilla/Maybe.h"                  // mozilla::Maybe
#include "mozilla/OperatorNewExtensions.h"  // mozilla::KnownNotNull
#include "mozilla/Range.h"                  // mozilla::Range
#include "mozilla/Span.h"                   // mozilla::{Span, Span}
#include "mozilla/Variant.h"                // mozilla::AsVariant

#include <stddef.h>  // size_t
#include <stdint.h>  // uint8_t, uint32_t

#include "jsapi.h"

#include "frontend/AbstractScopePtr.h"  // ScopeIndex
#include "frontend/BytecodeSection.h"   // EmitScriptThingsVector
#include "frontend/CompilationInfo.h"   // CompilationInfo
#include "frontend/Parser.h"  // NewEmptyLexicalScopeData, NewEmptyGlobalScopeData, NewEmptyVarScopeData, NewEmptyFunctionScopeData
#include "frontend/ParserAtom.h"        // ParserAtomsTable
#include "frontend/smoosh_generated.h"  // CVec, Smoosh*, smoosh_*
#include "frontend/SourceNotes.h"       // SrcNote
#include "frontend/Stencil.h"  // ScopeStencil, RegExpIndex, FunctionIndex, NullScriptThing
#include "frontend/TokenStream.h"  // TokenStreamAnyChars
#include "irregexp/RegExpAPI.h"    // irregexp::CheckPatternSyntax
#include "js/CharacterEncoding.h"  // JS::UTF8Chars, UTF8CharsToNewTwoByteCharsZ
#include "js/GCAPI.h"              // JS::AutoCheckCannotGC
#include "js/HeapAPI.h"            // JS::GCCellPtr
#include "js/RegExpFlags.h"        // JS::RegExpFlag, JS::RegExpFlags
#include "js/RootingAPI.h"         // JS::MutableHandle
#include "js/UniquePtr.h"          // js::UniquePtr
#include "js/Utility.h"            // JS::UniqueTwoByteChars, StringBufferArena
#include "vm/JSScript.h"           // JSScript
#include "vm/ScopeKind.h"          // ScopeKind
#include "vm/SharedStencil.h"  // ImmutableScriptData, ScopeNote, TryNote, GCThingIndex

using mozilla::Utf8Unit;

using namespace js::gc;
using namespace js::frontend;
using namespace js;

namespace js {

namespace frontend {

// Given the result of SmooshMonkey's parser, Convert the list of atoms into
// the list of ParserAtoms.
bool ConvertAtoms(JSContext* cx, const SmooshResult& result,
                  CompilationInfo& compilationInfo,
                  Vector<const ParserAtom*>& allAtoms) {
  size_t numAtoms = result.all_atoms_len;

  if (!allAtoms.reserve(numAtoms)) {
    return false;
  }

  for (size_t i = 0; i < numAtoms; i++) {
    auto s = reinterpret_cast<const mozilla::Utf8Unit*>(
        smoosh_get_atom_at(result, i));
    auto len = smoosh_get_atom_len_at(result, i);
    const ParserAtom* atom =
        compilationInfo.stencil.parserAtoms.internUtf8(cx, s, len)
            .unwrapOr(nullptr);
    if (!atom) {
      return false;
    }
    allAtoms.infallibleAppend(atom);
  }

  return true;
}

void CopyBindingNames(JSContext* cx, CVec<SmooshBindingName>& from,
                      Vector<const ParserAtom*>& allAtoms,
                      ParserBindingName* to) {
  // We're setting trailing array's content before setting its length.
  JS::AutoCheckCannotGC nogc(cx);

  size_t numBindings = from.len;
  for (size_t i = 0; i < numBindings; i++) {
    SmooshBindingName& name = from.data[i];
    new (mozilla::KnownNotNull, &to[i]) ParserBindingName(
        allAtoms[name.name], name.is_closed_over, name.is_top_level_function);
  }
}

void CopyBindingNames(JSContext* cx, CVec<COption<SmooshBindingName>>& from,
                      Vector<const ParserAtom*>& allAtoms,
                      ParserBindingName* to) {
  // We're setting trailing array's content before setting its length.
  JS::AutoCheckCannotGC nogc(cx);

  size_t numBindings = from.len;
  for (size_t i = 0; i < numBindings; i++) {
    COption<SmooshBindingName>& maybeName = from.data[i];
    if (maybeName.IsSome()) {
      SmooshBindingName& name = maybeName.AsSome();
      new (mozilla::KnownNotNull, &to[i]) ParserBindingName(
          allAtoms[name.name], name.is_closed_over, name.is_top_level_function);
    } else {
      new (mozilla::KnownNotNull, &to[i])
          ParserBindingName(nullptr, false, false);
    }
  }
}

// Given the result of SmooshMonkey's parser, convert a list of scope data
// into a list of ScopeStencil.
bool ConvertScopeStencil(JSContext* cx, const SmooshResult& result,
                         Vector<const ParserAtom*>& allAtoms,
                         CompilationInfo& compilationInfo, LifoAlloc& alloc) {
  for (size_t i = 0; i < result.scopes.len; i++) {
    SmooshScopeData& scopeData = result.scopes.data[i];
    ScopeIndex index;

    switch (scopeData.tag) {
      case SmooshScopeData::Tag::Global: {
        auto& global = scopeData.AsGlobal();

        size_t numBindings = global.bindings.len;
        ParserGlobalScopeData* data =
            NewEmptyGlobalScopeData(cx, alloc, numBindings);
        if (!data) {
          return false;
        }

        CopyBindingNames(cx, global.bindings, allAtoms,
                         data->trailingNames.start());

        data->letStart = global.let_start;
        data->constStart = global.const_start;
        data->length = numBindings;

        if (!ScopeStencil::createForGlobalScope(
                cx, compilationInfo.stencil, ScopeKind::Global, data, &index)) {
          return false;
        }
        break;
      }
      case SmooshScopeData::Tag::Var: {
        auto& var = scopeData.AsVar();

        size_t numBindings = var.bindings.len;

        ParserVarScopeData* data = NewEmptyVarScopeData(cx, alloc, numBindings);
        ;
        if (!data) {
          return false;
        }

        CopyBindingNames(cx, var.bindings, allAtoms,
                         data->trailingNames.start());

        // NOTE: data->nextFrameSlot is set in ScopeStencil::createForVarScope.

        data->length = numBindings;

        uint32_t firstFrameSlot = var.first_frame_slot;
        ScopeIndex enclosingIndex(var.enclosing);
        if (!ScopeStencil::createForVarScope(
                cx, compilationInfo.stencil, ScopeKind::FunctionBodyVar, data,
                firstFrameSlot, var.function_has_extensible_scope,
                mozilla::Some(enclosingIndex), &index)) {
          return false;
        }
        break;
      }
      case SmooshScopeData::Tag::Lexical: {
        auto& lexical = scopeData.AsLexical();

        size_t numBindings = lexical.bindings.len;
        ParserLexicalScopeData* data =
            NewEmptyLexicalScopeData(cx, alloc, numBindings);
        if (!data) {
          return false;
        }

        CopyBindingNames(cx, lexical.bindings, allAtoms,
                         data->trailingNames.start());

        // NOTE: data->nextFrameSlot is set in
        // ScopeStencil::createForLexicalScope.

        data->constStart = lexical.const_start;
        data->length = numBindings;

        uint32_t firstFrameSlot = lexical.first_frame_slot;
        ScopeIndex enclosingIndex(lexical.enclosing);
        if (!ScopeStencil::createForLexicalScope(
                cx, compilationInfo.stencil, ScopeKind::Lexical, data,
                firstFrameSlot, mozilla::Some(enclosingIndex), &index)) {
          return false;
        }
        break;
      }
      case SmooshScopeData::Tag::Function: {
        auto& function = scopeData.AsFunction();

        size_t numBindings = function.bindings.len;
        ParserFunctionScopeData* data =
            NewEmptyFunctionScopeData(cx, alloc, numBindings);
        if (!data) {
          return false;
        }

        CopyBindingNames(cx, function.bindings, allAtoms,
                         data->trailingNames.start());

        // NOTE: data->nextFrameSlot is set in
        // ScopeStencil::createForFunctionScope.

        data->hasParameterExprs = function.has_parameter_exprs;
        data->nonPositionalFormalStart = function.non_positional_formal_start;
        data->varStart = function.var_start;
        data->length = numBindings;

        bool hasParameterExprs = function.has_parameter_exprs;
        bool needsEnvironment = function.non_positional_formal_start;
        FunctionIndex functionIndex =
            FunctionIndex(function.function_index + 1);
        bool isArrow = function.is_arrow;

        ScopeIndex enclosingIndex(function.enclosing);
        if (!ScopeStencil::createForFunctionScope(
                cx, compilationInfo.stencil, data, hasParameterExprs,
                needsEnvironment, functionIndex, isArrow,
                mozilla::Some(enclosingIndex), &index)) {
          return false;
        }
        break;
      }
    }

    // `ConvertGCThings` depends on this condition.
    MOZ_ASSERT(index == i);
  }

  return true;
}

// Given the result of SmooshMonkey's parser, convert a list of RegExp data
// into a list of RegExpStencil.
bool ConvertRegExpData(JSContext* cx, const SmooshResult& result,
                       CompilationInfo& compilationInfo) {
  for (size_t i = 0; i < result.regexps.len; i++) {
    SmooshRegExpItem& item = result.regexps.data[i];
    auto s = smoosh_get_slice_at(result, item.pattern);
    auto len = smoosh_get_slice_len_at(result, item.pattern);

    JS::RegExpFlags::Flag flags = JS::RegExpFlag::NoFlags;
    if (item.global) {
      flags |= JS::RegExpFlag::Global;
    }
    if (item.ignore_case) {
      flags |= JS::RegExpFlag::IgnoreCase;
    }
    if (item.multi_line) {
      flags |= JS::RegExpFlag::Multiline;
    }
    if (item.dot_all) {
      flags |= JS::RegExpFlag::DotAll;
    }
    if (item.sticky) {
      flags |= JS::RegExpFlag::Sticky;
    }
    if (item.unicode) {
      flags |= JS::RegExpFlag::Unicode;
    }

    // FIXME: This check should be done at parse time.
    size_t length;
    JS::UniqueTwoByteChars pattern(
        UTF8CharsToNewTwoByteCharsZ(cx, JS::UTF8Chars(s, len), &length,
                                    StringBufferArena)
            .get());
    if (!pattern) {
      return false;
    }

    mozilla::Range<const char16_t> range(pattern.get(), length);

    TokenStreamAnyChars ts(cx, compilationInfo.input.options,
                           /* smg = */ nullptr);

    // See Parser<FullParseHandler, Unit>::newRegExp.

    LifoAllocScope allocScope(&cx->tempLifoAlloc());
    if (!irregexp::CheckPatternSyntax(cx, ts, range, flags)) {
      return false;
    }

    RegExpIndex index(compilationInfo.stencil.regExpData.length());
    if (!compilationInfo.stencil.regExpData.emplaceBack()) {
      js::ReportOutOfMemory(cx);
      return false;
    }

    if (!compilationInfo.stencil.regExpData[index].init(
            cx, range, JS::RegExpFlags(flags))) {
      return false;
    }

    // `ConvertGCThings` depends on this condition.
    MOZ_ASSERT(index.index == i);
  }

  return true;
}

// Convert SmooshImmutableScriptData into ImmutableScriptData.
UniquePtr<ImmutableScriptData> ConvertImmutableScriptData(
    JSContext* cx, const SmooshImmutableScriptData& smooshScriptData,
    bool isFunction) {
  Vector<ScopeNote, 0, SystemAllocPolicy> scopeNotes;
  if (!scopeNotes.resize(smooshScriptData.scope_notes.len)) {
    return nullptr;
  }
  for (size_t i = 0; i < smooshScriptData.scope_notes.len; i++) {
    SmooshScopeNote& scopeNote = smooshScriptData.scope_notes.data[i];
    scopeNotes[i].index = GCThingIndex(scopeNote.index);
    scopeNotes[i].start = scopeNote.start;
    scopeNotes[i].length = scopeNote.length;
    scopeNotes[i].parent = scopeNote.parent;
  }

  return ImmutableScriptData::new_(
      cx, smooshScriptData.main_offset, smooshScriptData.nfixed,
      smooshScriptData.nslots, GCThingIndex(smooshScriptData.body_scope_index),
      smooshScriptData.num_ic_entries, smooshScriptData.num_bytecode_type_sets,
      isFunction, smooshScriptData.fun_length,
      mozilla::Span(smooshScriptData.bytecode.data,
                    smooshScriptData.bytecode.len),
      mozilla::Span<const SrcNote>(), mozilla::Span<const uint32_t>(),
      scopeNotes, mozilla::Span<const TryNote>());
}

// Given the result of SmooshMonkey's parser, convert a list of GC things
// used by a script into ScriptThingsVector.
bool ConvertGCThings(JSContext* cx, const SmooshResult& result,
                     const SmooshScriptStencil& smooshScript,
                     Vector<const ParserAtom*>& allAtoms,
                     ScriptStencil& script) {
  auto& gcThings = script.gcThings;

  size_t ngcthings = smooshScript.gcthings.len;
  if (!gcThings.reserve(ngcthings)) {
    js::ReportOutOfMemory(cx);
    return false;
  }

  for (size_t i = 0; i < ngcthings; i++) {
    SmooshGCThing& item = smooshScript.gcthings.data[i];

    switch (item.tag) {
      case SmooshGCThing::Tag::Null: {
        gcThings.infallibleAppend(NullScriptThing());
        break;
      }
      case SmooshGCThing::Tag::Atom: {
        gcThings.infallibleAppend(mozilla::AsVariant(allAtoms[item.AsAtom()]));
        break;
      }
      case SmooshGCThing::Tag::Function: {
        gcThings.infallibleAppend(
            mozilla::AsVariant(FunctionIndex(item.AsFunction() + 1)));
        break;
      }
      case SmooshGCThing::Tag::Scope: {
        gcThings.infallibleAppend(
            mozilla::AsVariant(ScopeIndex(item.AsScope())));
        break;
      }
      case SmooshGCThing::Tag::RegExp: {
        gcThings.infallibleAppend(
            mozilla::AsVariant(RegExpIndex(item.AsRegExp())));
        break;
      }
    }
  }

  return true;
}

// Given the result of SmooshMonkey's parser, convert a specific script
// or function to a StencilScript, given a fixed set of source atoms.
//
// The StencilScript would then be in charge of handling the lifetime and
// (until GC things gets removed from stencil) tracing API of the GC.
bool ConvertScriptStencil(JSContext* cx, const SmooshResult& result,
                          const SmooshScriptStencil& smooshScript,
                          Vector<const ParserAtom*>& allAtoms,
                          CompilationInfo& compilationInfo,
                          ScriptStencil& script) {
  using ImmutableFlags = js::ImmutableScriptFlagsEnum;

  const JS::ReadOnlyCompileOptions& options = compilationInfo.input.options;

  script.immutableFlags = smooshScript.immutable_flags;

  // FIXME: The following flags should be set in jsparagus.
  script.immutableFlags.setFlag(ImmutableFlags::SelfHosted,
                                options.selfHostingMode);
  script.immutableFlags.setFlag(ImmutableFlags::ForceStrict,
                                options.forceStrictMode());
  script.immutableFlags.setFlag(ImmutableFlags::HasNonSyntacticScope,
                                options.nonSyntacticScope);

  if (&smooshScript == &result.top_level_script) {
    script.immutableFlags.setFlag(ImmutableFlags::TreatAsRunOnce,
                                  options.isRunOnce);
    script.immutableFlags.setFlag(ImmutableFlags::NoScriptRval,
                                  options.noScriptRval);
  }

  bool isFunction = script.immutableFlags.hasFlag(ImmutableFlags::IsFunction);

  if (smooshScript.immutable_script_data.IsSome()) {
    auto index = smooshScript.immutable_script_data.AsSome();
    auto immutableScriptData = ConvertImmutableScriptData(
        cx, result.script_data_list.data[index], isFunction);
    if (!immutableScriptData) {
      return false;
    }
    script.immutableScriptData = std::move(immutableScriptData);
  }

  script.extent.sourceStart = smooshScript.extent.source_start;
  script.extent.sourceEnd = smooshScript.extent.source_end;
  script.extent.toStringStart = smooshScript.extent.to_string_start;
  script.extent.toStringEnd = smooshScript.extent.to_string_end;
  script.extent.lineno = smooshScript.extent.lineno;
  script.extent.column = smooshScript.extent.column;

  if (isFunction) {
    if (smooshScript.fun_name.IsSome()) {
      script.functionAtom = allAtoms[smooshScript.fun_name.AsSome()];
    }
    script.functionFlags = FunctionFlags(smooshScript.fun_flags);
    script.nargs = smooshScript.fun_nargs;
    if (smooshScript.lazy_function_enclosing_scope_index.IsSome()) {
      script.lazyFunctionEnclosingScopeIndex_ = mozilla::Some(ScopeIndex(
          smooshScript.lazy_function_enclosing_scope_index.AsSome()));
    }
    script.isStandaloneFunction = smooshScript.is_standalone_function;
    script.wasFunctionEmitted = smooshScript.was_function_emitted;
    script.isSingletonFunction = smooshScript.is_singleton_function;
  }

  if (!ConvertGCThings(cx, result, smooshScript, allAtoms, script)) {
    return false;
  }

  return true;
}

// Free given SmooshResult on leaving scope.
class AutoFreeSmooshResult {
  SmooshResult* result_;

 public:
  AutoFreeSmooshResult() = delete;

  explicit AutoFreeSmooshResult(SmooshResult* result) : result_(result) {}
  ~AutoFreeSmooshResult() {
    if (result_) {
      smoosh_free(*result_);
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
      smoosh_free_parse_result(*result_);
    }
  }
};

void InitSmoosh() { smoosh_init(); }

void ReportSmooshCompileError(JSContext* cx, ErrorMetadata&& metadata,
                              int errorNumber, ...) {
  va_list args;
  va_start(args, errorNumber);
  ReportCompileErrorUTF8(cx, std::move(metadata), /* notes = */ nullptr,
                         errorNumber, &args);
  va_end(args);
}

/* static */
bool Smoosh::compileGlobalScriptToStencil(JSContext* cx,
                                          CompilationInfo& compilationInfo,
                                          JS::SourceText<Utf8Unit>& srcBuf,
                                          bool* unimplemented) {
  // FIXME: check info members and return with *unimplemented = true
  //        if any field doesn't match to smoosh_run.

  auto bytes = reinterpret_cast<const uint8_t*>(srcBuf.get());
  size_t length = srcBuf.length();

  const auto& options = compilationInfo.input.options;
  SmooshCompileOptions compileOptions;
  compileOptions.no_script_rval = options.noScriptRval;

  SmooshResult result = smoosh_run(bytes, length, &compileOptions);
  AutoFreeSmooshResult afsr(&result);

  if (result.error.data) {
    *unimplemented = false;
    ErrorMetadata metadata;
    metadata.filename = "<unknown>";
    metadata.lineNumber = 1;
    metadata.columnNumber = 0;
    metadata.isMuted = false;
    ReportSmooshCompileError(cx, std::move(metadata),
                             JSMSG_SMOOSH_COMPILE_ERROR,
                             reinterpret_cast<const char*>(result.error.data));
    return false;
  }

  if (result.unimplemented) {
    *unimplemented = true;
    return false;
  }

  *unimplemented = false;

  Vector<const ParserAtom*> allAtoms(cx);
  if (!ConvertAtoms(cx, result, compilationInfo, allAtoms)) {
    return false;
  }

  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  auto& alloc = allocScope.alloc();
  if (!ConvertScopeStencil(cx, result, allAtoms, compilationInfo, alloc)) {
    return false;
  }

  if (!ConvertRegExpData(cx, result, compilationInfo)) {
    return false;
  }

  if (!compilationInfo.stencil.scriptData.reserve(result.functions.len + 1)) {
    js::ReportOutOfMemory(cx);
    return false;
  }

  compilationInfo.stencil.scriptData.infallibleEmplaceBack();

  if (!ConvertScriptStencil(
          cx, result, result.top_level_script, allAtoms, compilationInfo,
          compilationInfo.stencil.scriptData[CompilationInfo::TopLevelIndex])) {
    return false;
  }

  for (size_t i = 0; i < result.functions.len; i++) {
    compilationInfo.stencil.scriptData.infallibleEmplaceBack();

    if (!ConvertScriptStencil(cx, result, result.functions.data[i], allAtoms,
                              compilationInfo,
                              compilationInfo.stencil.scriptData[i + 1])) {
      return false;
    }
  }

  return true;
}

/* static */
UniquePtr<CompilationInfo> Smoosh::compileGlobalScriptToStencil(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    JS::SourceText<Utf8Unit>& srcBuf, bool* unimplemented) {
  Rooted<UniquePtr<frontend::CompilationInfo>> compilationInfo(
      cx, js_new<frontend::CompilationInfo>(cx, options));
  if (!compilationInfo) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  if (!compilationInfo.get()->input.initForGlobal(cx)) {
    return nullptr;
  }

  if (!compileGlobalScriptToStencil(cx, *compilationInfo.get().get(), srcBuf,
                                    unimplemented)) {
    return nullptr;
  }

  return std::move(compilationInfo.get());
}

/* static */
bool Smoosh::compileGlobalScript(JSContext* cx,
                                 CompilationInfo& compilationInfo,
                                 JS::SourceText<Utf8Unit>& srcBuf,
                                 CompilationGCOutput& gcOutput,
                                 bool* unimplemented) {
  if (!compileGlobalScriptToStencil(cx, compilationInfo, srcBuf,
                                    unimplemented)) {
    return false;
  }

  if (!compilationInfo.instantiateStencils(cx, gcOutput)) {
    return false;
  }

#if defined(DEBUG) || defined(JS_JITSPEW)
  Sprinter sprinter(cx);
  if (!sprinter.init()) {
    return false;
  }
  if (!Disassemble(cx, gcOutput.script, true, &sprinter,
                   DisassembleSkeptically::Yes)) {
    return false;
  }
  printf("%s\n", sprinter.string());
  if (!Disassemble(cx, gcOutput.script, true, &sprinter,
                   DisassembleSkeptically::No)) {
    return false;
  }
  // (don't bother printing it)
#endif

  return true;
}

bool SmooshParseScript(JSContext* cx, const uint8_t* bytes, size_t length) {
  SmooshParseResult result = smoosh_test_parse_script(bytes, length);
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
  SmooshParseResult result = smoosh_test_parse_module(bytes, length);
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
