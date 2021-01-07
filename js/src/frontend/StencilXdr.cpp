/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/StencilXdr.h"  // StencilXDR

#include "mozilla/OperatorNewExtensions.h"  // mozilla::KnownNotNull
#include "mozilla/Variant.h"                // mozilla::AsVariant

#include <stddef.h>     // size_t
#include <stdint.h>     // uint8_t, uint16_t, uint32_t
#include <type_traits>  // std::has_unique_object_representations
#include <utility>      // std::forward

#include "frontend/ScriptIndex.h"  // ScriptIndex
#include "vm/JSScript.h"           // js::CheckCompileOptionsMatch
#include "vm/StencilEnums.h"       // js::ImmutableScriptFlagsEnum

using namespace js;
using namespace js::frontend;

template <typename NameType>
struct CanEncodeNameType {
  static constexpr bool value = false;
};

template <>
struct CanEncodeNameType<TaggedParserAtomIndex> {
  static constexpr bool value = true;
};

template <XDRMode mode, typename ScopeT>
/* static */ XDRResult StencilXDR::ScopeSpecificData(
    XDRState<mode>* xdr, BaseParserScopeData*& baseScopeData) {
  using SlotInfo = typename ScopeT::SlotInfo;
  using ScopeDataT = typename ScopeT::ParserData;

  static_assert(CanEncodeNameType<typename ScopeDataT::NameType>::value);
  static_assert(CanCopyDataToDisk<ScopeDataT>::value,
                "ScopeData cannot be bulk-copied to disk");

  static_assert(offsetof(ScopeDataT, slotInfo) == 0,
                "slotInfo should be the first field");
  static_assert(offsetof(ScopeDataT, trailingNames) == sizeof(SlotInfo),
                "trailingNames should be the second field");

  constexpr size_t SlotInfoSize = sizeof(SlotInfo);
  auto ComputeTotalLength = [](size_t length) {
    return SlotInfoSize +
           sizeof(AbstractBindingName<TaggedParserAtomIndex>) * length;
  };

  if (mode == XDR_ENCODE) {
    ScopeDataT* scopeData = static_cast<ScopeDataT*>(baseScopeData);
    const SlotInfo* slotInfo = &scopeData->slotInfo;
    uint32_t totalLength = ComputeTotalLength(slotInfo->length);
    MOZ_TRY(xdr->codeBytes(scopeData, totalLength));
  } else {
    // Peek the SlotInfo bytes without consuming buffer yet. Once we compute the
    // total length, we will read the entire scope data at once.
    SlotInfo slotInfo;
    const uint8_t* cursor = nullptr;
    MOZ_TRY(xdr->peekData(&cursor, SlotInfoSize));
    memcpy(&slotInfo, cursor, SlotInfoSize);

    // Allocate scope data with trailing names.
    uint32_t totalLength = ComputeTotalLength(slotInfo.length);
    ScopeDataT* scopeData =
        reinterpret_cast<ScopeDataT*>(xdr->stencilAlloc().alloc(totalLength));
    if (!scopeData) {
      js::ReportOutOfMemory(xdr->cx());
      return xdr->fail(JS::TranscodeResult_Throw);
    }

    // Decode SlotInfo and trailing names at once.
    MOZ_TRY(xdr->codeBytes(scopeData, totalLength));
    baseScopeData = scopeData;
  }

  return Ok();
}

template <XDRMode mode, typename T, size_t N, class AP>
static XDRResult XDRVectorUninitialized(XDRState<mode>* xdr,
                                        Vector<T, N, AP>& vec,
                                        uint32_t& length) {
  if (mode == XDR_ENCODE) {
    MOZ_ASSERT(vec.length() <= UINT32_MAX);
    length = vec.length();
  }

  MOZ_TRY(xdr->codeUint32(&length));

  if (mode == XDR_DECODE) {
    MOZ_ASSERT(vec.empty());
    if (!vec.resizeUninitialized(length)) {
      js::ReportOutOfMemory(xdr->cx());
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  return Ok();
}

template <XDRMode mode, typename T, size_t N, class AP>
static XDRResult XDRVector(XDRState<mode>* xdr, Vector<T, N, AP>& vec) {
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

template <XDRMode mode, typename T, size_t N, class AP>
static XDRResult XDRVectorContent(XDRState<mode>* xdr, Vector<T, N, AP>& vec) {
  static_assert(CanCopyDataToDisk<T>::value,
                "Vector content cannot be bulk-copied to disk.");

  uint32_t length;
  MOZ_TRY(XDRVectorUninitialized(xdr, vec, length));
  MOZ_TRY(xdr->codeBytes(vec.begin(), sizeof(T) * length));

  return Ok();
}

template <XDRMode mode, typename T>
static XDRResult XDRSpanUninitialized(XDRState<mode>* xdr,
                                      mozilla::Span<T>& span, uint32_t& size) {
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

  return Ok();
}

template <XDRMode mode, typename T>
static XDRResult XDRSpanInitialized(XDRState<mode>* xdr,
                                    mozilla::Span<T>& span) {
  uint32_t size;
  MOZ_TRY(XDRSpanUninitialized(xdr, span, size));

  if (mode == XDR_DECODE) {
    for (size_t i = 0; i < size; i++) {
      new (mozilla::KnownNotNull, &span[i]) T();
    }
  }

  return Ok();
}

template <XDRMode mode, typename T>
static XDRResult XDRSpanContent(XDRState<mode>* xdr, mozilla::Span<T>& span) {
  static_assert(CanCopyDataToDisk<T>::value,
                "Span cannot be bulk-copied to disk.");

  uint32_t size;
  MOZ_TRY(XDRSpanUninitialized(xdr, span, size));
  if (size) {
    MOZ_TRY(xdr->codeBytes(span.data(), sizeof(T) * size));
  }

  return Ok();
}

template <XDRMode mode>
static XDRResult XDRStencilModuleMetadata(XDRState<mode>* xdr,
                                          StencilModuleMetadata& stencil) {
  MOZ_TRY(XDRVectorContent(xdr, stencil.requestedModules));
  MOZ_TRY(XDRVectorContent(xdr, stencil.importEntries));
  MOZ_TRY(XDRVectorContent(xdr, stencil.localExportEntries));
  MOZ_TRY(XDRVectorContent(xdr, stencil.indirectExportEntries));
  MOZ_TRY(XDRVectorContent(xdr, stencil.starExportEntries));
  MOZ_TRY(XDRVectorContent(xdr, stencil.functionDecls));

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
/* static */ XDRResult StencilXDR::ScopeData(
    XDRState<mode>* xdr, ScopeStencil& stencil,
    BaseParserScopeData*& baseScopeData) {
  // In both decoding and encoding, stencil.kind_ is now known, and
  // can be assumed.  This allows the encoding to write out the bytes
  // for the specialized scope-data type without needing to encode
  // a distinguishing prefix.
  switch (stencil.kind_) {
    // FunctionScope
    case ScopeKind::Function: {
      // Extra parentheses is for template parameters inside macro.
      MOZ_TRY((StencilXDR::ScopeSpecificData<mode, FunctionScope>(
          xdr, baseScopeData)));
      break;
    }

    // VarScope
    case ScopeKind::FunctionBodyVar: {
      MOZ_TRY(
          (StencilXDR::ScopeSpecificData<mode, VarScope>(xdr, baseScopeData)));
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
      MOZ_TRY((StencilXDR::ScopeSpecificData<mode, LexicalScope>(
          xdr, baseScopeData)));
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
      MOZ_TRY(
          (StencilXDR::ScopeSpecificData<mode, EvalScope>(xdr, baseScopeData)));
      break;
    }

    // GlobalScope
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic: {
      MOZ_TRY((StencilXDR::ScopeSpecificData<mode, GlobalScope>(
          xdr, baseScopeData)));
      break;
    }

    // ModuleScope
    case ScopeKind::Module: {
      MOZ_TRY((StencilXDR::ScopeSpecificData<mode, ModuleScope>(
          xdr, baseScopeData)));
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
          ScriptIndex index;
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

  MOZ_TRY(XDRSpanContent(xdr, stencil.scopeData));
  MOZ_TRY(XDRSpanInitialized(xdr, stencil.scopeNames));
  MOZ_ASSERT(stencil.scopeData.size() == stencil.scopeNames.size());
  size_t scopeCount = stencil.scopeData.size();
  for (size_t i = 0; i < scopeCount; i++) {
    MOZ_TRY(StencilXDR::ScopeData(xdr, stencil.scopeData[i],
                                  stencil.scopeNames[i]));
  }

  MOZ_TRY(XDRSpanContent(xdr, stencil.regExpData));

  MOZ_TRY(XDRVector(xdr, stencil.bigIntData));
  for (auto& entry : stencil.bigIntData) {
    MOZ_TRY(StencilXDR::BigInt(xdr, entry));
  }

  MOZ_TRY(XDRVector(xdr, stencil.objLiteralData));
  for (auto& entry : stencil.objLiteralData) {
    MOZ_TRY(StencilXDR::ObjLiteral(xdr, entry));
  }

  MOZ_TRY(XDRSharedDataContainer(xdr, stencil.sharedData));

  MOZ_TRY(XDRSpanContent(xdr, stencil.gcThingData));

  // Now serialize the vector of ScriptStencils.

  MOZ_TRY(XDRSpanContent(xdr, stencil.scriptData));

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
