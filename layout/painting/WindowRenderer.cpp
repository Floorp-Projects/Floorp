/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowRenderer.h"

#include "gfxPlatform.h"
#include "mozilla/dom/Animation.h"  // for Animation
#include "mozilla/dom/AnimationEffect.h"
#include "mozilla/EffectSet.h"
#include "mozilla/layers/PersistentBufferProvider.h"  // for PersistentBufferProviderBasic, PersistentBufferProvider (ptr only)
#include "nsDisplayList.h"

using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace mozilla {

/**
 * StartFrameTimeRecording, together with StopFrameTimeRecording
 * enable recording of frame intervals.
 *
 * To allow concurrent consumers, a cyclic array is used which serves all
 * consumers, practically stateless with regard to consumers.
 *
 * To save resources, the buffer is allocated on first call to
 * StartFrameTimeRecording and recording is paused if no consumer which called
 * StartFrameTimeRecording is able to get valid results (because the cyclic
 * buffer was overwritten since that call).
 *
 * To determine availability of the data upon StopFrameTimeRecording:
 * - mRecording.mNextIndex increases on each RecordFrame, and never resets.
 * - Cyclic buffer position is realized as mNextIndex % bufferSize.
 * - StartFrameTimeRecording returns mNextIndex. When StopFrameTimeRecording is
 *   called, the required start index is passed as an arg, and we're able to
 *   calculate the required length. If this length is bigger than bufferSize, it
 *   means data was overwritten.  otherwise, we can return the entire sequence.
 * - To determine if we need to pause, mLatestStartIndex is updated to
 *   mNextIndex on each call to StartFrameTimeRecording. If this index gets
 *   overwritten, it means that all earlier start indices obtained via
 *   StartFrameTimeRecording were also overwritten, hence, no point in
 *   recording, so pause.
 * - mCurrentRunStartIndex indicates the oldest index of the recording after
 *   which the recording was not paused. If StopFrameTimeRecording is invoked
 *   with a start index older than this, it means that some frames were not
 *   recorded, so data is invalid.
 */
uint32_t FrameRecorder::StartFrameTimeRecording(int32_t aBufferSize) {
  if (mRecording.mIsPaused) {
    mRecording.mIsPaused = false;

    if (!mRecording.mIntervals.Length()) {  // Initialize recording buffers
      mRecording.mIntervals.SetLength(aBufferSize);
    }

    // After being paused, recent values got invalid. Update them to now.
    mRecording.mLastFrameTime = TimeStamp::Now();

    // Any recording which started before this is invalid, since we were paused.
    mRecording.mCurrentRunStartIndex = mRecording.mNextIndex;
  }

  // If we'll overwrite this index, there are no more consumers with aStartIndex
  // for which we're able to provide the full recording, so no point in keep
  // recording.
  mRecording.mLatestStartIndex = mRecording.mNextIndex;
  return mRecording.mNextIndex;
}

void FrameRecorder::RecordFrame() {
  if (!mRecording.mIsPaused) {
    TimeStamp now = TimeStamp::Now();
    uint32_t i = mRecording.mNextIndex % mRecording.mIntervals.Length();
    mRecording.mIntervals[i] =
        static_cast<float>((now - mRecording.mLastFrameTime).ToMilliseconds());
    mRecording.mNextIndex++;
    mRecording.mLastFrameTime = now;

    if (mRecording.mNextIndex >
        (mRecording.mLatestStartIndex + mRecording.mIntervals.Length())) {
      // We've just overwritten the most recent recording start -> pause.
      mRecording.mIsPaused = true;
    }
  }
}

void FrameRecorder::StopFrameTimeRecording(uint32_t aStartIndex,
                                           nsTArray<float>& aFrameIntervals) {
  uint32_t bufferSize = mRecording.mIntervals.Length();
  uint32_t length = mRecording.mNextIndex - aStartIndex;
  if (mRecording.mIsPaused || length > bufferSize ||
      aStartIndex < mRecording.mCurrentRunStartIndex) {
    // aStartIndex is too old. Also if aStartIndex was issued before
    // mRecordingNextIndex overflowed (uint32_t)
    //   and stopped after the overflow (would happen once every 828 days of
    //   constant 60fps).
    length = 0;
  }

  if (!length) {
    aFrameIntervals.Clear();
    return;  // empty recording, return empty arrays.
  }
  // Set length in advance to avoid possibly repeated reallocations
  aFrameIntervals.SetLength(length);

  uint32_t cyclicPos = aStartIndex % bufferSize;
  for (uint32_t i = 0; i < length; i++, cyclicPos++) {
    if (cyclicPos == bufferSize) {
      cyclicPos = 0;
    }
    aFrameIntervals[i] = mRecording.mIntervals[cyclicPos];
  }
}

already_AddRefed<PersistentBufferProvider>
WindowRenderer::CreatePersistentBufferProvider(
    const mozilla::gfx::IntSize& aSize, mozilla::gfx::SurfaceFormat aFormat,
    bool aWillReadFrequently) {
  RefPtr<PersistentBufferProviderBasic> bufferProvider;
  // If we are using remote canvas we don't want to use acceleration in
  // non-remote layer managers, so we always use the fallback software one.
  // If will-read-frequently is set, avoid using the preferred backend in
  // favor of the fallback backend in case the preferred backend provides
  // acceleration.
  if (!aWillReadFrequently &&
      (!gfxPlatform::UseRemoteCanvas() ||
       !gfxPlatform::IsBackendAccelerated(
           gfxPlatform::GetPlatform()->GetPreferredCanvasBackend()))) {
    bufferProvider = PersistentBufferProviderBasic::Create(
        aSize, aFormat,
        gfxPlatform::GetPlatform()->GetPreferredCanvasBackend());
  }

  if (!bufferProvider) {
    bufferProvider = PersistentBufferProviderBasic::Create(
        aSize, aFormat, gfxPlatform::GetPlatform()->GetFallbackCanvasBackend());
  }

  return bufferProvider.forget();
}

void WindowRenderer::AddPartialPrerenderedAnimation(
    uint64_t aCompositorAnimationId, dom::Animation* aAnimation) {
  mPartialPrerenderedAnimations.InsertOrUpdate(aCompositorAnimationId,
                                               RefPtr{aAnimation});
  aAnimation->SetPartialPrerendered(aCompositorAnimationId);
}
void WindowRenderer::RemovePartialPrerenderedAnimation(
    uint64_t aCompositorAnimationId, dom::Animation* aAnimation) {
  MOZ_ASSERT(aAnimation);
#ifdef DEBUG
  RefPtr<dom::Animation> animation;
  if (mPartialPrerenderedAnimations.Remove(aCompositorAnimationId,
                                           getter_AddRefs(animation)) &&
      // It may be possible that either animation's effect has already been
      // nulled out via Animation::SetEffect() so ignore such cases.
      aAnimation->GetEffect() && aAnimation->GetEffect()->AsKeyframeEffect() &&
      animation->GetEffect() && animation->GetEffect()->AsKeyframeEffect()) {
    MOZ_ASSERT(
        EffectSet::GetForEffect(aAnimation->GetEffect()->AsKeyframeEffect()) ==
        EffectSet::GetForEffect(animation->GetEffect()->AsKeyframeEffect()));
  }
#else
  mPartialPrerenderedAnimations.Remove(aCompositorAnimationId);
#endif
  aAnimation->ResetPartialPrerendered();
}
void WindowRenderer::UpdatePartialPrerenderedAnimations(
    const nsTArray<uint64_t>& aJankedAnimations) {
  for (uint64_t id : aJankedAnimations) {
    RefPtr<dom::Animation> animation;
    if (mPartialPrerenderedAnimations.Remove(id, getter_AddRefs(animation))) {
      animation->UpdatePartialPrerendered();
    }
  }
}

void FallbackRenderer::SetTarget(gfxContext* aTarget,
                                 layers::BufferMode aDoubleBuffering) {
  mTarget = aTarget;
  mBufferMode = aDoubleBuffering;
}

bool FallbackRenderer::BeginTransaction(const nsCString& aURL) {
  if (!mTarget) {
    return false;
  }

  return true;
}

void FallbackRenderer::EndTransactionWithColor(const nsIntRect& aRect,
                                               const gfx::DeviceColor& aColor) {
  mTarget->GetDrawTarget()->FillRect(Rect(aRect), ColorPattern(aColor));
}

void FallbackRenderer::EndTransactionWithList(nsDisplayListBuilder* aBuilder,
                                              nsDisplayList* aList,
                                              int32_t aAppUnitsPerDevPixel,
                                              EndTransactionFlags aFlags) {
  if (aFlags & EndTransactionFlags::END_NO_COMPOSITE) {
    return;
  }

  DrawTarget* dt = mTarget->GetDrawTarget();

  BackendType backend = gfxPlatform::GetPlatform()->GetContentBackendFor(
      LayersBackend::LAYERS_NONE);
  RefPtr<DrawTarget> dest =
      gfxPlatform::GetPlatform()->CreateDrawTargetForBackend(
          backend, dt->GetSize(), dt->GetFormat());
  if (dest) {
    gfxContext ctx(dest, /* aPreserveTransform */ true);

    nsRegion opaque = aList->GetOpaqueRegion(aBuilder);
    if (opaque.Contains(aList->GetComponentAlphaBounds(aBuilder))) {
      dest->SetPermitSubpixelAA(true);
    }

    aList->Paint(aBuilder, &ctx, aAppUnitsPerDevPixel);

    RefPtr<SourceSurface> snapshot = dest->Snapshot();
    dt->DrawSurface(snapshot, Rect(dest->GetRect()), Rect(dest->GetRect()),
                    DrawSurfaceOptions(),
                    DrawOptions(1.0f, CompositionOp::OP_SOURCE));
  }
}

}  // namespace mozilla
