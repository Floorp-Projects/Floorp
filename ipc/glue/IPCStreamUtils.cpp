/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCStreamUtils.h"

#include "ipc/IPCMessageUtilsSpecializations.h"

#include "nsIHttpHeaderVisitor.h"
#include "nsIIPCSerializableInputStream.h"
#include "mozIRemoteLazyInputStream.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/File.h"
#include "mozilla/ipc/IPCStream.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/InputStreamLengthHelper.h"
#include "mozilla/RemoteLazyInputStreamParent.h"
#include "mozilla/Unused.h"
#include "nsIMIMEInputStream.h"
#include "nsNetCID.h"

using namespace mozilla::dom;

namespace mozilla::ipc {

namespace {

class MIMEStreamHeaderVisitor final : public nsIHttpHeaderVisitor {
 public:
  explicit MIMEStreamHeaderVisitor(
      nsTArray<mozilla::ipc::HeaderEntry>& aHeaders)
      : mHeaders(aHeaders) {}

  NS_DECL_ISUPPORTS
  NS_IMETHOD VisitHeader(const nsACString& aName,
                         const nsACString& aValue) override {
    auto el = mHeaders.AppendElement();
    el->name() = aName;
    el->value() = aValue;
    return NS_OK;
  }

 private:
  ~MIMEStreamHeaderVisitor() = default;

  nsTArray<mozilla::ipc::HeaderEntry>& mHeaders;
};

NS_IMPL_ISUPPORTS(MIMEStreamHeaderVisitor, nsIHttpHeaderVisitor)

static bool SerializeLazyInputStream(nsIInputStream* aStream,
                                     IPCStream& aValue) {
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(XRE_IsParentProcess());

  // If we're serializing a MIME stream, ensure we preserve header data which
  // would not be preserved by a RemoteLazyInputStream wrapper.
  if (nsCOMPtr<nsIMIMEInputStream> mimeStream = do_QueryInterface(aStream)) {
    MIMEInputStreamParams params;
    params.startedReading() = false;

    nsCOMPtr<nsIHttpHeaderVisitor> visitor =
        new MIMEStreamHeaderVisitor(params.headers());
    if (NS_WARN_IF(NS_FAILED(mimeStream->VisitHeaders(visitor)))) {
      return false;
    }

    nsCOMPtr<nsIInputStream> dataStream;
    if (NS_FAILED(mimeStream->GetData(getter_AddRefs(dataStream)))) {
      return false;
    }
    if (dataStream) {
      IPCStream data;
      if (!SerializeLazyInputStream(dataStream, data)) {
        return false;
      }
      params.optionalStream().emplace(std::move(data.stream()));
    }

    aValue.stream() = std::move(params);
    return true;
  }

  RefPtr<RemoteLazyInputStream> lazyStream =
      RemoteLazyInputStream::WrapStream(aStream);
  if (NS_WARN_IF(!lazyStream)) {
    return false;
  }

  aValue.stream() = RemoteLazyInputStreamParams(WrapNotNull(lazyStream));

  return true;
}

}  // anonymous namespace

bool SerializeIPCStream(already_AddRefed<nsIInputStream> aInputStream,
                        IPCStream& aValue, bool aAllowLazy) {
  nsCOMPtr<nsIInputStream> stream(std::move(aInputStream));
  if (!stream) {
    MOZ_ASSERT_UNREACHABLE(
        "Use the Maybe<...> overload to serialize optional nsIInputStreams");
    return false;
  }

  // When requesting a delayed start stream from the parent process, serialize
  // it as a remote lazy stream to avoid bloating payloads.
  if (aAllowLazy && XRE_IsParentProcess()) {
    return SerializeLazyInputStream(stream, aValue);
  }

  if (nsCOMPtr<nsIIPCSerializableInputStream> serializable =
          do_QueryInterface(stream)) {
    // If you change this size, please also update the payload size in
    // test_reload_large_postdata.html.
    const uint64_t kTooLargeStream = 1024 * 1024;

    uint32_t sizeUsed = 0;
    serializable->Serialize(aValue.stream(), kTooLargeStream, &sizeUsed);

    MOZ_ASSERT(sizeUsed <= kTooLargeStream);

    if (aValue.stream().type() == InputStreamParams::T__None) {
      MOZ_CRASH("Serialize failed!");
    }
    return true;
  }

  InputStreamHelper::SerializeInputStreamAsPipe(stream, aValue.stream());
  if (aValue.stream().type() == InputStreamParams::T__None) {
    MOZ_ASSERT_UNREACHABLE("Serializing as a pipe failed");
    return false;
  }
  return true;
}

bool SerializeIPCStream(already_AddRefed<nsIInputStream> aInputStream,
                        Maybe<IPCStream>& aValue, bool aAllowLazy) {
  nsCOMPtr<nsIInputStream> stream(std::move(aInputStream));
  if (!stream) {
    aValue.reset();
    return true;
  }

  IPCStream value;
  if (SerializeIPCStream(stream.forget(), value, aAllowLazy)) {
    aValue = Some(value);
    return true;
  }
  return false;
}

already_AddRefed<nsIInputStream> DeserializeIPCStream(const IPCStream& aValue) {
  return InputStreamHelper::DeserializeInputStream(aValue.stream());
}

already_AddRefed<nsIInputStream> DeserializeIPCStream(
    const Maybe<IPCStream>& aValue) {
  if (aValue.isNothing()) {
    return nullptr;
  }

  return DeserializeIPCStream(aValue.ref());
}

}  // namespace mozilla::ipc

void IPC::ParamTraits<nsIInputStream*>::Write(IPC::MessageWriter* aWriter,
                                              nsIInputStream* aParam) {
  mozilla::Maybe<mozilla::ipc::IPCStream> stream;
  if (!mozilla::ipc::SerializeIPCStream(do_AddRef(aParam), stream,
                                        /* aAllowLazy */ true)) {
    MOZ_CRASH("Failed to serialize nsIInputStream");
  }

  WriteParam(aWriter, stream);
}

bool IPC::ParamTraits<nsIInputStream*>::Read(IPC::MessageReader* aReader,
                                             RefPtr<nsIInputStream>* aResult) {
  mozilla::Maybe<mozilla::ipc::IPCStream> ipcStream;
  if (!ReadParam(aReader, &ipcStream)) {
    return false;
  }

  *aResult = mozilla::ipc::DeserializeIPCStream(ipcStream);
  return true;
}
