/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Xdr.h"

#include "mozilla/ArrayUtils.h"   // mozilla::ArrayEqual
#include "mozilla/Assertions.h"   // MOZ_ASSERT, MOZ_ASSERT_IF
#include "mozilla/EndianUtils.h"  // mozilla::NativeEndian, MOZ_LITTLE_ENDIAN
#include "mozilla/RefPtr.h"       // RefPtr
#include "mozilla/Result.h"       // mozilla::{Result, Ok, Err}, MOZ_TRY
#include "mozilla/ScopeExit.h"    // mozilla::MakeScopeExit
#include "mozilla/Utf8.h"         // mozilla::Utf8Unit

#include <algorithm>    // std::transform
#include <stddef.h>     // size_t
#include <stdint.h>     // uint8_t, uint32_t, uintptr_t
#include <string>       // std::char_traits
#include <type_traits>  // std::is_same_v
#include <utility>      // std::move

#include "frontend/CompilationStencil.h"  // frontend::{CompilationStencil, ExtensibleCompilationStencil, CompilationStencilMerger, BorrowingCompilationStencil}
#include "frontend/StencilXdr.h"          // frontend::StencilXDR
#include "js/BuildId.h"                   // JS::BuildIdCharVector
#include "js/CompileOptions.h"            // JS::ReadOnlyCompileOptions
#include "js/Transcoding.h"  // JS::TranscodeResult, JS::TranscodeBuffer, JS::TranscodeRange
#include "js/UniquePtr.h"   // UniquePtr
#include "js/Utility.h"     // JS::FreePolicy, js_delete
#include "vm/JSContext.h"   // JSContext, ReportAllocationOverflow
#include "vm/JSScript.h"    // ScriptSource
#include "vm/Runtime.h"     // GetBuildId
#include "vm/StringType.h"  // JSString

using namespace js;

using mozilla::ArrayEqual;
using mozilla::Utf8Unit;

#ifdef DEBUG
bool XDRCoderBase::validateResultCode(JSContext* cx,
                                      JS::TranscodeResult code) const {
  // NOTE: This function is called to verify that we do not have a pending
  // exception on the JSContext at the same time as a TranscodeResult failure.
  if (cx->isHelperThreadContext()) {
    return true;
  }
  return cx->isExceptionPending() == bool(code == JS::TranscodeResult::Throw);
}
#endif

template <XDRMode mode>
XDRResult XDRState<mode>::codeChars(char* chars, size_t nchars) {
  return codeBytes(chars, nchars);
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeChars(Latin1Char* chars, size_t nchars) {
  static_assert(sizeof(Latin1Char) == 1,
                "Latin1Char must be 1 byte for nchars below to be the "
                "proper count of bytes");
  static_assert(std::is_same_v<Latin1Char, unsigned char>,
                "Latin1Char must be unsigned char to C++-safely reinterpret "
                "the bytes generically copied below as Latin1Char");
  return codeBytes(chars, nchars);
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeChars(Utf8Unit* units, size_t count) {
  if (count == 0) {
    return Ok();
  }

  if (mode == XDR_ENCODE) {
    uint8_t* ptr = buf->write(count);
    if (!ptr) {
      return fail(JS::TranscodeResult::Throw);
    }

    std::transform(units, units + count, ptr,
                   [](const Utf8Unit& unit) { return unit.toUint8(); });
  } else {
    const uint8_t* ptr = buf->read(count);
    if (!ptr) {
      return fail(JS::TranscodeResult::Failure_BadDecode);
    }

    std::transform(ptr, ptr + count, units,
                   [](const uint8_t& value) { return Utf8Unit(value); });
  }

  return Ok();
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeChars(char16_t* chars, size_t nchars) {
  if (nchars == 0) {
    return Ok();
  }

  size_t nbytes = nchars * sizeof(char16_t);
  if (mode == XDR_ENCODE) {
    uint8_t* ptr = buf->write(nbytes);
    if (!ptr) {
      return fail(JS::TranscodeResult::Throw);
    }

    // |mozilla::NativeEndian| correctly handles writing into unaligned |ptr|.
    mozilla::NativeEndian::copyAndSwapToLittleEndian(ptr, chars, nchars);
  } else {
    const uint8_t* ptr = buf->read(nbytes);
    if (!ptr) {
      return fail(JS::TranscodeResult::Failure_BadDecode);
    }

    // |mozilla::NativeEndian| correctly handles reading from unaligned |ptr|.
    mozilla::NativeEndian::copyAndSwapFromLittleEndian(chars, ptr, nchars);
  }
  return Ok();
}

template <XDRMode mode, typename CharT>
static XDRResult XDRCodeCharsZ(XDRState<mode>* xdr,
                               XDRTranscodeString<CharT>& buffer) {
  MOZ_ASSERT_IF(mode == XDR_ENCODE, !buffer.empty());
  MOZ_ASSERT_IF(mode == XDR_DECODE, buffer.empty());

  using OwnedString = js::UniquePtr<CharT[], JS::FreePolicy>;
  OwnedString owned;

  static_assert(JSString::MAX_LENGTH <= INT32_MAX,
                "String length must fit in int32_t");

  uint32_t length = 0;
  CharT* chars = nullptr;

  if (mode == XDR_ENCODE) {
    chars = const_cast<CharT*>(buffer.template ref<const CharT*>());

    // Set a reasonable limit on string length.
    size_t lengthSizeT = std::char_traits<CharT>::length(chars);
    if (lengthSizeT > JSString::MAX_LENGTH) {
      ReportAllocationOverflow(xdr->cx());
      return xdr->fail(JS::TranscodeResult::Throw);
    }
    length = static_cast<uint32_t>(lengthSizeT);
  }
  MOZ_TRY(xdr->codeUint32(&length));

  if (mode == XDR_DECODE) {
    owned = xdr->cx()->template make_pod_array<CharT>(length + 1);
    if (!owned) {
      return xdr->fail(JS::TranscodeResult::Throw);
    }
    chars = owned.get();
  }

  MOZ_TRY(xdr->codeChars(chars, length));
  if (mode == XDR_DECODE) {
    // Null-terminate and transfer ownership to caller.
    owned[length] = '\0';
    buffer.template construct<OwnedString>(std::move(owned));
  }

  return Ok();
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeCharsZ(XDRTranscodeString<char>& buffer) {
  return XDRCodeCharsZ(this, buffer);
}

template <XDRMode mode>
XDRResult XDRState<mode>::codeCharsZ(XDRTranscodeString<char16_t>& buffer) {
  return XDRCodeCharsZ(this, buffer);
}

JS_PUBLIC_API bool JS::GetScriptTranscodingBuildId(
    JS::BuildIdCharVector* buildId) {
  MOZ_ASSERT(buildId->empty());
  MOZ_ASSERT(GetBuildId);

  if (!GetBuildId(buildId)) {
    return false;
  }

  // Note: the buildId returned here is also used for the bytecode cache MIME
  // type so use plain ASCII characters.

  if (!buildId->reserve(buildId->length() + 4)) {
    return false;
  }

  buildId->infallibleAppend('-');

  // XDR depends on pointer size and endianness.
  static_assert(sizeof(uintptr_t) == 4 || sizeof(uintptr_t) == 8);
  buildId->infallibleAppend(sizeof(uintptr_t) == 4 ? '4' : '8');
  buildId->infallibleAppend(MOZ_LITTLE_ENDIAN() ? 'l' : 'b');

  return true;
}

template <XDRMode mode>
static XDRResult VersionCheck(XDRState<mode>* xdr) {
  JS::BuildIdCharVector buildId;
  if (!JS::GetScriptTranscodingBuildId(&buildId)) {
    ReportOutOfMemory(xdr->cx());
    return xdr->fail(JS::TranscodeResult::Throw);
  }
  MOZ_ASSERT(!buildId.empty());

  uint32_t buildIdLength;
  if (mode == XDR_ENCODE) {
    buildIdLength = buildId.length();
  }

  MOZ_TRY(xdr->codeUint32(&buildIdLength));

  if (mode == XDR_DECODE && buildIdLength != buildId.length()) {
    return xdr->fail(JS::TranscodeResult::Failure_BadBuildId);
  }

  if (mode == XDR_ENCODE) {
    MOZ_TRY(xdr->codeBytes(buildId.begin(), buildIdLength));
  } else {
    JS::BuildIdCharVector decodedBuildId;

    // buildIdLength is already checked against the length of current
    // buildId.
    if (!decodedBuildId.resize(buildIdLength)) {
      ReportOutOfMemory(xdr->cx());
      return xdr->fail(JS::TranscodeResult::Throw);
    }

    MOZ_TRY(xdr->codeBytes(decodedBuildId.begin(), buildIdLength));

    // We do not provide binary compatibility with older scripts.
    if (!ArrayEqual(decodedBuildId.begin(), buildId.begin(), buildIdLength)) {
      return xdr->fail(JS::TranscodeResult::Failure_BadBuildId);
    }
  }

  return Ok();
}

template <XDRMode mode>
static XDRResult XDRStencilHeader(XDRState<mode>* xdr,
                                  const JS::DecodeOptions* maybeOptions,
                                  RefPtr<ScriptSource>& source) {
  // The XDR-Stencil header is inserted at beginning of buffer, but it is
  // computed at the end the incremental-encoding process.

  MOZ_TRY(VersionCheck(xdr));
  MOZ_TRY(ScriptSource::XDR(xdr, maybeOptions, source));

  return Ok();
}

template class js::XDRState<XDR_ENCODE>;
template class js::XDRState<XDR_DECODE>;

XDRResult XDRStencilEncoder::codeStencil(
    const RefPtr<ScriptSource>& source,
    const frontend::CompilationStencil& stencil) {
#ifdef DEBUG
  auto sanityCheck = mozilla::MakeScopeExit(
      [&] { MOZ_ASSERT(validateResultCode(cx(), resultCode())); });
#endif

  MOZ_TRY(frontend::StencilXDR::checkCompilationStencil(this, stencil));

  MOZ_TRY(XDRStencilHeader(this, nullptr,
                           const_cast<RefPtr<ScriptSource>&>(source)));
  MOZ_TRY(frontend::StencilXDR::codeCompilationStencil(
      this, const_cast<frontend::CompilationStencil&>(stencil)));

  return Ok();
}

XDRResult XDRStencilEncoder::codeStencil(
    const frontend::CompilationStencil& stencil) {
  return codeStencil(stencil.source, stencil);
}

XDRIncrementalStencilEncoder::~XDRIncrementalStencilEncoder() {
  if (merger_) {
    js_delete(merger_);
  }
}

XDRResult XDRIncrementalStencilEncoder::setInitial(
    JSContext* cx,
    UniquePtr<frontend::ExtensibleCompilationStencil>&& initial) {
  MOZ_TRY(frontend::StencilXDR::checkCompilationStencil(*initial));

  merger_ = cx->new_<frontend::CompilationStencilMerger>();
  if (!merger_) {
    return mozilla::Err(JS::TranscodeResult::Throw);
  }

  if (!merger_->setInitial(
          cx, std::forward<UniquePtr<frontend::ExtensibleCompilationStencil>>(
                  initial))) {
    return mozilla::Err(JS::TranscodeResult::Throw);
  }

  return Ok();
}

XDRResult XDRIncrementalStencilEncoder::addDelazification(
    JSContext* cx, const frontend::CompilationStencil& delazification) {
  if (!merger_->addDelazification(cx, delazification)) {
    return mozilla::Err(JS::TranscodeResult::Throw);
  }

  return Ok();
}

XDRResult XDRIncrementalStencilEncoder::linearize(JSContext* cx,
                                                  JS::TranscodeBuffer& buffer,
                                                  ScriptSource* ss) {
  XDRStencilEncoder encoder(cx, buffer);
  RefPtr<ScriptSource> source(ss);
  {
    frontend::BorrowingCompilationStencil borrowingStencil(
        merger_->getResult());
    MOZ_TRY(encoder.codeStencil(source, borrowingStencil));
  }

  return Ok();
}

XDRResult XDRStencilDecoder::codeStencil(
    const JS::DecodeOptions& options, frontend::CompilationStencil& stencil) {
#ifdef DEBUG
  auto sanityCheck = mozilla::MakeScopeExit(
      [&] { MOZ_ASSERT(validateResultCode(cx(), resultCode())); });
#endif

  auto resetOptions = mozilla::MakeScopeExit([&] { options_ = nullptr; });
  options_ = &options;

  MOZ_TRY(XDRStencilHeader(this, &options, stencil.source));
  MOZ_TRY(frontend::StencilXDR::codeCompilationStencil(this, stencil));

  return Ok();
}
