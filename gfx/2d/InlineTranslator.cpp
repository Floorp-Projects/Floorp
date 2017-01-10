/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InlineTranslator.h"

#include "gfxContext.h"
#include "nsDeviceContext.h"
#include "mozilla/gfx/RecordedEvent.h"
#include "mozilla/gfx/RecordingTypes.h"
#include "mozilla/UniquePtr.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gfx {

InlineTranslator::InlineTranslator(DrawTarget* aDT, Matrix aMatrix)
{
  mBaseDT = aDT;
  mBaseTransform = aMatrix;
}

bool
InlineTranslator::TranslateRecording(std::istream& aRecording)
{
  uint32_t magicInt;
  ReadElement(aRecording, magicInt);
  if (magicInt != mozilla::gfx::kMagicInt) {
    return false;
  }

  uint16_t majorRevision;
  ReadElement(aRecording, majorRevision);
  if (majorRevision != kMajorRevision) {
    return false;
  }

  uint16_t minorRevision;
  ReadElement(aRecording, minorRevision);
  if (minorRevision > kMinorRevision) {
    return false;
  }

  int32_t eventType;
  ReadElement(aRecording, eventType);
  while (aRecording.good()) {
    UniquePtr<RecordedEvent> recordedEvent(
      RecordedEvent::LoadEventFromStream(aRecording,
      static_cast<RecordedEvent::EventType>(eventType)));

    // Make sure that the whole event was read from the stream successfully.
    if (!aRecording.good() || !recordedEvent) {
      return false;
    }

    if (recordedEvent->GetType() == RecordedEvent::SETTRANSFORM) {
      RecordedSetTransform* event = static_cast<RecordedSetTransform*>(recordedEvent.get());
      mBaseDT->SetTransform(event->mTransform * mBaseTransform);
    } else {
      if (!recordedEvent->PlayEvent(this)) {
        return false;
      }
    }

    ReadElement(aRecording, eventType);
  }

  return true;
}

already_AddRefed<DrawTarget>
InlineTranslator::CreateDrawTarget(ReferencePtr aRefPtr,
                                  const gfx::IntSize &aSize,
                                  gfx::SurfaceFormat aFormat)
{
  RefPtr<DrawTarget> drawTarget = mBaseDT;
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
