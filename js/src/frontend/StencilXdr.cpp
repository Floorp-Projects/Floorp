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

#include "frontend/CompilationInfo.h"  // BaseCompilationStencil
#include "frontend/ScriptIndex.h"      // ScriptIndex
#include "vm/JSScript.h"               // js::CheckCompileOptionsMatch
#include "vm/StencilEnums.h"           // js::ImmutableScriptFlagsEnum

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

  MOZ_TRY(xdr->align32());

  const SlotInfo* slotInfo;
  if (mode == XDR_ENCODE) {
    ScopeDataT* scopeData = static_cast<ScopeDataT*>(baseScopeData);
    slotInfo = &scopeData->slotInfo;
  } else {
    MOZ_TRY(xdr->peekData(&slotInfo));
  }

  uint32_t totalLength =
      sizeof(SlotInfo) +
      sizeof(AbstractBindingName<TaggedParserAtomIndex>) * slotInfo->length;
  MOZ_TRY(xdr->borrowedData(&baseScopeData, totalLength));

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
static XDRResult XDRVectorInitialized(XDRState<mode>* xdr,
                                      Vector<T, N, AP>& vec, uint32_t length) {
  MOZ_ASSERT_IF(mode == XDR_ENCODE, length == vec.length());

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
static XDRResult XDRVectorInitialized(XDRState<mode>* xdr,
                                      Vector<T, N, AP>& vec) {
  uint32_t length;
  if (mode == XDR_ENCODE) {
    MOZ_ASSERT(vec.length() <= UINT32_MAX);
    length = vec.length();
  }

  MOZ_TRY(xdr->codeUint32(&length));

  return XDRVectorInitialized(xdr, vec, length);
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
static XDRResult XDRSpanInitialized(XDRState<mode>* xdr, mozilla::Span<T>& span,
                                    uint32_t size) {
  MOZ_ASSERT_IF(mode == XDR_ENCODE, size == span.size());

  if (mode == XDR_DECODE) {
    MOZ_ASSERT(span.empty());
    if (size > 0) {
      auto* p = xdr->stencilAlloc().template newArrayUninitialized<T>(size);
      if (!p) {
        js::ReportOutOfMemory(xdr->cx());
        return xdr->fail(JS::TranscodeResult_Throw);
      }
      span = mozilla::Span(p, size);

      for (size_t i = 0; i < size; i++) {
        new (mozilla::KnownNotNull, &span[i]) T();
      }
    }
  }

  return Ok();
}

template <XDRMode mode, typename T>
static XDRResult XDRSpanContent(XDRState<mode>* xdr, mozilla::Span<T>& span,
                                uint32_t size) {
  static_assert(CanCopyDataToDisk<T>::value,
                "Span cannot be bulk-copied to disk.");
  MOZ_ASSERT_IF(mode == XDR_ENCODE, size == span.size());

  if (size) {
    MOZ_TRY(xdr->align32());

    T* data;
    if (mode == XDR_ENCODE) {
      data = span.data();
    }
    MOZ_TRY(xdr->borrowedData(&data, sizeof(T) * size));
    if (mode == XDR_DECODE) {
      span = mozilla::Span(data, size);
    }
  }

  return Ok();
}

template <XDRMode mode, typename T>
static XDRResult XDRSpanContent(XDRState<mode>* xdr, mozilla::Span<T>& span) {
  uint32_t size;
  if (mode == XDR_ENCODE) {
    MOZ_ASSERT(span.size() <= UINT32_MAX);
    size = span.size();
  }

  MOZ_TRY(xdr->codeUint32(&size));

  return XDRSpanContent(xdr, span, size);
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
      MOZ_TRY(XDRVectorInitialized(xdr, vec));
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
XDRResult XDRBaseCompilationStencilSpanSize(
    XDRState<mode>* xdr, uint32_t* scriptSize, uint32_t* gcThingSize,
    uint32_t* scopeSize, uint32_t* regExpSize, uint32_t* bigIntSize,
    uint32_t* objLiteralSize) {
  // Compress the series of span sizes, to avoid consuming extra space for
  // unused/small span sizes.
  // There will be align32 shortly after this section, so try to make the
  // padding smaller.

  enum XDRSpanSizeKind {
    // The scriptSize, gcThingSize, and scopeSize fit in 1 byte, and others have
    // a value of 0. The entire section takes 4 bytes, and expect no padding.
    Base8Kind,

    // All of the size values can fit in 1 byte each. The entire section takes 7
    // bytes, and expect 1 byte padding.
    All8Kind,

    // Other. This case is less than 1% in practice and indicates the stencil is
    // already quite large, so don't try to compress. Expect 3 bytes padding for
    // `sizeKind`.
    All32Kind,
  };

  uint8_t sizeKind = All32Kind;
  if (mode == XDR_ENCODE) {
    uint32_t mask_base = (*scriptSize) | (*gcThingSize) | (*scopeSize);
    uint32_t mask_ext = (*regExpSize) | (*bigIntSize) | (*objLiteralSize);

    if (mask_base <= 0xff) {
      if (mask_ext == 0x00) {
        sizeKind = Base8Kind;
      } else if (mask_ext <= 0xFF) {
        sizeKind = All8Kind;
      }
    }
  }
  MOZ_TRY(xdr->codeUint8(&sizeKind));

  if (sizeKind == All32Kind) {
    MOZ_TRY(xdr->codeUint32(scriptSize));
    MOZ_TRY(xdr->codeUint32(gcThingSize));
    MOZ_TRY(xdr->codeUint32(scopeSize));
    MOZ_TRY(xdr->codeUint32(regExpSize));
    MOZ_TRY(xdr->codeUint32(bigIntSize));
    MOZ_TRY(xdr->codeUint32(objLiteralSize));
  } else {
    uint8_t scriptSize8 = 0;
    uint8_t gcThingSize8 = 0;
    uint8_t scopeSize8 = 0;
    uint8_t regExpSize8 = 0;
    uint8_t bigIntSize8 = 0;
    uint8_t objLiteralSize8 = 0;

    if (mode == XDR_ENCODE) {
      scriptSize8 = uint8_t(*scriptSize);
      gcThingSize8 = uint8_t(*gcThingSize);
      scopeSize8 = uint8_t(*scopeSize);
      regExpSize8 = uint8_t(*regExpSize);
      bigIntSize8 = uint8_t(*bigIntSize);
      objLiteralSize8 = uint8_t(*objLiteralSize);
    }

    MOZ_TRY(xdr->codeUint8(&scriptSize8));
    MOZ_TRY(xdr->codeUint8(&gcThingSize8));
    MOZ_TRY(xdr->codeUint8(&scopeSize8));

    if (sizeKind == All8Kind) {
      MOZ_TRY(xdr->codeUint8(&regExpSize8));
      MOZ_TRY(xdr->codeUint8(&bigIntSize8));
      MOZ_TRY(xdr->codeUint8(&objLiteralSize8));
    } else {
      MOZ_ASSERT(regExpSize8 == 0);
      MOZ_ASSERT(bigIntSize8 == 0);
      MOZ_ASSERT(objLiteralSize8 == 0);
    }

    if (mode == XDR_DECODE) {
      *scriptSize = scriptSize8;
      *gcThingSize = gcThingSize8;
      *scopeSize = scopeSize8;
      *regExpSize = regExpSize8;
      *bigIntSize = bigIntSize8;
      *objLiteralSize = objLiteralSize8;
    }
  }

  return Ok();
}

template <XDRMode mode>
XDRResult XDRBaseCompilationStencil(XDRState<mode>* xdr,
                                    BaseCompilationStencil& stencil) {
  MOZ_TRY(xdr->codeUint32(&stencil.functionKey));

  uint32_t scriptSize, gcThingSize, scopeSize;
  uint32_t regExpSize, bigIntSize, objLiteralSize;
  if (mode == XDR_ENCODE) {
    scriptSize = stencil.scriptData.size();
    gcThingSize = stencil.gcThingData.size();
    scopeSize = stencil.scopeData.size();
    MOZ_ASSERT(scopeSize == stencil.scopeNames.size());

    regExpSize = stencil.regExpData.size();
    bigIntSize = stencil.bigIntData.length();
    objLiteralSize = stencil.objLiteralData.length();
  }
  MOZ_TRY(XDRBaseCompilationStencilSpanSize(xdr, &scriptSize, &gcThingSize,
                                            &scopeSize, &regExpSize,
                                            &bigIntSize, &objLiteralSize));

  // All of the vector-indexed data elements referenced by the
  // main script tree must be materialized first.

  MOZ_TRY(XDRSpanContent(xdr, stencil.scopeData, scopeSize));
  MOZ_TRY(XDRSpanInitialized(xdr, stencil.scopeNames, scopeSize));
  MOZ_ASSERT(stencil.scopeData.size() == stencil.scopeNames.size());
  for (uint32_t i = 0; i < scopeSize; i++) {
    MOZ_TRY(StencilXDR::ScopeData(xdr, stencil.scopeData[i],
                                  stencil.scopeNames[i]));
  }

  MOZ_TRY(XDRSpanContent(xdr, stencil.regExpData, regExpSize));

  MOZ_TRY(XDRVectorInitialized(xdr, stencil.bigIntData, bigIntSize));
  for (auto& entry : stencil.bigIntData) {
    MOZ_TRY(StencilXDR::BigInt(xdr, entry));
  }

  MOZ_TRY(XDRVectorInitialized(xdr, stencil.objLiteralData, objLiteralSize));
  for (auto& entry : stencil.objLiteralData) {
    MOZ_TRY(StencilXDR::ObjLiteral(xdr, entry));
  }

  MOZ_TRY(XDRSharedDataContainer(xdr, stencil.sharedData));

  MOZ_TRY(XDRSpanContent(xdr, stencil.gcThingData, gcThingSize));

  // Now serialize the vector of ScriptStencils.

  MOZ_TRY(XDRSpanContent(xdr, stencil.scriptData, scriptSize));

  return Ok();
}

template XDRResult XDRBaseCompilationStencil(XDRState<XDR_ENCODE>* xdr,
                                             BaseCompilationStencil& stencil);

template XDRResult XDRBaseCompilationStencil(XDRState<XDR_DECODE>* xdr,
                                             BaseCompilationStencil& stencil);

template <XDRMode mode>
XDRResult XDRCompilationStencil(XDRState<mode>* xdr,
                                CompilationStencil& stencil) {
  if (!stencil.asmJS.empty()) {
    return xdr->fail(JS::TranscodeResult_Failure_AsmJSNotSupported);
  }

  MOZ_TRY(XDRBaseCompilationStencil(xdr, stencil));

  MOZ_TRY(XDRSpanContent(xdr, stencil.scriptExtra));

  // We don't support coding non-initial CompilationStencil.
  MOZ_ASSERT(stencil.isInitialStencil());

  if (stencil.scriptExtra[CompilationStencil::TopLevelIndex].isModule()) {
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
