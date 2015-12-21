/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTranslator.h"

#include "gfxContext.h"
#include "nsDeviceContext.h"
#include "mozilla/gfx/RecordedEvent.h"
#include "mozilla/gfx/RecordingTypes.h"
#include "mozilla/UniquePtr.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layout {

PrintTranslator::PrintTranslator(nsDeviceContext* aDeviceContext)
  : mDeviceContext(aDeviceContext)
{
  RefPtr<gfxContext> context = mDeviceContext->CreateRenderingContext();
  mBaseDT = context->GetDrawTarget();
}

bool
PrintTranslator::TranslateRecording(std::istream& aRecording)
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
    if (!aRecording.good()) {
      return false;
    }

    recordedEvent->PlayEvent(this);
    ReadElement(aRecording, eventType);
  }

  return true;
}

already_AddRefed<DrawTarget>
PrintTranslator::CreateDrawTarget(ReferencePtr aRefPtr,
                                  const gfx::IntSize &aSize,
                                  gfx::SurfaceFormat aFormat)
{
  RefPtr<gfxContext> context = mDeviceContext->CreateRenderingContext();
  if (!context) {
    NS_WARNING("Failed to create rendering context for print.");
    return nullptr;
  }

  RefPtr<DrawTarget> drawTarget = context->GetDrawTarget();
  AddDrawTarget(aRefPtr, drawTarget);
  return drawTarget.forget();
}

FontType
PrintTranslator::GetDesiredFontType()
{
  switch (mBaseDT->GetBackendType()) {
    case BackendType::DIRECT2D:
      return FontType::DWRITE;
    case BackendType::CAIRO:
      return FontType::CAIRO;
    case BackendType::SKIA:
      return FontType::SKIA;
    case BackendType::COREGRAPHICS:
    case BackendType::COREGRAPHICS_ACCELERATED:
      return FontType::COREGRAPHICS;
    default:
      return FontType::CAIRO;
  }
}

} // namespace layout
} // namespace mozilla