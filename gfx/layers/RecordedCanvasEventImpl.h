/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_RecordedCanvasEventImpl_h
#define mozilla_layers_RecordedCanvasEventImpl_h

#include "mozilla/gfx/RecordedEvent.h"
#include "mozilla/gfx/RecordingTypes.h"
#include "mozilla/layers/CanvasTranslator.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/TextureClient.h"

namespace mozilla {
namespace layers {

using gfx::BackendType;
using gfx::DrawTarget;
using gfx::IntSize;
using gfx::RecordedEvent;
using gfx::RecordedEventDerived;
using EventType = gfx::RecordedEvent::EventType;
using gfx::ReadElement;
using gfx::ReferencePtr;
using gfx::SurfaceFormat;
using gfx::WriteElement;
using ipc::SharedMemoryBasic;

const EventType CANVAS_BEGIN_TRANSACTION = EventType::LAST;
const EventType CANVAS_END_TRANSACTION = EventType(EventType::LAST + 1);
const EventType CANVAS_FLUSH = EventType(EventType::LAST + 2);
const EventType TEXTURE_LOCK = EventType(EventType::LAST + 3);
const EventType TEXTURE_UNLOCK = EventType(EventType::LAST + 4);
const EventType CACHE_DATA_SURFACE = EventType(EventType::LAST + 5);
const EventType PREPARE_DATA_FOR_SURFACE = EventType(EventType::LAST + 6);
const EventType GET_DATA_FOR_SURFACE = EventType(EventType::LAST + 7);
const EventType ADD_SURFACE_ALIAS = EventType(EventType::LAST + 8);
const EventType REMOVE_SURFACE_ALIAS = EventType(EventType::LAST + 9);
const EventType DEVICE_CHANGE_ACKNOWLEDGED = EventType(EventType::LAST + 10);
const EventType CANVAS_DRAW_TARGET_CREATION = EventType(EventType::LAST + 11);
const EventType TEXTURE_DESTRUCTION = EventType(EventType::LAST + 12);
const EventType CHECKPOINT = EventType(EventType::LAST + 13);
const EventType PAUSE_TRANSLATION = EventType(EventType::LAST + 14);
const EventType RECYCLE_BUFFER = EventType(EventType::LAST + 15);
const EventType DROP_BUFFER = EventType(EventType::LAST + 16);
const EventType PREPARE_SHMEM = EventType(EventType::LAST + 17);
const EventType PRESENT_TEXTURE = EventType(EventType::LAST + 18);
const EventType LAST_CANVAS_EVENT_TYPE = PRESENT_TEXTURE;

class RecordedCanvasBeginTransaction final
    : public RecordedEventDerived<RecordedCanvasBeginTransaction> {
 public:
  RecordedCanvasBeginTransaction()
      : RecordedEventDerived(CANVAS_BEGIN_TRANSACTION) {}

  template <class S>
  MOZ_IMPLICIT RecordedCanvasBeginTransaction(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "RecordedCanvasBeginTransaction"; }
};

inline bool RecordedCanvasBeginTransaction::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  aTranslator->BeginTransaction();
  return true;
}

template <class S>
void RecordedCanvasBeginTransaction::Record(S& aStream) const {}

template <class S>
RecordedCanvasBeginTransaction::RecordedCanvasBeginTransaction(S& aStream)
    : RecordedEventDerived(CANVAS_BEGIN_TRANSACTION) {}

class RecordedCanvasEndTransaction final
    : public RecordedEventDerived<RecordedCanvasEndTransaction> {
 public:
  RecordedCanvasEndTransaction()
      : RecordedEventDerived(CANVAS_END_TRANSACTION) {}

  template <class S>
  MOZ_IMPLICIT RecordedCanvasEndTransaction(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "RecordedCanvasEndTransaction"; }
};

inline bool RecordedCanvasEndTransaction::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  aTranslator->EndTransaction();
  return true;
}

template <class S>
void RecordedCanvasEndTransaction::Record(S& aStream) const {}

template <class S>
RecordedCanvasEndTransaction::RecordedCanvasEndTransaction(S& aStream)
    : RecordedEventDerived(CANVAS_END_TRANSACTION) {}

class RecordedCanvasFlush final
    : public RecordedEventDerived<RecordedCanvasFlush> {
 public:
  RecordedCanvasFlush() : RecordedEventDerived(CANVAS_FLUSH) {}

  template <class S>
  MOZ_IMPLICIT RecordedCanvasFlush(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "RecordedCanvasFlush"; }
};

inline bool RecordedCanvasFlush::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  aTranslator->Flush();
  return true;
}

template <class S>
void RecordedCanvasFlush::Record(S& aStream) const {}

template <class S>
RecordedCanvasFlush::RecordedCanvasFlush(S& aStream)
    : RecordedEventDerived(CANVAS_FLUSH) {}

class RecordedTextureLock final
    : public RecordedEventDerived<RecordedTextureLock> {
 public:
  RecordedTextureLock(int64_t aTextureId, const OpenMode aMode,
                      bool aInvalidContents)
      : RecordedEventDerived(TEXTURE_LOCK),
        mTextureId(aTextureId),
        mMode(aMode),
        mInvalidContents(aInvalidContents) {}

  template <class S>
  MOZ_IMPLICIT RecordedTextureLock(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "TextureLock"; }

 private:
  int64_t mTextureId;
  OpenMode mMode;
  bool mInvalidContents;
};

inline bool RecordedTextureLock::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  if (!aTranslator->LockTexture(mTextureId, mMode, mInvalidContents)) {
    return false;
  }
  return true;
}

template <class S>
void RecordedTextureLock::Record(S& aStream) const {
  WriteElement(aStream, mTextureId);
  WriteElement(aStream, mMode);
  WriteElement(aStream, mInvalidContents);
}

template <class S>
RecordedTextureLock::RecordedTextureLock(S& aStream)
    : RecordedEventDerived(TEXTURE_LOCK) {
  ReadElement(aStream, mTextureId);
  ReadElementConstrained(aStream, mMode, OpenMode::OPEN_NONE,
                         OpenMode::OPEN_READ_WRITE_ASYNC);
  ReadElement(aStream, mInvalidContents);
}

class RecordedTextureUnlock final
    : public RecordedEventDerived<RecordedTextureUnlock> {
 public:
  explicit RecordedTextureUnlock(int64_t aTextureId)
      : RecordedEventDerived(TEXTURE_UNLOCK), mTextureId(aTextureId) {}

  template <class S>
  MOZ_IMPLICIT RecordedTextureUnlock(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "TextureUnlock"; }

 private:
  int64_t mTextureId;
};

inline bool RecordedTextureUnlock::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  if (!aTranslator->UnlockTexture(mTextureId)) {
    return false;
  }
  return true;
}

template <class S>
void RecordedTextureUnlock::Record(S& aStream) const {
  WriteElement(aStream, mTextureId);
}

template <class S>
RecordedTextureUnlock::RecordedTextureUnlock(S& aStream)
    : RecordedEventDerived(TEXTURE_UNLOCK) {
  ReadElement(aStream, mTextureId);
}

class RecordedCacheDataSurface final
    : public RecordedEventDerived<RecordedCacheDataSurface> {
 public:
  explicit RecordedCacheDataSurface(gfx::SourceSurface* aSurface)
      : RecordedEventDerived(CACHE_DATA_SURFACE), mSurface(aSurface) {}

  template <class S>
  MOZ_IMPLICIT RecordedCacheDataSurface(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "RecordedCacheDataSurface"; }

 private:
  ReferencePtr mSurface;
};

inline bool RecordedCacheDataSurface::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  gfx::SourceSurface* surface = aTranslator->LookupSourceSurface(mSurface);
  if (!surface) {
    return false;
  }

  RefPtr<gfx::DataSourceSurface> dataSurface = surface->GetDataSurface();

  aTranslator->AddDataSurface(mSurface, std::move(dataSurface));
  return true;
}

template <class S>
void RecordedCacheDataSurface::Record(S& aStream) const {
  WriteElement(aStream, mSurface);
}

template <class S>
RecordedCacheDataSurface::RecordedCacheDataSurface(S& aStream)
    : RecordedEventDerived(CACHE_DATA_SURFACE) {
  ReadElement(aStream, mSurface);
}

class RecordedPrepareDataForSurface final
    : public RecordedEventDerived<RecordedPrepareDataForSurface> {
 public:
  explicit RecordedPrepareDataForSurface(const gfx::SourceSurface* aSurface)
      : RecordedEventDerived(PREPARE_DATA_FOR_SURFACE), mSurface(aSurface) {}

  template <class S>
  MOZ_IMPLICIT RecordedPrepareDataForSurface(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "RecordedPrepareDataForSurface"; }

 private:
  ReferencePtr mSurface;
};

inline bool RecordedPrepareDataForSurface::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  RefPtr<gfx::DataSourceSurface> dataSurface =
      aTranslator->LookupDataSurface(mSurface);
  if (!dataSurface) {
    gfx::SourceSurface* surface = aTranslator->LookupSourceSurface(mSurface);
    if (!surface) {
      return false;
    }

    dataSurface = surface->GetDataSurface();
    if (!dataSurface) {
      return false;
    }
  }

  auto preparedMap = MakeUnique<gfx::DataSourceSurface::ScopedMap>(
      dataSurface, gfx::DataSourceSurface::READ);
  if (!preparedMap->IsMapped()) {
    return false;
  }

  aTranslator->SetPreparedMap(mSurface, std::move(preparedMap));

  return true;
}

template <class S>
void RecordedPrepareDataForSurface::Record(S& aStream) const {
  WriteElement(aStream, mSurface);
}

template <class S>
RecordedPrepareDataForSurface::RecordedPrepareDataForSurface(S& aStream)
    : RecordedEventDerived(PREPARE_DATA_FOR_SURFACE) {
  ReadElement(aStream, mSurface);
}

class RecordedGetDataForSurface final
    : public RecordedEventDerived<RecordedGetDataForSurface> {
 public:
  explicit RecordedGetDataForSurface(const gfx::SourceSurface* aSurface)
      : RecordedEventDerived(GET_DATA_FOR_SURFACE), mSurface(aSurface) {}

  template <class S>
  MOZ_IMPLICIT RecordedGetDataForSurface(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "RecordedGetDataForSurface"; }

 private:
  ReferencePtr mSurface;
};

inline bool RecordedGetDataForSurface::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  aTranslator->GetDataSurface(mSurface.mLongPtr);
  return true;
}

template <class S>
void RecordedGetDataForSurface::Record(S& aStream) const {
  WriteElement(aStream, mSurface);
}

template <class S>
RecordedGetDataForSurface::RecordedGetDataForSurface(S& aStream)
    : RecordedEventDerived(GET_DATA_FOR_SURFACE) {
  ReadElement(aStream, mSurface);
}

class RecordedAddSurfaceAlias final
    : public RecordedEventDerived<RecordedAddSurfaceAlias> {
 public:
  RecordedAddSurfaceAlias(ReferencePtr aSurfaceAlias,
                          const RefPtr<gfx::SourceSurface>& aActualSurface)
      : RecordedEventDerived(ADD_SURFACE_ALIAS),
        mSurfaceAlias(aSurfaceAlias),
        mActualSurface(aActualSurface) {}

  template <class S>
  MOZ_IMPLICIT RecordedAddSurfaceAlias(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "RecordedAddSurfaceAlias"; }

 private:
  ReferencePtr mSurfaceAlias;
  ReferencePtr mActualSurface;
};

inline bool RecordedAddSurfaceAlias::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  RefPtr<gfx::SourceSurface> surface =
      aTranslator->LookupSourceSurface(mActualSurface);
  if (!surface) {
    return false;
  }

  aTranslator->AddSourceSurface(mSurfaceAlias, surface);
  return true;
}

template <class S>
void RecordedAddSurfaceAlias::Record(S& aStream) const {
  WriteElement(aStream, mSurfaceAlias);
  WriteElement(aStream, mActualSurface);
}

template <class S>
RecordedAddSurfaceAlias::RecordedAddSurfaceAlias(S& aStream)
    : RecordedEventDerived(ADD_SURFACE_ALIAS) {
  ReadElement(aStream, mSurfaceAlias);
  ReadElement(aStream, mActualSurface);
}

class RecordedRemoveSurfaceAlias final
    : public RecordedEventDerived<RecordedRemoveSurfaceAlias> {
 public:
  explicit RecordedRemoveSurfaceAlias(ReferencePtr aSurfaceAlias)
      : RecordedEventDerived(REMOVE_SURFACE_ALIAS),
        mSurfaceAlias(aSurfaceAlias) {}

  template <class S>
  MOZ_IMPLICIT RecordedRemoveSurfaceAlias(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "RecordedRemoveSurfaceAlias"; }

 private:
  ReferencePtr mSurfaceAlias;
};

inline bool RecordedRemoveSurfaceAlias::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  aTranslator->RemoveSourceSurface(mSurfaceAlias);
  return true;
}

template <class S>
void RecordedRemoveSurfaceAlias::Record(S& aStream) const {
  WriteElement(aStream, mSurfaceAlias);
}

template <class S>
RecordedRemoveSurfaceAlias::RecordedRemoveSurfaceAlias(S& aStream)
    : RecordedEventDerived(REMOVE_SURFACE_ALIAS) {
  ReadElement(aStream, mSurfaceAlias);
}

class RecordedDeviceChangeAcknowledged final
    : public RecordedEventDerived<RecordedDeviceChangeAcknowledged> {
 public:
  RecordedDeviceChangeAcknowledged()
      : RecordedEventDerived(DEVICE_CHANGE_ACKNOWLEDGED) {}

  template <class S>
  MOZ_IMPLICIT RecordedDeviceChangeAcknowledged(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final {
    return "RecordedDeviceChangeAcknowledged";
  }
};

inline bool RecordedDeviceChangeAcknowledged::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  aTranslator->DeviceChangeAcknowledged();
  return true;
}

template <class S>
void RecordedDeviceChangeAcknowledged::Record(S& aStream) const {}

template <class S>
RecordedDeviceChangeAcknowledged::RecordedDeviceChangeAcknowledged(S& aStream)
    : RecordedEventDerived(DEVICE_CHANGE_ACKNOWLEDGED) {}

class RecordedCanvasDrawTargetCreation final
    : public RecordedEventDerived<RecordedCanvasDrawTargetCreation> {
 public:
  RecordedCanvasDrawTargetCreation(ReferencePtr aRefPtr, int64_t aTextureId,
                                   RemoteTextureOwnerId aTextureOwnerId,
                                   BackendType aType, const IntSize& aSize,
                                   SurfaceFormat aFormat)
      : RecordedEventDerived(CANVAS_DRAW_TARGET_CREATION),
        mRefPtr(aRefPtr),
        mTextureId(aTextureId),
        mTextureOwnerId(aTextureOwnerId),
        mBackendType(aType),
        mSize(aSize),
        mFormat(aFormat) {}

  template <class S>
  MOZ_IMPLICIT RecordedCanvasDrawTargetCreation(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "Canvas DrawTarget Creation"; }

  ReferencePtr mRefPtr;
  int64_t mTextureId = -1;
  RemoteTextureOwnerId mTextureOwnerId;
  BackendType mBackendType = BackendType::NONE;
  IntSize mSize;
  SurfaceFormat mFormat = SurfaceFormat::UNKNOWN;
};

inline bool RecordedCanvasDrawTargetCreation::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  RefPtr<DrawTarget> newDT = aTranslator->CreateDrawTarget(
      mRefPtr, mTextureId, mTextureOwnerId, mSize, mFormat);
  if (newDT) {
    aTranslator->SetCurrentDrawTarget(mRefPtr);
  }

  // If we couldn't create a DrawTarget this will probably cause us to crash
  // with nullptr later in the playback, so return false to abort.
  return !!newDT;
}

template <class S>
void RecordedCanvasDrawTargetCreation::Record(S& aStream) const {
  WriteElement(aStream, mRefPtr);
  WriteElement(aStream, mTextureId);
  WriteElement(aStream, mTextureOwnerId.mId);
  WriteElement(aStream, mBackendType);
  WriteElement(aStream, mSize);
  WriteElement(aStream, mFormat);
}

template <class S>
RecordedCanvasDrawTargetCreation::RecordedCanvasDrawTargetCreation(S& aStream)
    : RecordedEventDerived(CANVAS_DRAW_TARGET_CREATION) {
  ReadElement(aStream, mRefPtr);
  ReadElement(aStream, mTextureId);
  ReadElement(aStream, mTextureOwnerId.mId);
  ReadElementConstrained(aStream, mBackendType, BackendType::NONE,
                         BackendType::WEBRENDER_TEXT);
  ReadElement(aStream, mSize);
  ReadElementConstrained(aStream, mFormat, SurfaceFormat::A8R8G8B8_UINT32,
                         SurfaceFormat::UNKNOWN);
}

class RecordedTextureDestruction final
    : public RecordedEventDerived<RecordedTextureDestruction> {
 public:
  RecordedTextureDestruction(int64_t aTextureId, RemoteTextureTxnType aTxnType,
                             RemoteTextureTxnId aTxnId)
      : RecordedEventDerived(TEXTURE_DESTRUCTION),
        mTextureId(aTextureId),
        mTxnType(aTxnType),
        mTxnId(aTxnId) {}

  template <class S>
  MOZ_IMPLICIT RecordedTextureDestruction(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "RecordedTextureDestruction"; }

 private:
  int64_t mTextureId = 0;
  RemoteTextureTxnType mTxnType = 0;
  RemoteTextureTxnId mTxnId = 0;
};

inline bool RecordedTextureDestruction::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  aTranslator->RemoveTexture(mTextureId, mTxnType, mTxnId);
  return true;
}

template <class S>
void RecordedTextureDestruction::Record(S& aStream) const {
  WriteElement(aStream, mTextureId);
  WriteElement(aStream, mTxnType);
  WriteElement(aStream, mTxnId);
}

template <class S>
RecordedTextureDestruction::RecordedTextureDestruction(S& aStream)
    : RecordedEventDerived(TEXTURE_DESTRUCTION) {
  ReadElement(aStream, mTextureId);
  ReadElement(aStream, mTxnType);
  ReadElement(aStream, mTxnId);
}

class RecordedCheckpoint final
    : public RecordedEventDerived<RecordedCheckpoint> {
 public:
  RecordedCheckpoint() : RecordedEventDerived(CHECKPOINT) {}

  template <class S>
  MOZ_IMPLICIT RecordedCheckpoint(S& aStream)
      : RecordedEventDerived(CHECKPOINT) {}

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const {
    aTranslator->CheckpointReached();
    return true;
  }

  template <class S>
  void Record(S& aStream) const {}

  std::string GetName() const final { return "RecordedCheckpoint"; }
};

class RecordedPauseTranslation final
    : public RecordedEventDerived<RecordedPauseTranslation> {
 public:
  RecordedPauseTranslation() : RecordedEventDerived(PAUSE_TRANSLATION) {}

  template <class S>
  MOZ_IMPLICIT RecordedPauseTranslation(S& aStream)
      : RecordedEventDerived(PAUSE_TRANSLATION) {}

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const {
    aTranslator->PauseTranslation();
    return true;
  }

  template <class S>
  void Record(S& aStream) const {}

  std::string GetName() const final { return "RecordedPauseTranslation"; }
};

class RecordedRecycleBuffer final
    : public RecordedEventDerived<RecordedRecycleBuffer> {
 public:
  RecordedRecycleBuffer() : RecordedEventDerived(RECYCLE_BUFFER) {}

  template <class S>
  MOZ_IMPLICIT RecordedRecycleBuffer(S& aStream)
      : RecordedEventDerived(RECYCLE_BUFFER) {}

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const {
    aTranslator->RecycleBuffer();
    return true;
  }

  template <class S>
  void Record(S& aStream) const {}

  std::string GetName() const final { return "RecordedNextBuffer"; }
};

class RecordedDropBuffer final
    : public RecordedEventDerived<RecordedDropBuffer> {
 public:
  RecordedDropBuffer() : RecordedEventDerived(DROP_BUFFER) {}

  template <class S>
  MOZ_IMPLICIT RecordedDropBuffer(S& aStream)
      : RecordedEventDerived(DROP_BUFFER) {}

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const {
    // Use the next buffer without recycling which drops the current buffer.
    aTranslator->NextBuffer();
    return true;
  }

  template <class S>
  void Record(S& aStream) const {}

  std::string GetName() const final { return "RecordedDropAndMoveNextBuffer"; }
};

class RecordedPrepareShmem final
    : public RecordedEventDerived<RecordedPrepareShmem> {
 public:
  explicit RecordedPrepareShmem(int64_t aTextureId)
      : RecordedEventDerived(PREPARE_SHMEM), mTextureId(aTextureId) {}

  template <class S>
  MOZ_IMPLICIT RecordedPrepareShmem(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "RecordedPrepareShmem"; }

 private:
  int64_t mTextureId = 0;
};

inline bool RecordedPrepareShmem::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  aTranslator->PrepareShmem(mTextureId);
  return true;
}

template <class S>
void RecordedPrepareShmem::Record(S& aStream) const {
  WriteElement(aStream, mTextureId);
}

template <class S>
RecordedPrepareShmem::RecordedPrepareShmem(S& aStream)
    : RecordedEventDerived(PREPARE_SHMEM) {
  ReadElement(aStream, mTextureId);
}

class RecordedPresentTexture final
    : public RecordedEventDerived<RecordedPresentTexture> {
 public:
  RecordedPresentTexture(int64_t aTextureId, RemoteTextureId aId)
      : RecordedEventDerived(PRESENT_TEXTURE),
        mTextureId(aTextureId),
        mLastRemoteTextureId(aId) {}

  template <class S>
  MOZ_IMPLICIT RecordedPresentTexture(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "PresentTexture"; }

 private:
  int64_t mTextureId;
  RemoteTextureId mLastRemoteTextureId;
};

inline bool RecordedPresentTexture::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  if (!aTranslator->PresentTexture(mTextureId, mLastRemoteTextureId)) {
    return false;
  }
  return true;
}

template <class S>
void RecordedPresentTexture::Record(S& aStream) const {
  WriteElement(aStream, mTextureId);
  WriteElement(aStream, mLastRemoteTextureId.mId);
}

template <class S>
RecordedPresentTexture::RecordedPresentTexture(S& aStream)
    : RecordedEventDerived(PRESENT_TEXTURE) {
  ReadElement(aStream, mTextureId);
  ReadElement(aStream, mLastRemoteTextureId.mId);
}

#define FOR_EACH_CANVAS_EVENT(f)                                    \
  f(CANVAS_BEGIN_TRANSACTION, RecordedCanvasBeginTransaction);      \
  f(CANVAS_END_TRANSACTION, RecordedCanvasEndTransaction);          \
  f(CANVAS_FLUSH, RecordedCanvasFlush);                             \
  f(TEXTURE_LOCK, RecordedTextureLock);                             \
  f(TEXTURE_UNLOCK, RecordedTextureUnlock);                         \
  f(CACHE_DATA_SURFACE, RecordedCacheDataSurface);                  \
  f(PREPARE_DATA_FOR_SURFACE, RecordedPrepareDataForSurface);       \
  f(GET_DATA_FOR_SURFACE, RecordedGetDataForSurface);               \
  f(ADD_SURFACE_ALIAS, RecordedAddSurfaceAlias);                    \
  f(REMOVE_SURFACE_ALIAS, RecordedRemoveSurfaceAlias);              \
  f(DEVICE_CHANGE_ACKNOWLEDGED, RecordedDeviceChangeAcknowledged);  \
  f(CANVAS_DRAW_TARGET_CREATION, RecordedCanvasDrawTargetCreation); \
  f(TEXTURE_DESTRUCTION, RecordedTextureDestruction);               \
  f(CHECKPOINT, RecordedCheckpoint);                                \
  f(PAUSE_TRANSLATION, RecordedPauseTranslation);                   \
  f(RECYCLE_BUFFER, RecordedRecycleBuffer);                         \
  f(DROP_BUFFER, RecordedDropBuffer);                               \
  f(PREPARE_SHMEM, RecordedPrepareShmem);                           \
  f(PRESENT_TEXTURE, RecordedPresentTexture);

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_RecordedCanvasEventImpl_h
