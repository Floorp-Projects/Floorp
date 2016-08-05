/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/Preferences.h"
#include "mozilla/TaskQueue.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "MediaData.h"
#include "MediaFormatReader.h"
#include "MP4Decoder.h"
#include "MockMediaDecoderOwner.h"
#include "MockMediaResource.h"
#include "VideoFrameContainer.h"

using namespace mozilla;
using namespace mozilla::dom;

class MockMP4Decoder : public MP4Decoder
{
public:
  MockMP4Decoder()
    : MP4Decoder(new MockMediaDecoderOwner())
  {}

  // Avoid the assertion.
  AbstractCanonical<media::NullableTimeUnit>* CanonicalDurationOrNull() override
  {
    return nullptr;
  }
};

class MediaFormatReaderBinding
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaFormatReaderBinding);
  RefPtr<MockMP4Decoder> mDecoder;
  RefPtr<MockMediaResource> mResource;
  RefPtr<MediaDecoderReader> mReader;
  RefPtr<TaskQueue> mTaskQueue;
  explicit MediaFormatReaderBinding(const char* aFileName = "gizmo.mp4")
    : mDecoder(new MockMP4Decoder())
    , mResource(new MockMediaResource(aFileName))
    , mReader(new MediaFormatReader(mDecoder,
                                    new MP4Demuxer(mResource),
                                    new VideoFrameContainer(
                                      (HTMLMediaElement*)(0x1),
                                      layers::LayerManager::CreateImageContainer(
                                        layers::ImageContainer::ASYNCHRONOUS))
                                    ))
    , mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK)))
  {
  }

  bool Init() {
    // Init Resource.
    nsresult rv = mResource->Open(nullptr);
    if (NS_FAILED(rv)) {
      return false;
    }
    mDecoder->SetResource(mResource);
    // Init Reader.
    rv = mReader->Init();
    if (NS_FAILED(rv)) {
      return false;
    }
    return true;
  }

  void OnMetadataReadAudio(MetadataHolder* aMetadata)
  {
    EXPECT_TRUE(aMetadata);
    mReader->RequestAudioData()
      ->Then(mReader->OwnerThread(), __func__, this,
             &MediaFormatReaderBinding::OnAudioRawDataDemuxed,
             &MediaFormatReaderBinding::OnNotDemuxed);
  }

  void OnMetadataReadVideo(MetadataHolder* aMetadata)
  {
    EXPECT_TRUE(aMetadata);
    mReader->RequestVideoData(true, 0)
      ->Then(mReader->OwnerThread(), __func__, this,
             &MediaFormatReaderBinding::OnVideoRawDataDemuxed,
             &MediaFormatReaderBinding::OnNotDemuxed);
  }

  void OnMetadataNotRead(ReadMetadataFailureReason aReason) {
    EXPECT_TRUE(false);
    ReaderShutdown();
  }

  void OnAudioRawDataDemuxed(MediaData* aAudioSample)
  {
    EXPECT_TRUE(aAudioSample);
    EXPECT_EQ(MediaData::RAW_DATA, aAudioSample->mType);
    ReaderShutdown();
  }

  void OnVideoRawDataDemuxed(MediaData* aVideoSample)
  {
    EXPECT_TRUE(aVideoSample);
    EXPECT_EQ(MediaData::RAW_DATA, aVideoSample->mType);
    ReaderShutdown();
  }

  void OnNotDemuxed(MediaDecoderReader::NotDecodedReason aReason)
  {
    EXPECT_TRUE(false);
    ReaderShutdown();
  }

  void ReaderShutdown()
  {
    RefPtr<MediaFormatReaderBinding> self = this;
    mReader->Shutdown()
      ->Then(mTaskQueue, __func__,
             [self]() {
               self->mTaskQueue->BeginShutdown();
             },
             [self]() {
               EXPECT_TRUE(false);
               self->mTaskQueue->BeginShutdown();
             }); //Then
  }
  template<class Function>
  void RunTestAndWait(Function&& aFunction)
  {
    RefPtr<Runnable> r = NS_NewRunnableFunction(Forward<Function>(aFunction));
    mTaskQueue->Dispatch(r.forget());
    mTaskQueue->AwaitShutdownAndIdle();
  }
private:
  ~MediaFormatReaderBinding()
  {
    mDecoder->Shutdown();
  }
};


template <typename T>
T GetPref(const char* aPrefKey);

template <>
bool GetPref<bool>(const char* aPrefKey)
{
  return Preferences::GetBool(aPrefKey);
}

template <typename T>
void SetPref(const char* a, T value);

template <>
void SetPref<bool>(const char* aPrefKey, bool aValue)
{
  Unused << Preferences::SetBool(aPrefKey, aValue);
}

template <typename T>
class PreferencesRAII
{
public:
  explicit PreferencesRAII(const char* aPrefKey, T aValue)
  : mPrefKey(aPrefKey)
  {
    mDefaultPref = GetPref<T>(aPrefKey);
    SetPref(aPrefKey, aValue);
  }
  ~PreferencesRAII()
  {
    SetPref(mPrefKey, mDefaultPref);
  }
private:
  T mDefaultPref;
  const char* mPrefKey;
};

TEST(MediaFormatReader, RequestAudioRawData)
{
  PreferencesRAII<bool> pref =
    PreferencesRAII<bool>("media.use-blank-decoder",
                          true);
  RefPtr<MediaFormatReaderBinding> b = new MediaFormatReaderBinding();
  if (!b->Init())
  {
    EXPECT_TRUE(false);
    // Stop the test since initialization failed.
    return;
  }
  if (!b->mReader->IsDemuxOnlySupported())
  {
    EXPECT_TRUE(false);
    // Stop the test since the reader cannot support demuxed-only demand.
    return;
  }
  // Switch to demuxed-only mode.
  b->mReader->SetDemuxOnly(true);
  // To ensure the MediaDecoderReader::InitializationTask and
  // MediaDecoderReader::SetDemuxOnly can be done.
  NS_ProcessNextEvent();
  auto testCase = [b]() {
    InvokeAsync(b->mReader->OwnerThread(),
                b->mReader.get(),
                __func__,
                &MediaDecoderReader::AsyncReadMetadata)
      ->Then(b->mReader->OwnerThread(), __func__, b.get(),
             &MediaFormatReaderBinding::OnMetadataReadAudio,
             &MediaFormatReaderBinding::OnMetadataNotRead);
  };
  b->RunTestAndWait(testCase);
}
TEST(MediaFormatReader, RequestVideoRawData)
{
  PreferencesRAII<bool> pref =
    PreferencesRAII<bool>("media.use-blank-decoder",
                          true);
  RefPtr<MediaFormatReaderBinding> b = new MediaFormatReaderBinding();
  if (!b->Init())
  {
    EXPECT_TRUE(false);
    // Stop the test since initialization failed.
    return;
  }
  if (!b->mReader->IsDemuxOnlySupported())
  {
    EXPECT_TRUE(false);
    // Stop the test since the reader cannot support demuxed-only demand.
    return;
  }
  // Switch to demuxed-only mode.
  b->mReader->SetDemuxOnly(true);
  // To ensure the MediaDecoderReader::InitializationTask and
  // MediaDecoderReader::SetDemuxOnly can be done.
  NS_ProcessNextEvent();
  auto testCase = [b]() {
    InvokeAsync(b->mReader->OwnerThread(),
                b->mReader.get(),
                __func__,
                &MediaDecoderReader::AsyncReadMetadata)
      ->Then(b->mReader->OwnerThread(), __func__, b.get(),
             &MediaFormatReaderBinding::OnMetadataReadVideo,
             &MediaFormatReaderBinding::OnMetadataNotRead);
  };
  b->RunTestAndWait(testCase);
}
