/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(MFTEncoder_h_)
#  define MFTEncoder_h_

#  include <functional>
#  include "mozilla/RefPtr.h"
#  include "nsISupportsImpl.h"
#  include "nsDeque.h"
#  include "WMF.h"

namespace mozilla {

class MFTEncoder final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MFTEncoder)

  HRESULT Create(const GUID& aSubtype);
  HRESULT Destroy();
  HRESULT SetMediaTypes(IMFMediaType* aInputType, IMFMediaType* aOutputType);
  HRESULT SetModes(UINT32 aBitsPerSec);
  HRESULT SetBitrate(UINT32 aBitsPerSec);

  HRESULT CreateInputSample(RefPtr<IMFSample>* aSample, size_t aSize);
  HRESULT PushInput(RefPtr<IMFSample>&& aInput);
  HRESULT TakeOutput(nsTArray<RefPtr<IMFSample>>& aOutput);
  HRESULT Drain(nsTArray<RefPtr<IMFSample>>& aOutput);

  HRESULT GetMPEGSequenceHeader(nsTArray<UINT8>& aHeader);

  static nsCString GetFriendlyName(const GUID& aSubtype);

  struct Info final {
    GUID mSubtype;
    nsCString mName;
  };

 private:
  ~MFTEncoder() { Destroy(); };

  static nsTArray<Info>& Infos();
  static nsTArray<Info> Enumerate();
  static Maybe<Info> GetInfo(const GUID& aSubtype);

  already_AddRefed<IMFActivate> CreateFactory(const GUID& aSubtype);
  HRESULT EnableAsync();
  HRESULT GetStreamIDs();
  GUID MatchInputSubtype(IMFMediaType* aInputType);
  HRESULT SendMFTMessage(MFT_MESSAGE_TYPE aMsg, ULONG_PTR aData);

  HRESULT ProcessEvents();
  HRESULT ProcessInput();
  HRESULT ProcessOutput();

  RefPtr<IMFTransform> mEncoder;
  // For MFT object creation. See
  // https://docs.microsoft.com/en-us/windows/win32/medfound/activation-objects
  RefPtr<IMFActivate> mFactory;
  // For encoder configuration. See
  // https://docs.microsoft.com/en-us/windows/win32/directshow/encoder-api
  RefPtr<ICodecAPI> mConfig;
  // For async MFT events. See
  // https://docs.microsoft.com/en-us/windows/win32/medfound/asynchronous-mfts#events
  RefPtr<IMFMediaEventGenerator> mEventSource;

  DWORD mInputStreamID;
  DWORD mOutputStreamID;
  MFT_INPUT_STREAM_INFO mInputStreamInfo;
  MFT_OUTPUT_STREAM_INFO mOutputStreamInfo;
  bool mOutputStreamProvidesSample;

  size_t mNumNeedInput;
  enum class DrainState { DRAINED, DRAINABLE, DRAINING };
  DrainState mDrainState = DrainState::DRAINABLE;

  nsRefPtrDeque<IMFSample> mPendingInputs;
  nsTArray<RefPtr<IMFSample>> mOutputs;
};

}  // namespace mozilla

#endif
