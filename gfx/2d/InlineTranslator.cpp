/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InlineTranslator.h"
#include "RecordedEventImpl.h"
#include "DrawEventRecorder.h"

#include "gfxContext.h"
#include "nsDeviceContext.h"
#include "mozilla/gfx/RecordingTypes.h"
#include "mozilla/UniquePtr.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gfx {

InlineTranslator::InlineTranslator(DrawTarget* aDT, void* aFontContext)
  : mBaseDT(aDT)
  , mFontContext(aFontContext)
{
}

bool
InlineTranslator::TranslateRecording(char *aData, size_t aLen)
{
  // an istream like class for reading from memory
  struct MemReader {
    MemReader(char *aData, size_t aLen) : mData(aData), mEnd(aData + aLen) {}
    void read(char* s, streamsize n) {
      if (n <= (mEnd - mData)) {
        memcpy(s, mData, n);
        mData += n;
      } else {
        // We've requested more data than is available
        // set the Reader into an eof state
        mData = mEnd + 1;
      }
    }
    bool eof() {
      return mData > mEnd;
    }
    bool good() {
      return !eof();
    }

    char *mData;
    char *mEnd;
  };
  MemReader reader(aData, aLen);

  uint32_t magicInt;
  ReadElement(reader, magicInt);
  if (magicInt != mozilla::gfx::kMagicInt) {
    return false;
  }

  uint16_t majorRevision;
  ReadElement(reader, majorRevision);
  if (majorRevision != kMajorRevision) {
    return false;
  }

  uint16_t minorRevision;
  ReadElement(reader, minorRevision);
  if (minorRevision > kMinorRevision) {
    return false;
  }

  int32_t eventType;
  ReadElement(reader, eventType);
  while (reader.good()) {
    UniquePtr<RecordedEvent> recordedEvent(
      RecordedEvent::LoadEvent(reader,
      static_cast<RecordedEvent::EventType>(eventType)));

    // Make sure that the whole event was read from the stream successfully.
    if (!reader.good() || !recordedEvent) {
      return false;
    }

    if (!recordedEvent->PlayEvent(this)) {
      return false;
    }

    ReadElement(reader, eventType);
  }

  return true;
}

already_AddRefed<DrawTarget>
InlineTranslator::CreateDrawTarget(ReferencePtr aRefPtr,
                                  const gfx::IntSize &aSize,
                                  gfx::SurfaceFormat aFormat)
{
  RefPtr<DrawTarget> drawTarget = mBaseDT;
  AddDrawTarget(aRefPtr, drawTarget);
  return drawTarget.forget();
}

FontType
InlineTranslator::GetDesiredFontType()
{
  switch (mBaseDT->GetBackendType()) {
    case BackendType::DIRECT2D:
      return FontType::DWRITE;
    case BackendType::CAIRO:
      return FontType::CAIRO;
    case BackendType::SKIA:
      return FontType::SKIA;
    default:
      return FontType::CAIRO;
  }
}

} // namespace gfx
} // namespace mozilla
