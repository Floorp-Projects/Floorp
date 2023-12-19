/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceCanvasRecording.h"

#include "mozilla/gfx/CanvasManagerChild.h"
#include "mozilla/layers/CanvasChild.h"
#include "mozilla/layers/CanvasDrawEventRecorder.h"
#include "RecordedCanvasEventImpl.h"

namespace mozilla::layers {

SourceSurfaceCanvasRecording::SourceSurfaceCanvasRecording(
    int64_t aTextureId, const RefPtr<gfx::SourceSurface>& aRecordedSuface,
    CanvasChild* aCanvasChild, const RefPtr<CanvasDrawEventRecorder>& aRecorder)
    : mTextureId(aTextureId),
      mRecordedSurface(aRecordedSuface),
      mCanvasChild(aCanvasChild),
      mRecorder(aRecorder) {}

SourceSurfaceCanvasRecording::~SourceSurfaceCanvasRecording() {
  if (!mRecorder) {
    return;
  }

  if (IsOnOwningThread()) {
    DestroyOnOwningThread();
    return;
  }

  ReferencePtr surfaceAlias = this;
  mRecorder->AddPendingDeletion(
      [recorder = std::move(mRecorder), surfaceAlias,
       aliasedSurface = std::move(mRecordedSurface),
       canvasChild = std::move(mCanvasChild)]() -> void {
        recorder->UntrackDestroyedRecordedSurface(surfaceAlias);
        recorder->RemoveStoredObject(surfaceAlias);
        recorder->RecordEvent(RecordedRemoveSurfaceAlias(surfaceAlias));
      });
}

void SourceSurfaceCanvasRecording::Init() {
  MOZ_ASSERT(mRecordedSurface);
  MOZ_ASSERT(mCanvasChild);
  MOZ_ASSERT(mRecorder);
  // It's important that AddStoredObject is called first because that will
  // run any pending processing required by recorded objects that have been
  // deleted off the main thread.
  mRecorder->TrackRecordedSurface(this);
  mRecorder->AddStoredObject(this);
  mRecorder->RecordEvent(RecordedAddSurfaceAlias(this, mRecordedSurface));
}

void SourceSurfaceCanvasRecording::DestroyOnOwningThread() {
  if (mRecorder) {
    ReferencePtr surfaceAlias = this;
    mRecorder->UntrackRecordedSurface(this);
    mRecorder->RemoveStoredObject(surfaceAlias);
    mRecorder->RecordEvent(RecordedRemoveSurfaceAlias(surfaceAlias));
    mRecorder = nullptr;
  }

  mRecordedSurface = nullptr;
  mCanvasChild = nullptr;
}

already_AddRefed<gfx::DataSourceSurface>
SourceSurfaceCanvasRecording::GetDataSurface() {
  EnsureDataSurfaceOnOwningThread();
  return do_AddRef(mDataSourceSurface);
}

bool SourceSurfaceCanvasRecording::IsOnOwningThread() const {
  return mCanvasChild->IsOnOwningThread();
}

void SourceSurfaceCanvasRecording::EnsureDataSurfaceOnOwningThread() {
  if (mDataSourceSurface) {
    return;
  }

  // The data can only be retrieved on the owning/recording thread.
  if (IsOnOwningThread()) {
    mDataSourceSurface =
        mCanvasChild->GetDataSurface(mTextureId, mRecordedSurface, mDetached);
  }
}

}  // namespace mozilla::layers
