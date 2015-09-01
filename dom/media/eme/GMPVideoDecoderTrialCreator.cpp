/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GMPVideoDecoderTrialCreator.h"
#include "mozilla/Preferences.h"
#include "prsystem.h"
#include "GMPVideoHost.h"
#include "mozilla/EMEUtils.h"
#include "nsServiceManagerUtils.h"
#include "GMPService.h"
#include "VideoUtils.h"
#include "nsPrintfCString.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace dom {

static already_AddRefed<nsIThread>
GetGMPThread()
{
  nsCOMPtr<mozIGeckoMediaPluginService> gmps =
    do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (!gmps) {
    return nullptr;
  }
  nsCOMPtr<nsIThread> gmpThread;
  nsresult rv = gmps->GetThread(getter_AddRefs(gmpThread));
  if (NS_FAILED(rv)) {
    return nullptr;
  }
  return gmpThread.forget();
}

#ifdef DEBUG
static bool
OnGMPThread()
{
  nsCOMPtr<nsIThread> currentThread;
  NS_GetCurrentThread(getter_AddRefs(currentThread));
  nsCOMPtr<nsIThread> gmpThread(GetGMPThread());
  return !!gmpThread && !!currentThread && gmpThread == currentThread;
}
#endif

static const char*
TrialCreatePrefName(const nsAString& aKeySystem)
{
  if (aKeySystem.EqualsLiteral("com.adobe.primetime")) {
    return "media.gmp-eme-adobe.trial-create";
  }
  if (aKeySystem.EqualsLiteral("org.w3.clearkey")) {
    return "media.gmp-eme-clearkey.trial-create";
  }
  return nullptr;
}

/* static */
GMPVideoDecoderTrialCreator::TrialCreateState
GMPVideoDecoderTrialCreator::GetCreateTrialState(const nsAString& aKeySystem)
{
  if (Preferences::GetBool("media.gmp.always-trial-create", false)) {
    return Pending;
  }

  const char* pref = TrialCreatePrefName(aKeySystem);
  if (!pref) {
    return Pending;
  }
  switch (Preferences::GetInt(pref, (int)Pending)) {
    case 0: return Pending;
    case 1: return Succeeded;
    case 2: return Failed;
    default: return Pending;
  }
}

void
GMPVideoDecoderTrialCreator::TrialCreateGMPVideoDecoderFailed(const nsAString& aKeySystem,
                                                              const nsACString& aReason)
{
  MOZ_ASSERT(NS_IsMainThread());

  EME_LOG("GMPVideoDecoderTrialCreator::TrialCreateGMPVideoDecoderFailed(%s)",
          NS_ConvertUTF16toUTF8(aKeySystem).get());

  TrialCreateData* data = mTestCreate.Get(aKeySystem);
  if (!data) {
    return;
  }
  data->mStatus = Failed;
  const char* pref = TrialCreatePrefName(aKeySystem);
  if (pref) {
    Preferences::SetInt(pref, (int)Failed);
  }
  for (nsRefPtr<AbstractPromiseLike>& promise: data->mPending) {
    promise->Reject(NS_ERROR_DOM_NOT_SUPPORTED_ERR, aReason);
  }
  data->mPending.Clear();
  data->mTest = nullptr;
}

void
GMPVideoDecoderTrialCreator::TrialCreateGMPVideoDecoderSucceeded(const nsAString& aKeySystem)
{
  MOZ_ASSERT(NS_IsMainThread());

  EME_LOG("GMPVideoDecoderTrialCreator::TrialCreateGMPVideoDecoderSucceeded(%s)",
    NS_ConvertUTF16toUTF8(aKeySystem).get());

  TrialCreateData* data = mTestCreate.Get(aKeySystem);
  if (!data) {
    return;
  }
  data->mStatus = Succeeded;
  const char* pref = TrialCreatePrefName(aKeySystem);
  if (pref) {
    Preferences::SetInt(pref, (int)Succeeded);
  }
  for (nsRefPtr<AbstractPromiseLike>& promise : data->mPending) {
    promise->Resolve();
  }
  data->mPending.Clear();
  data->mTest = nullptr;
}

nsresult
TestGMPVideoDecoder::Start()
{
  MOZ_ASSERT(NS_IsMainThread());

  mGMPService = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (!mGMPService) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIThread> thread(GetGMPThread());
  if (!thread) {
    return NS_ERROR_FAILURE;
  }
  nsRefPtr<nsIRunnable> task(NS_NewRunnableMethod(this, &TestGMPVideoDecoder::CreateGMPVideoDecoder));
  return thread->Dispatch(task, NS_DISPATCH_NORMAL);
}

struct ExpectedPlaneDecodePlane {
  GMPPlaneType mPlane;
  size_t mLength;
  uint8_t mValue;
  int32_t mSize; // width & height
};

static const ExpectedPlaneDecodePlane sExpectedPlanes[3] = {
  {
    kGMPYPlane,
    112 * 112, // 12544
    0x4c,
    112
  },
  { // U
    kGMPUPlane,
    56 * 56, // 3136
    0x55,
    56,
  },
  { // V
    kGMPVPlane,
    56 * 56, // 3136
    0xff,
    56,
  }
};

static bool TestDecodedFrame(GMPVideoi420Frame* aDecodedFrame)
{
  MOZ_ASSERT(OnGMPThread());

  if (aDecodedFrame->Width() != 112 || aDecodedFrame->Height() != 112) {
    EME_LOG("TestDecodedFrame() - Invalid decoded frame dimensions");
    return false;
  }
  for (const ExpectedPlaneDecodePlane& plane : sExpectedPlanes) {
    int32_t stride = aDecodedFrame->Stride(plane.mPlane);
    if (stride < plane.mSize) {
      EME_LOG("TestDecodedFrame() - Insufficient decoded frame stride");
      return false;
    }
    int32_t length = plane.mSize * plane.mSize;
    if (aDecodedFrame->AllocatedSize(plane.mPlane) < length) {
      EME_LOG("TestDecodedFrame() - Insufficient decoded frame allocated size");
      return false;
    }
    const uint8_t* data = aDecodedFrame->Buffer(plane.mPlane);
    for (int32_t row = 0; row < plane.mSize; row++) {
      for (int32_t i = 0; i < plane.mSize; i++) {
        size_t off = (stride * row) + i;
        if (data[off] != plane.mValue) {
          EME_LOG("TestDecodedFrame() - Invalid decoded frame contents");
          return false;
        }
      }
    }
  }
  return true;
}

void
TestGMPVideoDecoder::Decoded(GMPVideoi420Frame* aDecodedFrame)
{
  MOZ_ASSERT(OnGMPThread());

  if (!mReceivedDecoded) {
    mReceivedDecoded = true;
  } else {
    EME_LOG("Received multiple decoded frames");
    ReportFailure(NS_LITERAL_CSTRING("TestGMPVideoDecoder received multiple decoded frames"));
    return;
  }

  GMPUniquePtr<GMPVideoi420Frame> decodedFrame(aDecodedFrame);
  if (!TestDecodedFrame(aDecodedFrame)) {
    EME_LOG("decoded frame failed verification");
    ReportFailure(NS_LITERAL_CSTRING("TestGMPVideoDecoder decoded frame failed verification"));
  }
}

void
TestGMPVideoDecoder::DrainComplete()
{
  EME_LOG("TestGMPVideoDecoder::DrainComplete()");
  MOZ_ASSERT(OnGMPThread());
  ReportSuccess();
}

void
TestGMPVideoDecoder::Error(GMPErr aErr)
{
  EME_LOG("TestGMPVideoDecoder::ReceivedDecodedFrame()");
  MOZ_ASSERT(OnGMPThread());
  ReportFailure(nsPrintfCString("TestGMPVideoDecoder error %d", aErr));
}

void
TestGMPVideoDecoder::Terminated()
{
  EME_LOG("TestGMPVideoDecoder::Terminated()");
  MOZ_ASSERT(OnGMPThread());
  ReportFailure(NS_LITERAL_CSTRING("TestGMPVideoDecoder GMP terminated"));
}

void
TestGMPVideoDecoder::ReportFailure(const nsACString& aReason)
{
  MOZ_ASSERT(OnGMPThread());

  if (mGMP) {
    mGMP->Close();
    mGMP = nullptr;
  }

  nsRefPtr<nsIRunnable> task;
  task = NS_NewRunnableMethodWithArgs<nsString, nsCString>(mInstance,
    &GMPVideoDecoderTrialCreator::TrialCreateGMPVideoDecoderFailed,
    mKeySystem,
    aReason);
  NS_DispatchToMainThread(task, NS_DISPATCH_NORMAL);
}

void
TestGMPVideoDecoder::ReportSuccess()
{
  MOZ_ASSERT(OnGMPThread());

  if (mGMP) {
    mGMP->Close();
    mGMP = nullptr;
  }

  nsRefPtr<nsIRunnable> task;
  task = NS_NewRunnableMethodWithArg<nsString>(mInstance,
    &GMPVideoDecoderTrialCreator::TrialCreateGMPVideoDecoderSucceeded,
    mKeySystem);
  NS_DispatchToMainThread(task, NS_DISPATCH_NORMAL);
}

// A solid red, 112x112 frame. Display size 100x100 pixels.
// Generated with ImageMagick/ffmpeg
// $ convert -size 100x100 xc:rgb\(255, 0, 0\) red.png
// $ ffmpeg -f image2 -i red.png -r 24 -b:v 200k -c:v libx264 -profile:v baseline -level 1 -v:r 24 red.mp4 -y
static const uint8_t sTestH264Frame[] = {
  0x00, 0x00, 0x02, 0x81, 0x06, 0x05, 0xff, 0xff, 0x7d, 0xdc, 0x45, 0xe9, 0xbd, 0xe6, 0xd9,
  0x48, 0xb7, 0x96, 0x2c, 0xd8, 0x20, 0xd9, 0x23, 0xee, 0xef, 0x78, 0x32, 0x36, 0x34, 0x20,
  0x2d, 0x20, 0x63, 0x6f, 0x72, 0x65, 0x20, 0x31, 0x33, 0x35, 0x20, 0x72, 0x32, 0x33, 0x34,
  0x35, 0x20, 0x66, 0x30, 0x63, 0x31, 0x63, 0x35, 0x33, 0x20, 0x2d, 0x20, 0x48, 0x2e, 0x32,
  0x36, 0x34, 0x2f, 0x4d, 0x50, 0x45, 0x47, 0x2d, 0x34, 0x20, 0x41, 0x56, 0x43, 0x20, 0x63,
  0x6f, 0x64, 0x65, 0x63, 0x20, 0x2d, 0x20, 0x43, 0x6f, 0x70, 0x79, 0x6c, 0x65, 0x66, 0x74,
  0x20, 0x32, 0x30, 0x30, 0x33, 0x2d, 0x32, 0x30, 0x31, 0x33, 0x20, 0x2d, 0x20, 0x68, 0x74,
  0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x77, 0x77, 0x77, 0x2e, 0x76, 0x69, 0x64, 0x65, 0x6f, 0x6c,
  0x61, 0x6e, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x78, 0x32, 0x36, 0x34, 0x2e, 0x68, 0x74, 0x6d,
  0x6c, 0x20, 0x2d, 0x20, 0x6f, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x3a, 0x20, 0x63, 0x61,
  0x62, 0x61, 0x63, 0x3d, 0x30, 0x20, 0x72, 0x65, 0x66, 0x3d, 0x33, 0x20, 0x64, 0x65, 0x62,
  0x6c, 0x6f, 0x63, 0x6b, 0x3d, 0x31, 0x3a, 0x30, 0x3a, 0x30, 0x20, 0x61, 0x6e, 0x61, 0x6c,
  0x79, 0x73, 0x65, 0x3d, 0x30, 0x78, 0x31, 0x3a, 0x30, 0x78, 0x31, 0x31, 0x31, 0x20, 0x6d,
  0x65, 0x3d, 0x68, 0x65, 0x78, 0x20, 0x73, 0x75, 0x62, 0x6d, 0x65, 0x3d, 0x37, 0x20, 0x70,
  0x73, 0x79, 0x3d, 0x31, 0x20, 0x70, 0x73, 0x79, 0x5f, 0x72, 0x64, 0x3d, 0x31, 0x2e, 0x30,
  0x30, 0x3a, 0x30, 0x2e, 0x30, 0x30, 0x20, 0x6d, 0x69, 0x78, 0x65, 0x64, 0x5f, 0x72, 0x65,
  0x66, 0x3d, 0x31, 0x20, 0x6d, 0x65, 0x5f, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x3d, 0x31, 0x36,
  0x20, 0x63, 0x68, 0x72, 0x6f, 0x6d, 0x61, 0x5f, 0x6d, 0x65, 0x3d, 0x31, 0x20, 0x74, 0x72,
  0x65, 0x6c, 0x6c, 0x69, 0x73, 0x3d, 0x31, 0x20, 0x38, 0x78, 0x38, 0x64, 0x63, 0x74, 0x3d,
  0x30, 0x20, 0x63, 0x71, 0x6d, 0x3d, 0x30, 0x20, 0x64, 0x65, 0x61, 0x64, 0x7a, 0x6f, 0x6e,
  0x65, 0x3d, 0x32, 0x31, 0x2c, 0x31, 0x31, 0x20, 0x66, 0x61, 0x73, 0x74, 0x5f, 0x70, 0x73,
  0x6b, 0x69, 0x70, 0x3d, 0x31, 0x20, 0x63, 0x68, 0x72, 0x6f, 0x6d, 0x61, 0x5f, 0x71, 0x70,
  0x5f, 0x6f, 0x66, 0x66, 0x73, 0x65, 0x74, 0x3d, 0x2d, 0x32, 0x20, 0x74, 0x68, 0x72, 0x65,
  0x61, 0x64, 0x73, 0x3d, 0x31, 0x32, 0x20, 0x6c, 0x6f, 0x6f, 0x6b, 0x61, 0x68, 0x65, 0x61,
  0x64, 0x5f, 0x74, 0x68, 0x72, 0x65, 0x61, 0x64, 0x73, 0x3d, 0x31, 0x20, 0x73, 0x6c, 0x69,
  0x63, 0x65, 0x64, 0x5f, 0x74, 0x68, 0x72, 0x65, 0x61, 0x64, 0x73, 0x3d, 0x30, 0x20, 0x6e,
  0x72, 0x3d, 0x30, 0x20, 0x64, 0x65, 0x63, 0x69, 0x6d, 0x61, 0x74, 0x65, 0x3d, 0x31, 0x20,
  0x69, 0x6e, 0x74, 0x65, 0x72, 0x6c, 0x61, 0x63, 0x65, 0x64, 0x3d, 0x30, 0x20, 0x62, 0x6c,
  0x75, 0x72, 0x61, 0x79, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x61, 0x74, 0x3d, 0x30, 0x20, 0x63,
  0x6f, 0x6e, 0x73, 0x74, 0x72, 0x61, 0x69, 0x6e, 0x65, 0x64, 0x5f, 0x69, 0x6e, 0x74, 0x72,
  0x61, 0x3d, 0x30, 0x20, 0x62, 0x66, 0x72, 0x61, 0x6d, 0x65, 0x73, 0x3d, 0x30, 0x20, 0x77,
  0x65, 0x69, 0x67, 0x68, 0x74, 0x70, 0x3d, 0x30, 0x20, 0x6b, 0x65, 0x79, 0x69, 0x6e, 0x74,
  0x3d, 0x32, 0x35, 0x30, 0x20, 0x6b, 0x65, 0x79, 0x69, 0x6e, 0x74, 0x5f, 0x6d, 0x69, 0x6e,
  0x3d, 0x32, 0x34, 0x20, 0x73, 0x63, 0x65, 0x6e, 0x65, 0x63, 0x75, 0x74, 0x3d, 0x34, 0x30,
  0x20, 0x69, 0x6e, 0x74, 0x72, 0x61, 0x5f, 0x72, 0x65, 0x66, 0x72, 0x65, 0x73, 0x68, 0x3d,
  0x30, 0x20, 0x72, 0x63, 0x5f, 0x6c, 0x6f, 0x6f, 0x6b, 0x61, 0x68, 0x65, 0x61, 0x64, 0x3d,
  0x34, 0x30, 0x20, 0x72, 0x63, 0x3d, 0x61, 0x62, 0x72, 0x20, 0x6d, 0x62, 0x74, 0x72, 0x65,
  0x65, 0x3d, 0x31, 0x20, 0x62, 0x69, 0x74, 0x72, 0x61, 0x74, 0x65, 0x3d, 0x32, 0x30, 0x30,
  0x20, 0x72, 0x61, 0x74, 0x65, 0x74, 0x6f, 0x6c, 0x3d, 0x31, 0x2e, 0x30, 0x20, 0x71, 0x63,
  0x6f, 0x6d, 0x70, 0x3d, 0x30, 0x2e, 0x36, 0x30, 0x20, 0x71, 0x70, 0x6d, 0x69, 0x6e, 0x3d,
  0x30, 0x20, 0x71, 0x70, 0x6d, 0x61, 0x78, 0x3d, 0x36, 0x39, 0x20, 0x71, 0x70, 0x73, 0x74,
  0x65, 0x70, 0x3d, 0x34, 0x20, 0x69, 0x70, 0x5f, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x3d, 0x31,
  0x2e, 0x34, 0x30, 0x20, 0x61, 0x71, 0x3d, 0x31, 0x3a, 0x31, 0x2e, 0x30, 0x30, 0x00, 0x80,
  0x00, 0x00, 0x00, 0x39, 0x65, 0x88, 0x84, 0x0c, 0xf1, 0x18, 0xa0, 0x00, 0x23, 0xbf, 0x1c,
  0x00, 0x04, 0x3c, 0x63, 0x80, 0x00, 0x98, 0x44, 0x9c, 0x9c, 0x9c, 0x9c, 0x9c, 0x9d, 0x75,
  0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75,
  0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75,
  0xe0
};

static GMPUniquePtr<GMPVideoEncodedFrame>
CreateFrame(GMPVideoHost* aHost)
{
  MOZ_ASSERT(OnGMPThread());

  GMPVideoFrame* ftmp = nullptr;
  GMPErr err = aHost->CreateFrame(kGMPEncodedVideoFrame, &ftmp);
  if (GMP_FAILED(err)) {
    return nullptr;
  }

  GMPUniquePtr<GMPVideoEncodedFrame> frame(static_cast<GMPVideoEncodedFrame*>(ftmp));
  err = frame->CreateEmptyFrame(MOZ_ARRAY_LENGTH(sTestH264Frame));
  if (GMP_FAILED(err)) {
    return nullptr;
  }

  memcpy(frame->Buffer(), sTestH264Frame, MOZ_ARRAY_LENGTH(sTestH264Frame));
  frame->SetBufferType(GMP_BufferLength32);

  frame->SetEncodedWidth(100);
  frame->SetEncodedHeight(100);
  frame->SetTimeStamp(0);
  frame->SetCompleteFrame(true);
  frame->SetDuration(41666);
  frame->SetFrameType(kGMPKeyFrame);

  return frame;
}

static const uint8_t sTestH264CodecSpecific[] = {
  0x01, 0x42, 0xc0, 0x0a, 0xff, 0xe1, 0x00, 0x18, 0x67, 0x42, 0xc0, 0x0a, 0xd9, 0x07, 0x3f,
  0x9e, 0x79, 0xb2, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x03, 0x00, 0x60, 0x1e, 0x24,
  0x4c, 0x90, 0x01, 0x00, 0x04, 0x68, 0xcb, 0x8c, 0xb2
};

void
TestGMPVideoDecoder::Callback::Done(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost)
{
  MOZ_ASSERT(OnGMPThread());

  if (!aHost || !aGMP) {
    mInstance->ReportFailure(NS_LITERAL_CSTRING("TestGMPVideoDecoder null host or GMP on Get"));
    return;
  }

  nsRefPtr<nsIRunnable> task;
  task = NS_NewRunnableMethodWithArgs<GMPVideoDecoderProxy*, GMPVideoHost*>(mInstance,
    &TestGMPVideoDecoder::ActorCreated,
    aGMP, aHost);
  NS_DispatchToMainThread(task, NS_DISPATCH_NORMAL);
}

void
TestGMPVideoDecoder::ActorCreated(GMPVideoDecoderProxy* aGMP,
                                  GMPVideoHost* aHost)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aHost && aGMP);

  // Add crash handler.
  nsRefPtr<gmp::GeckoMediaPluginService> service =
    gmp::GeckoMediaPluginService::GetGeckoMediaPluginService();
  service->AddPluginCrashedEventTarget(aGMP->GetPluginId(), mWindow);

  nsCOMPtr<nsIThread> thread(GetGMPThread());
  if (!thread) {
    mInstance->TrialCreateGMPVideoDecoderFailed(mKeySystem,
      NS_LITERAL_CSTRING("Failed to get GMP thread in TestGMPVideoDecoder::ActorCreated"));
    return;
  }

  nsRefPtr<nsIRunnable> task;
  task = NS_NewRunnableMethodWithArgs<GMPVideoDecoderProxy*, GMPVideoHost*>(this,
    &TestGMPVideoDecoder::InitGMPDone,
    aGMP, aHost);
  thread->Dispatch(task, NS_DISPATCH_NORMAL);
}

void
TestGMPVideoDecoder::InitGMPDone(GMPVideoDecoderProxy* aGMP,
                                 GMPVideoHost* aHost)
{
  MOZ_ASSERT(OnGMPThread());
  MOZ_ASSERT(aHost && aGMP);

  mGMP = aGMP;
  mHost = aHost;

  GMPVideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.mGMPApiVersion = kGMPVersion33;

  codec.mCodecType = kGMPVideoCodecH264;
  codec.mWidth = 100;
  codec.mHeight = 100;

  nsTArray<uint8_t> codecSpecific;
  codecSpecific.AppendElement(0); // mPacketizationMode.
  codecSpecific.AppendElements(sTestH264CodecSpecific,
                               MOZ_ARRAY_LENGTH(sTestH264CodecSpecific));

  nsresult rv = mGMP->InitDecode(codec,
                                 codecSpecific,
                                 this,
                                 PR_GetNumberOfProcessors());
  if (NS_FAILED(rv)) {
    EME_LOG("InitGMPDone() - InitDecode() failed!");
    ReportFailure(NS_LITERAL_CSTRING("TestGMPVideoDecoder InitDecode() returned failure"));
    return;
  }

  GMPUniquePtr<GMPVideoEncodedFrame> frame = CreateFrame(aHost);
  if (!frame) {
    EME_LOG("InitGMPDone() - Decode() failed to create frame!");
    ReportFailure(NS_LITERAL_CSTRING("TestGMPVideoDecoder Decode() failed to create frame"));
    return;
  }
  nsTArray<uint8_t> info; // No codec specific per-frame info to pass.
  rv = mGMP->Decode(Move(frame), false, info, 0);
  if (NS_FAILED(rv)) {
    EME_LOG("InitGMPDone() - Decode() failed to send Decode message!");
    ReportFailure(NS_LITERAL_CSTRING("TestGMPVideoDecoder Decode() returned failure"));
    return;
  }

  rv = mGMP->Drain();
  if (NS_FAILED(rv)) {
    EME_LOG("InitGMPDone() - Drain() failed to send Drain message!");
    ReportFailure(NS_LITERAL_CSTRING("TestGMPVideoDecoder Drain() returned failure"));
    return;
  }
}

void
TestGMPVideoDecoder::CreateGMPVideoDecoder()
{
  MOZ_ASSERT(OnGMPThread());

  nsTArray<nsCString> tags;
  tags.AppendElement(NS_LITERAL_CSTRING("h264"));
  tags.AppendElement(NS_ConvertUTF16toUTF8(mKeySystem));

  UniquePtr<GetGMPVideoDecoderCallback> callback(new Callback(this));
  if (NS_FAILED(mGMPService->GetGMPVideoDecoder(&tags,
                                                NS_LITERAL_CSTRING("fakeNodeId1234567890fakeNodeId12"),
                                                Move(callback)))) {
    ReportFailure(NS_LITERAL_CSTRING("TestGMPVideoDecoder GMPService GetGMPVideoDecoder returned failure"));
  }
}

void
GMPVideoDecoderTrialCreator::MaybeAwaitTrialCreate(const nsAString& aKeySystem,
                                                   AbstractPromiseLike* aPromisey,
                                                   nsPIDOMWindow* aParent)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mTestCreate.Contains(aKeySystem)) {
    mTestCreate.Put(aKeySystem, new TrialCreateData(aKeySystem));
  }
  TrialCreateData* data = mTestCreate.Get(aKeySystem);
  MOZ_ASSERT(data);

  switch (data->mStatus) {
    case TrialCreateState::Succeeded: {
      EME_LOG("GMPVideoDecoderTrialCreator::MaybeAwaitTrialCreate(%s) already succeeded",
              NS_ConvertUTF16toUTF8(aKeySystem).get());
      aPromisey->Resolve();
      break;
    }
    case TrialCreateState::Failed: {
      // Something is broken about this configuration. Report as unsupported.
      EME_LOG("GMPVideoDecoderTrialCreator::MaybeAwaitTrialCreate(%s) already failed",
              NS_ConvertUTF16toUTF8(aKeySystem).get());
      aPromisey->Reject(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                        NS_LITERAL_CSTRING("navigator.requestMediaKeySystemAccess trial CDM creation failed"));
      break;
    }
    case TrialCreateState::Pending: {
      EME_LOG("GMPVideoDecoderTrialCreator::MaybeAwaitTrialCreate(%s) pending",
              NS_ConvertUTF16toUTF8(aKeySystem).get());
      // Add request to the list of pending items waiting.
      data->mPending.AppendElement(aPromisey);
      if (!data->mTest) {
        // Not already waiting for CDM to be created. Create and Init
        // a CDM, to test whether it will work.
        data->mTest = new TestGMPVideoDecoder(this, aKeySystem, aParent);
        if (NS_FAILED(data->mTest->Start())) {
          TrialCreateGMPVideoDecoderFailed(aKeySystem,
            NS_LITERAL_CSTRING("TestGMPVideoDecoder::Start() failed"));
          return;
        }

        // Promise will call InitMediaKeysPromiseHandler when Init()
        // succeeds/fails.
      }
      break;
    }
  }
  return;
}

} // namespace dom
} // namespace mozilla
