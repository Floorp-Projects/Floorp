/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MP4Demuxer.h"
#include "mozilla/MozPromise.h"
#include "MediaDataDemuxer.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Unused.h"
#include "MockMediaResource.h"
#include "VideoUtils.h"

using namespace mozilla;
using media::TimeUnit;

#define DO_FAIL                           \
  [binding]() -> void {                   \
    EXPECT_TRUE(false);                   \
    binding->mTaskQueue->BeginShutdown(); \
  }

class MP4DemuxerBinding {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MP4DemuxerBinding);

  RefPtr<MockMediaResource> resource;
  RefPtr<MP4Demuxer> mDemuxer;
  RefPtr<TaskQueue> mTaskQueue;
  RefPtr<MediaTrackDemuxer> mAudioTrack;
  RefPtr<MediaTrackDemuxer> mVideoTrack;
  uint32_t mIndex;
  nsTArray<RefPtr<MediaRawData>> mSamples;
  nsTArray<int64_t> mKeyFrameTimecodes;
  MozPromiseHolder<GenericPromise> mCheckTrackKeyFramePromise;
  MozPromiseHolder<GenericPromise> mCheckTrackSamples;

  explicit MP4DemuxerBinding(const char* aFileName = "dash_dashinit.mp4")
      : resource(new MockMediaResource(aFileName)),
        mDemuxer(new MP4Demuxer(resource)),
        mTaskQueue(
            new TaskQueue(GetMediaThreadPool(MediaThreadType::SUPERVISOR))),
        mIndex(0) {
    EXPECT_EQ(NS_OK, resource->Open());
  }

  template <typename Function>
  void RunTestAndWait(const Function& aFunction) {
    Function func(aFunction);
    RefPtr<MP4DemuxerBinding> binding = this;
    mDemuxer->Init()->Then(mTaskQueue, __func__, std::move(func), DO_FAIL);
    mTaskQueue->AwaitShutdownAndIdle();
  }

  RefPtr<GenericPromise> CheckTrackKeyFrame(MediaTrackDemuxer* aTrackDemuxer) {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

    RefPtr<MediaTrackDemuxer> track = aTrackDemuxer;
    RefPtr<MP4DemuxerBinding> binding = this;

    auto time = TimeUnit::Invalid();
    while (mIndex < mSamples.Length()) {
      uint32_t i = mIndex++;
      if (mSamples[i]->mKeyframe) {
        time = mSamples[i]->mTime;
        break;
      }
    }

    RefPtr<GenericPromise> p = mCheckTrackKeyFramePromise.Ensure(__func__);

    if (!time.IsValid()) {
      mCheckTrackKeyFramePromise.Resolve(true, __func__);
      return p;
    }

    DispatchTask([track, time, binding]() {
      track->Seek(time)->Then(
          binding->mTaskQueue, __func__,
          [track, time, binding]() {
            track->GetSamples()->Then(
                binding->mTaskQueue, __func__,
                [track, time,
                 binding](RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples) {
                  EXPECT_EQ(time, aSamples->GetSamples()[0]->mTime);
                  binding->CheckTrackKeyFrame(track);
                },
                DO_FAIL);
          },
          DO_FAIL);
    });

    return p;
  }

  RefPtr<GenericPromise> CheckTrackSamples(MediaTrackDemuxer* aTrackDemuxer) {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

    RefPtr<MediaTrackDemuxer> track = aTrackDemuxer;
    RefPtr<MP4DemuxerBinding> binding = this;

    RefPtr<GenericPromise> p = mCheckTrackSamples.Ensure(__func__);

    DispatchTask([track, binding]() {
      track->GetSamples()->Then(
          binding->mTaskQueue, __func__,
          [track, binding](RefPtr<MediaTrackDemuxer::SamplesHolder> aSamples) {
            if (aSamples->GetSamples().Length()) {
              binding->mSamples.AppendElements(aSamples->GetSamples());
              binding->CheckTrackSamples(track);
            }
          },
          [binding](const MediaResult& aError) {
            if (aError == NS_ERROR_DOM_MEDIA_END_OF_STREAM) {
              EXPECT_TRUE(binding->mSamples.Length() > 1);
              for (uint32_t i = 0; i < (binding->mSamples.Length() - 1); i++) {
                EXPECT_LT(binding->mSamples[i]->mTimecode,
                          binding->mSamples[i + 1]->mTimecode);
                if (binding->mSamples[i]->mKeyframe) {
                  binding->mKeyFrameTimecodes.AppendElement(
                      binding->mSamples[i]->mTimecode.ToMicroseconds());
                }
              }
              binding->mCheckTrackSamples.Resolve(true, __func__);
            } else {
              EXPECT_TRUE(false);
              binding->mCheckTrackSamples.Reject(aError, __func__);
            }
          });
    });

    return p;
  }

 private:
  template <typename FunctionType>
  void DispatchTask(FunctionType aFun) {
    RefPtr<Runnable> r =
        NS_NewRunnableFunction("MP4DemuxerBinding::DispatchTask", aFun);
    Unused << mTaskQueue->Dispatch(r.forget());
  }

  virtual ~MP4DemuxerBinding() = default;
};

TEST(MP4Demuxer, Seek)
{
  RefPtr<MP4DemuxerBinding> binding = new MP4DemuxerBinding();

  binding->RunTestAndWait([binding]() {
    binding->mVideoTrack =
        binding->mDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
    binding->CheckTrackSamples(binding->mVideoTrack)
        ->Then(
            binding->mTaskQueue, __func__,
            [binding]() {
              binding->CheckTrackKeyFrame(binding->mVideoTrack)
                  ->Then(
                      binding->mTaskQueue, __func__,
                      [binding]() { binding->mTaskQueue->BeginShutdown(); },
                      DO_FAIL);
            },
            DO_FAIL);
  });
}

static nsCString ToCryptoString(const CryptoSample& aCrypto) {
  nsCString res;
  if (aCrypto.IsEncrypted()) {
    res.AppendPrintf("%d ", aCrypto.mIVSize);
    for (size_t i = 0; i < aCrypto.mKeyId.Length(); i++) {
      res.AppendPrintf("%02x", aCrypto.mKeyId[i]);
    }
    res.AppendLiteral(" ");
    for (size_t i = 0; i < aCrypto.mIV.Length(); i++) {
      res.AppendPrintf("%02x", aCrypto.mIV[i]);
    }
    EXPECT_EQ(aCrypto.mPlainSizes.Length(), aCrypto.mEncryptedSizes.Length());
    for (size_t i = 0; i < aCrypto.mPlainSizes.Length(); i++) {
      res.AppendPrintf(" %d,%d", aCrypto.mPlainSizes[i],
                       aCrypto.mEncryptedSizes[i]);
    }
  } else {
    res.AppendLiteral("no crypto");
  }
  return res;
}

TEST(MP4Demuxer, CENCFragVideo)
{
  const char* video[] = {
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000000 "
      "5,684 5,16980",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000450 "
      "5,1826",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000004c3 "
      "5,1215",
      "16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000050f "
      "5,1302",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000561 "
      "5,939",
      "16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000059c "
      "5,763",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000005cc "
      "5,672",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000005f6 "
      "5,748",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000625 "
      "5,1025",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000666 "
      "5,730",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000694 "
      "5,897",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000006cd "
      "5,643",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000006f6 "
      "5,556",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000719 "
      "5,527",
      "16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000073a "
      "5,606",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000760 "
      "5,701",
      "16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000078c "
      "5,531",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000007ae "
      "5,562",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000007d2 "
      "5,576",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000007f6 "
      "5,514",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000817 "
      "5,404",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000831 "
      "5,635",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000859 "
      "5,433",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000875 "
      "5,478",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000893 "
      "5,474",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000008b1 "
      "5,462",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000008ce "
      "5,473",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000008ec "
      "5,437",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000908 "
      "5,418",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000923 "
      "5,475",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000941 "
      "5,23133",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000ee7 "
      "5,475",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f05 "
      "5,402",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f1f "
      "5,415",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f39 "
      "5,408",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f53 "
      "5,442",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f6f "
      "5,385",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f88 "
      "5,368",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f9f "
      "5,354",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000fb6 "
      "5,400",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000fcf "
      "5,399",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000fe8 "
      "5,1098",
      "16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000102d "
      "5,1508",
      "16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000108c "
      "5,1345",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000010e1 "
      "5,1945",
      "16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000115b "
      "5,1824",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000011cd "
      "5,2133",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000001253 "
      "5,2486",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000012ef "
      "5,1739",
      "16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000135c "
      "5,1836",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000013cf "
      "5,2367",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000001463 "
      "5,2571",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000001504 "
      "5,3008",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000015c0 "
      "5,3255",
      "16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000168c "
      "5,3225",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000001756 "
      "5,3118",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000001819 "
      "5,2407",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000018b0 "
      "5,2400",
      "16 7e571d037e571d037e571d037e571d03 00000000000000000000000000001946 "
      "5,2158",
      "16 7e571d037e571d037e571d037e571d03 000000000000000000000000000019cd "
      "5,2392",
  };

  RefPtr<MP4DemuxerBinding> binding = new MP4DemuxerBinding("gizmo-frag.mp4");

  binding->RunTestAndWait([binding, video]() {
    // grab all video samples.
    binding->mVideoTrack =
        binding->mDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
    binding->CheckTrackSamples(binding->mVideoTrack)
        ->Then(
            binding->mTaskQueue, __func__,
            [binding, video]() {
              for (uint32_t i = 0; i < binding->mSamples.Length(); i++) {
                nsCString text = ToCryptoString(binding->mSamples[i]->mCrypto);
                EXPECT_STREQ(video[i++], text.get());
              }
              EXPECT_EQ(ArrayLength(video), binding->mSamples.Length());
              binding->mTaskQueue->BeginShutdown();
            },
            DO_FAIL);
  });
}

TEST(MP4Demuxer, CENCFragAudio)
{
  const char* audio[] = {
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000000 "
      "0,281",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000012 "
      "0,257",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000023 "
      "0,246",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000033 "
      "0,257",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000044 "
      "0,260",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000055 "
      "0,260",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000066 "
      "0,272",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000077 "
      "0,280",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000089 "
      "0,284",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000009b "
      "0,290",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000000ae "
      "0,278",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000000c0 "
      "0,268",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000000d1 "
      "0,307",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000000e5 "
      "0,290",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000000f8 "
      "0,304",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000010b "
      "0,316",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000011f "
      "0,308",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000133 "
      "0,301",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000146 "
      "0,318",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000015a "
      "0,311",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000016e "
      "0,303",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000181 "
      "0,325",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000196 "
      "0,334",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000001ab "
      "0,344",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000001c1 "
      "0,344",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000001d7 "
      "0,387",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000001f0 "
      "0,396",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000209 "
      "0,368",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000220 "
      "0,373",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000238 "
      "0,425",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000253 "
      "0,428",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000026e "
      "0,426",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000289 "
      "0,427",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000002a4 "
      "0,424",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000002bf "
      "0,447",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000002db "
      "0,446",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000002f7 "
      "0,442",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000313 "
      "0,444",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000032f "
      "0,374",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000347 "
      "0,405",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000361 "
      "0,372",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000379 "
      "0,395",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000392 "
      "0,435",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000003ae "
      "0,426",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000003c9 "
      "0,430",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000003e4 "
      "0,390",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000003fd "
      "0,335",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000412 "
      "0,339",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000428 "
      "0,352",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000043e "
      "0,364",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000455 "
      "0,398",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000046e "
      "0,451",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000048b "
      "0,448",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000004a7 "
      "0,436",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000004c3 "
      "0,424",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000004de "
      "0,428",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000004f9 "
      "0,413",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000513 "
      "0,430",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000052e "
      "0,450",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000054b "
      "0,386",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000564 "
      "0,320",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000578 "
      "0,347",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000058e "
      "0,382",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000005a6 "
      "0,437",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000005c2 "
      "0,387",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000005db "
      "0,340",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000005f1 "
      "0,337",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000607 "
      "0,389",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000620 "
      "0,428",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000063b "
      "0,426",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000656 "
      "0,446",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000672 "
      "0,456",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000068f "
      "0,468",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000006ad "
      "0,468",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000006cb "
      "0,463",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000006e8 "
      "0,467",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000706 "
      "0,460",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000723 "
      "0,446",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000073f "
      "0,453",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000075c "
      "0,448",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000778 "
      "0,446",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000794 "
      "0,439",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000007b0 "
      "0,436",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000007cc "
      "0,441",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000007e8 "
      "0,465",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000806 "
      "0,448",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000822 "
      "0,448",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000083e "
      "0,469",
      "16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000085c "
      "0,431",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000877 "
      "0,437",
      "16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000893 "
      "0,474",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000008b1 "
      "0,436",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000008cd "
      "0,433",
      "16 7e571d047e571d047e571d047e571d04 000000000000000000000000000008e9 "
      "0,481",
  };

  RefPtr<MP4DemuxerBinding> binding = new MP4DemuxerBinding("gizmo-frag.mp4");

  binding->RunTestAndWait([binding, audio]() {
    // grab all audio samples.
    binding->mAudioTrack =
        binding->mDemuxer->GetTrackDemuxer(TrackInfo::kAudioTrack, 0);
    binding->CheckTrackSamples(binding->mAudioTrack)
        ->Then(
            binding->mTaskQueue, __func__,
            [binding, audio]() {
              EXPECT_TRUE(binding->mSamples.Length() > 1);
              for (uint32_t i = 0; i < binding->mSamples.Length(); i++) {
                nsCString text = ToCryptoString(binding->mSamples[i]->mCrypto);
                EXPECT_STREQ(audio[i++], text.get());
              }
              EXPECT_EQ(ArrayLength(audio), binding->mSamples.Length());
              binding->mTaskQueue->BeginShutdown();
            },
            DO_FAIL);
  });
}

TEST(MP4Demuxer, GetNextKeyframe)
{
  RefPtr<MP4DemuxerBinding> binding = new MP4DemuxerBinding("gizmo-frag.mp4");

  binding->RunTestAndWait([binding]() {
    // Insert a [0,end] buffered range, to simulate Moof's being buffered
    // via MSE.
    auto len = binding->resource->GetLength();
    binding->resource->MockAddBufferedRange(0, len);

    // gizmp-frag has two keyframes; one at dts=cts=0, and another at
    // dts=cts=1000000. Verify we get expected results.
    TimeUnit time;
    binding->mVideoTrack =
        binding->mDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
    binding->mVideoTrack->Reset();
    binding->mVideoTrack->GetNextRandomAccessPoint(&time);
    EXPECT_EQ(time.ToMicroseconds(), 0);
    binding->mVideoTrack->GetSamples()->Then(
        binding->mTaskQueue, __func__,
        [binding]() {
          TimeUnit time;
          binding->mVideoTrack->GetNextRandomAccessPoint(&time);
          EXPECT_EQ(time.ToMicroseconds(), 1000000);
          binding->mTaskQueue->BeginShutdown();
        },
        DO_FAIL);
  });
}

TEST(MP4Demuxer, ZeroInLastMoov)
{
  RefPtr<MP4DemuxerBinding> binding =
      new MP4DemuxerBinding("short-zero-in-moov.mp4");
  binding->RunTestAndWait([binding]() {
    // It demuxes without error. That is sufficient.
    binding->mTaskQueue->BeginShutdown();
  });
}

TEST(MP4Demuxer, ZeroInMoovQuickTime)
{
  RefPtr<MP4DemuxerBinding> binding =
      new MP4DemuxerBinding("short-zero-inband.mov");
  binding->RunTestAndWait([binding]() {
    // It demuxes without error. That is sufficient.
    binding->mTaskQueue->BeginShutdown();
  });
}

TEST(MP4Demuxer, IgnoreMinus1Duration)
{
  RefPtr<MP4DemuxerBinding> binding =
      new MP4DemuxerBinding("negative_duration.mp4");
  binding->RunTestAndWait([binding]() {
    // It demuxes without error. That is sufficient.
    binding->mTaskQueue->BeginShutdown();
  });
}

#undef DO_FAIL
