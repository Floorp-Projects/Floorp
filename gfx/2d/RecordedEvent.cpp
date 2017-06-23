/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
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

#define LOAD_EVENT_TYPE(_typeenum, _class) \
  case _typeenum: return new _class(aStream)

RecordedEvent *
RecordedEvent::LoadEventFromStream(std::istream &aStream, EventType aType)
{
  switch (aType) {
    LOAD_EVENT_TYPE(DRAWTARGETCREATION, RecordedDrawTargetCreation);
    LOAD_EVENT_TYPE(DRAWTARGETDESTRUCTION, RecordedDrawTargetDestruction);
    LOAD_EVENT_TYPE(FILLRECT, RecordedFillRect);
    LOAD_EVENT_TYPE(STROKERECT, RecordedStrokeRect);
    LOAD_EVENT_TYPE(STROKELINE, RecordedStrokeLine);
    LOAD_EVENT_TYPE(CLEARRECT, RecordedClearRect);
    LOAD_EVENT_TYPE(COPYSURFACE, RecordedCopySurface);
    LOAD_EVENT_TYPE(SETTRANSFORM, RecordedSetTransform);
    LOAD_EVENT_TYPE(PUSHCLIPRECT, RecordedPushClipRect);
    LOAD_EVENT_TYPE(PUSHCLIP, RecordedPushClip);
    LOAD_EVENT_TYPE(POPCLIP, RecordedPopClip);
    LOAD_EVENT_TYPE(FILL, RecordedFill);
    LOAD_EVENT_TYPE(FILLGLYPHS, RecordedFillGlyphs);
    LOAD_EVENT_TYPE(MASK, RecordedMask);
    LOAD_EVENT_TYPE(STROKE, RecordedStroke);
    LOAD_EVENT_TYPE(DRAWSURFACE, RecordedDrawSurface);
    LOAD_EVENT_TYPE(DRAWSURFACEWITHSHADOW, RecordedDrawSurfaceWithShadow);
    LOAD_EVENT_TYPE(DRAWFILTER, RecordedDrawFilter);
    LOAD_EVENT_TYPE(PATHCREATION, RecordedPathCreation);
    LOAD_EVENT_TYPE(PATHDESTRUCTION, RecordedPathDestruction);
    LOAD_EVENT_TYPE(SOURCESURFACECREATION, RecordedSourceSurfaceCreation);
    LOAD_EVENT_TYPE(SOURCESURFACEDESTRUCTION, RecordedSourceSurfaceDestruction);
    LOAD_EVENT_TYPE(FILTERNODECREATION, RecordedFilterNodeCreation);
    LOAD_EVENT_TYPE(FILTERNODEDESTRUCTION, RecordedFilterNodeDestruction);
    LOAD_EVENT_TYPE(GRADIENTSTOPSCREATION, RecordedGradientStopsCreation);
    LOAD_EVENT_TYPE(GRADIENTSTOPSDESTRUCTION, RecordedGradientStopsDestruction);
    LOAD_EVENT_TYPE(SNAPSHOT, RecordedSnapshot);
    LOAD_EVENT_TYPE(SCALEDFONTCREATION, RecordedScaledFontCreation);
    LOAD_EVENT_TYPE(SCALEDFONTDESTRUCTION, RecordedScaledFontDestruction);
    LOAD_EVENT_TYPE(MASKSURFACE, RecordedMaskSurface);
    LOAD_EVENT_TYPE(FILTERNODESETATTRIBUTE, RecordedFilterNodeSetAttribute);
    LOAD_EVENT_TYPE(FILTERNODESETINPUT, RecordedFilterNodeSetInput);
    LOAD_EVENT_TYPE(CREATESIMILARDRAWTARGET, RecordedCreateSimilarDrawTarget);
    LOAD_EVENT_TYPE(FONTDATA, RecordedFontData);
    LOAD_EVENT_TYPE(FONTDESC, RecordedFontDescriptor);
    LOAD_EVENT_TYPE(PUSHLAYER, RecordedPushLayer);
    LOAD_EVENT_TYPE(POPLAYER, RecordedPopLayer);
    LOAD_EVENT_TYPE(UNSCALEDFONTCREATION, RecordedUnscaledFontCreation);
    LOAD_EVENT_TYPE(UNSCALEDFONTDESTRUCTION, RecordedUnscaledFontDestruction);
  default:
    return nullptr;
  }
}

string
RecordedEvent::GetEventName(EventType aType)
{
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
  default:
    return "Unknown";
  }
}

already_AddRefed<DrawTarget>
Translator::CreateDrawTarget(ReferencePtr aRefPtr, const IntSize &aSize,
                             SurfaceFormat aFormat)
{
  RefPtr<DrawTarget> newDT =
    GetReferenceDrawTarget()->CreateSimilarDrawTarget(aSize, aFormat);
  AddDrawTarget(aRefPtr, newDT);
  return newDT.forget();
}

}
}
