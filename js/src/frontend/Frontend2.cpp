/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/Frontend2.h"

#include "mozilla/Maybe.h"                  // mozilla::Maybe
#include "mozilla/OperatorNewExtensions.h"  // mozilla::KnownNotNull
#include "mozilla/Range.h"                  // mozilla::Range
#include "mozilla/Span.h"                   // mozilla::{Span, MakeSpan}
#include "mozilla/Variant.h"                // mozilla::AsVariant

#include <stddef.h>  // size_t
#include <stdint.h>  // uint8_t, uint32_t

#include "jsapi.h"

#include "frontend/AbstractScopePtr.h"  // ScopeIndex
#include "frontend/BytecodeSection.h"   // EmitScriptThingsVector
#include "frontend/CompilationInfo.h"   // CompilationInfo
#include "frontend/Parser.h"  // NewEmptyLexicalScopeData, NewEmptyGlobalScopeData
#include "frontend/smoosh_generated.h"  // CVec, Smoosh*, smoosh_*
#include "frontend/SourceNotes.h"       // SrcNote
#include "frontend/Stencil.h"           // ScopeCreationData, RegExpIndex
#include "frontend/TokenStream.h"       // TokenStreamAnyChars
#include "gc/Rooting.h"                 // RootedScriptSourceObject
#include "irregexp/RegExpAPI.h"         // irregexp::CheckPatternSyntax
#include "js/CharacterEncoding.h"  // JS::UTF8Chars, UTF8CharsToNewTwoByteCharsZ
#include "js/GCAPI.h"              // JS::AutoCheckCannotGC
#include "js/GCVector.h"           // JS::RootedVector
#include "js/HeapAPI.h"            // JS::GCCellPtr
#include "js/RegExpFlags.h"        // JS::RegExpFlag, JS::RegExpFlags
#include "js/RootingAPI.h"         // JS::Handle, JS::Rooted
#include "js/TypeDecls.h"  // Rooted{Script,Value,String,Object}, JS*:HandleVector, JS::MutableHandleVector
#include "js/UniquePtr.h"      // js::UniquePtr
#include "js/Utility.h"        // JS::UniqueTwoByteChars, StringBufferArena
#include "vm/JSAtom.h"         // AtomizeUTF8Chars
#include "vm/JSScript.h"       // JSScript
#include "vm/Scope.h"          // BindingName
#include "vm/ScopeKind.h"      // ScopeKind
#include "vm/SharedStencil.h"  // ImmutableScriptData, ScopeNote, TryNote
#include "vm/StringType.h"     // JSAtom

#include "vm/JSContext-inl.h"  // AutoKeepAtoms (used by BytecodeCompiler)

using mozilla::Utf8Unit;

using namespace js::gc;
using namespace js::frontend;
using namespace js;

namespace js {

namespace frontend {

// Given the result of SmooshMonkey's parser, Convert the list of atoms into
// the list of JSAtoms.
bool ConvertAtoms(JSContext* cx, const SmooshResult& result,
                  CompilationInfo& compilationInfo,
                  JS::MutableHandleVector<JSAtom*> allAtoms) {
  size_t numAtoms = result.all_atoms_len;

  if (!allAtoms.reserve(numAtoms)) {
    return false;
  }

  for (size_t i = 0; i < numAtoms; i++) {
    auto s = smoosh_get_atom_at(result, i);
    auto len = smoosh_get_atom_len_at(result, i);
    JSAtom* atom = AtomizeUTF8Chars(cx, s, len);
    if (!atom) {
      return false;
    }
    allAtoms.infallibleAppend(atom);
  }

  return true;
}

void CopyBindingNames(JSContext* cx, CVec<SmooshBindingName>& from,
                      JS::HandleVector<JSAtom*> allAtoms, BindingName* to) {
  // We're setting trailing array's content before setting its length.
  JS::AutoCheckCannotGC nogc(cx);

  size_t numBindings = from.len;
  for (size_t i = 0; i < numBindings; i++) {
    SmooshBindingName& name = from.data[i];
    new (mozilla::KnownNotNull, &to[i]) BindingName(
        allAtoms[name.name], name.is_closed_over, name.is_top_level_function);
  }
}

// Given the result of SmooshMonkey's parser, convert a list of scope data
// into a list of ScopeCreationData.
bool ConvertScopeCreationData(JSContext* cx, const SmooshResult& result,
                              JS::HandleVector<JSAtom*> allAtoms,
                              CompilationInfo& compilationInfo) {
  auto& alloc = compilationInfo.allocScope.alloc();

  for (size_t i = 0; i < result.scopes.len; i++) {
    SmooshScopeData& scopeData = result.scopes.data[i];
    ScopeIndex index;

    switch (scopeData.tag) {
      case SmooshScopeData::Tag::Global: {
        auto& global = scopeData.AsGlobal();

        size_t numBindings = global.bindings.len;
        JS::Rooted<GlobalScope::Data*> data(
            cx, NewEmptyGlobalScopeData(cx, alloc, numBindings));
        if (!data) {
          return false;
        }

        CopyBindingNames(cx, global.bindings, allAtoms,
                         data->trailingNames.start());

        data->letStart = global.let_start;
        data->constStart = global.const_start;
        data->length = numBindings;

        if (!ScopeCreationData::create(cx, compilationInfo, ScopeKind::Global,
                                       data, &index)) {
          return false;
        }
        break;
      }
      case SmooshScopeData::Tag::Lexical: {
        auto& lexical = scopeData.AsLexical();

        size_t numBindings = lexical.bindings.len;
        JS::Rooted<LexicalScope::Data*> data(
            cx, NewEmptyLexicalScopeData(cx, alloc, numBindings));
        if (!data) {
          return false;
        }

        CopyBindingNames(cx, lexical.bindings, allAtoms,
                         data->trailingNames.start());

        // NOTE: data->nextFrameSlot is set in ScopeCreationData::create.

        data->constStart = lexical.const_start;
        data->length = numBindings;

        uint32_t firstFrameSlot = lexical.first_frame_slot;
        ScopeIndex enclosingIndex(lexical.enclosing);
        Rooted<AbstractScopePtr> enclosing(
            cx, AbstractScopePtr(compilationInfo, enclosingIndex));
        if (!ScopeCreationData::create(cx, compilationInfo, ScopeKind::Lexical,
                                       data, firstFrameSlot, enclosing,
                                       &index)) {
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
// into a list of RegExpCreationData.
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

    TokenStreamAnyChars ts(cx, compilationInfo.options, /* smg = */ nullptr);

    // See Parser<FullParseHandler, Unit>::newRegExp.

    LifoAllocScope allocScope(&cx->tempLifoAlloc());
    if (!irregexp::CheckPatternSyntax(cx, ts, range, flags)) {
      return false;
    }

    RegExpIndex index(compilationInfo.regExpData.length());
    if (!compilationInfo.regExpData.emplaceBack()) {
      return false;
    }

    if (!compilationInfo.regExpData[index].init(cx, range,
                                                JS::RegExpFlags(flags))) {
      return false;
    }

    // `ConvertGCThings` depends on this condition.
    MOZ_ASSERT(index.index == i);
  }

  return true;
}

// Convert SmooshImmutableScriptData into ImmutableScriptData.
UniquePtr<ImmutableScriptData> ConvertImmutableScriptData(
    JSContext* cx, const SmooshImmutableScriptData& smooshScriptData) {
  Vector<ScopeNote, 0, SystemAllocPolicy> scopeNotes;
  if (!scopeNotes.resize(smooshScriptData.scope_notes.len)) {
    return nullptr;
  }
  for (size_t i = 0; i < smooshScriptData.scope_notes.len; i++) {
    SmooshScopeNote& scopeNote = smooshScriptData.scope_notes.data[i];
    scopeNotes[i].index = scopeNote.index;
    scopeNotes[i].start = scopeNote.start;
    scopeNotes[i].length = scopeNote.length;
    scopeNotes[i].parent = scopeNote.parent;
  }

  // FIXME: Support functions.
  bool isFunction = false;
  int funLength = 0;

  return ImmutableScriptData::new_(
      cx, smooshScriptData.main_offset, smooshScriptData.nfixed,
      smooshScriptData.nslots, smooshScriptData.body_scope_index,
      smooshScriptData.num_ic_entries, smooshScriptData.num_bytecode_type_sets,
      isFunction, funLength,
      mozilla::MakeSpan(smooshScriptData.bytecode.data,
                        smooshScriptData.bytecode.len),
      mozilla::Span<const SrcNote>(), mozilla::Span<const uint32_t>(),
      scopeNotes, mozilla::Span<const TryNote>());
}

// Given the result of SmooshMonkey's parser, convert a list of GC things
// used by a script into ScriptThingsVector.
bool ConvertGCThings(JSContext* cx, const SmooshResult& result,
                     const SmooshScriptStencil& smooshStencil,
                     JS::HandleVector<JSAtom*> allAtoms,
                     MutableHandle<ScriptStencil> stencil) {
  auto& gcThings = stencil.get().gcThings;

  size_t ngcthings = smooshStencil.gcthings.len;
  if (!gcThings.reserve(ngcthings)) {
    return false;
  }

  for (size_t i = 0; i < ngcthings; i++) {
    SmooshGCThing& item = smooshStencil.gcthings.data[i];

    switch (item.tag) {
      case SmooshGCThing::Tag::Atom: {
        gcThings.infallibleAppend(
            mozilla::AsVariant(allAtoms[item.AsAtom()].get()));
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
                          const SmooshScriptStencil& smooshStencil,
                          // FIXME: Store length in SmooshScriptStencil.
                          size_t length, JS::HandleVector<JSAtom*> allAtoms,
                          CompilationInfo& compilationInfo,
                          MutableHandle<ScriptStencil> stencil) {
  auto index = smooshStencil.immutable_script_data.AsSome();
  auto immutableScriptData =
      ConvertImmutableScriptData(cx, result.script_data_list.data[index]);
  if (!immutableScriptData) {
    return false;
  }

  // FIXME: Support extent.
  stencil.get().extent = SourceExtent::makeGlobalExtent(length);

  using ImmutableFlags = js::ImmutableScriptFlagsEnum;

  const JS::ReadOnlyCompileOptions& options = compilationInfo.options;

  stencil.get().immutableFlags = result.top_level_script.immutable_flags;

  // FIXME: The following flags should be set in jsparagus.
  stencil.get().immutableFlags.setFlag(ImmutableFlags::SelfHosted,
                                       options.selfHostingMode);
  stencil.get().immutableFlags.setFlag(ImmutableFlags::ForceStrict,
                                       options.forceStrictMode());
  stencil.get().immutableFlags.setFlag(ImmutableFlags::NoScriptRval,
                                       options.noScriptRval);
  stencil.get().immutableFlags.setFlag(ImmutableFlags::TreatAsRunOnce,
                                       options.isRunOnce);

  stencil.get().immutableScriptData = std::move(immutableScriptData);

  if (!ConvertGCThings(cx, result, smooshStencil, allAtoms, stencil)) {
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
JSScript* Smoosh::compileGlobalScript(CompilationInfo& compilationInfo,
                                      JS::SourceText<Utf8Unit>& srcBuf,
                                      bool* unimplemented) {
  // FIXME: check info members and return with *unimplemented = true
  //        if any field doesn't match to smoosh_run.

  auto bytes = reinterpret_cast<const uint8_t*>(srcBuf.get());
  size_t length = srcBuf.length();

  JSContext* cx = compilationInfo.cx;

  const auto& options = compilationInfo.options;
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
    return nullptr;
  }

  if (result.unimplemented) {
    *unimplemented = true;
    return nullptr;
  }

  *unimplemented = false;

  JS::RootedVector<JSAtom*> allAtoms(cx);
  if (!ConvertAtoms(cx, result, compilationInfo, &allAtoms)) {
    return nullptr;
  }

  if (!ConvertScopeCreationData(cx, result, allAtoms, compilationInfo)) {
    return nullptr;
  }

  if (!ConvertRegExpData(cx, result, compilationInfo)) {
    return nullptr;
  }

  // FIXME: Support functions.
  if (!ConvertScriptStencil(cx, result, result.top_level_script, length,
                            allAtoms, compilationInfo,
                            &compilationInfo.topLevel)) {
    return nullptr;
  }

  if (!compilationInfo.instantiateStencils()) {
    return nullptr;
  }

#if defined(DEBUG) || defined(JS_JITSPEW)
  Sprinter sprinter(cx);
  if (!sprinter.init()) {
    return nullptr;
  }
  if (!Disassemble(cx, compilationInfo.script, true, &sprinter,
                   DisassembleSkeptically::Yes)) {
    return nullptr;
  }
  printf("%s\n", sprinter.string());
  if (!Disassemble(cx, compilationInfo.script, true, &sprinter,
                   DisassembleSkeptically::No)) {
    return nullptr;
  }
  // (don't bother printing it)
#endif

  return compilationInfo.script;
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
