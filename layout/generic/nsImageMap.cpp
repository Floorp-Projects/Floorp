/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* code for HTML client-side image maps */

#include "nsImageMap.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h" // for nsIDOMEvent::InternalDOMEvent()
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/UniquePtr.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsPresContext.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsImageFrame.h"
#include "nsCoord.h"
#include "nsIScriptError.h"
#include "nsIStringBundle.h"
#include "nsContentUtils.h"
#include "ImageLayers.h"

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

using namespace mozilla;
using namespace mozilla::gfx;

class Area {
public:
  explicit Area(nsIContent* aArea);
  virtual ~Area();

  virtual void ParseCoords(const nsAString& aSpec);

  virtual bool IsInside(nscoord x, nscoord y) const = 0;
  virtual void Draw(nsIFrame* aFrame, DrawTarget& aDrawTarget,
                    const ColorPattern& aColor,
                    const StrokeOptions& aStrokeOptions) = 0;
  virtual void GetRect(nsIFrame* aFrame, nsRect& aRect) = 0;

  void HasFocus(bool aHasFocus);

  nsCOMPtr<nsIContent> mArea;
  UniquePtr<nscoord[]> mCoords;
  int32_t mNumCoords;
  bool mHasFocus;
};

Area::Area(nsIContent* aArea)
  : mArea(aArea)
{
  MOZ_COUNT_CTOR(Area);
  NS_PRECONDITION(mArea, "How did that happen?");
  mNumCoords = 0;
  mHasFocus = false;
}

Area::~Area()
{
  MOZ_COUNT_DTOR(Area);
}

#include <stdlib.h>

inline bool
is_space(char c)
{
  return (c == ' ' ||
          c == '\f' ||
          c == '\n' ||
          c == '\r' ||
          c == '\t' ||
          c == '\v');
}

static void logMessage(nsIContent*      aContent,
                       const nsAString& aCoordsSpec,
                       int32_t          aFlags,
                       const char* aMessageName) {
  nsIDocument* doc = aContent->OwnerDoc();

  nsContentUtils::ReportToConsole(
     aFlags, NS_LITERAL_CSTRING("Layout: ImageMap"), doc,
     nsContentUtils::eLAYOUT_PROPERTIES,
     aMessageName,
     nullptr,  /* params */
     0, /* params length */
     nullptr,
     PromiseFlatString(NS_LITERAL_STRING("coords=\"") +
                       aCoordsSpec +
                       NS_LITERAL_STRING("\""))); /* source line */
}

void Area::ParseCoords(const nsAString& aSpec)
{
  char* cp = ToNewCString(aSpec);
  if (cp) {
    char *tptr;
    char *n_str;
    int32_t i, cnt;

    /*
     * Nothing in an empty list
     */
    mNumCoords = 0;
    mCoords = nullptr;
    if (*cp == '\0')
    {
      free(cp);
      return;
    }

    /*
     * Skip beginning whitespace, all whitespace is empty list.
     */
    n_str = cp;
    while (is_space(*n_str))
    {
      n_str++;
    }
    if (*n_str == '\0')
    {
      free(cp);
      return;
    }

    /*
     * Make a pass where any two numbers separated by just whitespace
     * are given a comma separator.  Count entries while passing.
     */
    cnt = 0;
    while (*n_str != '\0')
    {
      bool has_comma;

      /*
       * Skip to a separator
       */
      tptr = n_str;
      while (!is_space(*tptr) && *tptr != ',' && *tptr != '\0')
      {
        tptr++;
      }
      n_str = tptr;

      /*
       * If no more entries, break out here
       */
      if (*n_str == '\0')
      {
        break;
      }

      /*
       * Skip to the end of the separator, noting if we have a
       * comma.
       */
      has_comma = false;
      while (is_space(*tptr) || *tptr == ',')
      {
        if (*tptr == ',')
        {
          if (!has_comma)
          {
            has_comma = true;
          }
          else
          {
            break;
          }
        }
        tptr++;
      }
      /*
       * If this was trailing whitespace we skipped, we are done.
       */
      if ((*tptr == '\0') && !has_comma)
      {
        break;
      }
      /*
       * Else if the separator is all whitespace, and this is not the
       * end of the string, add a comma to the separator.
       */
      else if (!has_comma)
      {
        *n_str = ',';
      }

      /*
       * count the entry skipped.
       */
      cnt++;

      n_str = tptr;
    }
    /*
     * count the last entry in the list.
     */
    cnt++;

    /*
     * Allocate space for the coordinate array.
     */
    UniquePtr<nscoord[]> value_list = MakeUnique<nscoord[]>(cnt);
    if (!value_list)
    {
      free(cp);
      return;
    }

    /*
     * Second pass to copy integer values into list.
     */
    tptr = cp;
    for (i=0; i<cnt; i++)
    {
      char *ptr;

      ptr = strchr(tptr, ',');
      if (ptr)
      {
        *ptr = '\0';
      }
      /*
       * Strip whitespace in front of number because I don't
       * trust atoi to do it on all platforms.
       */
      while (is_space(*tptr))
      {
        tptr++;
      }
      if (*tptr == '\0')
      {
        value_list[i] = 0;
      }
      else
      {
        value_list[i] = (nscoord) ::atoi(tptr);
      }
      if (ptr)
      {
        *ptr = ',';
        tptr = ptr + 1;
      }
    }

    mNumCoords = cnt;
    mCoords = Move(value_list);

    free(cp);
  }
}

void Area::HasFocus(bool aHasFocus)
{
  mHasFocus = aHasFocus;
}

//----------------------------------------------------------------------

class DefaultArea : public Area {
public:
  explicit DefaultArea(nsIContent* aArea);

  virtual bool IsInside(nscoord x, nscoord y) const override;
  virtual void Draw(nsIFrame* aFrame, DrawTarget& aDrawTarget,
                    const ColorPattern& aColor,
                    const StrokeOptions& aStrokeOptions) override;
  virtual void GetRect(nsIFrame* aFrame, nsRect& aRect) override;
};

DefaultArea::DefaultArea(nsIContent* aArea)
  : Area(aArea)
{
}

bool DefaultArea::IsInside(nscoord x, nscoord y) const
{
  return true;
}

void DefaultArea::Draw(nsIFrame* aFrame, DrawTarget& aDrawTarget,
                       const ColorPattern& aColor,
                       const StrokeOptions& aStrokeOptions)
{
  if (mHasFocus) {
    nsRect r(nsPoint(0, 0), aFrame->GetSize());
    const nscoord kOnePixel = nsPresContext::CSSPixelsToAppUnits(1);
    r.width -= kOnePixel;
    r.height -= kOnePixel;
    Rect rect =
      ToRect(nsLayoutUtils::RectToGfxRect(r, aFrame->PresContext()->AppUnitsPerDevPixel()));
    StrokeSnappedEdgesOfRect(rect, aDrawTarget, aColor, aStrokeOptions);
  }
}

void DefaultArea::GetRect(nsIFrame* aFrame, nsRect& aRect)
{
  aRect = aFrame->GetRect();
  aRect.MoveTo(0, 0);
}

//----------------------------------------------------------------------

class RectArea : public Area {
public:
  explicit RectArea(nsIContent* aArea);

  virtual void ParseCoords(const nsAString& aSpec) override;
  virtual bool IsInside(nscoord x, nscoord y) const override;
  virtual void Draw(nsIFrame* aFrame, DrawTarget& aDrawTarget,
                    const ColorPattern& aColor,
                    const StrokeOptions& aStrokeOptions) override;
  virtual void GetRect(nsIFrame* aFrame, nsRect& aRect) override;
};

RectArea::RectArea(nsIContent* aArea)
  : Area(aArea)
{
}

void RectArea::ParseCoords(const nsAString& aSpec)
{
  Area::ParseCoords(aSpec);

  bool saneRect = true;
  int32_t flag = nsIScriptError::warningFlag;
  if (mNumCoords >= 4) {
    if (mCoords[0] > mCoords[2]) {
      // x-coords in reversed order
      nscoord x = mCoords[2];
      mCoords[2] = mCoords[0];
      mCoords[0] = x;
      saneRect = false;
    }

    if (mCoords[1] > mCoords[3]) {
      // y-coords in reversed order
      nscoord y = mCoords[3];
      mCoords[3] = mCoords[1];
      mCoords[1] = y;
      saneRect = false;
    }

    if (mNumCoords > 4) {
      // Someone missed the concept of a rect here
      saneRect = false;
    }
  } else {
    saneRect = false;
    flag = nsIScriptError::errorFlag;
  }

  if (!saneRect) {
    logMessage(mArea, aSpec, flag, "ImageMapRectBoundsError");
  }
}

bool RectArea::IsInside(nscoord x, nscoord y) const
{
  if (mNumCoords >= 4) {       // Note: > is for nav compatibility
    nscoord x1 = mCoords[0];
    nscoord y1 = mCoords[1];
    nscoord x2 = mCoords[2];
    nscoord y2 = mCoords[3];
    NS_ASSERTION(x1 <= x2 && y1 <= y2,
                 "Someone screwed up RectArea::ParseCoords");
    if ((x >= x1) && (x <= x2) && (y >= y1) && (y <= y2)) {
      return true;
    }
  }
  return false;
}

void RectArea::Draw(nsIFrame* aFrame, DrawTarget& aDrawTarget,
                    const ColorPattern& aColor,
                    const StrokeOptions& aStrokeOptions)
{
  if (mHasFocus) {
    if (mNumCoords >= 4) {
      nscoord x1 = nsPresContext::CSSPixelsToAppUnits(mCoords[0]);
      nscoord y1 = nsPresContext::CSSPixelsToAppUnits(mCoords[1]);
      nscoord x2 = nsPresContext::CSSPixelsToAppUnits(mCoords[2]);
      nscoord y2 = nsPresContext::CSSPixelsToAppUnits(mCoords[3]);
      NS_ASSERTION(x1 <= x2 && y1 <= y2,
                   "Someone screwed up RectArea::ParseCoords");
      nsRect r(x1, y1, x2 - x1, y2 - y1);
      Rect rect =
        ToRect(nsLayoutUtils::RectToGfxRect(r, aFrame->PresContext()->AppUnitsPerDevPixel()));
      StrokeSnappedEdgesOfRect(rect, aDrawTarget, aColor, aStrokeOptions);
    }
  }
}

void RectArea::GetRect(nsIFrame* aFrame, nsRect& aRect)
{
  if (mNumCoords >= 4) {
    nscoord x1 = nsPresContext::CSSPixelsToAppUnits(mCoords[0]);
    nscoord y1 = nsPresContext::CSSPixelsToAppUnits(mCoords[1]);
    nscoord x2 = nsPresContext::CSSPixelsToAppUnits(mCoords[2]);
    nscoord y2 = nsPresContext::CSSPixelsToAppUnits(mCoords[3]);
    NS_ASSERTION(x1 <= x2 && y1 <= y2,
                 "Someone screwed up RectArea::ParseCoords");

    aRect.SetRect(x1, y1, x2, y2);
  }
}

//----------------------------------------------------------------------

class PolyArea : public Area {
public:
  explicit PolyArea(nsIContent* aArea);

  virtual void ParseCoords(const nsAString& aSpec) override;
  virtual bool IsInside(nscoord x, nscoord y) const override;
  virtual void Draw(nsIFrame* aFrame, DrawTarget& aDrawTarget,
                    const ColorPattern& aColor,
                    const StrokeOptions& aStrokeOptions) override;
  virtual void GetRect(nsIFrame* aFrame, nsRect& aRect) override;
};

PolyArea::PolyArea(nsIContent* aArea)
  : Area(aArea)
{
}

void PolyArea::ParseCoords(const nsAString& aSpec)
{
  Area::ParseCoords(aSpec);

  if (mNumCoords >= 2) {
    if (mNumCoords & 1U) {
      logMessage(mArea,
                 aSpec,
                 nsIScriptError::warningFlag,
                 "ImageMapPolyOddNumberOfCoords");
    }
  } else {
    logMessage(mArea,
               aSpec,
               nsIScriptError::errorFlag,
               "ImageMapPolyWrongNumberOfCoords");
  }
}

bool PolyArea::IsInside(nscoord x, nscoord y) const
{
  if (mNumCoords >= 6) {
    int32_t intersects = 0;
    nscoord wherex = x;
    nscoord wherey = y;
    int32_t totalv = mNumCoords / 2;
    int32_t totalc = totalv * 2;
    nscoord xval = mCoords[totalc - 2];
    nscoord yval = mCoords[totalc - 1];
    int32_t end = totalc;
    int32_t pointer = 1;

    if ((yval >= wherey) != (mCoords[pointer] >= wherey)) {
      if ((xval >= wherex) == (mCoords[0] >= wherex)) {
        intersects += (xval >= wherex) ? 1 : 0;
      } else {
        intersects += ((xval - (yval - wherey) *
                        (mCoords[0] - xval) /
                        (mCoords[pointer] - yval)) >= wherex) ? 1 : 0;
      }
    }

    // XXX I wonder what this is doing; this is a translation of ptinpoly.c
    while (pointer < end)  {
      yval = mCoords[pointer];
      pointer += 2;
      if (yval >= wherey)  {
        while((pointer < end) && (mCoords[pointer] >= wherey))
          pointer+=2;
        if (pointer >= end)
          break;
        if ((mCoords[pointer-3] >= wherex) ==
            (mCoords[pointer-1] >= wherex)) {
          intersects += (mCoords[pointer-3] >= wherex) ? 1 : 0;
        } else {
          intersects +=
            ((mCoords[pointer-3] - (mCoords[pointer-2] - wherey) *
              (mCoords[pointer-1] - mCoords[pointer-3]) /
              (mCoords[pointer] - mCoords[pointer - 2])) >= wherex) ? 1:0;
        }
      }  else  {
        while((pointer < end) && (mCoords[pointer] < wherey))
          pointer+=2;
        if (pointer >= end)
          break;
        if ((mCoords[pointer-3] >= wherex) ==
            (mCoords[pointer-1] >= wherex)) {
          intersects += (mCoords[pointer-3] >= wherex) ? 1:0;
        } else {
          intersects +=
            ((mCoords[pointer-3] - (mCoords[pointer-2] - wherey) *
              (mCoords[pointer-1] - mCoords[pointer-3]) /
              (mCoords[pointer] - mCoords[pointer - 2])) >= wherex) ? 1:0;
        }
      }
    }
    if ((intersects & 1) != 0) {
      return true;
    }
  }
  return false;
}

void PolyArea::Draw(nsIFrame* aFrame, DrawTarget& aDrawTarget,
                    const ColorPattern& aColor,
                    const StrokeOptions& aStrokeOptions)
{
  if (mHasFocus) {
    if (mNumCoords >= 6) {
      // Where possible, we want all horizontal and vertical lines to align on
      // pixel rows or columns, and to start at pixel boundaries so that one
      // pixel dashing neatly sits on pixels to give us neat lines. To achieve
      // that we draw each line segment as a separate path, snapping it to
      // device pixels if applicable.
      nsPresContext* pc = aFrame->PresContext();
      Point p1(pc->CSSPixelsToDevPixels(mCoords[0]),
               pc->CSSPixelsToDevPixels(mCoords[1]));
      Point p2, p1snapped, p2snapped;
      for (int32_t i = 2; i < mNumCoords; i += 2) {
        p2.x = pc->CSSPixelsToDevPixels(mCoords[i]);
        p2.y = pc->CSSPixelsToDevPixels(mCoords[i+1]);
        p1snapped = p1;
        p2snapped = p2;
        SnapLineToDevicePixelsForStroking(p1snapped, p2snapped, aDrawTarget,
                                          aStrokeOptions.mLineWidth);
        aDrawTarget.StrokeLine(p1snapped, p2snapped, aColor, aStrokeOptions);
        p1 = p2;
      }
      p2.x = pc->CSSPixelsToDevPixels(mCoords[0]);
      p2.y = pc->CSSPixelsToDevPixels(mCoords[1]);
      p1snapped = p1;
      p2snapped = p2;
      SnapLineToDevicePixelsForStroking(p1snapped, p2snapped, aDrawTarget,
                                        aStrokeOptions.mLineWidth);
      aDrawTarget.StrokeLine(p1snapped, p2snapped, aColor, aStrokeOptions);
    }
  }
}

void PolyArea::GetRect(nsIFrame* aFrame, nsRect& aRect)
{
  if (mNumCoords >= 6) {
    nscoord x1, x2, y1, y2, xtmp, ytmp;
    x1 = x2 = nsPresContext::CSSPixelsToAppUnits(mCoords[0]);
    y1 = y2 = nsPresContext::CSSPixelsToAppUnits(mCoords[1]);
    for (int32_t i = 2; i < mNumCoords; i += 2) {
      xtmp = nsPresContext::CSSPixelsToAppUnits(mCoords[i]);
      ytmp = nsPresContext::CSSPixelsToAppUnits(mCoords[i+1]);
      x1 = x1 < xtmp ? x1 : xtmp;
      y1 = y1 < ytmp ? y1 : ytmp;
      x2 = x2 > xtmp ? x2 : xtmp;
      y2 = y2 > ytmp ? y2 : ytmp;
    }

    aRect.SetRect(x1, y1, x2, y2);
  }
}

//----------------------------------------------------------------------

class CircleArea : public Area {
public:
  explicit CircleArea(nsIContent* aArea);

  virtual void ParseCoords(const nsAString& aSpec) override;
  virtual bool IsInside(nscoord x, nscoord y) const override;
  virtual void Draw(nsIFrame* aFrame, DrawTarget& aDrawTarget,
                    const ColorPattern& aColor,
                    const StrokeOptions& aStrokeOptions) override;
  virtual void GetRect(nsIFrame* aFrame, nsRect& aRect) override;
};

CircleArea::CircleArea(nsIContent* aArea)
  : Area(aArea)
{
}

void CircleArea::ParseCoords(const nsAString& aSpec)
{
  Area::ParseCoords(aSpec);

  bool wrongNumberOfCoords = false;
  int32_t flag = nsIScriptError::warningFlag;
  if (mNumCoords >= 3) {
    if (mCoords[2] < 0) {
      logMessage(mArea,
                 aSpec,
                 nsIScriptError::errorFlag,
                 "ImageMapCircleNegativeRadius");
    }

    if (mNumCoords > 3) {
      wrongNumberOfCoords = true;
    }
  } else {
    wrongNumberOfCoords = true;
    flag = nsIScriptError::errorFlag;
  }

  if (wrongNumberOfCoords) {
    logMessage(mArea,
               aSpec,
               flag,
               "ImageMapCircleWrongNumberOfCoords");
  }
}

bool CircleArea::IsInside(nscoord x, nscoord y) const
{
  // Note: > is for nav compatibility
  if (mNumCoords >= 3) {
    nscoord x1 = mCoords[0];
    nscoord y1 = mCoords[1];
    nscoord radius = mCoords[2];
    if (radius < 0) {
      return false;
    }
    nscoord dx = x1 - x;
    nscoord dy = y1 - y;
    nscoord dist = (dx * dx) + (dy * dy);
    if (dist <= (radius * radius)) {
      return true;
    }
  }
  return false;
}

void CircleArea::Draw(nsIFrame* aFrame, DrawTarget& aDrawTarget,
                      const ColorPattern& aColor,
                      const StrokeOptions& aStrokeOptions)
{
  if (mHasFocus) {
    if (mNumCoords >= 3) {
      Point center(aFrame->PresContext()->CSSPixelsToDevPixels(mCoords[0]),
                   aFrame->PresContext()->CSSPixelsToDevPixels(mCoords[1]));
      Float diameter =
        2 * aFrame->PresContext()->CSSPixelsToDevPixels(mCoords[2]);
      if (diameter <= 0) {
        return;
      }
      RefPtr<PathBuilder> builder = aDrawTarget.CreatePathBuilder();
      AppendEllipseToPath(builder, center, Size(diameter, diameter));
      RefPtr<Path> circle = builder->Finish();
      aDrawTarget.Stroke(circle, aColor, aStrokeOptions);
    }
  }
}

void CircleArea::GetRect(nsIFrame* aFrame, nsRect& aRect)
{
  if (mNumCoords >= 3) {
    nscoord x1 = nsPresContext::CSSPixelsToAppUnits(mCoords[0]);
    nscoord y1 = nsPresContext::CSSPixelsToAppUnits(mCoords[1]);
    nscoord radius = nsPresContext::CSSPixelsToAppUnits(mCoords[2]);
    if (radius < 0) {
      return;
    }

    aRect.SetRect(x1 - radius, y1 - radius, x1 + radius, y1 + radius);
  }
}

//----------------------------------------------------------------------


nsImageMap::nsImageMap() :
  mImageFrame(nullptr),
  mContainsBlockContents(false)
{
}

nsImageMap::~nsImageMap()
{
  NS_ASSERTION(mAreas.Length() == 0, "Destroy was not called");
}

NS_IMPL_ISUPPORTS(nsImageMap,
                  nsIMutationObserver,
                  nsIDOMEventListener)

nsresult
nsImageMap::GetBoundsForAreaContent(nsIContent *aContent,
                                    nsRect& aBounds)
{
  NS_ENSURE_TRUE(aContent && mImageFrame, NS_ERROR_INVALID_ARG);

  // Find the Area struct associated with this content node, and return bounds
  uint32_t i, n = mAreas.Length();
  for (i = 0; i < n; i++) {
    Area* area = mAreas.ElementAt(i);
    if (area->mArea == aContent) {
      aBounds = nsRect();
      area->GetRect(mImageFrame, aBounds);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

void
nsImageMap::FreeAreas()
{
  uint32_t i, n = mAreas.Length();
  for (i = 0; i < n; i++) {
    Area* area = mAreas.ElementAt(i);
    if (area->mArea->IsInUncomposedDoc()) {
      NS_ASSERTION(area->mArea->GetPrimaryFrame() == mImageFrame,
                   "Unexpected primary frame");

      area->mArea->SetPrimaryFrame(nullptr);
    }

    area->mArea->RemoveSystemEventListener(NS_LITERAL_STRING("focus"), this,
                                           false);
    area->mArea->RemoveSystemEventListener(NS_LITERAL_STRING("blur"), this,
                                           false);
    delete area;
  }
  mAreas.Clear();
}

nsresult
nsImageMap::Init(nsImageFrame* aImageFrame, nsIContent* aMap)
{
  NS_PRECONDITION(aMap, "null ptr");
  if (!aMap) {
    return NS_ERROR_NULL_POINTER;
  }
  mImageFrame = aImageFrame;

  mMap = aMap;
  mMap->AddMutationObserver(this);

  // "Compile" the areas in the map into faster access versions
  return UpdateAreas();
}


nsresult
nsImageMap::SearchForAreas(nsIContent* aParent, bool& aFoundArea,
                           bool& aFoundAnchor)
{
  nsresult rv = NS_OK;
  uint32_t i, n = aParent->GetChildCount();

  // Look for <area> or <a> elements. We'll use whichever type we find first.
  for (i = 0; i < n; i++) {
    nsIContent *child = aParent->GetChildAt(i);

    // If we haven't determined that the map element contains an
    // <a> element yet, then look for <area>.
    if (!aFoundAnchor && child->IsHTMLElement(nsGkAtoms::area)) {
      aFoundArea = true;
      rv = AddArea(child);
      NS_ENSURE_SUCCESS(rv, rv);

      // Continue to next child. This stops mContainsBlockContents from
      // getting set. It also makes us ignore children of <area>s which
      // is consistent with how we react to dynamic insertion of such
      // children.
      continue;
    }

    // If we haven't determined that the map element contains an
    // <area> element yet, then look for <a>.
    if (!aFoundArea && child->IsHTMLElement(nsGkAtoms::a)) {
      aFoundAnchor = true;
      rv = AddArea(child);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (child->IsElement()) {
      mContainsBlockContents = true;
      rv = SearchForAreas(child, aFoundArea, aFoundAnchor);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult
nsImageMap::UpdateAreas()
{
  // Get rid of old area data
  FreeAreas();

  bool foundArea = false;
  bool foundAnchor = false;
  mContainsBlockContents = false;

  nsresult rv = SearchForAreas(mMap, foundArea, foundAnchor);
#ifdef ACCESSIBILITY
  if (NS_SUCCEEDED(rv)) {
    nsAccessibilityService* accService = GetAccService();
    if (accService) {
      accService->UpdateImageMap(mImageFrame);
    }
  }
#endif
  return rv;
}

nsresult
nsImageMap::AddArea(nsIContent* aArea)
{
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::rect, &nsGkAtoms::rectangle,
     &nsGkAtoms::circle, &nsGkAtoms::circ,
     &nsGkAtoms::_default,
     &nsGkAtoms::poly, &nsGkAtoms::polygon,
     nullptr};

  Area* area;
  switch (aArea->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::shape,
                                 strings, eIgnoreCase)) {
  case nsIContent::ATTR_VALUE_NO_MATCH:
  case nsIContent::ATTR_MISSING:
  case 0:
  case 1:
    area = new RectArea(aArea);
    break;
  case 2:
  case 3:
    area = new CircleArea(aArea);
    break;
  case 4:
    area = new DefaultArea(aArea);
    break;
  case 5:
  case 6:
    area = new PolyArea(aArea);
    break;
  default:
    area = nullptr;
    NS_NOTREACHED("FindAttrValueIn returned an unexpected value.");
    break;
  }
  if (!area)
    return NS_ERROR_OUT_OF_MEMORY;

  //Add focus listener to track area focus changes
  aArea->AddSystemEventListener(NS_LITERAL_STRING("focus"), this, false,
                                false);
  aArea->AddSystemEventListener(NS_LITERAL_STRING("blur"), this, false,
                                false);

  // This is a nasty hack.  It needs to go away: see bug 135040.  Once this is
  // removed, the code added to RestyleManager::RestyleElement,
  // nsCSSFrameConstructor::ContentRemoved (both hacks there), and
  // RestyleManager::ProcessRestyledFrames to work around this issue can
  // be removed.
  aArea->SetPrimaryFrame(mImageFrame);

  nsAutoString coords;
  aArea->GetAttr(kNameSpaceID_None, nsGkAtoms::coords, coords);
  area->ParseCoords(coords);
  mAreas.AppendElement(area);
  return NS_OK;
}

nsIContent*
nsImageMap::GetArea(nscoord aX, nscoord aY) const
{
  NS_ASSERTION(mMap, "Not initialized");
  uint32_t i, n = mAreas.Length();
  for (i = 0; i < n; i++) {
    Area* area = mAreas.ElementAt(i);
    if (area->IsInside(aX, aY)) {
      return area->mArea;
    }
  }

  return nullptr;
}

nsIContent*
nsImageMap::GetAreaAt(uint32_t aIndex) const
{
  return mAreas.ElementAt(aIndex)->mArea;
}

void
nsImageMap::Draw(nsIFrame* aFrame, DrawTarget& aDrawTarget,
                 const ColorPattern& aColor,
                 const StrokeOptions& aStrokeOptions)
{
  uint32_t i, n = mAreas.Length();
  for (i = 0; i < n; i++) {
    Area* area = mAreas.ElementAt(i);
    area->Draw(aFrame, aDrawTarget, aColor, aStrokeOptions);
  }
}

void
nsImageMap::MaybeUpdateAreas(nsIContent *aContent)
{
  if (aContent == mMap || mContainsBlockContents) {
    UpdateAreas();
  }
}

void
nsImageMap::AttributeChanged(nsIDocument*  aDocument,
                             dom::Element* aElement,
                             int32_t       aNameSpaceID,
                             nsIAtom*      aAttribute,
                             int32_t       aModType,
                             const nsAttrValue* aOldValue)
{
  // If the parent of the changing content node is our map then update
  // the map.  But only do this if the node is an HTML <area> or <a>
  // and the attribute that's changing is "shape" or "coords" -- those
  // are the only cases we care about.
  if ((aElement->NodeInfo()->Equals(nsGkAtoms::area) ||
       aElement->NodeInfo()->Equals(nsGkAtoms::a)) &&
      aElement->IsHTMLElement() &&
      aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::shape ||
       aAttribute == nsGkAtoms::coords)) {
    MaybeUpdateAreas(aElement->GetParent());
  } else if (aElement == mMap &&
             aNameSpaceID == kNameSpaceID_None &&
             (aAttribute == nsGkAtoms::name ||
              aAttribute == nsGkAtoms::id) &&
             mImageFrame) {
    // ID or name has changed. Let ImageFrame recreate ImageMap.
    mImageFrame->DisconnectMap();
  }
}

void
nsImageMap::ContentAppended(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aFirstNewContent,
                            int32_t     /* unused */)
{
  MaybeUpdateAreas(aContainer);
}

void
nsImageMap::ContentInserted(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            int32_t /* unused */)
{
  MaybeUpdateAreas(aContainer);
}

void
nsImageMap::ContentRemoved(nsIDocument *aDocument,
                           nsIContent* aContainer,
                           nsIContent* aChild,
                           int32_t aIndexInContainer,
                           nsIContent* aPreviousSibling)
{
  MaybeUpdateAreas(aContainer);
}

void
nsImageMap::ParentChainChanged(nsIContent* aContent)
{
  NS_ASSERTION(aContent == mMap,
               "Unexpected ParentChainChanged notification!");
  if (mImageFrame) {
    mImageFrame->DisconnectMap();
  }
}

nsresult
nsImageMap::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString eventType;
  aEvent->GetType(eventType);
  bool focus = eventType.EqualsLiteral("focus");
  MOZ_ASSERT(focus == !eventType.EqualsLiteral("blur"),
             "Unexpected event type");

  //Set which one of our areas changed focus
  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(
    aEvent->InternalDOMEvent()->GetTarget());
  if (!targetContent) {
    return NS_OK;
  }
  uint32_t i, n = mAreas.Length();
  for (i = 0; i < n; i++) {
    Area* area = mAreas.ElementAt(i);
    if (area->mArea == targetContent) {
      //Set or Remove internal focus
      area->HasFocus(focus);
      //Now invalidate the rect
      if (mImageFrame) {
        mImageFrame->InvalidateFrame();
      }
      break;
    }
  }
  return NS_OK;
}

void
nsImageMap::Destroy(void)
{
  FreeAreas();
  mImageFrame = nullptr;
  mMap->RemoveMutationObserver(this);
}
