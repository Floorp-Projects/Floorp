/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MP4Demuxer.h"
#include "MP4Stream.h"
#include "MozPromise.h"
#include "MediaDataDemuxer.h"
#include "mozilla/SharedThreadPool.h"
#include "TaskQueue.h"
#include "mozilla/ArrayUtils.h"
#include "MockMediaResource.h"
#include "VideoUtils.h"

using namespace mozilla;
using namespace mp4_demuxer;

class AutoTaskQueue;

#define DO_FAIL []()->void { EXPECT_TRUE(false); }

class MP4DemuxerBinding
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MP4DemuxerBinding);

  nsRefPtr<MockMediaResource> resource;
  nsRefPtr<MP4Demuxer> mDemuxer;
  nsRefPtr<TaskQueue> mTaskQueue;
  nsRefPtr<MediaTrackDemuxer> mAudioTrack;
  nsRefPtr<MediaTrackDemuxer> mVideoTrack;
  uint32_t mIndex;
  nsTArray<nsRefPtr<MediaRawData>> mSamples;
  nsTArray<int64_t> mKeyFrameTimecodes;
  MozPromiseHolder<GenericPromise> mCheckTrackKeyFramePromise;
  MozPromiseHolder<GenericPromise> mCheckTrackSamples;

  explicit MP4DemuxerBinding(const char* aFileName = "dash_dashinit.mp4")
    : resource(new MockMediaResource(aFileName))
    , mDemuxer(new MP4Demuxer(resource))
    , mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK)))
    , mIndex(0)
  {
    EXPECT_EQ(NS_OK, resource->Open(nullptr));
  }

  template<typename Function>
  void RunTestAndWait(const Function& aFunction)
  {
    Function func(aFunction);
    mDemuxer->Init()->Then(mTaskQueue, __func__, Move(func), DO_FAIL);
    mTaskQueue->AwaitShutdownAndIdle();
  }

  nsRefPtr<GenericPromise>
  CheckTrackKeyFrame(MediaTrackDemuxer* aTrackDemuxer)
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

    nsRefPtr<MediaTrackDemuxer> track = aTrackDemuxer;
    nsRefPtr<MP4DemuxerBinding> self = this;

    int64_t time = -1;
    while (mIndex < mSamples.Length()) {
      uint32_t i = mIndex++;
      if (mSamples[i]->mKeyframe) {
        time = mSamples[i]->mTime;
        break;
      }
    }

    nsRefPtr<GenericPromise> p = mCheckTrackKeyFramePromise.Ensure(__func__);

    if (time == -1) {
      mCheckTrackKeyFramePromise.Resolve(true, __func__);
      return p;
    }


    DispatchTask(
      [track, time, self] () {
        track->Seek(media::TimeUnit::FromMicroseconds(time))->Then(self->mTaskQueue, __func__,
          [track, time, self] () {
            track->GetSamples()->Then(self->mTaskQueue, __func__,
              [track, time, self] (nsRefPtr<MediaTrackDemuxer::SamplesHolder> aSamples) {
                EXPECT_EQ(time, aSamples->mSamples[0]->mTime);
                self->CheckTrackKeyFrame(track);
              },
              DO_FAIL
            );
          },
          DO_FAIL
        );
      }
    );

    return p;
  }

  nsRefPtr<GenericPromise>
  CheckTrackSamples(MediaTrackDemuxer* aTrackDemuxer)
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

    nsRefPtr<MediaTrackDemuxer> track = aTrackDemuxer;
    nsRefPtr<MP4DemuxerBinding> self = this;

    nsRefPtr<GenericPromise> p = mCheckTrackSamples.Ensure(__func__);

    DispatchTask(
      [track, self] () {
        track->GetSamples()->Then(self->mTaskQueue, __func__,
          [track, self] (nsRefPtr<MediaTrackDemuxer::SamplesHolder> aSamples) {
            if (aSamples->mSamples.Length()) {
              self->mSamples.AppendElements(aSamples->mSamples);
              self->CheckTrackSamples(track);
            }
          },
          [self] (DemuxerFailureReason aReason) {
            if (aReason == DemuxerFailureReason::DEMUXER_ERROR) {
              EXPECT_TRUE(false);
              self->mCheckTrackSamples.Reject(NS_ERROR_FAILURE, __func__);
            } else if (aReason == DemuxerFailureReason::END_OF_STREAM) {
              EXPECT_TRUE(self->mSamples.Length() > 1);
              for (uint32_t i = 0; i < (self->mSamples.Length() - 1); i++) {
                EXPECT_LT(self->mSamples[i]->mTimecode, self->mSamples[i + 1]->mTimecode);
                if (self->mSamples[i]->mKeyframe) {
                  self->mKeyFrameTimecodes.AppendElement(self->mSamples[i]->mTimecode);
                }
              }
              self->mCheckTrackSamples.Resolve(true, __func__);
            }
          }
        );
      }
    );

    return p;
  }

private:

  template<typename FunctionType>
  void
  DispatchTask(FunctionType aFun)
  {
    nsRefPtr<nsRunnable> r = NS_NewRunnableFunction(aFun);
    mTaskQueue->Dispatch(r.forget());
  }

  virtual ~MP4DemuxerBinding()
  {
  }
};

TEST(MP4Demuxer, Seek)
{
  nsRefPtr<MP4DemuxerBinding> binding = new MP4DemuxerBinding();

  binding->RunTestAndWait([binding] () {
    binding->mVideoTrack = binding->mDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
    binding->CheckTrackSamples(binding->mVideoTrack)
      ->Then(binding->mTaskQueue, __func__,
        [binding] () {
          binding->CheckTrackKeyFrame(binding->mVideoTrack)
            ->Then(binding->mTaskQueue, __func__,
              [binding] () {
                binding->mTaskQueue->BeginShutdown();
              }, DO_FAIL);
        }, DO_FAIL);
  });
}

static nsCString
ToCryptoString(const CryptoSample& aCrypto)
{
  nsCString res;
  if (aCrypto.mValid) {
    res.AppendPrintf("%d %d ", aCrypto.mMode, aCrypto.mIVSize);
    for (size_t i = 0; i < aCrypto.mKeyId.Length(); i++) {
      res.AppendPrintf("%02x", aCrypto.mKeyId[i]);
    }
    res.Append(" ");
    for (size_t i = 0; i < aCrypto.mIV.Length(); i++) {
      res.AppendPrintf("%02x", aCrypto.mIV[i]);
    }
    EXPECT_EQ(aCrypto.mPlainSizes.Length(), aCrypto.mEncryptedSizes.Length());
    for (size_t i = 0; i < aCrypto.mPlainSizes.Length(); i++) {
      res.AppendPrintf(" %d,%d", aCrypto.mPlainSizes[i],
                       aCrypto.mEncryptedSizes[i]);
    }
  } else {
    res.Append("no crypto");
  }
  return res;
}

#ifndef XP_WIN // VC2013 doesn't support C++11 array initialization.

TEST(MP4Demuxer, CENCFrag)
{
  const char* video[] = {
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000000 5,684 5,16980",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000450 5,1826",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000004c3 5,1215",
    "1 16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000050f 5,1302",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000561 5,939",
    "1 16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000059c 5,763",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000005cc 5,672",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000005f6 5,748",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000625 5,1025",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000666 5,730",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000694 5,897",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000006cd 5,643",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000006f6 5,556",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000719 5,527",
    "1 16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000073a 5,606",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000760 5,701",
    "1 16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000078c 5,531",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000007ae 5,562",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000007d2 5,576",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000007f6 5,514",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000817 5,404",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000831 5,635",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000859 5,433",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000875 5,478",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000893 5,474",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000008b1 5,462",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000008ce 5,473",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000008ec 5,437",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000908 5,418",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000923 5,475",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000941 5,23133",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000ee7 5,475",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f05 5,402",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f1f 5,415",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f39 5,408",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f53 5,442",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f6f 5,385",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f88 5,368",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000f9f 5,354",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000fb6 5,400",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000fcf 5,399",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000000fe8 5,1098",
    "1 16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000102d 5,1508",
    "1 16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000108c 5,1345",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000010e1 5,1945",
    "1 16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000115b 5,1824",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000011cd 5,2133",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000001253 5,2486",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000012ef 5,1739",
    "1 16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000135c 5,1836",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000013cf 5,2367",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000001463 5,2571",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000001504 5,3008",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000015c0 5,3255",
    "1 16 7e571d037e571d037e571d037e571d03 0000000000000000000000000000168c 5,3225",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000001756 5,3118",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000001819 5,2407",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000018b0 5,2400",
    "1 16 7e571d037e571d037e571d037e571d03 00000000000000000000000000001946 5,2158",
    "1 16 7e571d037e571d037e571d037e571d03 000000000000000000000000000019cd 5,2392",
  };

  nsRefPtr<MP4DemuxerBinding> binding = new MP4DemuxerBinding("gizmo-frag.mp4");

  binding->RunTestAndWait([binding, video] () {
    // grab all video samples.
    binding->mVideoTrack = binding->mDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
    binding->CheckTrackSamples(binding->mVideoTrack)
      ->Then(binding->mTaskQueue, __func__,
        [binding, video] () {
          for (uint32_t i = 0; i < binding->mSamples.Length(); i++) {
            nsCString text = ToCryptoString(binding->mSamples[i]->mCrypto);
            EXPECT_STREQ(video[i++], text.get());
          }
          EXPECT_EQ(ArrayLength(video), binding->mSamples.Length());
          binding->mTaskQueue->BeginShutdown();
        }, DO_FAIL);
  });

  const char* audio[] = {
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000000 0,281",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000012 0,257",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000023 0,246",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000033 0,257",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000044 0,260",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000055 0,260",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000066 0,272",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000077 0,280",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000089 0,284",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000009b 0,290",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000000ae 0,278",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000000c0 0,268",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000000d1 0,307",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000000e5 0,290",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000000f8 0,304",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000010b 0,316",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000011f 0,308",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000133 0,301",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000146 0,318",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000015a 0,311",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000016e 0,303",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000181 0,325",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000196 0,334",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000001ab 0,344",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000001c1 0,344",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000001d7 0,387",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000001f0 0,396",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000209 0,368",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000220 0,373",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000238 0,425",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000253 0,428",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000026e 0,426",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000289 0,427",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000002a4 0,424",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000002bf 0,447",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000002db 0,446",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000002f7 0,442",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000313 0,444",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000032f 0,374",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000347 0,405",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000361 0,372",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000379 0,395",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000392 0,435",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000003ae 0,426",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000003c9 0,430",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000003e4 0,390",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000003fd 0,335",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000412 0,339",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000428 0,352",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000043e 0,364",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000455 0,398",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000046e 0,451",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000048b 0,448",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000004a7 0,436",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000004c3 0,424",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000004de 0,428",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000004f9 0,413",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000513 0,430",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000052e 0,450",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000054b 0,386",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000564 0,320",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000578 0,347",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000058e 0,382",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000005a6 0,437",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000005c2 0,387",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000005db 0,340",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000005f1 0,337",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000607 0,389",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000620 0,428",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000063b 0,426",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000656 0,446",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000672 0,456",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000068f 0,468",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000006ad 0,468",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000006cb 0,463",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000006e8 0,467",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000706 0,460",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000723 0,446",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000073f 0,453",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000075c 0,448",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000778 0,446",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000794 0,439",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000007b0 0,436",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000007cc 0,441",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000007e8 0,465",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000806 0,448",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000822 0,448",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000083e 0,469",
    "1 16 7e571d047e571d047e571d047e571d04 0000000000000000000000000000085c 0,431",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000877 0,437",
    "1 16 7e571d047e571d047e571d047e571d04 00000000000000000000000000000893 0,474",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000008b1 0,436",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000008cd 0,433",
    "1 16 7e571d047e571d047e571d047e571d04 000000000000000000000000000008e9 0,481",
  };
  nsRefPtr<MP4DemuxerBinding> audiobinding = new MP4DemuxerBinding("gizmo-frag.mp4");

  audiobinding->RunTestAndWait([audiobinding, audio] () {
    // grab all audio samples.
    audiobinding->mAudioTrack = audiobinding->mDemuxer->GetTrackDemuxer(TrackInfo::kAudioTrack, 0);
    audiobinding->CheckTrackSamples(audiobinding->mAudioTrack)
      ->Then(audiobinding->mTaskQueue, __func__,
        [audiobinding, audio] () {
          EXPECT_TRUE(audiobinding->mSamples.Length() > 1);
          for (uint32_t i = 0; i < audiobinding->mSamples.Length(); i++) {
            nsCString text = ToCryptoString(audiobinding->mSamples[i]->mCrypto);
            EXPECT_STREQ(audio[i++], text.get());
          }
          EXPECT_EQ(ArrayLength(audio), audiobinding->mSamples.Length());
          audiobinding->mTaskQueue->BeginShutdown();
        }, DO_FAIL);
  });
}

#endif

TEST(MP4Demuxer, GetNextKeyframe)
{
  nsRefPtr<MP4DemuxerBinding> binding = new MP4DemuxerBinding("gizmo-frag.mp4");

  binding->RunTestAndWait([binding] () {
    // Insert a [0,end] buffered range, to simulate Moof's being buffered
    // via MSE.
    auto len = binding->resource->GetLength();
    binding->resource->MockAddBufferedRange(0, len);

    // gizmp-frag has two keyframes; one at dts=cts=0, and another at
    // dts=cts=1000000. Verify we get expected results.
    media::TimeUnit time;
    binding->mVideoTrack = binding->mDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
    binding->mVideoTrack->Reset();
    binding->mVideoTrack->GetNextRandomAccessPoint(&time);
    EXPECT_EQ(time.ToMicroseconds(), 0);
    binding->mVideoTrack->GetSamples()->Then(binding->mTaskQueue, __func__,
      [binding] () {
        media::TimeUnit time;
        binding->mVideoTrack->GetNextRandomAccessPoint(&time);
        EXPECT_EQ(time.ToMicroseconds(), 1000000);
        binding->mTaskQueue->BeginShutdown();
      },
      DO_FAIL
    );
  });
}

TEST(MP4Demuxer, ZeroInMoov)
{
  nsRefPtr<MP4DemuxerBinding> binding = new MP4DemuxerBinding("short-zero-in-moov.mp4");
  binding->RunTestAndWait([binding] () {
    // It demuxes without error. That is sufficient.
    binding->mTaskQueue->BeginShutdown();
  });
}

