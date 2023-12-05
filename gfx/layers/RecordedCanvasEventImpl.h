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
const EventType NEXT_TEXTURE_ID = EventType(EventType::LAST + 11);
const EventType TEXTURE_DESTRUCTION = EventType(EventType::LAST + 12);

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
  RecordedTextureLock(int64_t aTextureId, const OpenMode aMode)
      : RecordedEventDerived(TEXTURE_LOCK),
        mTextureId(aTextureId),
        mMode(aMode) {}

  template <class S>
  MOZ_IMPLICIT RecordedTextureLock(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "TextureLock"; }

 private:
  int64_t mTextureId;
  OpenMode mMode;
};

inline bool RecordedTextureLock::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  TextureData* textureData = aTranslator->LookupTextureData(mTextureId);
  if (!textureData) {
    return false;
  }

  textureData->Lock(mMode);
  return true;
}

template <class S>
void RecordedTextureLock::Record(S& aStream) const {
  WriteElement(aStream, mTextureId);
  WriteElement(aStream, mMode);
}

template <class S>
RecordedTextureLock::RecordedTextureLock(S& aStream)
    : RecordedEventDerived(TEXTURE_LOCK) {
  ReadElement(aStream, mTextureId);
  ReadElementConstrained(aStream, mMode, OpenMode::OPEN_NONE,
                         OpenMode::OPEN_READ_WRITE_ASYNC);
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
  TextureData* textureData = aTranslator->LookupTextureData(mTextureId);
  if (!textureData) {
    return false;
  }

  textureData->Unlock();
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
  gfx::SourceSurface* surface = aTranslator->LookupSourceSurface(mSurface);
  if (!surface) {
    return false;
  }

  UniquePtr<gfx::DataSourceSurface::ScopedMap> map =
      aTranslator->GetPreparedMap(mSurface);
  if (!map) {
    return false;
  }

  int32_t dataFormatWidth =
      surface->GetSize().width * BytesPerPixel(surface->GetFormat());
  int32_t srcStride = map->GetStride();
  if (dataFormatWidth > srcStride) {
    return false;
  }

  char* src = reinterpret_cast<char*>(map->GetData());
  char* endSrc = src + (map->GetSurface()->GetSize().height * srcStride);
  while (src < endSrc) {
    aTranslator->ReturnWrite(src, dataFormatWidth);
    src += srcStride;
  }

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

class RecordedNextTextureId final
    : public RecordedEventDerived<RecordedNextTextureId> {
 public:
  explicit RecordedNextTextureId(int64_t aNextTextureId)
      : RecordedEventDerived(NEXT_TEXTURE_ID), mNextTextureId(aNextTextureId) {}

  template <class S>
  MOZ_IMPLICIT RecordedNextTextureId(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "RecordedNextTextureId"; }

 private:
  int64_t mNextTextureId;
};

inline bool RecordedNextTextureId::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  aTranslator->SetNextTextureId(mNextTextureId);
  return true;
}

template <class S>
void RecordedNextTextureId::Record(S& aStream) const {
  WriteElement(aStream, mNextTextureId);
}

template <class S>
RecordedNextTextureId::RecordedNextTextureId(S& aStream)
    : RecordedEventDerived(NEXT_TEXTURE_ID) {
  ReadElement(aStream, mNextTextureId);
}

class RecordedTextureDestruction final
    : public RecordedEventDerived<RecordedTextureDestruction> {
 public:
  explicit RecordedTextureDestruction(int64_t aTextureId)
      : RecordedEventDerived(TEXTURE_DESTRUCTION), mTextureId(aTextureId) {}

  template <class S>
  MOZ_IMPLICIT RecordedTextureDestruction(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "RecordedTextureDestruction"; }

 private:
  int64_t mTextureId;
};

inline bool RecordedTextureDestruction::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  aTranslator->RemoveTexture(mTextureId);
  return true;
}

template <class S>
void RecordedTextureDestruction::Record(S& aStream) const {
  WriteElement(aStream, mTextureId);
}

template <class S>
RecordedTextureDestruction::RecordedTextureDestruction(S& aStream)
    : RecordedEventDerived(TEXTURE_DESTRUCTION) {
  ReadElement(aStream, mTextureId);
}

#define FOR_EACH_CANVAS_EVENT(f)                                   \
  f(CANVAS_BEGIN_TRANSACTION, RecordedCanvasBeginTransaction);     \
  f(CANVAS_END_TRANSACTION, RecordedCanvasEndTransaction);         \
  f(CANVAS_FLUSH, RecordedCanvasFlush);                            \
  f(TEXTURE_LOCK, RecordedTextureLock);                            \
  f(TEXTURE_UNLOCK, RecordedTextureUnlock);                        \
  f(CACHE_DATA_SURFACE, RecordedCacheDataSurface);                 \
  f(PREPARE_DATA_FOR_SURFACE, RecordedPrepareDataForSurface);      \
  f(GET_DATA_FOR_SURFACE, RecordedGetDataForSurface);              \
  f(ADD_SURFACE_ALIAS, RecordedAddSurfaceAlias);                   \
  f(REMOVE_SURFACE_ALIAS, RecordedRemoveSurfaceAlias);             \
  f(DEVICE_CHANGE_ACKNOWLEDGED, RecordedDeviceChangeAcknowledged); \
  f(NEXT_TEXTURE_ID, RecordedNextTextureId);                       \
  f(TEXTURE_DESTRUCTION, RecordedTextureDestruction);

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_RecordedCanvasEventImpl_h
