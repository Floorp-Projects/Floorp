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
  RecordedTextureLock(DrawTarget* aDT, const OpenMode aMode)
      : RecordedEventDerived(TEXTURE_LOCK), mDT(aDT), mMode(aMode) {}

  template <class S>
  MOZ_IMPLICIT RecordedTextureLock(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "TextureLock"; }

 private:
  ReferencePtr mDT;
  OpenMode mMode;
};

inline bool RecordedTextureLock::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  TextureData* textureData = aTranslator->LookupTextureData(mDT);
  if (!textureData) {
    return false;
  }

  gfx::AutoSerializeWithMoz2D serializeWithMoz2D(
      aTranslator->GetReferenceDrawTarget()->GetBackendType());
  textureData->Lock(mMode);
  return true;
}

template <class S>
void RecordedTextureLock::Record(S& aStream) const {
  WriteElement(aStream, mDT);
  WriteElement(aStream, mMode);
}

template <class S>
RecordedTextureLock::RecordedTextureLock(S& aStream)
    : RecordedEventDerived(TEXTURE_LOCK) {
  ReadElement(aStream, mDT);
  ReadElementConstrained(aStream, mMode, OpenMode::OPEN_NONE,
                         OpenMode::OPEN_READ_WRITE_ASYNC);
}

class RecordedTextureUnlock final
    : public RecordedEventDerived<RecordedTextureUnlock> {
 public:
  explicit RecordedTextureUnlock(DrawTarget* aDT)
      : RecordedEventDerived(TEXTURE_UNLOCK), mDT(aDT) {}

  template <class S>
  MOZ_IMPLICIT RecordedTextureUnlock(S& aStream);

  bool PlayCanvasEvent(CanvasTranslator* aTranslator) const;

  template <class S>
  void Record(S& aStream) const;

  std::string GetName() const final { return "TextureUnlock"; }

 private:
  ReferencePtr mDT;
};

inline bool RecordedTextureUnlock::PlayCanvasEvent(
    CanvasTranslator* aTranslator) const {
  TextureData* textureData = aTranslator->LookupTextureData(mDT);
  if (!textureData) {
    return false;
  }

  gfx::AutoSerializeWithMoz2D serializeWithMoz2D(
      aTranslator->GetReferenceDrawTarget()->GetBackendType());
  textureData->Unlock();
  return true;
}

template <class S>
void RecordedTextureUnlock::Record(S& aStream) const {
  WriteElement(aStream, mDT);
}

template <class S>
RecordedTextureUnlock::RecordedTextureUnlock(S& aStream)
    : RecordedEventDerived(TEXTURE_UNLOCK) {
  ReadElement(aStream, mDT);
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
  RefPtr<gfx::SourceSurface> surface =
      aTranslator->LookupSourceSurface(mSurface);

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
    RefPtr<gfx::SourceSurface> surface =
        aTranslator->LookupSourceSurface(mSurface);
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
  RefPtr<gfx::SourceSurface> surface =
      aTranslator->LookupSourceSurface(mSurface);
  if (!surface) {
    return false;
  }

  UniquePtr<gfx::DataSourceSurface::ScopedMap> map =
      aTranslator->GetPreparedMap(mSurface);

  gfx::IntSize ssSize = surface->GetSize();
  size_t dataFormatWidth = ssSize.width * BytesPerPixel(surface->GetFormat());
  int32_t srcStride = map->GetStride();
  char* src = reinterpret_cast<char*>(map->GetData());
  char* endSrc = src + (ssSize.height * srcStride);
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

#define FOR_EACH_CANVAS_EVENT(f)                               \
  f(CANVAS_BEGIN_TRANSACTION, RecordedCanvasBeginTransaction); \
  f(CANVAS_END_TRANSACTION, RecordedCanvasEndTransaction);     \
  f(CANVAS_FLUSH, RecordedCanvasFlush);                        \
  f(TEXTURE_LOCK, RecordedTextureLock);                        \
  f(TEXTURE_UNLOCK, RecordedTextureUnlock);                    \
  f(CACHE_DATA_SURFACE, RecordedCacheDataSurface);             \
  f(PREPARE_DATA_FOR_SURFACE, RecordedPrepareDataForSurface);  \
  f(GET_DATA_FOR_SURFACE, RecordedGetDataForSurface);

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_RecordedCanvasEventImpl_h
