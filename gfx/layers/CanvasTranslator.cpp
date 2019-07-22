/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "CanvasTranslator.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "RecordedCanvasEventImpl.h"

#if defined(XP_WIN)
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/layers/TextureD3D11.h"
#endif

namespace mozilla {
namespace layers {

// When in a transaction we wait for a short time because we're expecting more
// events from the content process. We don't want to wait for too long in case
// other content processes are waiting for events to process.
static const TimeDuration kReadEventTimeout = TimeDuration::FromMilliseconds(5);

static TextureData* CreateTextureData(TextureType aTextureType,
                                      const gfx::IntSize& aSize,
                                      gfx::SurfaceFormat aFormat) {
  TextureData* textureData = nullptr;
  switch (aTextureType) {
#ifdef XP_WIN
    case TextureType::D3D11: {
      RefPtr<ID3D11Device> device =
          gfx::DeviceManagerDx::Get()->GetCanvasDevice();
      MOZ_RELEASE_ASSERT(device, "Failed to get a device for canvas drawing.");
      textureData =
          D3D11TextureData::Create(aSize, aFormat, ALLOC_CLEAR_BUFFER, device);
      break;
    }
#endif
    default:
      MOZ_CRASH("Unsupported TextureType for CanvasTranslator.");
  }

  if (!textureData) {
    MOZ_CRASH("Failed to create TextureData.");
  }

  return textureData;
}

/* static */
UniquePtr<CanvasTranslator> CanvasTranslator::Create(
    const TextureType& aTextureType,
    const ipc::SharedMemoryBasic::Handle& aReadHandle,
    const CrossProcessSemaphoreHandle& aReaderSem,
    const CrossProcessSemaphoreHandle& aWriterSem,
    UniquePtr<CanvasEventRingBuffer::ReaderServices> aReaderServices) {
  TextureData* textureData = CreateTextureData(aTextureType, gfx::IntSize(1, 1),
                                               gfx::SurfaceFormat::B8G8R8A8);
  textureData->Lock(OpenMode::OPEN_READ_WRITE);
  RefPtr<gfx::DrawTarget> dt = textureData->BorrowDrawTarget();
  return UniquePtr<CanvasTranslator>(
      new CanvasTranslator(aTextureType, textureData, dt, aReadHandle,
                           aReaderSem, aWriterSem, std::move(aReaderServices)));
}

CanvasTranslator::CanvasTranslator(
    const TextureType& aTextureType, TextureData* aTextureData,
    gfx::DrawTarget* aDT, const ipc::SharedMemoryBasic::Handle& aReadHandle,
    const CrossProcessSemaphoreHandle& aReaderSem,
    const CrossProcessSemaphoreHandle& aWriterSem,
    UniquePtr<CanvasEventRingBuffer::ReaderServices> aReaderServices)
    : gfx::InlineTranslator(aDT),
      mTextureType(aTextureType),
      mReferenceTextureData(aTextureData) {
  mStream.InitReader(aReadHandle, aReaderSem, aWriterSem,
                     std::move(aReaderServices));
}

CanvasTranslator::~CanvasTranslator() { mReferenceTextureData->Unlock(); }

bool CanvasTranslator::TranslateRecording() {
  int32_t eventType = mStream.ReadNextEvent();
  while (mStream.good()) {
    bool success = RecordedEvent::DoWithEventFromStream(
        mStream, static_cast<RecordedEvent::EventType>(eventType),
        [&](RecordedEvent* recordedEvent) -> bool {
          // Make sure that the whole event was read from the stream.
          if (!mStream.good()) {
            return false;
          }

          return recordedEvent->PlayEvent(this);
        });

    if (!success && !HandleExtensionEvent(eventType)) {
      gfxDevCrash(gfx::LogReason::PlayEventFailed)
          << "Failed to play canvas event type: " << eventType;
      mIsValid = false;
      return true;
    }

    if (!mIsInTransaction) {
      return mStream.StopIfEmpty();
    }

    if (!mStream.HasDataToRead()) {
      // We're going to wait for the next event, so take the opportunity to
      // flush the rendering.
      Flush();
      if (!mStream.WaitForDataToRead(kReadEventTimeout, 0)) {
        return true;
      }
    }

    eventType = mStream.ReadNextEvent();
  }

  mIsValid = false;
  return true;
}

#define READ_AND_PLAY_CANVAS_EVENT_TYPE(_typeenum, _class) \
  case _typeenum: {                                        \
    auto e = _class(mStream);                              \
    if (!mStream.good()) {                                 \
      return false;                                        \
    }                                                      \
    return e.PlayCanvasEvent(this);                        \
  }

bool CanvasTranslator::HandleExtensionEvent(int32_t aType) {
  // This is where we handle extensions to the Moz2D Recording events to handle
  // canvas specific things.
  switch (aType) {
    FOR_EACH_CANVAS_EVENT(READ_AND_PLAY_CANVAS_EVENT_TYPE)
    default:
      return false;
  }
}

void CanvasTranslator::BeginTransaction() { mIsInTransaction = true; }

void CanvasTranslator::Flush() {
#if defined(XP_WIN)
  gfx::AutoSerializeWithMoz2D serializeWithMoz2D(
      GetReferenceDrawTarget()->GetBackendType());
  RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetCanvasDevice();
  RefPtr<ID3D11DeviceContext> deviceContext;
  device->GetImmediateContext(getter_AddRefs(deviceContext));
  deviceContext->Flush();
#endif
}

void CanvasTranslator::EndTransaction() {
  Flush();
  mIsInTransaction = false;
}

void CanvasTranslator::AddSurfaceDescriptor(gfx::ReferencePtr aRefPtr,
                                            TextureData* aTextureData) {
  UniquePtr<SurfaceDescriptor> descriptor = MakeUnique<SurfaceDescriptor>();
  if (!aTextureData->Serialize(*descriptor)) {
    MOZ_CRASH("Failed to serialize");
  }

  MonitorAutoLock lock(mSurfaceDescriptorsMonitor);
  mSurfaceDescriptors[aRefPtr] = std::move(descriptor);
  mSurfaceDescriptorsMonitor.Notify();
}

already_AddRefed<gfx::DrawTarget> CanvasTranslator::CreateDrawTarget(
    gfx::ReferencePtr aRefPtr, const gfx::IntSize& aSize,
    gfx::SurfaceFormat aFormat) {
  gfx::AutoSerializeWithMoz2D serializeWithMoz2D(
      GetReferenceDrawTarget()->GetBackendType());
  TextureData* textureData = CreateTextureData(mTextureType, aSize, aFormat);
  textureData->Lock(OpenMode::OPEN_READ_WRITE);
  mTextureDatas[aRefPtr] = UniquePtr<TextureData>(textureData);
  AddSurfaceDescriptor(aRefPtr, textureData);
  RefPtr<gfx::DrawTarget> dt = textureData->BorrowDrawTarget();
  AddDrawTarget(aRefPtr, dt);

  return dt.forget();
}

void CanvasTranslator::RemoveDrawTarget(gfx::ReferencePtr aDrawTarget) {
  InlineTranslator::RemoveDrawTarget(aDrawTarget);
  gfx::AutoSerializeWithMoz2D serializeWithMoz2D(
      GetReferenceDrawTarget()->GetBackendType());
  mTextureDatas.erase(aDrawTarget);
}

TextureData* CanvasTranslator::LookupTextureData(
    gfx::ReferencePtr aDrawTarget) {
  TextureMap::const_iterator result = mTextureDatas.find(aDrawTarget);
  if (result == mTextureDatas.end()) {
    return nullptr;
  }
  return result->second.get();
}

UniquePtr<SurfaceDescriptor> CanvasTranslator::WaitForSurfaceDescriptor(
    gfx::ReferencePtr aDrawTarget) {
  MonitorAutoLock lock(mSurfaceDescriptorsMonitor);
  DescriptorMap::iterator result;
  while ((result = mSurfaceDescriptors.find(aDrawTarget)) ==
         mSurfaceDescriptors.end()) {
    mSurfaceDescriptorsMonitor.Wait();
  }

  UniquePtr<SurfaceDescriptor> descriptor = std::move(result->second);
  mSurfaceDescriptors.erase(aDrawTarget);
  return descriptor;
}

gfx::DataSourceSurface* CanvasTranslator::LookupDataSurface(
    gfx::ReferencePtr aRefPtr) {
  return mDataSurfaces.GetWeak(aRefPtr);
}

void CanvasTranslator::AddDataSurface(
    gfx::ReferencePtr aRefPtr, RefPtr<gfx::DataSourceSurface>&& aSurface) {
  mDataSurfaces.Put(aRefPtr, aSurface);
}

void CanvasTranslator::RemoveDataSurface(gfx::ReferencePtr aRefPtr) {
  mDataSurfaces.Remove(aRefPtr);
}

void CanvasTranslator::SetPreparedMap(
    gfx::ReferencePtr aSurface,
    UniquePtr<gfx::DataSourceSurface::ScopedMap> aMap) {
  mMappedSurface = aSurface;
  mPreparedMap = std::move(aMap);
}

UniquePtr<gfx::DataSourceSurface::ScopedMap> CanvasTranslator::GetPreparedMap(
    gfx::ReferencePtr aSurface) {
  MOZ_RELEASE_ASSERT(mMappedSurface == aSurface,
                     "aSurface must match previously stored surface.");

  mMappedSurface = nullptr;
  return std::move(mPreparedMap);
}

}  // namespace layers
}  // namespace mozilla
