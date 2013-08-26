/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
// This is also necessary to ensure our definition of M_SQRT1_2 is picked up
#include "nsSVGUtils.h"
#include <algorithm>

// Keep others in (case-insensitive) order:
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxMatrix.h"
#include "gfxPlatform.h"
#include "gfxRect.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Preferences.h"
#include "nsCSSFrameConstructor.h"
#include "nsDisplayList.h"
#include "nsFrameList.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsINameSpaceManager.h"
#include "nsIPresShell.h"
#include "nsISVGChildFrame.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsStyleCoord.h"
#include "nsStyleStruct.h"
#include "nsSVGClipPathFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsSVGEffects.h"
#include "nsSVGFilterFrame.h"
#include "nsSVGFilterPaintCallback.h"
#include "nsSVGForeignObjectFrame.h"
#include "nsSVGGeometryFrame.h"
#include "nsSVGGlyphFrame.h"
#include "nsSVGInnerSVGFrame.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGLength2.h"
#include "nsSVGMaskFrame.h"
#include "nsSVGOuterSVGFrame.h"
#include "mozilla/dom/SVGPathElement.h"
#include "nsSVGPathGeometryElement.h"
#include "nsSVGPathGeometryFrame.h"
#include "nsSVGPaintServerFrame.h"
#include "mozilla/dom/SVGSVGElement.h"
#include "nsSVGTextContainerFrame.h"
#include "nsTextFrame.h"
#include "SVGContentUtils.h"
#include "mozilla/unused.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

// c = n / 255
// (c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1 / 2.4) - 0.055) * 255 + 0.5
static const uint8_t glinearRGBTosRGBMap[256] = {
  0,  13,  22,  28,  34,  38,  42,  46,
 50,  53,  56,  59,  61,  64,  66,  69,
 71,  73,  75,  77,  79,  81,  83,  85,
 86,  88,  90,  92,  93,  95,  96,  98,
 99, 101, 102, 104, 105, 106, 108, 109,
110, 112, 113, 114, 115, 117, 118, 119,
120, 121, 122, 124, 125, 126, 127, 128,
129, 130, 131, 132, 133, 134, 135, 136,
137, 138, 139, 140, 141, 142, 143, 144,
145, 146, 147, 148, 148, 149, 150, 151,
152, 153, 154, 155, 155, 156, 157, 158,
159, 159, 160, 161, 162, 163, 163, 164,
165, 166, 167, 167, 168, 169, 170, 170,
171, 172, 173, 173, 174, 175, 175, 176,
177, 178, 178, 179, 180, 180, 181, 182,
182, 183, 184, 185, 185, 186, 187, 187,
188, 189, 189, 190, 190, 191, 192, 192,
193, 194, 194, 195, 196, 196, 197, 197,
198, 199, 199, 200, 200, 201, 202, 202,
203, 203, 204, 205, 205, 206, 206, 207,
208, 208, 209, 209, 210, 210, 211, 212,
212, 213, 213, 214, 214, 215, 215, 216,
216, 217, 218, 218, 219, 219, 220, 220,
221, 221, 222, 222, 223, 223, 224, 224,
225, 226, 226, 227, 227, 228, 228, 229,
229, 230, 230, 231, 231, 232, 232, 233,
233, 234, 234, 235, 235, 236, 236, 237,
237, 238, 238, 238, 239, 239, 240, 240,
241, 241, 242, 242, 243, 243, 244, 244,
245, 245, 246, 246, 246, 247, 247, 248,
248, 249, 249, 250, 250, 251, 251, 251,
252, 252, 253, 253, 254, 254, 255, 255
};

// c = n / 255
// c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4)) * 255 + 0.5
static const uint8_t gsRGBToLinearRGBMap[256] = {
  0,   0,   0,   0,   0,   0,   0,   1,
  1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   2,   2,   2,   2,   2,   2,
  2,   2,   3,   3,   3,   3,   3,   3,
  4,   4,   4,   4,   4,   5,   5,   5,
  5,   6,   6,   6,   6,   7,   7,   7,
  8,   8,   8,   8,   9,   9,   9,  10,
 10,  10,  11,  11,  12,  12,  12,  13,
 13,  13,  14,  14,  15,  15,  16,  16,
 17,  17,  17,  18,  18,  19,  19,  20,
 20,  21,  22,  22,  23,  23,  24,  24,
 25,  25,  26,  27,  27,  28,  29,  29,
 30,  30,  31,  32,  32,  33,  34,  35,
 35,  36,  37,  37,  38,  39,  40,  41,
 41,  42,  43,  44,  45,  45,  46,  47,
 48,  49,  50,  51,  51,  52,  53,  54,
 55,  56,  57,  58,  59,  60,  61,  62,
 63,  64,  65,  66,  67,  68,  69,  70,
 71,  72,  73,  74,  76,  77,  78,  79,
 80,  81,  82,  84,  85,  86,  87,  88,
 90,  91,  92,  93,  95,  96,  97,  99,
100, 101, 103, 104, 105, 107, 108, 109,
111, 112, 114, 115, 116, 118, 119, 121,
122, 124, 125, 127, 128, 130, 131, 133,
134, 136, 138, 139, 141, 142, 144, 146,
147, 149, 151, 152, 154, 156, 157, 159,
161, 163, 164, 166, 168, 170, 171, 173,
175, 177, 179, 181, 183, 184, 186, 188,
190, 192, 194, 196, 198, 200, 202, 204,
206, 208, 210, 212, 214, 216, 218, 220,
222, 224, 226, 229, 231, 233, 235, 237,
239, 242, 244, 246, 248, 250, 253, 255
};

static bool sSVGDisplayListHitTestingEnabled;
static bool sSVGDisplayListPaintingEnabled;
static bool sSVGTextCSSFramesEnabled;

bool
NS_SVGDisplayListHitTestingEnabled()
{
  return sSVGDisplayListHitTestingEnabled;
}

bool
NS_SVGDisplayListPaintingEnabled()
{
  return sSVGDisplayListPaintingEnabled;
}

bool
NS_SVGTextCSSFramesEnabled()
{
  return sSVGTextCSSFramesEnabled;
}

// we only take the address of this:
static mozilla::gfx::UserDataKey sSVGAutoRenderStateKey;

SVGAutoRenderState::SVGAutoRenderState(nsRenderingContext *aContext,
                                       RenderMode aMode)
  : mContext(aContext)
  , mOriginalRenderState(nullptr)
  , mMode(aMode)
  , mPaintingToWindow(false)
{
  mOriginalRenderState = aContext->RemoveUserData(&sSVGAutoRenderStateKey);
  // We always remove ourselves from aContext before it dies, so
  // passing nullptr as the destroy function is okay.
  aContext->AddUserData(&sSVGAutoRenderStateKey, this, nullptr);
}

SVGAutoRenderState::~SVGAutoRenderState()
{
  mContext->RemoveUserData(&sSVGAutoRenderStateKey);
  if (mOriginalRenderState) {
    mContext->AddUserData(&sSVGAutoRenderStateKey, mOriginalRenderState, nullptr);
  }
}

void
SVGAutoRenderState::SetPaintingToWindow(bool aPaintingToWindow)
{
  mPaintingToWindow = aPaintingToWindow;
}

/* static */ SVGAutoRenderState::RenderMode
SVGAutoRenderState::GetRenderMode(nsRenderingContext *aContext)
{
  void *state = aContext->GetUserData(&sSVGAutoRenderStateKey);
  if (state) {
    return static_cast<SVGAutoRenderState*>(state)->mMode;
  }
  return NORMAL;
}

/* static */ bool
SVGAutoRenderState::IsPaintingToWindow(nsRenderingContext *aContext)
{
  void *state = aContext->GetUserData(&sSVGAutoRenderStateKey);
  if (state) {
    return static_cast<SVGAutoRenderState*>(state)->mPaintingToWindow;
  }
  return false;
}

void
nsSVGUtils::Init()
{
  Preferences::AddBoolVarCache(&sSVGDisplayListHitTestingEnabled,
                               "svg.display-lists.hit-testing.enabled");

  Preferences::AddBoolVarCache(&sSVGDisplayListPaintingEnabled,
                               "svg.display-lists.painting.enabled");

  Preferences::AddBoolVarCache(&sSVGTextCSSFramesEnabled,
                               "svg.text.css-frames.enabled");
}

void
nsSVGUtils::UnPremultiplyImageDataAlpha(uint8_t *data, 
                                        int32_t stride,
                                        const nsIntRect &rect)
{
  for (int32_t y = rect.y; y < rect.YMost(); y++) {
    for (int32_t x = rect.x; x < rect.XMost(); x++) {
      uint8_t *pixel = data + stride * y + 4 * x;

      uint8_t a = pixel[GFX_ARGB32_OFFSET_A];
      if (a == 255)
        continue;

      if (a) {
        pixel[GFX_ARGB32_OFFSET_B] = (255 * pixel[GFX_ARGB32_OFFSET_B]) / a;
        pixel[GFX_ARGB32_OFFSET_G] = (255 * pixel[GFX_ARGB32_OFFSET_G]) / a;
        pixel[GFX_ARGB32_OFFSET_R] = (255 * pixel[GFX_ARGB32_OFFSET_R]) / a;
      } else {
        pixel[GFX_ARGB32_OFFSET_B] = 0;
        pixel[GFX_ARGB32_OFFSET_G] = 0;
        pixel[GFX_ARGB32_OFFSET_R] = 0;
      }
    }
  }
}

void
nsSVGUtils::PremultiplyImageDataAlpha(uint8_t *data, 
                                      int32_t stride,
                                      const nsIntRect &rect)
{
  for (int32_t y = rect.y; y < rect.YMost(); y++) {
    for (int32_t x = rect.x; x < rect.XMost(); x++) {
      uint8_t *pixel = data + stride * y + 4 * x;

      uint8_t a = pixel[GFX_ARGB32_OFFSET_A];
      if (a == 255)
        continue;

      FAST_DIVIDE_BY_255(pixel[GFX_ARGB32_OFFSET_B],
                         pixel[GFX_ARGB32_OFFSET_B] * a);
      FAST_DIVIDE_BY_255(pixel[GFX_ARGB32_OFFSET_G],
                         pixel[GFX_ARGB32_OFFSET_G] * a);
      FAST_DIVIDE_BY_255(pixel[GFX_ARGB32_OFFSET_R],
                         pixel[GFX_ARGB32_OFFSET_R] * a);
    }
  }
}

void
nsSVGUtils::ConvertImageDataToLinearRGB(uint8_t *data, 
                                        int32_t stride,
                                        const nsIntRect &rect)
{
  for (int32_t y = rect.y; y < rect.YMost(); y++) {
    for (int32_t x = rect.x; x < rect.XMost(); x++) {
      uint8_t *pixel = data + stride * y + 4 * x;

      pixel[GFX_ARGB32_OFFSET_B] =
        gsRGBToLinearRGBMap[pixel[GFX_ARGB32_OFFSET_B]];
      pixel[GFX_ARGB32_OFFSET_G] =
        gsRGBToLinearRGBMap[pixel[GFX_ARGB32_OFFSET_G]];
      pixel[GFX_ARGB32_OFFSET_R] =
        gsRGBToLinearRGBMap[pixel[GFX_ARGB32_OFFSET_R]];
    }
  }
}

void
nsSVGUtils::ConvertImageDataFromLinearRGB(uint8_t *data, 
                                          int32_t stride,
                                          const nsIntRect &rect)
{
  for (int32_t y = rect.y; y < rect.YMost(); y++) {
    for (int32_t x = rect.x; x < rect.XMost(); x++) {
      uint8_t *pixel = data + stride * y + 4 * x;

      pixel[GFX_ARGB32_OFFSET_B] =
        glinearRGBTosRGBMap[pixel[GFX_ARGB32_OFFSET_B]];
      pixel[GFX_ARGB32_OFFSET_G] =
        glinearRGBTosRGBMap[pixel[GFX_ARGB32_OFFSET_G]];
      pixel[GFX_ARGB32_OFFSET_R] =
        glinearRGBTosRGBMap[pixel[GFX_ARGB32_OFFSET_R]];
    }
  }
}

void
nsSVGUtils::ComputesRGBLuminanceMask(uint8_t *aData,
                                     int32_t aStride,
                                     const nsIntRect &aRect,
                                     float aOpacity)
{
  for (int32_t y = aRect.y; y < aRect.YMost(); y++) {
    for (int32_t x = aRect.x; x < aRect.XMost(); x++) {
      uint8_t *pixel = aData + aStride * y + 4 * x;
      uint8_t a = pixel[GFX_ARGB32_OFFSET_A];

      uint8_t luminance;
      if (a) {
        /* sRGB -> intensity (unpremultiply cancels out the
         * (a/255.0) multiplication with aOpacity */
        luminance =
          static_cast<uint8_t>
                     ((pixel[GFX_ARGB32_OFFSET_R] * 0.2125 +
                          pixel[GFX_ARGB32_OFFSET_G] * 0.7154 +
                          pixel[GFX_ARGB32_OFFSET_B] * 0.0721) *
                          aOpacity);
      } else {
        luminance = 0;
      }

      memset(pixel, luminance, 4);
    }
  }
}

void
nsSVGUtils::ComputeLinearRGBLuminanceMask(uint8_t *aData,
                                          int32_t aStride,
                                          const nsIntRect &aRect,
                                          float aOpacity)
{
  for (int32_t y = aRect.y; y < aRect.YMost(); y++) {
    for (int32_t x = aRect.x; x < aRect.XMost(); x++) {
      uint8_t *pixel = aData + aStride * y + 4 * x;
      uint8_t a = pixel[GFX_ARGB32_OFFSET_A];

      uint8_t luminance;
      // unpremultiply
      if (a) {
        if (a != 255) {
          pixel[GFX_ARGB32_OFFSET_B] =
            (255 * pixel[GFX_ARGB32_OFFSET_B]) / a;
          pixel[GFX_ARGB32_OFFSET_G] =
            (255 * pixel[GFX_ARGB32_OFFSET_G]) / a;
          pixel[GFX_ARGB32_OFFSET_R] =
            (255 * pixel[GFX_ARGB32_OFFSET_R]) / a;
        }

        /* sRGB -> linearRGB -> intensity */
        luminance =
          static_cast<uint8_t>
                     ((gsRGBToLinearRGBMap[pixel[GFX_ARGB32_OFFSET_R]] *
                          0.2125 +
                          gsRGBToLinearRGBMap[pixel[GFX_ARGB32_OFFSET_G]] *
                          0.7154 +
                          gsRGBToLinearRGBMap[pixel[GFX_ARGB32_OFFSET_B]] *
                          0.0721) * (a / 255.0) * aOpacity);
      } else {
        luminance = 0;
      }

      memset(pixel, luminance, 4);
    }
  }
}

void
nsSVGUtils::ComputeAlphaMask(uint8_t *aData,
                             int32_t aStride,
                             const nsIntRect &aRect,
                             float aOpacity)
{
  for (int32_t y = aRect.y; y < aRect.YMost(); y++) {
    for (int32_t x = aRect.x; x < aRect.XMost(); x++) {
      uint8_t *pixel = aData + aStride * y + 4 * x;
      uint8_t luminance = pixel[GFX_ARGB32_OFFSET_A] * aOpacity;
      memset(pixel, luminance, 4);
    }
  }
}

float
nsSVGUtils::CoordToFloat(nsPresContext *aPresContext,
                         nsSVGElement *aContent,
                         const nsStyleCoord &aCoord)
{
  switch (aCoord.GetUnit()) {
  case eStyleUnit_Factor:
    // user units
    return aCoord.GetFactorValue();

  case eStyleUnit_Coord:
    return nsPresContext::AppUnitsToFloatCSSPixels(aCoord.GetCoordValue());

  case eStyleUnit_Percent: {
      SVGSVGElement* ctx = aContent->GetCtx();
      return ctx ? aCoord.GetPercentValue() * ctx->GetLength(SVGContentUtils::XY) : 0.0f;
    }
  default:
    return 0.0f;
  }
}

nsSVGDisplayContainerFrame*
nsSVGUtils::GetNearestSVGViewport(nsIFrame *aFrame)
{
  NS_ASSERTION(aFrame->IsFrameOfType(nsIFrame::eSVG), "SVG frame expected");
  if (aFrame->GetType() == nsGkAtoms::svgOuterSVGFrame) {
    return nullptr;
  }
  while ((aFrame = aFrame->GetParent())) {
    NS_ASSERTION(aFrame->IsFrameOfType(nsIFrame::eSVG), "SVG frame expected");
    if (aFrame->GetType() == nsGkAtoms::svgInnerSVGFrame ||
        aFrame->GetType() == nsGkAtoms::svgOuterSVGFrame) {
      return do_QueryFrame(aFrame);
    }
  }
  NS_NOTREACHED("This is not reached. It's only needed to compile.");
  return nullptr;
}

nsRect
nsSVGUtils::GetPostFilterVisualOverflowRect(nsIFrame *aFrame,
                                            const nsRect &aPreFilterRect)
{
  NS_ABORT_IF_FALSE(aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT,
                    "Called on invalid frame type");

  nsSVGFilterFrame *filter = nsSVGEffects::GetFilterFrame(aFrame);
  if (!filter) {
    return aPreFilterRect;
  }

  return filter->GetPostFilterBounds(aFrame, nullptr, &aPreFilterRect);
}

bool
nsSVGUtils::OuterSVGIsCallingReflowSVG(nsIFrame *aFrame)
{
  return GetOuterSVGFrame(aFrame)->IsCallingReflowSVG();
}

bool
nsSVGUtils::AnyOuterSVGIsCallingReflowSVG(nsIFrame* aFrame)
{
  nsSVGOuterSVGFrame* outer = GetOuterSVGFrame(aFrame);
  do {
    if (outer->IsCallingReflowSVG()) {
      return true;
    }
    outer = GetOuterSVGFrame(outer->GetParent());
  } while (outer);
  return false;
}

void
nsSVGUtils::ScheduleReflowSVG(nsIFrame *aFrame)
{
  NS_ABORT_IF_FALSE(aFrame->IsFrameOfType(nsIFrame::eSVG),
                    "Passed bad frame!");

  // If this is triggered, the callers should be fixed to call us before
  // ReflowSVG is called. If we try to mark dirty bits on frames while we're
  // in the process of removing them, things will get messed up.
  NS_ASSERTION(!OuterSVGIsCallingReflowSVG(aFrame),
               "Do not call under nsISVGChildFrame::ReflowSVG!");

  // We don't call nsSVGEffects::InvalidateRenderingObservers here because
  // we should only be called under InvalidateAndScheduleReflowSVG (which
  // calls InvalidateBounds) or nsSVGDisplayContainerFrame::InsertFrames
  // (at which point the frame has no observers).

  if (aFrame->GetStateBits() & NS_FRAME_IS_NONDISPLAY) {
    return;
  }

  if (aFrame->GetStateBits() &
      (NS_FRAME_IS_DIRTY | NS_FRAME_FIRST_REFLOW)) {
    // Nothing to do if we're already dirty, or if the outer-<svg>
    // hasn't yet had its initial reflow.
    return;
  }

  nsSVGOuterSVGFrame *outerSVGFrame = nullptr;

  // We must not add dirty bits to the nsSVGOuterSVGFrame or else
  // PresShell::FrameNeedsReflow won't work when we pass it in below.
  if (aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG) {
    outerSVGFrame = static_cast<nsSVGOuterSVGFrame*>(aFrame);
  } else {
    aFrame->AddStateBits(NS_FRAME_IS_DIRTY);

    nsIFrame *f = aFrame->GetParent();
    while (f && !(f->GetStateBits() & NS_STATE_IS_OUTER_SVG)) {
      if (f->GetStateBits() &
          (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) {
        return;
      }
      f->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
      f = f->GetParent();
      NS_ABORT_IF_FALSE(f->IsFrameOfType(nsIFrame::eSVG),
                        "NS_STATE_IS_OUTER_SVG check above not valid!");
    }

    outerSVGFrame = static_cast<nsSVGOuterSVGFrame*>(f);

    NS_ABORT_IF_FALSE(outerSVGFrame &&
                      outerSVGFrame->GetType() == nsGkAtoms::svgOuterSVGFrame,
                      "Did not find nsSVGOuterSVGFrame!");
  }

  if (outerSVGFrame->GetStateBits() & NS_FRAME_IN_REFLOW) {
    // We're currently under an nsSVGOuterSVGFrame::Reflow call so there is no
    // need to call PresShell::FrameNeedsReflow, since we have an
    // nsSVGOuterSVGFrame::DidReflow call pending.
    return;
  }

  nsFrameState dirtyBit =
    (outerSVGFrame == aFrame ? NS_FRAME_IS_DIRTY : NS_FRAME_HAS_DIRTY_CHILDREN);

  aFrame->PresContext()->PresShell()->FrameNeedsReflow(
    outerSVGFrame, nsIPresShell::eResize, dirtyBit);
}

bool
nsSVGUtils::NeedsReflowSVG(nsIFrame *aFrame)
{
  NS_ABORT_IF_FALSE(aFrame->IsFrameOfType(nsIFrame::eSVG),
                    "SVG uses bits differently!");

  // The flags we test here may change, hence why we have this separate
  // function.
  return NS_SUBTREE_DIRTY(aFrame);
}

void
nsSVGUtils::NotifyAncestorsOfFilterRegionChange(nsIFrame *aFrame)
{
  NS_ABORT_IF_FALSE(!(aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG),
                    "Not expecting to be called on the outer SVG Frame");

  aFrame = aFrame->GetParent();

  while (aFrame) {
    if (aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG)
      return;

    nsSVGFilterProperty *property = nsSVGEffects::GetFilterProperty(aFrame);
    if (property) {
      property->Invalidate();
    }
    aFrame = aFrame->GetParent();
  }
}

float
nsSVGUtils::ObjectSpace(const gfxRect &aRect, const nsSVGLength2 *aLength)
{
  float axis;

  switch (aLength->GetCtxType()) {
  case SVGContentUtils::X:
    axis = aRect.Width();
    break;
  case SVGContentUtils::Y:
    axis = aRect.Height();
    break;
  case SVGContentUtils::XY:
    axis = float(SVGContentUtils::ComputeNormalizedHypotenuse(
                   aRect.Width(), aRect.Height()));
    break;
  default:
    NS_NOTREACHED("unexpected ctx type");
    axis = 0.0f;
    break;
  }
  if (aLength->IsPercentage()) {
    // Multiply first to avoid precision errors:
    return axis * aLength->GetAnimValInSpecifiedUnits() / 100;
  }
  return aLength->GetAnimValue(static_cast<SVGSVGElement*>(nullptr)) * axis;
}

float
nsSVGUtils::UserSpace(nsSVGElement *aSVGElement, const nsSVGLength2 *aLength)
{
  return aLength->GetAnimValue(aSVGElement);
}

float
nsSVGUtils::UserSpace(nsIFrame *aNonSVGContext, const nsSVGLength2 *aLength)
{
  return aLength->GetAnimValue(aNonSVGContext);
}

nsSVGOuterSVGFrame *
nsSVGUtils::GetOuterSVGFrame(nsIFrame *aFrame)
{
  while (aFrame) {
    if (aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG) {
      return static_cast<nsSVGOuterSVGFrame*>(aFrame);
    }
    aFrame = aFrame->GetParent();
  }

  return nullptr;
}

nsIFrame*
nsSVGUtils::GetOuterSVGFrameAndCoveredRegion(nsIFrame* aFrame, nsRect* aRect)
{
  nsISVGChildFrame* svg = do_QueryFrame(aFrame);
  if (!svg)
    return nullptr;
  *aRect = (aFrame->GetStateBits() & NS_FRAME_IS_NONDISPLAY) ?
             nsRect(0, 0, 0, 0) : svg->GetCoveredRegion();
  return GetOuterSVGFrame(aFrame);
}

gfxMatrix
nsSVGUtils::GetCanvasTM(nsIFrame *aFrame, uint32_t aFor)
{
  // XXX yuck, we really need a common interface for GetCanvasTM

  if (!aFrame->IsFrameOfType(nsIFrame::eSVG)) {
    return nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(aFrame);
  }

  if (!(aFrame->GetStateBits() & NS_FRAME_IS_NONDISPLAY)) {
    if ((aFor == nsISVGChildFrame::FOR_PAINTING &&
         NS_SVGDisplayListPaintingEnabled()) ||
        (aFor == nsISVGChildFrame::FOR_HIT_TESTING &&
         NS_SVGDisplayListHitTestingEnabled())) {
      return nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(aFrame);
    }
  }

  nsIAtom* type = aFrame->GetType();
  if (type == nsGkAtoms::svgForeignObjectFrame) {
    return static_cast<nsSVGForeignObjectFrame*>(aFrame)->GetCanvasTM(aFor);
  }
  if (type == nsGkAtoms::svgOuterSVGFrame) {
    return nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(aFrame);
  }

  nsSVGContainerFrame *containerFrame = do_QueryFrame(aFrame);
  if (containerFrame) {
    return containerFrame->GetCanvasTM(aFor);
  }

  return static_cast<nsSVGGeometryFrame*>(aFrame)->GetCanvasTM(aFor);
}

gfxMatrix
nsSVGUtils::GetUserToCanvasTM(nsIFrame *aFrame, uint32_t aFor)
{
  NS_ASSERTION(aFor == nsISVGChildFrame::FOR_OUTERSVG_TM,
               "Unexpected aFor?");

  nsISVGChildFrame* svgFrame = do_QueryFrame(aFrame);
  NS_ASSERTION(svgFrame, "bad frame");

  gfxMatrix tm;
  if (svgFrame) {
    nsSVGElement *content = static_cast<nsSVGElement*>(aFrame->GetContent());
    tm = content->PrependLocalTransformsTo(
                    GetCanvasTM(aFrame->GetParent(), aFor),
                    nsSVGElement::eUserSpaceToParent);
  }
  return tm;
}

void 
nsSVGUtils::NotifyChildrenOfSVGChange(nsIFrame *aFrame, uint32_t aFlags)
{
  nsIFrame *kid = aFrame->GetFirstPrincipalChild();

  while (kid) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      SVGFrame->NotifySVGChanged(aFlags); 
    } else {
      NS_ASSERTION(kid->IsFrameOfType(nsIFrame::eSVG) || kid->IsSVGText(),
                   "SVG frame expected");
      // recurse into the children of container frames e.g. <clipPath>, <mask>
      // in case they have child frames with transformation matrices
      if (kid->IsFrameOfType(nsIFrame::eSVG)) {
        NotifyChildrenOfSVGChange(kid, aFlags);
      }
    }
    kid = kid->GetNextSibling();
  }
}

// ************************************************************

class SVGPaintCallback : public nsSVGFilterPaintCallback
{
public:
  virtual void Paint(nsRenderingContext *aContext, nsIFrame *aTarget,
                     const nsIntRect* aDirtyRect)
  {
    nsISVGChildFrame *svgChildFrame = do_QueryFrame(aTarget);
    NS_ASSERTION(svgChildFrame, "Expected SVG frame here");

    nsIntRect* dirtyRect = nullptr;
    nsIntRect tmpDirtyRect;

    // aDirtyRect is in user-space pixels, we need to convert to
    // outer-SVG-frame-relative device pixels.
    if (aDirtyRect) {
      gfxMatrix userToDeviceSpace =
        nsSVGUtils::GetCanvasTM(aTarget, nsISVGChildFrame::FOR_PAINTING);
      if (userToDeviceSpace.IsSingular()) {
        return;
      }
      gfxRect dirtyBounds = userToDeviceSpace.TransformBounds(
        gfxRect(aDirtyRect->x, aDirtyRect->y, aDirtyRect->width, aDirtyRect->height));
      dirtyBounds.RoundOut();
      if (gfxUtils::GfxRectToIntRect(dirtyBounds, &tmpDirtyRect)) {
        dirtyRect = &tmpDirtyRect;
      }
    }

    svgChildFrame->PaintSVG(aContext, dirtyRect);
  }
};

void
nsSVGUtils::PaintFrameWithEffects(nsRenderingContext *aContext,
                                  const nsIntRect *aDirtyRect,
                                  nsIFrame *aFrame)
{
  NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
               (aFrame->GetStateBits() & NS_FRAME_IS_NONDISPLAY) ||
               aFrame->PresContext()->IsGlyph(),
               "If display lists are enabled, only painting of non-display "
               "SVG should take this code path");

  nsISVGChildFrame *svgChildFrame = do_QueryFrame(aFrame);
  if (!svgChildFrame)
    return;

  float opacity = aFrame->StyleDisplay()->mOpacity;
  if (opacity == 0.0f)
    return;

  const nsIContent* content = aFrame->GetContent();
  if (content->IsSVG() &&
      !static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
    return;
  }

  /* Properties are added lazily and may have been removed by a restyle,
     so make sure all applicable ones are set again. */

  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(aFrame);

  bool isOK = true;
  nsSVGFilterFrame *filterFrame = effectProperties.GetFilterFrame(&isOK);

  if (aDirtyRect &&
      !(aFrame->GetStateBits() & NS_FRAME_IS_NONDISPLAY)) {
    // Here we convert aFrame's paint bounds to outer-<svg> device space,
    // compare it to aDirtyRect, and return early if they don't intersect.
    // We don't do this optimization for nondisplay SVG since nondisplay
    // SVG doesn't maintain bounds/overflow rects.
    nsRect overflowRect = aFrame->GetVisualOverflowRectRelativeToSelf();
    if (aFrame->IsFrameOfType(nsIFrame::eSVGGeometry) ||
        aFrame->IsSVGText()) {
      // Unlike containers, leaf frames do not include GetPosition() in
      // GetCanvasTM().
      overflowRect = overflowRect + aFrame->GetPosition();
    }
    int32_t appUnitsPerDevPx = aFrame->PresContext()->AppUnitsPerDevPixel();
    gfxMatrix tm = GetCanvasTM(aFrame, nsISVGChildFrame::FOR_PAINTING);
    if (aFrame->IsFrameOfType(nsIFrame::eSVG | nsIFrame::eSVGContainer)) {
      gfxMatrix childrenOnlyTM;
      if (static_cast<nsSVGContainerFrame*>(aFrame)->
            HasChildrenOnlyTransform(&childrenOnlyTM)) {
        // Undo the children-only transform:
        tm = childrenOnlyTM.Invert() * tm;
      }
    }
    nsIntRect bounds = TransformFrameRectToOuterSVG(overflowRect,
                         tm, aFrame->PresContext()).
                           ToOutsidePixels(appUnitsPerDevPx);
    if (!aDirtyRect->Intersects(bounds)) {
      return;
    }
  }

  /* SVG defines the following rendering model:
   *
   *  1. Render fill
   *  2. Render stroke
   *  3. Render markers
   *  4. Apply filter
   *  5. Apply clipping, masking, group opacity
   *
   * We follow this, but perform a couple of optimizations:
   *
   * + Use cairo's clipPath when representable natively (single object
   *   clip region).
   *
   * + Merge opacity and masking if both used together.
   */

  if (opacity != 1.0f && CanOptimizeOpacity(aFrame))
    opacity = 1.0f;

  gfxContext *gfx = aContext->ThebesContext();
  bool complexEffects = false;

  nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame(&isOK);
  nsSVGMaskFrame *maskFrame = effectProperties.GetMaskFrame(&isOK);

  bool isTrivialClip = clipPathFrame ? clipPathFrame->IsTrivial() : true;

  if (!isOK) {
    // Some resource is invalid. We shouldn't paint anything.
    return;
  }
  
  gfxMatrix matrix;
  if (clipPathFrame || maskFrame)
    matrix = GetCanvasTM(aFrame, nsISVGChildFrame::FOR_PAINTING);

  /* Check if we need to do additional operations on this child's
   * rendering, which necessitates rendering into another surface. */
  if (opacity != 1.0f || maskFrame || (clipPathFrame && !isTrivialClip)) {
    complexEffects = true;
    gfx->Save();
    if (!(aFrame->GetStateBits() & NS_FRAME_IS_NONDISPLAY)) {
      // aFrame has a valid visual overflow rect, so clip to it before calling
      // PushGroup() to minimize the size of the surfaces we'll composite:
      gfxContextMatrixAutoSaveRestore matrixAutoSaveRestore(gfx);
      gfx->Multiply(GetCanvasTM(aFrame, nsISVGChildFrame::FOR_PAINTING));
      nsRect overflowRect = aFrame->GetVisualOverflowRectRelativeToSelf();
      if (aFrame->IsFrameOfType(nsIFrame::eSVGGeometry) ||
          aFrame->IsSVGText()) {
        // Unlike containers, leaf frames do not include GetPosition() in
        // GetCanvasTM().
        overflowRect = overflowRect + aFrame->GetPosition();
      }
      aContext->IntersectClip(overflowRect);
    }
    gfx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
  }

  /* If this frame has only a trivial clipPath, set up cairo's clipping now so
   * we can just do normal painting and get it clipped appropriately.
   */
  if (clipPathFrame && isTrivialClip) {
    gfx->Save();
    clipPathFrame->ClipPaint(aContext, aFrame, matrix);
  }

  /* Paint the child */
  if (filterFrame) {
    nsRect* dirtyRect = nullptr;
    nsRect tmpDirtyRect;
    if (aDirtyRect) {
      // aDirtyRect is in outer-<svg> device pixels, but the filter code needs
      // it in frame space.
      gfxMatrix userToDeviceSpace =
        GetUserToCanvasTM(aFrame, nsISVGChildFrame::FOR_OUTERSVG_TM);
      if (userToDeviceSpace.IsSingular()) {
        return;
      }
      gfxMatrix deviceToUserSpace = userToDeviceSpace;
      deviceToUserSpace.Invert();
      gfxRect dirtyBounds = deviceToUserSpace.TransformBounds(
                              gfxRect(aDirtyRect->x, aDirtyRect->y,
                                      aDirtyRect->width, aDirtyRect->height));
      tmpDirtyRect =
        nsLayoutUtils::RoundGfxRectToAppRect(
          dirtyBounds, aFrame->PresContext()->AppUnitsPerCSSPixel()) -
        aFrame->GetPosition();
      dirtyRect = &tmpDirtyRect;
    }
    SVGPaintCallback paintCallback;
    filterFrame->PaintFilteredFrame(aContext, aFrame, &paintCallback, dirtyRect);
  } else {
    svgChildFrame->PaintSVG(aContext, aDirtyRect);
  }

  if (clipPathFrame && isTrivialClip) {
    gfx->Restore();
  }

  /* No more effects, we're done. */
  if (!complexEffects)
    return;

  gfx->PopGroupToSource();

  nsRefPtr<gfxPattern> maskSurface =
    maskFrame ? maskFrame->ComputeMaskAlpha(aContext, aFrame,
                                            matrix, opacity) : nullptr;

  nsRefPtr<gfxPattern> clipMaskSurface;
  if (clipPathFrame && !isTrivialClip) {
    gfx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);

    nsresult rv = clipPathFrame->ClipPaint(aContext, aFrame, matrix);
    clipMaskSurface = gfx->PopGroup();

    if (NS_SUCCEEDED(rv) && clipMaskSurface) {
      // Still more set after clipping, so clip to another surface
      if (maskSurface || opacity != 1.0f) {
        gfx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
        gfx->Mask(clipMaskSurface);
        gfx->PopGroupToSource();
      } else {
        gfx->Mask(clipMaskSurface);
      }
    }
  }

  if (maskSurface) {
    gfx->Mask(maskSurface);
  } else if (opacity != 1.0f) {
    gfx->Paint(opacity);
  }

  gfx->Restore();
}

bool
nsSVGUtils::HitTestClip(nsIFrame *aFrame, const nsPoint &aPoint)
{
  nsSVGEffects::EffectProperties props =
    nsSVGEffects::GetEffectProperties(aFrame);
  if (!props.mClipPath)
    return true;

  bool isOK = true;
  nsSVGClipPathFrame *clipPathFrame = props.GetClipPathFrame(&isOK);
  if (!clipPathFrame || !isOK) {
    // clipPath is not a valid resource, so nothing gets painted, so
    // hit-testing must fail.
    return false;
  }

  return clipPathFrame->ClipHitTest(aFrame, GetCanvasTM(aFrame,
                                    nsISVGChildFrame::FOR_HIT_TESTING), aPoint);
}

nsIFrame *
nsSVGUtils::HitTestChildren(nsIFrame *aFrame, const nsPoint &aPoint)
{
  // Traverse the list in reverse order, so that if we get a hit we know that's
  // the topmost frame that intersects the point; then we can just return it.
  nsIFrame* result = nullptr;
  for (nsIFrame* current = aFrame->PrincipalChildList().LastChild();
       current;
       current = current->GetPrevSibling()) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(current);
    if (SVGFrame) {
      const nsIContent* content = current->GetContent();
      if (content->IsSVG() &&
          !static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
        continue;
      }
      result = SVGFrame->GetFrameForPoint(aPoint);
      if (result)
        break;
    }
  }

  if (result && !HitTestClip(aFrame, aPoint))
    result = nullptr;

  return result;
}

nsRect
nsSVGUtils::GetCoveredRegion(const nsFrameList &aFrames)
{
  nsRect rect;

  for (nsIFrame* kid = aFrames.FirstChild();
       kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* child = do_QueryFrame(kid);
    if (child) {
      nsRect childRect = child->GetCoveredRegion();
      rect.UnionRect(rect, childRect);
    }
  }

  return rect;
}

nsPoint
nsSVGUtils::TransformOuterSVGPointToChildFrame(nsPoint aPoint,
                                               const gfxMatrix& aFrameToCanvasTM,
                                               nsPresContext* aPresContext)
{
  NS_ABORT_IF_FALSE(!aFrameToCanvasTM.IsSingular(),
                    "Callers must not pass a singular matrix");
  gfxMatrix canvasDevToFrameUserSpace = aFrameToCanvasTM;
  canvasDevToFrameUserSpace.Invert();
  gfxPoint devPt = gfxPoint(aPoint.x, aPoint.y) /
    aPresContext->AppUnitsPerDevPixel();
  gfxPoint userPt = canvasDevToFrameUserSpace.Transform(devPt);
  gfxPoint appPt = (userPt * aPresContext->AppUnitsPerCSSPixel()).Round();
  userPt.x = clamped(appPt.x, gfxFloat(nscoord_MIN), gfxFloat(nscoord_MAX));
  userPt.y = clamped(appPt.y, gfxFloat(nscoord_MIN), gfxFloat(nscoord_MAX));
  // now guaranteed to be safe:
  return nsPoint(nscoord(userPt.x), nscoord(userPt.y));
}

nsRect
nsSVGUtils::TransformFrameRectToOuterSVG(const nsRect& aRect,
                                         const gfxMatrix& aMatrix,
                                         nsPresContext* aPresContext)
{
  gfxRect r(aRect.x, aRect.y, aRect.width, aRect.height);
  r.Scale(1.0 / nsPresContext::AppUnitsPerCSSPixel());
  return nsLayoutUtils::RoundGfxRectToAppRect(
    aMatrix.TransformBounds(r), aPresContext->AppUnitsPerDevPixel());
}

gfxIntSize
nsSVGUtils::ConvertToSurfaceSize(const gfxSize& aSize,
                                 bool *aResultOverflows)
{
  gfxIntSize surfaceSize(ClampToInt(ceil(aSize.width)), ClampToInt(ceil(aSize.height)));

  *aResultOverflows = surfaceSize.width != ceil(aSize.width) ||
    surfaceSize.height != ceil(aSize.height);

  if (!gfxASurface::CheckSurfaceSize(surfaceSize)) {
    surfaceSize.width = std::min(NS_SVG_OFFSCREEN_MAX_DIMENSION,
                               surfaceSize.width);
    surfaceSize.height = std::min(NS_SVG_OFFSCREEN_MAX_DIMENSION,
                                surfaceSize.height);
    *aResultOverflows = true;
  }

  return surfaceSize;
}

bool
nsSVGUtils::HitTestRect(const gfxMatrix &aMatrix,
                        float aRX, float aRY, float aRWidth, float aRHeight,
                        float aX, float aY)
{
  gfxRect rect(aRX, aRY, aRWidth, aRHeight);
  if (rect.IsEmpty() || aMatrix.IsSingular()) {
    return false;
  }
  gfxMatrix toRectSpace = aMatrix;
  toRectSpace.Invert();
  gfxPoint p = toRectSpace.Transform(gfxPoint(aX, aY));
  return rect.x <= p.x && p.x <= rect.XMost() &&
         rect.y <= p.y && p.y <= rect.YMost();
}

gfxRect
nsSVGUtils::GetClipRectForFrame(nsIFrame *aFrame,
                                float aX, float aY, float aWidth, float aHeight)
{
  const nsStyleDisplay* disp = aFrame->StyleDisplay();

  if (!(disp->mClipFlags & NS_STYLE_CLIP_RECT)) {
    NS_ASSERTION(disp->mClipFlags == NS_STYLE_CLIP_AUTO,
                 "We don't know about this type of clip.");
    return gfxRect(aX, aY, aWidth, aHeight);
  }

  if (disp->mOverflowX == NS_STYLE_OVERFLOW_HIDDEN ||
      disp->mOverflowY == NS_STYLE_OVERFLOW_HIDDEN) {

    nsIntRect clipPxRect =
      disp->mClip.ToOutsidePixels(aFrame->PresContext()->AppUnitsPerDevPixel());
    gfxRect clipRect =
      gfxRect(clipPxRect.x, clipPxRect.y, clipPxRect.width, clipPxRect.height);

    if (NS_STYLE_CLIP_RIGHT_AUTO & disp->mClipFlags) {
      clipRect.width = aWidth - clipRect.X();
    }
    if (NS_STYLE_CLIP_BOTTOM_AUTO & disp->mClipFlags) {
      clipRect.height = aHeight - clipRect.Y();
    }

    if (disp->mOverflowX != NS_STYLE_OVERFLOW_HIDDEN) {
      clipRect.x = aX;
      clipRect.width = aWidth;
    }
    if (disp->mOverflowY != NS_STYLE_OVERFLOW_HIDDEN) {
      clipRect.y = aY;
      clipRect.height = aHeight;
    }
     
    return clipRect;
  }
  return gfxRect(aX, aY, aWidth, aHeight);
}

void
nsSVGUtils::CompositeSurfaceMatrix(gfxContext *aContext,
                                   gfxASurface *aSurface,
                                   const gfxMatrix &aCTM, float aOpacity)
{
  if (aCTM.IsSingular())
    return;
  
  if (aContext->IsCairo()) {
    aContext->Save();
    aContext->Multiply(aCTM);
    aContext->SetSource(aSurface);
    aContext->Paint(aOpacity);
    aContext->Restore();
  } else {
    DrawTarget *dt = aContext->GetDrawTarget();
    Matrix oldMat = dt->GetTransform();
    RefPtr<SourceSurface> surf =
      gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(dt, aSurface);
    dt->SetTransform(ToMatrix(aCTM) * oldMat);

    gfxSize size = aSurface->GetSize();
    NS_ASSERTION(size.width >= 0 && size.height >= 0, "Failure to get size for aSurface.");

    gfxPoint pt = aSurface->GetDeviceOffset();

    dt->FillRect(Rect(-pt.x, -pt.y, size.width, size.height),
                 SurfacePattern(surf, EXTEND_CLAMP,
                                Matrix(1.0f, 0, 0, 1.0f, -pt.x, -pt.y)),
                 DrawOptions(aOpacity));

    dt->SetTransform(oldMat);
  }
}

void
nsSVGUtils::CompositePatternMatrix(gfxContext *aContext,
                                   gfxPattern *aPattern,
                                   const gfxMatrix &aCTM, float aWidth, float aHeight, float aOpacity)
{
  if (aCTM.IsSingular())
    return;

  aContext->Save();
  SetClipRect(aContext, aCTM, gfxRect(0, 0, aWidth, aHeight));
  aContext->Multiply(aCTM);
  aContext->SetPattern(aPattern);
  aContext->Paint(aOpacity);
  aContext->Restore();
}

void
nsSVGUtils::SetClipRect(gfxContext *aContext,
                        const gfxMatrix &aCTM,
                        const gfxRect &aRect)
{
  if (aCTM.IsSingular())
    return;

  gfxContextMatrixAutoSaveRestore matrixAutoSaveRestore(aContext);
  aContext->Multiply(aCTM);
  aContext->Clip(aRect);
}

void
nsSVGUtils::ClipToGfxRect(nsIntRect* aRect, const gfxRect& aGfxRect)
{
  gfxRect r = aGfxRect;
  r.RoundOut();
  gfxRect r2(aRect->x, aRect->y, aRect->width, aRect->height);
  r = r.Intersect(r2);
  *aRect = nsIntRect(int32_t(r.X()), int32_t(r.Y()),
                     int32_t(r.Width()), int32_t(r.Height()));
}

gfxRect
nsSVGUtils::GetBBox(nsIFrame *aFrame, uint32_t aFlags)
{
  if (aFrame->GetContent()->IsNodeOfType(nsINode::eTEXT)) {
    aFrame = aFrame->GetParent();
  }
  gfxRect bbox;
  nsISVGChildFrame *svg = do_QueryFrame(aFrame);
  if (svg || aFrame->IsSVGText()) {
    // It is possible to apply a gradient, pattern, clipping path, mask or
    // filter to text. When one of these facilities is applied to text
    // the bounding box is the entire text element in all
    // cases.
    if (aFrame->IsSVGText()) {
      nsIFrame* ancestor = GetFirstNonAAncestorFrame(aFrame);
      if (ancestor && ancestor->IsSVGText()) {
        while (ancestor->GetType() != nsGkAtoms::svgTextFrame2) {
          ancestor = ancestor->GetParent();
        }
      }
      svg = do_QueryFrame(ancestor);
    } else {
      nsSVGTextContainerFrame* metrics = do_QueryFrame(
        GetFirstNonAAncestorFrame(aFrame));
      if (metrics) {
        while (aFrame->GetType() != nsGkAtoms::svgTextFrame) {
          aFrame = aFrame->GetParent();
        }
        svg = do_QueryFrame(aFrame);
      }
    }
    nsIContent* content = aFrame->GetContent();
    if (content->IsSVG() &&
        !static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
      return bbox;
    }
    gfxMatrix matrix;
    if (aFrame->GetType() == nsGkAtoms::svgForeignObjectFrame) {
      // The spec says getBBox "Returns the tight bounding box in *current user
      // space*". So we should really be doing this for all elements, but that
      // needs investigation to check that we won't break too much content.
      NS_ABORT_IF_FALSE(content->IsSVG(), "bad cast");
      nsSVGElement *element = static_cast<nsSVGElement*>(content);
      matrix = element->PrependLocalTransformsTo(matrix,
                          nsSVGElement::eChildToUserSpace);
    }
    return svg->GetBBoxContribution(matrix, aFlags);
  }
  return nsSVGIntegrationUtils::GetSVGBBoxForNonSVGFrame(aFrame);
}

gfxRect
nsSVGUtils::GetRelativeRect(uint16_t aUnits, const nsSVGLength2 *aXYWH,
                            const gfxRect &aBBox, nsIFrame *aFrame)
{
  float x, y, width, height;
  if (aUnits == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    x = aBBox.X() + ObjectSpace(aBBox, &aXYWH[0]);
    y = aBBox.Y() + ObjectSpace(aBBox, &aXYWH[1]);
    width = ObjectSpace(aBBox, &aXYWH[2]);
    height = ObjectSpace(aBBox, &aXYWH[3]);
  } else {
    x = UserSpace(aFrame, &aXYWH[0]);
    y = UserSpace(aFrame, &aXYWH[1]);
    width = UserSpace(aFrame, &aXYWH[2]);
    height = UserSpace(aFrame, &aXYWH[3]);
  }
  return gfxRect(x, y, width, height);
}

bool
nsSVGUtils::CanOptimizeOpacity(nsIFrame *aFrame)
{
  if (!(aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT)) {
    return false;
  }
  nsIAtom *type = aFrame->GetType();
  if (type != nsGkAtoms::svgImageFrame &&
      type != nsGkAtoms::svgPathGeometryFrame) {
    return false;
  }
  if (aFrame->StyleSVGReset()->SingleFilter()) {
    return false;
  }
  // XXX The SVG WG is intending to allow fill, stroke and markers on <image>
  if (type == nsGkAtoms::svgImageFrame) {
    return true;
  }
  const nsStyleSVG *style = aFrame->StyleSVG();
  if (style->HasMarker()) {
    return false;
  }
  if (style->mFill.mType == eStyleSVGPaintType_None ||
      style->mFillOpacity <= 0 ||
      !HasStroke(aFrame)) {
    return true;
  }
  return false;
}

float
nsSVGUtils::MaxExpansion(const gfxMatrix &aMatrix)
{
  // maximum expansion derivation from
  // http://lists.cairographics.org/archives/cairo/2004-October/001980.html
  // and also implemented in cairo_matrix_transformed_circle_major_axis
  double a = aMatrix.xx;
  double b = aMatrix.yx;
  double c = aMatrix.xy;
  double d = aMatrix.yy;
  double f = (a * a + b * b + c * c + d * d) / 2;
  double g = (a * a + b * b - c * c - d * d) / 2;
  double h = a * c + b * d;
  return sqrt(f + sqrt(g * g + h * h));
}

gfxMatrix
nsSVGUtils::AdjustMatrixForUnits(const gfxMatrix &aMatrix,
                                 nsSVGEnum *aUnits,
                                 nsIFrame *aFrame)
{
  if (aFrame &&
      aUnits->GetAnimValue() == SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    gfxRect bbox = GetBBox(aFrame);
    return gfxMatrix().Scale(bbox.Width(), bbox.Height()) *
           gfxMatrix().Translate(gfxPoint(bbox.X(), bbox.Y())) *
           aMatrix;
  }
  return aMatrix;
}

nsIFrame*
nsSVGUtils::GetFirstNonAAncestorFrame(nsIFrame* aStartFrame)
{
  for (nsIFrame *ancestorFrame = aStartFrame; ancestorFrame;
       ancestorFrame = ancestorFrame->GetParent()) {
    if (ancestorFrame->GetType() != nsGkAtoms::svgAFrame) {
      return ancestorFrame;
    }
  }
  return nullptr;
}

gfxMatrix
nsSVGUtils::GetStrokeTransform(nsIFrame *aFrame)
{
  if (aFrame->GetContent()->IsNodeOfType(nsINode::eTEXT)) {
    aFrame = aFrame->GetParent();
  }

  if (aFrame->StyleSVGReset()->mVectorEffect ==
      NS_STYLE_VECTOR_EFFECT_NON_SCALING_STROKE) {
 
    nsIContent *content = aFrame->GetContent();
    NS_ABORT_IF_FALSE(content->IsSVG(), "bad cast");

    // a non-scaling stroke is in the screen co-ordinate
    // space rather so we need to invert the transform
    // to the screen co-ordinate space to get there.
    // See http://www.w3.org/TR/SVGTiny12/painting.html#NonScalingStroke
    gfxMatrix transform = SVGContentUtils::GetCTM(
                            static_cast<nsSVGElement*>(content), true);
    if (!transform.IsSingular()) {
      return transform.Invert();
    }
  }
  return gfxMatrix();
}

// The logic here comes from _cairo_stroke_style_max_distance_from_path
static gfxRect
PathExtentsToMaxStrokeExtents(const gfxRect& aPathExtents,
                              nsIFrame* aFrame,
                              double aStyleExpansionFactor,
                              const gfxMatrix& aMatrix)
{
  double style_expansion =
    aStyleExpansionFactor * nsSVGUtils::GetStrokeWidth(aFrame);

  gfxMatrix matrix = aMatrix;
  matrix.Multiply(nsSVGUtils::GetStrokeTransform(aFrame));

  double dx = style_expansion * (fabs(matrix.xx) + fabs(matrix.xy));
  double dy = style_expansion * (fabs(matrix.yy) + fabs(matrix.yx));

  gfxRect strokeExtents = aPathExtents;
  strokeExtents.Inflate(dx, dy);
  return strokeExtents;
}

/*static*/ gfxRect
nsSVGUtils::PathExtentsToMaxStrokeExtents(const gfxRect& aPathExtents,
                                          nsSVGGeometryFrame* aFrame,
                                          const gfxMatrix& aMatrix)
{
  return ::PathExtentsToMaxStrokeExtents(aPathExtents, aFrame, 0.5, aMatrix);
}

/*static*/ gfxRect
nsSVGUtils::PathExtentsToMaxStrokeExtents(const gfxRect& aPathExtents,
                                          nsTextFrame* aFrame,
                                          const gfxMatrix& aMatrix)
{
  NS_ASSERTION(aFrame->IsSVGText(), "expected an nsTextFrame for SVG text");
  return ::PathExtentsToMaxStrokeExtents(aPathExtents, aFrame, 0.5, aMatrix);
}

/*static*/ gfxRect
nsSVGUtils::PathExtentsToMaxStrokeExtents(const gfxRect& aPathExtents,
                                          nsSVGPathGeometryFrame* aFrame,
                                          const gfxMatrix& aMatrix)
{
  double styleExpansionFactor = 0.5;

  if (static_cast<nsSVGPathGeometryElement*>(aFrame->GetContent())->IsMarkable()) {
    const nsStyleSVG* style = aFrame->StyleSVG();

    if (style->mStrokeLinecap == NS_STYLE_STROKE_LINECAP_SQUARE) {
      styleExpansionFactor = M_SQRT1_2;
    }

    if (style->mStrokeLinejoin == NS_STYLE_STROKE_LINEJOIN_MITER &&
        styleExpansionFactor < style->mStrokeMiterlimit &&
        aFrame->GetContent()->Tag() != nsGkAtoms::line) {
      styleExpansionFactor = style->mStrokeMiterlimit;
    }
  }

  return ::PathExtentsToMaxStrokeExtents(aPathExtents,
                                         aFrame,
                                         styleExpansionFactor,
                                         aMatrix);
}

// ----------------------------------------------------------------------

/* static */ nscolor
nsSVGUtils::GetFallbackOrPaintColor(gfxContext *aContext, nsStyleContext *aStyleContext,
                                    nsStyleSVGPaint nsStyleSVG::*aFillOrStroke)
{
  const nsStyleSVGPaint &paint = aStyleContext->StyleSVG()->*aFillOrStroke;
  nsStyleContext *styleIfVisited = aStyleContext->GetStyleIfVisited();
  bool isServer = paint.mType == eStyleSVGPaintType_Server ||
                  paint.mType == eStyleSVGPaintType_ObjectFill ||
                  paint.mType == eStyleSVGPaintType_ObjectStroke;
  nscolor color = isServer ? paint.mFallbackColor : paint.mPaint.mColor;
  if (styleIfVisited) {
    const nsStyleSVGPaint &paintIfVisited =
      styleIfVisited->StyleSVG()->*aFillOrStroke;
    // To prevent Web content from detecting if a user has visited a URL
    // (via URL loading triggered by paint servers or performance
    // differences between paint servers or between a paint server and a
    // color), we do not allow whether links are visited to change which
    // paint server is used or switch between paint servers and simple
    // colors.  A :visited style may only override a simple color with
    // another simple color.
    if (paintIfVisited.mType == eStyleSVGPaintType_Color &&
        paint.mType == eStyleSVGPaintType_Color) {
      nscolor colors[2] = { color, paintIfVisited.mPaint.mColor };
      return nsStyleContext::CombineVisitedColors(
               colors, aStyleContext->RelevantLinkVisited());
    }
  }
  return color;
}

static void
SetupFallbackOrPaintColor(gfxContext *aContext, nsStyleContext *aStyleContext,
                          nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                          float aOpacity)
{
  nscolor color = nsSVGUtils::GetFallbackOrPaintColor(
    aContext, aStyleContext, aFillOrStroke);

  aContext->SetColor(gfxRGBA(NS_GET_R(color)/255.0,
                             NS_GET_G(color)/255.0,
                             NS_GET_B(color)/255.0,
                             NS_GET_A(color)/255.0 * aOpacity));
}

static float
MaybeOptimizeOpacity(nsIFrame *aFrame, float aFillOrStrokeOpacity)
{
  float opacity = aFrame->StyleDisplay()->mOpacity;
  if (opacity < 1 && nsSVGUtils::CanOptimizeOpacity(aFrame)) {
    return aFillOrStrokeOpacity * opacity;
  }
  return aFillOrStrokeOpacity;
}

/* static */ bool
nsSVGUtils::SetupObjectPaint(gfxContext *aContext,
                             gfxTextObjectPaint *aObjectPaint,
                             const nsStyleSVGPaint &aPaint,
                             float aOpacity)
{
  nsRefPtr<gfxPattern> pattern;

  if (!aObjectPaint) {
    return false;
  }

  switch (aPaint.mType) {
    case eStyleSVGPaintType_ObjectFill:
      pattern = aObjectPaint->GetFillPattern(aOpacity, aContext->CurrentMatrix());
      break;
    case eStyleSVGPaintType_ObjectStroke:
      pattern = aObjectPaint->GetStrokePattern(aOpacity, aContext->CurrentMatrix());
      break;
    default:
      return false;
  }

  if (!pattern) {
    return false;
  }

  aContext->SetPattern(pattern);

  return true;
}

bool
nsSVGUtils::SetupCairoFillPaint(nsIFrame *aFrame, gfxContext* aContext,
                                gfxTextObjectPaint *aObjectPaint)
{
  const nsStyleSVG* style = aFrame->StyleSVG();
  if (style->mFill.mType == eStyleSVGPaintType_None)
    return false;

  if (style->mFillRule == NS_STYLE_FILL_RULE_EVENODD)
    aContext->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
  else
    aContext->SetFillRule(gfxContext::FILL_RULE_WINDING);

  float opacity = MaybeOptimizeOpacity(aFrame,
                                       GetOpacity(style->mFillOpacitySource,
                                                  style->mFillOpacity,
                                                  aObjectPaint));
  nsSVGPaintServerFrame *ps =
    nsSVGEffects::GetPaintServer(aFrame, &style->mFill, nsSVGEffects::FillProperty());
  if (ps && ps->SetupPaintServer(aContext, aFrame, &nsStyleSVG::mFill, opacity))
    return true;

  if (SetupObjectPaint(aContext, aObjectPaint, style->mFill, opacity)) {
    return true;
  }

  // On failure, use the fallback colour in case we have an
  // objectBoundingBox where the width or height of the object is zero.
  // See http://www.w3.org/TR/SVG11/coords.html#ObjectBoundingBox
  SetupFallbackOrPaintColor(aContext, aFrame->StyleContext(),
                            &nsStyleSVG::mFill, opacity);

  return true;
}

bool
nsSVGUtils::SetupCairoStrokePaint(nsIFrame *aFrame, gfxContext* aContext,
                                  gfxTextObjectPaint *aObjectPaint)
{
  const nsStyleSVG* style = aFrame->StyleSVG();
  if (style->mStroke.mType == eStyleSVGPaintType_None)
    return false;

  float opacity = MaybeOptimizeOpacity(aFrame,
                                       GetOpacity(style->mStrokeOpacitySource,
                                                  style->mStrokeOpacity,
                                                  aObjectPaint));

  nsSVGPaintServerFrame *ps =
    nsSVGEffects::GetPaintServer(aFrame, &style->mStroke, nsSVGEffects::StrokeProperty());
  if (ps && ps->SetupPaintServer(aContext, aFrame, &nsStyleSVG::mStroke, opacity))
    return true;

  if (SetupObjectPaint(aContext, aObjectPaint, style->mStroke, opacity)) {
    return true;
  }

  // On failure, use the fallback colour in case we have an
  // objectBoundingBox where the width or height of the object is zero.
  // See http://www.w3.org/TR/SVG11/coords.html#ObjectBoundingBox
  SetupFallbackOrPaintColor(aContext, aFrame->StyleContext(),
                            &nsStyleSVG::mStroke, opacity);

  return true;
}

/* static */ float
nsSVGUtils::GetOpacity(nsStyleSVGOpacitySource aOpacityType,
                       const float& aOpacity,
                       gfxTextObjectPaint *aOuterObjectPaint)
{
  float opacity = 1.0f;
  switch (aOpacityType) {
  case eStyleSVGOpacitySource_Normal:
    opacity = aOpacity;
    break;
  case eStyleSVGOpacitySource_ObjectFillOpacity:
    if (aOuterObjectPaint) {
      opacity = aOuterObjectPaint->GetFillOpacity();
    } else {
      NS_WARNING("objectFillOpacity used outside of an SVG glyph");
    }
    break;
  case eStyleSVGOpacitySource_ObjectStrokeOpacity:
    if (aOuterObjectPaint) {
      opacity = aOuterObjectPaint->GetStrokeOpacity();
    } else {
      NS_WARNING("objectStrokeOpacity used outside of an SVG glyph");
    }
    break;
  default:
    NS_NOTREACHED("Unknown object opacity inheritance type for SVG glyph");
  }
  return opacity;
}

bool
nsSVGUtils::HasStroke(nsIFrame* aFrame, gfxTextObjectPaint *aObjectPaint)
{
  const nsStyleSVG *style = aFrame->StyleSVG();
  return style->mStroke.mType != eStyleSVGPaintType_None &&
         style->mStrokeOpacity > 0 &&
         GetStrokeWidth(aFrame, aObjectPaint) > 0;
}

float
nsSVGUtils::GetStrokeWidth(nsIFrame* aFrame, gfxTextObjectPaint *aObjectPaint)
{
  const nsStyleSVG *style = aFrame->StyleSVG();
  if (aObjectPaint && style->mStrokeWidthFromObject) {
    return aObjectPaint->GetStrokeWidth();
  }

  nsIContent* content = aFrame->GetContent();
  if (content->IsNodeOfType(nsINode::eTEXT)) {
    content = content->GetParent();
  }

  nsSVGElement *ctx = static_cast<nsSVGElement*>(content);

  return CoordToFloat(aFrame->PresContext(), ctx,
                      style->mStrokeWidth);
}

void
nsSVGUtils::SetupCairoStrokeBBoxGeometry(nsIFrame* aFrame,
                                         gfxContext *aContext,
                                         gfxTextObjectPaint *aObjectPaint)
{
  float width = GetStrokeWidth(aFrame, aObjectPaint);
  if (width <= 0)
    return;
  aContext->SetLineWidth(width);

  // Apply any stroke-specific transform
  aContext->Multiply(GetStrokeTransform(aFrame));

  const nsStyleSVG* style = aFrame->StyleSVG();
  
  switch (style->mStrokeLinecap) {
  case NS_STYLE_STROKE_LINECAP_BUTT:
    aContext->SetLineCap(gfxContext::LINE_CAP_BUTT);
    break;
  case NS_STYLE_STROKE_LINECAP_ROUND:
    aContext->SetLineCap(gfxContext::LINE_CAP_ROUND);
    break;
  case NS_STYLE_STROKE_LINECAP_SQUARE:
    aContext->SetLineCap(gfxContext::LINE_CAP_SQUARE);
    break;
  }

  aContext->SetMiterLimit(style->mStrokeMiterlimit);

  switch (style->mStrokeLinejoin) {
  case NS_STYLE_STROKE_LINEJOIN_MITER:
    aContext->SetLineJoin(gfxContext::LINE_JOIN_MITER);
    break;
  case NS_STYLE_STROKE_LINEJOIN_ROUND:
    aContext->SetLineJoin(gfxContext::LINE_JOIN_ROUND);
    break;
  case NS_STYLE_STROKE_LINEJOIN_BEVEL:
    aContext->SetLineJoin(gfxContext::LINE_JOIN_BEVEL);
    break;
  }
}

static bool
GetStrokeDashData(nsIFrame* aFrame,
                  FallibleTArray<gfxFloat>& aDashes,
                  gfxFloat* aDashOffset,
                  gfxTextObjectPaint *aObjectPaint)
{
  const nsStyleSVG* style = aFrame->StyleSVG();
  nsPresContext *presContext = aFrame->PresContext();
  nsIContent *content = aFrame->GetContent();
  nsSVGElement *ctx = static_cast<nsSVGElement*>
    (content->IsNodeOfType(nsINode::eTEXT) ?
     content->GetParent() : content);

  gfxFloat totalLength = 0.0;
  if (aObjectPaint && style->mStrokeDasharrayFromObject) {
    aDashes = aObjectPaint->GetStrokeDashArray();

    for (uint32_t i = 0; i < aDashes.Length(); i++) {
      if (aDashes[i] < 0.0) {
        return false;
      }
      totalLength += aDashes[i];
    }

  } else {
    uint32_t count = style->mStrokeDasharrayLength;
    if (!count || !aDashes.SetLength(count)) {
      return false;
    }

    gfxFloat pathScale = 1.0;

    if (content->Tag() == nsGkAtoms::path) {
      pathScale = static_cast<SVGPathElement*>(content)->
        GetPathLengthScale(SVGPathElement::eForStroking);
      if (pathScale <= 0) {
        return false;
      }
    }

    const nsStyleCoord *dasharray = style->mStrokeDasharray;

    for (uint32_t i = 0; i < count; i++) {
      aDashes[i] = nsSVGUtils::CoordToFloat(presContext,
                                            ctx,
                                            dasharray[i]) * pathScale;
      if (aDashes[i] < 0.0) {
        return false;
      }
      totalLength += aDashes[i];
    }
  }

  if (aObjectPaint && style->mStrokeDashoffsetFromObject) {
    *aDashOffset = aObjectPaint->GetStrokeDashOffset();
  } else {
    *aDashOffset = nsSVGUtils::CoordToFloat(presContext,
                                            ctx,
                                            style->mStrokeDashoffset);
  }
  
  return (totalLength > 0.0);
}

void
nsSVGUtils::SetupCairoStrokeGeometry(nsIFrame* aFrame, gfxContext* aContext,
                                     gfxTextObjectPaint *aObjectPaint)
{
  SetupCairoStrokeBBoxGeometry(aFrame, aContext, aObjectPaint);

  AutoFallibleTArray<gfxFloat, 10> dashes;
  gfxFloat dashOffset;
  if (GetStrokeDashData(aFrame, dashes, &dashOffset, aObjectPaint)) {
    aContext->SetDash(dashes.Elements(), dashes.Length(), dashOffset);
  }
}

uint16_t
nsSVGUtils::GetGeometryHitTestFlags(nsIFrame* aFrame)
{
  uint16_t flags = 0;

  switch(aFrame->StyleVisibility()->mPointerEvents) {
  case NS_STYLE_POINTER_EVENTS_NONE:
    break;
  case NS_STYLE_POINTER_EVENTS_AUTO:
  case NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED:
    if (aFrame->StyleVisibility()->IsVisible()) {
      if (aFrame->StyleSVG()->mFill.mType != eStyleSVGPaintType_None)
        flags |= SVG_HIT_TEST_FILL;
      if (aFrame->StyleSVG()->mStroke.mType != eStyleSVGPaintType_None)
        flags |= SVG_HIT_TEST_STROKE;
      if (aFrame->StyleSVG()->mStrokeOpacity > 0)
        flags |= SVG_HIT_TEST_CHECK_MRECT;
    }
    break;
  case NS_STYLE_POINTER_EVENTS_VISIBLEFILL:
    if (aFrame->StyleVisibility()->IsVisible()) {
      flags |= SVG_HIT_TEST_FILL;
    }
    break;
  case NS_STYLE_POINTER_EVENTS_VISIBLESTROKE:
    if (aFrame->StyleVisibility()->IsVisible()) {
      flags |= SVG_HIT_TEST_STROKE;
    }
    break;
  case NS_STYLE_POINTER_EVENTS_VISIBLE:
    if (aFrame->StyleVisibility()->IsVisible()) {
      flags |= SVG_HIT_TEST_FILL | SVG_HIT_TEST_STROKE;
    }
    break;
  case NS_STYLE_POINTER_EVENTS_PAINTED:
    if (aFrame->StyleSVG()->mFill.mType != eStyleSVGPaintType_None)
      flags |= SVG_HIT_TEST_FILL;
    if (aFrame->StyleSVG()->mStroke.mType != eStyleSVGPaintType_None)
      flags |= SVG_HIT_TEST_STROKE;
    if (aFrame->StyleSVG()->mStrokeOpacity)
      flags |= SVG_HIT_TEST_CHECK_MRECT;
    break;
  case NS_STYLE_POINTER_EVENTS_FILL:
    flags |= SVG_HIT_TEST_FILL;
    break;
  case NS_STYLE_POINTER_EVENTS_STROKE:
    flags |= SVG_HIT_TEST_STROKE;
    break;
  case NS_STYLE_POINTER_EVENTS_ALL:
    flags |= SVG_HIT_TEST_FILL | SVG_HIT_TEST_STROKE;
    break;
  default:
    NS_ERROR("not reached");
    break;
  }

  return flags;
}

bool
nsSVGUtils::SetupCairoStroke(nsIFrame* aFrame, gfxContext* aContext,
                             gfxTextObjectPaint *aObjectPaint)
{
  if (!HasStroke(aFrame, aObjectPaint)) {
    return false;
  }
  SetupCairoStrokeGeometry(aFrame, aContext, aObjectPaint);

  return SetupCairoStrokePaint(aFrame, aContext, aObjectPaint);
}

bool
nsSVGUtils::PaintSVGGlyph(Element* aElement, gfxContext* aContext,
                          gfxFont::DrawMode aDrawMode,
                          gfxTextObjectPaint* aObjectPaint)
{
  nsIFrame* frame = aElement->GetPrimaryFrame();
  nsISVGChildFrame* svgFrame = do_QueryFrame(frame);
  if (!svgFrame) {
    return false;
  }
  nsRenderingContext context;
  context.Init(frame->PresContext()->DeviceContext(), aContext);
  context.AddUserData(&gfxTextObjectPaint::sUserDataKey, aObjectPaint, nullptr);
  nsresult rv = svgFrame->PaintSVG(&context, nullptr);
  return NS_SUCCEEDED(rv);
}

bool
nsSVGUtils::GetSVGGlyphExtents(Element* aElement,
                               const gfxMatrix& aSVGToAppSpace,
                               gfxRect* aResult)
{
  nsIFrame* frame = aElement->GetPrimaryFrame();
  nsISVGChildFrame* svgFrame = do_QueryFrame(frame);
  if (!svgFrame) {
    return false;
  }
  *aResult = svgFrame->GetBBoxContribution(aSVGToAppSpace,
    nsSVGUtils::eBBoxIncludeFill | nsSVGUtils::eBBoxIncludeFillGeometry |
    nsSVGUtils::eBBoxIncludeStroke | nsSVGUtils::eBBoxIncludeStrokeGeometry |
    nsSVGUtils::eBBoxIncludeMarkers);
  return true;
}

nsRect
nsSVGUtils::ToCanvasBounds(const gfxRect &aUserspaceRect,
                           const gfxMatrix &aToCanvas,
                           const nsPresContext *presContext)
{
  return nsLayoutUtils::RoundGfxRectToAppRect(
                          aToCanvas.TransformBounds(aUserspaceRect),
                          presContext->AppUnitsPerDevPixel());
}
