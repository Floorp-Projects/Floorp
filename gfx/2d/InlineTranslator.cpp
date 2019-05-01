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
    : mBaseDT(aDT), mFontContext(aFontContext) {}

bool InlineTranslator::TranslateRecording(char* aData, size_t aLen) {
  // an istream like class for reading from memory
  struct MemReader {
    MemReader(char* aData, size_t aLen) : mData(aData), mEnd(aData + aLen) {}
    void read(char* s, std::streamsize n) {
      if (n <= (mEnd - mData)) {
        memcpy(s, mData, n);
        mData += n;
      } else {
        // We've requested more data than is available
        // set the Reader into an eof state
        mData = mEnd + 1;
      }
    }
    bool eof() { return mData > mEnd; }
    bool good() { return !eof(); }

    char* mData;
    char* mEnd;
  };
  MemReader reader(aData, aLen);

  uint32_t magicInt;
  ReadElement(reader, magicInt);
  if (magicInt != mozilla::gfx::kMagicInt) {
    mError = "Magic";
    return false;
  }

  uint16_t majorRevision;
  ReadElement(reader, majorRevision);
  if (majorRevision != kMajorRevision) {
    mError = "Major";
    return false;
  }

  uint16_t minorRevision;
  ReadElement(reader, minorRevision);
  if (minorRevision > kMinorRevision) {
    mError = "Minor";
    return false;
  }

  int32_t eventType;
  ReadElement(reader, eventType);
  while (reader.good()) {
    bool success = RecordedEvent::DoWithEvent(
        reader, static_cast<RecordedEvent::EventType>(eventType),
        [&](RecordedEvent* recordedEvent) {
          // Make sure that the whole event was read from the stream
          // successfully.
          if (!reader.good()) {
            mError = " READ";
            return false;
          }

          if (!recordedEvent->PlayEvent(this)) {
            mError = " PLAY";
            return false;
          }

          return true;
        });
    if (!success) {
      mError = RecordedEvent::GetEventName(
                   static_cast<RecordedEvent::EventType>(eventType)) +
               mError;
      return false;
    }

    ReadElement(reader, eventType);
  }

  return true;
}

already_AddRefed<DrawTarget> InlineTranslator::CreateDrawTarget(
    ReferencePtr aRefPtr, const gfx::IntSize& aSize,
    gfx::SurfaceFormat aFormat) {
  RefPtr<DrawTarget> drawTarget = mBaseDT;
  AddDrawTarget(aRefPtr, drawTarget);
  return drawTarget.forget();
}

}  // namespace gfx
}  // namespace mozilla
