/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/StencilXdr.h"  // StencilXDR

#include "mozilla/Variant.h"  // mozilla::AsVariant

#include <stddef.h>     // size_t
#include <stdint.h>     // uint8_t, uint16_t, uint32_t
#include <type_traits>  // std::has_unique_object_representations
#include <utility>      // std::forward

#include "vm/JSScript.h"      // js::CheckCompileOptionsMatch
#include "vm/StencilEnums.h"  // js::ImmutableScriptFlagsEnum

using namespace js;
using namespace js::frontend;

template <XDRMode mode>
/* static */ XDRResult StencilXDR::Script(XDRState<mode>* xdr,
                                          ScriptStencil& stencil) {
  enum class XdrFlags : uint8_t {
    HasMemberInitializers = 0,
    HasFunctionAtom,
    HasScopeIndex,
    WasFunctionEmitted,
    AllowRelazify,
    HasSharedData,
  };

  struct XdrFields {
    uint32_t immutableFlags;
    uint32_t numMemberInitializers;
    uint32_t numGcThings;
    uint16_t functionFlags;
    uint16_t nargs;
    uint32_t scopeIndex;
  };

  uint8_t xdrFlags = 0;
  XdrFields xdrFields = {};

  if (mode == XDR_ENCODE) {
    xdrFields.immutableFlags = stencil.immutableFlags;

    if (stencil.memberInitializers.isSome()) {
      xdrFlags |= 1 << uint8_t(XdrFlags::HasMemberInitializers);
    }
    xdrFields.numMemberInitializers =
        stencil.memberInitializers
            .map([](auto i) { return i.numMemberInitializers; })
            .valueOr(0);

    xdrFields.numGcThings = stencil.gcThings.size();

    if (stencil.functionAtom) {
      xdrFlags |= 1 << uint8_t(XdrFlags::HasFunctionAtom);
    }

    xdrFields.functionFlags = stencil.functionFlags.toRaw();
    xdrFields.nargs = stencil.nargs;

    if (stencil.lazyFunctionEnclosingScopeIndex_.isSome()) {
      xdrFlags |= 1 << uint8_t(XdrFlags::HasScopeIndex);
    }
    xdrFields.scopeIndex =
        stencil.lazyFunctionEnclosingScopeIndex_.valueOr(ScopeIndex());

    if (stencil.wasFunctionEmitted) {
      xdrFlags |= 1 << uint8_t(XdrFlags::WasFunctionEmitted);
    }
    if (stencil.allowRelazify) {
      xdrFlags |= 1 << uint8_t(XdrFlags::AllowRelazify);
    }
    if (stencil.hasSharedData) {
      xdrFlags |= 1 << uint8_t(XdrFlags::HasSharedData);
    }
  }

#ifdef __cpp_lib_has_unique_object_representations
  // We check endianess before decoding so if structures are fully packed, we
  // may transcode them directly as raw bytes.
  static_assert(std::has_unique_object_representations<XdrFields>(),
                "XdrFields structure must be fully packed");
  static_assert(std::has_unique_object_representations<SourceExtent>(),
                "XdrFields structure must be fully packed");
#endif

  MOZ_TRY(xdr->codeUint8(&xdrFlags));
  MOZ_TRY(xdr->codeBytes(&xdrFields, sizeof(XdrFields)));
  MOZ_TRY(xdr->codeBytes(&stencil.extent, sizeof(SourceExtent)));

  if (mode == XDR_DECODE) {
    MOZ_ASSERT(xdr->hasOptions());

    if (!(xdrFields.immutableFlags &
          uint32_t(ImmutableScriptFlagsEnum::IsFunction))) {
      MOZ_ASSERT(!xdr->isMultiDecode());
      if (!js::CheckCompileOptionsMatch(
              xdr->options(), ImmutableScriptFlags(xdrFields.immutableFlags),
              xdr->isMultiDecode())) {
        return xdr->fail(JS::TranscodeResult_Failure_WrongCompileOption);
      }
    }

    stencil.immutableFlags = xdrFields.immutableFlags;

    if (xdrFlags & (1 << uint8_t(XdrFlags::HasMemberInitializers))) {
      stencil.memberInitializers.emplace(xdrFields.numMemberInitializers);
    }

    MOZ_ASSERT(stencil.gcThings.empty());
    if (xdrFields.numGcThings > 0) {
      // Allocated TaggedScriptThingIndex array and initialize to safe value.
      mozilla::Span<TaggedScriptThingIndex> stencilThings =
          NewScriptThingSpanUninitialized(xdr->cx(), xdr->stencilAlloc(),
                                          xdrFields.numGcThings);
      if (stencilThings.empty()) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
      stencil.gcThings = stencilThings;
    }

    stencil.functionFlags = FunctionFlags(xdrFields.functionFlags);
    stencil.nargs = xdrFields.nargs;

    if (xdrFlags & (1 << uint8_t(XdrFlags::HasScopeIndex))) {
      stencil.lazyFunctionEnclosingScopeIndex_.emplace(xdrFields.scopeIndex);
    }

    if (xdrFlags & (1 << uint8_t(XdrFlags::WasFunctionEmitted))) {
      stencil.wasFunctionEmitted = true;
    }
    if (xdrFlags & (1 << uint8_t(XdrFlags::AllowRelazify))) {
      stencil.allowRelazify = true;
    }
    if (xdrFlags & (1 << uint8_t(XdrFlags::HasSharedData))) {
      stencil.hasSharedData = true;
    }
  }

#ifdef __cpp_lib_has_unique_object_representations
  // We check endianess before decoding so if structures are fully packed, we
  // may transcode them directly as raw bytes.
  static_assert(
      std::has_unique_object_representations<TaggedScriptThingIndex>(),
      "TaggedScriptThingIndex structure must be fully packed");
#endif

  MOZ_TRY(xdr->codeBytes(
      const_cast<TaggedScriptThingIndex*>(stencil.gcThings.data()),
      sizeof(TaggedScriptThingIndex) * xdrFields.numGcThings));

  if (xdrFlags & (1 << uint8_t(XdrFlags::HasFunctionAtom))) {
    MOZ_TRY(XDRTaggedParserAtomIndex(xdr, &stencil.functionAtom));
  }

  return Ok();
};

template <typename NameType>
struct CanEncodeNameType {
  static constexpr bool value = false;
};

template <>
struct CanEncodeNameType<TaggedParserAtomIndex> {
  static constexpr bool value = true;
};

template <typename ScopeDataT, XDRMode mode>
static XDRResult XDRParserTrailingNames(XDRState<mode>* xdr, ScopeDataT& data,
                                        uint32_t length) {
#ifdef __cpp_lib_has_unique_object_representations
  // We check endianess before decoding so if structures are fully packed, we
  // may transcode them directly as raw bytes.
  static_assert(std::has_unique_object_representations<
                    AbstractTrailingNamesArray<TaggedParserAtomIndex>>(),
                "trailingNames structure must be fully packed");
#endif
  static_assert(CanEncodeNameType<typename ScopeDataT::NameType>::value);

  MOZ_TRY(xdr->codeBytes(
      data.trailingNames.start(),
      sizeof(AbstractBindingName<TaggedParserAtomIndex>) * length));

  return Ok();
}

template <typename ScopeT, typename InitF>
static ParserScopeData<ScopeT>* NewEmptyScopeData(JSContext* cx,
                                                  LifoAlloc& alloc,
                                                  uint32_t length, InitF init) {
  using Data = ParserScopeData<ScopeT>;

  size_t dataSize = SizeOfScopeData<Data>(length);
  void* raw = alloc.alloc(dataSize);
  if (!raw) {
    js::ReportOutOfMemory(cx);
    return nullptr;
  }

  Data* data = new (raw) Data(length);
  init(data);
  return data;
}

template <XDRMode mode>
/* static */ XDRResult StencilXDR::FunctionScopeData(XDRState<mode>* xdr,
                                                     ScopeStencil& stencil) {
  ParserFunctionScopeData* data =
      static_cast<ParserFunctionScopeData*>(stencil.data_);

  uint32_t nextFrameSlot = 0;
  uint8_t hasParameterExprs = 0;
  uint16_t nonPositionalFormalStart = 0;
  uint16_t varStart = 0;
  uint32_t length = 0;

  if (mode == XDR_ENCODE) {
    nextFrameSlot = data->nextFrameSlot;
    hasParameterExprs = data->hasParameterExprs ? 1 : 0;
    nonPositionalFormalStart = data->nonPositionalFormalStart;
    varStart = data->varStart;
    length = data->length;
  }

  MOZ_TRY(xdr->codeUint32(&nextFrameSlot));
  MOZ_TRY(xdr->codeUint8(&hasParameterExprs));
  MOZ_TRY(xdr->codeUint16(&nonPositionalFormalStart));
  MOZ_TRY(xdr->codeUint16(&varStart));
  MOZ_TRY(xdr->codeUint32(&length));

  // Reconstruct the scope-data object for decode.
  if (mode == XDR_DECODE) {
    stencil.data_ = data = NewEmptyScopeData<FunctionScope>(
        xdr->cx(), xdr->stencilAlloc(), length, [&](auto data) {
          data->nextFrameSlot = nextFrameSlot;
          MOZ_ASSERT(hasParameterExprs <= 1);
          data->hasParameterExprs = hasParameterExprs;
          data->nonPositionalFormalStart = nonPositionalFormalStart;
          data->varStart = varStart;
          data->length = length;
        });
    if (!data) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  // Decode each name in TrailingNames.
  MOZ_TRY(XDRParserTrailingNames(xdr, *data, length));

  return Ok();
}

template <XDRMode mode>
/* static */ XDRResult StencilXDR::VarScopeData(XDRState<mode>* xdr,
                                                ScopeStencil& stencil) {
  ParserVarScopeData* data = static_cast<ParserVarScopeData*>(stencil.data_);

  uint32_t nextFrameSlot = 0;
  uint32_t length = 0;

  if (mode == XDR_ENCODE) {
    nextFrameSlot = data->nextFrameSlot;
    length = data->length;
  }

  MOZ_TRY(xdr->codeUint32(&nextFrameSlot));
  MOZ_TRY(xdr->codeUint32(&length));

  // Reconstruct the scope-data object for decode.
  if (mode == XDR_DECODE) {
    stencil.data_ = data = NewEmptyScopeData<VarScope>(
        xdr->cx(), xdr->stencilAlloc(), length, [&](auto data) {
          data->nextFrameSlot = nextFrameSlot;
          data->length = length;
        });
    if (!data) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  // Decode each name in TrailingNames.
  MOZ_TRY(XDRParserTrailingNames(xdr, *data, length));

  return Ok();
}

template <XDRMode mode>
/* static */ XDRResult StencilXDR::LexicalScopeData(XDRState<mode>* xdr,
                                                    ScopeStencil& stencil) {
  ParserLexicalScopeData* data =
      static_cast<ParserLexicalScopeData*>(stencil.data_);

  uint32_t nextFrameSlot = 0;
  uint32_t constStart = 0;
  uint32_t length = 0;

  if (mode == XDR_ENCODE) {
    nextFrameSlot = data->nextFrameSlot;
    constStart = data->constStart;
    length = data->length;
  }

  MOZ_TRY(xdr->codeUint32(&nextFrameSlot));
  MOZ_TRY(xdr->codeUint32(&constStart));
  MOZ_TRY(xdr->codeUint32(&length));

  // Reconstruct the scope-data object for decode.
  if (mode == XDR_DECODE) {
    stencil.data_ = data = NewEmptyScopeData<LexicalScope>(
        xdr->cx(), xdr->stencilAlloc(), length, [&](auto data) {
          data->nextFrameSlot = nextFrameSlot;
          data->constStart = constStart;
          data->length = length;
        });
    if (!data) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  // Decode each name in TrailingNames.
  MOZ_TRY(XDRParserTrailingNames(xdr, *data, length));

  return Ok();
}

template <XDRMode mode>
/* static */ XDRResult StencilXDR::GlobalScopeData(XDRState<mode>* xdr,
                                                   ScopeStencil& stencil) {
  ParserGlobalScopeData* data =
      static_cast<ParserGlobalScopeData*>(stencil.data_);

  uint32_t letStart = 0;
  uint32_t constStart = 0;
  uint32_t length = 0;

  if (mode == XDR_ENCODE) {
    letStart = data->letStart;
    constStart = data->constStart;
    length = data->length;
  }

  MOZ_TRY(xdr->codeUint32(&letStart));
  MOZ_TRY(xdr->codeUint32(&constStart));
  MOZ_TRY(xdr->codeUint32(&length));

  // Reconstruct the scope-data object for decode.
  if (mode == XDR_DECODE) {
    stencil.data_ = data = NewEmptyScopeData<GlobalScope>(
        xdr->cx(), xdr->stencilAlloc(), length, [&](auto data) {
          data->letStart = letStart;
          data->constStart = constStart;
          data->length = length;
        });
    if (!data) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  // Decode each name in TrailingNames.
  MOZ_TRY(XDRParserTrailingNames(xdr, *data, length));

  return Ok();
}

template <XDRMode mode>
/* static */ XDRResult StencilXDR::ModuleScopeData(XDRState<mode>* xdr,
                                                   ScopeStencil& stencil) {
  ParserModuleScopeData* data =
      static_cast<ParserModuleScopeData*>(stencil.data_);

  uint32_t nextFrameSlot = 0;
  uint32_t varStart = 0;
  uint32_t letStart = 0;
  uint32_t constStart = 0;
  uint32_t length = 0;

  if (mode == XDR_ENCODE) {
    nextFrameSlot = data->nextFrameSlot;
    varStart = data->varStart;
    letStart = data->letStart;
    constStart = data->constStart;
    length = data->length;
  }

  MOZ_TRY(xdr->codeUint32(&nextFrameSlot));
  MOZ_TRY(xdr->codeUint32(&varStart));
  MOZ_TRY(xdr->codeUint32(&letStart));
  MOZ_TRY(xdr->codeUint32(&constStart));
  MOZ_TRY(xdr->codeUint32(&length));

  // Reconstruct the scope-data object for decode.
  if (mode == XDR_DECODE) {
    stencil.data_ = data = NewEmptyScopeData<ModuleScope>(
        xdr->cx(), xdr->stencilAlloc(), length, [&](auto data) {
          data->nextFrameSlot = nextFrameSlot;
          data->varStart = varStart;
          data->letStart = letStart;
          data->constStart = constStart;
          data->length = length;
        });
    if (!data) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  // Decode each name in TrailingNames.
  MOZ_TRY(XDRParserTrailingNames(xdr, *data, length));

  return Ok();
}

template <XDRMode mode>
/* static */ XDRResult StencilXDR::EvalScopeData(XDRState<mode>* xdr,
                                                 ScopeStencil& stencil) {
  ParserEvalScopeData* data = static_cast<ParserEvalScopeData*>(stencil.data_);

  uint32_t nextFrameSlot = 0;
  uint32_t length = 0;

  if (mode == XDR_ENCODE) {
    nextFrameSlot = data->nextFrameSlot;
    length = data->length;
  }

  MOZ_TRY(xdr->codeUint32(&nextFrameSlot));
  MOZ_TRY(xdr->codeUint32(&length));

  // Reconstruct the scope-data object for decode.
  if (mode == XDR_DECODE) {
    stencil.data_ = data = NewEmptyScopeData<EvalScope>(
        xdr->cx(), xdr->stencilAlloc(), length, [&](auto data) {
          data->nextFrameSlot = nextFrameSlot;
          data->length = length;
        });
    if (!data) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  // Decode each name in TrailingNames.
  MOZ_TRY(XDRParserTrailingNames(xdr, *data, length));

  return Ok();
}

template <XDRMode mode, typename VecType>
static XDRResult XDRVector(XDRState<mode>* xdr, VecType& vec) {
  uint32_t length;

  if (mode == XDR_ENCODE) {
    MOZ_ASSERT(vec.length() <= UINT32_MAX);
    length = vec.length();
  }

  MOZ_TRY(xdr->codeUint32(&length));

  if (mode == XDR_DECODE) {
    MOZ_ASSERT(vec.empty());
    if (!vec.resize(length)) {
      js::ReportOutOfMemory(xdr->cx());
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  return Ok();
}

template <typename ItemType, XDRMode mode, typename VecType>
static XDRResult XDRVectorContent(XDRState<mode>* xdr, VecType& vec) {
#ifdef __cpp_lib_has_unique_object_representations
  static_assert(std::has_unique_object_representations<ItemType>(),
                "vector item structure must be fully packed");
#endif

  uint32_t length;

  if (mode == XDR_ENCODE) {
    MOZ_ASSERT(vec.length() <= UINT32_MAX);
    length = vec.length();
  }

  MOZ_TRY(xdr->codeUint32(&length));

  if (mode == XDR_DECODE) {
    MOZ_ASSERT(vec.empty());
    if (!vec.growByUninitialized(length)) {
      js::ReportOutOfMemory(xdr->cx());
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  MOZ_TRY(xdr->codeBytes(vec.begin(), sizeof(ItemType) * length));

  return Ok();
}

template <XDRMode mode, typename T>
static XDRResult XDRSpanContent(XDRState<mode>* xdr, mozilla::Span<T>& span) {
#ifdef __cpp_lib_has_unique_object_representations
  static_assert(std::has_unique_object_representations<T>(),
                "span item structure must be fully packed");
#endif

  uint32_t size;

  if (mode == XDR_ENCODE) {
    MOZ_ASSERT(span.size() <= UINT32_MAX);
    size = span.size();
  }

  MOZ_TRY(xdr->codeUint32(&size));

  if (mode == XDR_DECODE) {
    MOZ_ASSERT(span.empty());
    if (size > 0) {
      auto* p = xdr->stencilAlloc().template newArrayUninitialized<T>(size);
      if (!p) {
        js::ReportOutOfMemory(xdr->cx());
        return xdr->fail(JS::TranscodeResult_Throw);
      }
      span = mozilla::Span(p, size);
    }
  }

  MOZ_TRY(xdr->codeBytes(span.data(), sizeof(T) * size));

  return Ok();
}

template <XDRMode mode>
static XDRResult XDRStencilModuleEntryVector(
    XDRState<mode>* xdr, StencilModuleMetadata::EntryVector& vec) {
  uint64_t length;

  if (mode == XDR_ENCODE) {
    length = vec.length();
  }

  MOZ_TRY(xdr->codeUint64(&length));

  if (mode == XDR_DECODE) {
    MOZ_ASSERT(vec.empty());
    if (!vec.resize(length)) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  for (StencilModuleEntry& entry : vec) {
    MOZ_TRY(xdr->codeUint32(&entry.lineno));
    MOZ_TRY(xdr->codeUint32(&entry.column));

    MOZ_TRY(XDRTaggedParserAtomIndex(xdr, &entry.specifier));
    MOZ_TRY(XDRTaggedParserAtomIndex(xdr, &entry.localName));
    MOZ_TRY(XDRTaggedParserAtomIndex(xdr, &entry.importName));
    MOZ_TRY(XDRTaggedParserAtomIndex(xdr, &entry.exportName));
  }

  return Ok();
}

template <XDRMode mode>
static XDRResult XDRStencilModuleMetadata(XDRState<mode>* xdr,
                                          StencilModuleMetadata& stencil) {
  MOZ_TRY(XDRStencilModuleEntryVector(xdr, stencil.requestedModules));
  MOZ_TRY(XDRStencilModuleEntryVector(xdr, stencil.importEntries));
  MOZ_TRY(XDRStencilModuleEntryVector(xdr, stencil.localExportEntries));
  MOZ_TRY(XDRStencilModuleEntryVector(xdr, stencil.indirectExportEntries));
  MOZ_TRY(XDRStencilModuleEntryVector(xdr, stencil.starExportEntries));

  {
    uint64_t length;

    if (mode == XDR_ENCODE) {
      length = stencil.functionDecls.length();
    }

    MOZ_TRY(xdr->codeUint64(&length));

    if (mode == XDR_DECODE) {
      MOZ_ASSERT(stencil.functionDecls.empty());
      if (!stencil.functionDecls.resize(length)) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
    }

    for (GCThingIndex& entry : stencil.functionDecls) {
      MOZ_TRY(xdr->codeUint32(&entry.index));
    }
  }

  uint8_t isAsync = 0;
  if (mode == XDR_ENCODE) {
    if (stencil.isAsync) {
      isAsync = stencil.isAsync ? 1 : 0;
    }
  }

  MOZ_TRY(xdr->codeUint8(&isAsync));

  if (mode == XDR_DECODE) {
    stencil.isAsync = isAsync == 1;
  }

  return Ok();
}

template <XDRMode mode>
/* static */ XDRResult StencilXDR::Scope(XDRState<mode>* xdr,
                                         ScopeStencil& stencil) {
  enum class XdrFlags {
    HasEnclosing,
    HasEnvironment,
    IsArrow,
  };

  uint8_t xdrFlags = 0;
  uint8_t kind;

  if (mode == XDR_ENCODE) {
    kind = static_cast<uint8_t>(stencil.kind_);
    if (stencil.enclosing_.isSome()) {
      xdrFlags |= 1 << uint8_t(XdrFlags::HasEnclosing);
    }
    if (stencil.numEnvironmentSlots_.isSome()) {
      xdrFlags |= 1 << uint8_t(XdrFlags::HasEnvironment);
    }
    if (stencil.isArrow_) {
      xdrFlags |= 1 << uint8_t(XdrFlags::IsArrow);
    }
  }

  MOZ_TRY(xdr->codeUint8(&xdrFlags));
  MOZ_TRY(xdr->codeUint8(&kind));
  MOZ_TRY(xdr->codeUint32(&stencil.firstFrameSlot_));

  if (mode == XDR_DECODE) {
    stencil.kind_ = static_cast<ScopeKind>(kind);
  }

  if (xdrFlags & (1 << uint8_t(XdrFlags::HasEnclosing))) {
    if (mode == XDR_DECODE) {
      stencil.enclosing_ = mozilla::Some(ScopeIndex());
    }
    MOZ_ASSERT(stencil.enclosing_.isSome());
    MOZ_TRY(xdr->codeUint32(&stencil.enclosing_->index));
  }

  if (xdrFlags & (1 << uint8_t(XdrFlags::HasEnvironment))) {
    if (mode == XDR_DECODE) {
      stencil.numEnvironmentSlots_ = mozilla::Some(0);
    }
    MOZ_ASSERT(stencil.numEnvironmentSlots_.isSome());
    MOZ_TRY(xdr->codeUint32(&stencil.numEnvironmentSlots_.ref()));
  }

  if (xdrFlags & (1 << uint8_t(XdrFlags::IsArrow))) {
    if (mode == XDR_DECODE) {
      stencil.isArrow_ = true;
    }
  }

  if (stencil.kind_ == ScopeKind::Function) {
    if (mode == XDR_DECODE) {
      stencil.functionIndex_ = mozilla::Some(FunctionIndex());
    }
    MOZ_ASSERT(stencil.functionIndex_.isSome());
    MOZ_TRY(xdr->codeUint32(&stencil.functionIndex_->index));
  }

  // In both decoding and encoding, stencil.kind_ is now known, and
  // can be assumed.  This allows the encoding to write out the bytes
  // for the specialized scope-data type without needing to encode
  // a distinguishing prefix.
  switch (stencil.kind_) {
    // FunctionScope
    case ScopeKind::Function: {
      MOZ_TRY(StencilXDR::FunctionScopeData(xdr, stencil));
      break;
    }

    // VarScope
    case ScopeKind::FunctionBodyVar: {
      MOZ_TRY(StencilXDR::VarScopeData(xdr, stencil));
      break;
    }

    // LexicalScope
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
    case ScopeKind::FunctionLexical:
    case ScopeKind::ClassBody: {
      MOZ_TRY(StencilXDR::LexicalScopeData(xdr, stencil));
      break;
    }

    // WithScope
    case ScopeKind::With: {
      // With scopes carry no scope data.
      break;
    }

    // EvalScope
    case ScopeKind::Eval:
    case ScopeKind::StrictEval: {
      MOZ_TRY(StencilXDR::EvalScopeData(xdr, stencil));
      break;
    }

    // GlobalScope
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic: {
      MOZ_TRY(StencilXDR::GlobalScopeData(xdr, stencil));
      break;
    }

    // ModuleScope
    case ScopeKind::Module: {
      MOZ_TRY(StencilXDR::ModuleScopeData(xdr, stencil));
      break;
    }

    // WasmInstanceScope & WasmFunctionScope should not appear in stencils.
    case ScopeKind::WasmInstance:
    case ScopeKind::WasmFunction:
    default:
      MOZ_ASSERT_UNREACHABLE("XDR unrecognized ScopeKind.");
  }

  return Ok();
}

template <XDRMode mode>
/* static */ XDRResult StencilXDR::ObjLiteral(XDRState<mode>* xdr,
                                              ObjLiteralStencil& stencil) {
  uint8_t flags = 0;

  if (mode == XDR_ENCODE) {
    flags = stencil.flags_.serialize();
  }
  MOZ_TRY(xdr->codeUint8(&flags));
  if (mode == XDR_DECODE) {
    stencil.flags_.deserialize(flags);
  }

  MOZ_TRY(XDRSpanContent(xdr, stencil.code_));

  return Ok();
}

template <XDRMode mode>
/* static */ XDRResult StencilXDR::BigInt(XDRState<mode>* xdr,
                                          BigIntStencil& stencil) {
  uint64_t length;

  if (mode == XDR_ENCODE) {
    length = stencil.length_;
  }

  MOZ_TRY(xdr->codeUint64(&length));

  XDRTranscodeString<char16_t> chars;

  if (mode == XDR_DECODE) {
    stencil.buf_ = xdr->cx()->template make_pod_array<char16_t>(length);
    if (!stencil.buf_) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    stencil.length_ = length;
  }

  return xdr->codeChars(stencil.buf_.get(), stencil.length_);
}

template <XDRMode mode>
/* static */
XDRResult StencilXDR::SharedData(XDRState<mode>* xdr,
                                 RefPtr<SharedImmutableScriptData>& sisd) {
  if (mode == XDR_ENCODE) {
    MOZ_TRY(XDRImmutableScriptData<mode>(xdr, sisd->isd_));
  } else {
    JSContext* cx = xdr->cx();
    UniquePtr<SharedImmutableScriptData> data(
        SharedImmutableScriptData::create(cx));
    if (!data) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
    MOZ_TRY(XDRImmutableScriptData<mode>(xdr, data->isd_));
    sisd = data.release();
  }

  return Ok();
}

template
    /* static */
    XDRResult
    StencilXDR::SharedData(XDRState<XDR_ENCODE>* xdr,
                           RefPtr<SharedImmutableScriptData>& sisd);

template
    /* static */
    XDRResult
    StencilXDR::SharedData(XDRState<XDR_DECODE>* xdr,
                           RefPtr<SharedImmutableScriptData>& sisd);

namespace js {

template <XDRMode mode>
XDRResult XDRCompilationInput(XDRState<mode>* xdr, CompilationInput& input) {
  // XDR the ScriptSource

  // Instrumented scripts cannot be encoded, as they have extra instructions
  // which are not normally present. Globals with instrumentation enabled must
  // compile scripts via the bytecode emitter, which will insert these
  // instructions.
  if (mode == XDR_ENCODE) {
    if (!!input.options.instrumentationKinds) {
      return xdr->fail(JS::TranscodeResult_Failure);
    }
  }

  // Copy the options out for passing into `ScriptSource::XDR`.
  mozilla::Maybe<JS::CompileOptions> opts;
  opts.emplace(xdr->cx(), input.options);

  Rooted<ScriptSourceHolder> holder(xdr->cx());
  if (mode == XDR_ENCODE) {
    holder.get().reset(input.source_.get());
  }
  MOZ_TRY(ScriptSource::XDR(xdr, opts, &holder));

  if (mode == XDR_DECODE) {
    input.source_.reset(holder.get().get());
  }

  return Ok();
}

template XDRResult XDRCompilationInput(XDRState<XDR_ENCODE>* xdr,
                                       CompilationInput& input);

template XDRResult XDRCompilationInput(XDRState<XDR_DECODE>* xdr,
                                       CompilationInput& input);

template <XDRMode mode>
XDRResult XDRSharedDataContainer(XDRState<mode>* xdr,
                                 SharedDataContainer& sharedData) {
  enum class Kind : uint8_t {
    Single,
    Vector,
    Map,
  };

  uint8_t kind;
  if (mode == XDR_ENCODE) {
    if (sharedData.storage.is<SharedDataContainer::SingleSharedData>()) {
      kind = uint8_t(Kind::Single);
    } else if (sharedData.storage.is<SharedDataContainer::SharedDataVector>()) {
      kind = uint8_t(Kind::Vector);
    } else {
      MOZ_ASSERT(sharedData.storage.is<SharedDataContainer::SharedDataMap>());
      kind = uint8_t(Kind::Map);
    }
  }
  MOZ_TRY(xdr->codeUint8(&kind));

  switch (Kind(kind)) {
    case Kind::Single: {
      if (mode == XDR_DECODE) {
        sharedData.storage.emplace<SharedDataContainer::SingleSharedData>();
      }
      auto& data =
          sharedData.storage.as<SharedDataContainer::SingleSharedData>();
      MOZ_TRY(StencilXDR::SharedData<mode>(xdr, data));
      break;
    }

    case Kind::Vector: {
      if (mode == XDR_DECODE) {
        sharedData.storage.emplace<SharedDataContainer::SharedDataVector>();
      }
      auto& vec =
          sharedData.storage.as<SharedDataContainer::SharedDataVector>();
      MOZ_TRY(XDRVector(xdr, vec));
      for (auto& entry : vec) {
        // NOTE: There can be nullptr, even if we don't perform syntax parsing,
        //       because of constant folding.
        uint8_t exists;
        if (mode == XDR_ENCODE) {
          exists = !!entry;
        }

        MOZ_TRY(xdr->codeUint8(&exists));

        if (exists) {
          MOZ_TRY(StencilXDR::SharedData<mode>(xdr, entry));
        }
      }
      break;
    }

    case Kind::Map: {
      if (mode == XDR_DECODE) {
        sharedData.storage.emplace<SharedDataContainer::SharedDataMap>();
      }
      auto& map = sharedData.storage.as<SharedDataContainer::SharedDataMap>();

      uint32_t count;
      if (mode == XDR_ENCODE) {
        count = map.count();
      }
      MOZ_TRY(xdr->codeUint32(&count));
      if (mode == XDR_DECODE) {
        if (!map.reserve(count)) {
          js::ReportOutOfMemory(xdr->cx());
          return xdr->fail(JS::TranscodeResult_Throw);
        }
      }

      if (mode == XDR_ENCODE) {
        for (auto iter = map.iter(); !iter.done(); iter.next()) {
          uint32_t index = iter.get().key().index;
          auto& data = iter.get().value();
          MOZ_TRY(xdr->codeUint32(&index));
          MOZ_TRY(StencilXDR::SharedData<mode>(xdr, data));
        }
      } else {
        for (uint32_t i = 0; i < count; i++) {
          FunctionIndex index;
          MOZ_TRY(xdr->codeUint32(&index.index));

          RefPtr<SharedImmutableScriptData> data;
          MOZ_TRY(StencilXDR::SharedData<mode>(xdr, data));

          if (!map.putNew(index, data)) {
            js::ReportOutOfMemory(xdr->cx());
            return xdr->fail(JS::TranscodeResult_Throw);
          }
        }
      }

      break;
    }
  }

  return Ok();
}

template XDRResult XDRSharedDataContainer(XDRState<XDR_ENCODE>* xdr,
                                          SharedDataContainer& sharedData);

template XDRResult XDRSharedDataContainer(XDRState<XDR_DECODE>* xdr,
                                          SharedDataContainer& sharedData);

template <XDRMode mode>
XDRResult XDRCompilationStencil(XDRState<mode>* xdr,
                                CompilationStencil& stencil) {
  if (!stencil.asmJS.empty()) {
    return xdr->fail(JS::TranscodeResult_Failure_AsmJSNotSupported);
  }

  // All of the vector-indexed data elements referenced by the
  // main script tree must be materialized first.

  MOZ_TRY(XDRVector(xdr, stencil.scopeData));
  for (auto& entry : stencil.scopeData) {
    MOZ_TRY(StencilXDR::Scope(xdr, entry));
  }

  MOZ_TRY(XDRVectorContent<RegExpStencil>(xdr, stencil.regExpData));

  MOZ_TRY(XDRVector(xdr, stencil.bigIntData));
  for (auto& entry : stencil.bigIntData) {
    MOZ_TRY(StencilXDR::BigInt(xdr, entry));
  }

  MOZ_TRY(XDRVector(xdr, stencil.objLiteralData));
  for (auto& entry : stencil.objLiteralData) {
    MOZ_TRY(StencilXDR::ObjLiteral(xdr, entry));
  }

  MOZ_TRY(XDRSharedDataContainer(xdr, stencil.sharedData));

  // Now serialize the vector of ScriptStencils.

  MOZ_TRY(XDRVector(xdr, stencil.scriptData));
  for (auto& entry : stencil.scriptData) {
    MOZ_TRY(StencilXDR::Script(xdr, entry));
  }

  if (stencil.scriptData[CompilationInfo::TopLevelIndex].isModule()) {
    if (mode == XDR_DECODE) {
      stencil.moduleMetadata.emplace();
    }

    MOZ_TRY(XDRStencilModuleMetadata(xdr, *stencil.moduleMetadata));
  }

  return Ok();
}
template XDRResult XDRCompilationStencil(XDRState<XDR_ENCODE>* xdr,
                                         CompilationStencil& stencil);

template XDRResult XDRCompilationStencil(XDRState<XDR_DECODE>* xdr,
                                         CompilationStencil& stencil);

}  // namespace js
