/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RecordedEventImpl.h"

#include "PathRecording.h"
#include "RecordingTypes.h"
#include "Tools.h"
#include "Filters.h"
#include "Logging.h"
#include "ScaledFontBase.h"
#include "SFNTData.h"

namespace mozilla {
namespace gfx {

using namespace std;

RecordedEvent* RecordedEvent::LoadEventFromStream(std::istream& aStream,
                                                  EventType aType) {
  return LoadEvent(aStream, aType);
}

RecordedEvent* RecordedEvent::LoadEventFromStream(EventStream& aStream,
                                                  EventType aType) {
  return LoadEvent(aStream, aType);
}

string RecordedEvent::GetEventName(EventType aType) {
  switch (aType) {
    case DRAWTARGETCREATION:
      return "DrawTarget Creation";
    case DRAWTARGETDESTRUCTION:
      return "DrawTarget Destruction";
    case FILLRECT:
      return "FillRect";
    case STROKERECT:
      return "StrokeRect";
    case STROKELINE:
      return "StrokeLine";
    case CLEARRECT:
      return "ClearRect";
    case COPYSURFACE:
      return "CopySurface";
    case SETTRANSFORM:
      return "SetTransform";
    case PUSHCLIP:
      return "PushClip";
    case PUSHCLIPRECT:
      return "PushClipRect";
    case POPCLIP:
      return "PopClip";
    case FILL:
      return "Fill";
    case FILLGLYPHS:
      return "FillGlyphs";
    case MASK:
      return "Mask";
    case STROKE:
      return "Stroke";
    case DRAWSURFACE:
      return "DrawSurface";
    case DRAWDEPENDENTSURFACE:
      return "DrawDependentSurface";
    case DRAWSURFACEWITHSHADOW:
      return "DrawSurfaceWithShadow";
    case DRAWFILTER:
      return "DrawFilter";
    case PATHCREATION:
      return "PathCreation";
    case PATHDESTRUCTION:
      return "PathDestruction";
    case SOURCESURFACECREATION:
      return "SourceSurfaceCreation";
    case SOURCESURFACEDESTRUCTION:
      return "SourceSurfaceDestruction";
    case FILTERNODECREATION:
      return "FilterNodeCreation";
    case FILTERNODEDESTRUCTION:
      return "FilterNodeDestruction";
    case GRADIENTSTOPSCREATION:
      return "GradientStopsCreation";
    case GRADIENTSTOPSDESTRUCTION:
      return "GradientStopsDestruction";
    case SNAPSHOT:
      return "Snapshot";
    case SCALEDFONTCREATION:
      return "ScaledFontCreation";
    case SCALEDFONTDESTRUCTION:
      return "ScaledFontDestruction";
    case MASKSURFACE:
      return "MaskSurface";
    case FILTERNODESETATTRIBUTE:
      return "SetAttribute";
    case FILTERNODESETINPUT:
      return "SetInput";
    case CREATESIMILARDRAWTARGET:
      return "CreateSimilarDrawTarget";
    case FONTDATA:
      return "FontData";
    case FONTDESC:
      return "FontDescriptor";
    case PUSHLAYER:
      return "PushLayer";
    case POPLAYER:
      return "PopLayer";
    case UNSCALEDFONTCREATION:
      return "UnscaledFontCreation";
    case UNSCALEDFONTDESTRUCTION:
      return "UnscaledFontDestruction";
    case EXTERNALSURFACECREATION:
      return "ExternalSourceSurfaceCreation";
    default:
      return "Unknown";
  }
}

template <class S>
void RecordedEvent::RecordUnscaledFontImpl(UnscaledFont* aUnscaledFont,
                                           S& aOutput) {
  RecordedFontData fontData(aUnscaledFont);
  RecordedFontDetails fontDetails;
  if (fontData.GetFontDetails(fontDetails)) {
    // Try to serialise the whole font, just in case this is a web font that
    // is not present on the system.
    WriteElement(aOutput, fontData.mType);
    fontData.RecordToStream(aOutput);

    auto r = RecordedUnscaledFontCreation(aUnscaledFont, fontDetails);
    WriteElement(aOutput, r.mType);
    r.RecordToStream(aOutput);
  } else {
    // If that fails, record just the font description and try to load it from
    // the system on the other side.
    RecordedFontDescriptor fontDesc(aUnscaledFont);
    if (fontDesc.IsValid()) {
      WriteElement(aOutput, fontDesc.RecordedEvent::mType);
      fontDesc.RecordToStream(aOutput);
    } else {
      gfxWarning()
          << "DrawTargetRecording::FillGlyphs failed to serialise UnscaledFont";
    }
  }
}

void RecordedEvent::RecordUnscaledFont(UnscaledFont* aUnscaledFont,
                                       std::ostream* aOutput) {
  RecordUnscaledFontImpl(aUnscaledFont, *aOutput);
}

void RecordedEvent::RecordUnscaledFont(UnscaledFont* aUnscaledFont,
                                       MemStream& aOutput) {
  RecordUnscaledFontImpl(aUnscaledFont, aOutput);
}

already_AddRefed<DrawTarget> Translator::CreateDrawTarget(
    ReferencePtr aRefPtr, const IntSize& aSize, SurfaceFormat aFormat) {
  RefPtr<DrawTarget> newDT =
      GetReferenceDrawTarget()->CreateSimilarDrawTarget(aSize, aFormat);
  AddDrawTarget(aRefPtr, newDT);
  return newDT.forget();
}

}  // namespace gfx
}  // namespace mozilla
