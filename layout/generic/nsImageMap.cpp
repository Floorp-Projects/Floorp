/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* code for HTML client-side image maps */

#include "nsImageMap.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIRenderingContext.h"
#include "nsPresContext.h"
#include "nsIURL.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsTextFragment.h"
#include "nsIContent.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIDOMEventReceiver.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsFrameManager.h"
#include "nsCoord.h"
#include "nsIImageMap.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIStringBundle.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"

static NS_DEFINE_CID(kCStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

class Area {
public:
  Area(nsIContent* aArea);
  virtual ~Area();

  virtual void ParseCoords(const nsAString& aSpec);

  virtual PRBool IsInside(nscoord x, nscoord y) const = 0;
  virtual void Draw(nsPresContext* aCX,
                    nsIRenderingContext& aRC) = 0;
  virtual void GetRect(nsPresContext* aCX, nsRect& aRect) = 0;

  void HasFocus(PRBool aHasFocus);

  void GetHREF(nsAString& aHref) const;
  void GetArea(nsIContent** aArea) const;

  nsCOMPtr<nsIContent> mArea;
  nscoord* mCoords;
  PRInt32 mNumCoords;
  PRPackedBool mHasFocus;
};

Area::Area(nsIContent* aArea)
  : mArea(aArea)
{
  MOZ_COUNT_CTOR(Area);
  mCoords = nsnull;
  mNumCoords = 0;
  mHasFocus = PR_FALSE;
}

Area::~Area()
{
  MOZ_COUNT_DTOR(Area);
  delete [] mCoords;
}

void 
Area::GetHREF(nsAString& aHref) const
{
  aHref.Truncate();
  if (mArea) {
    mArea->GetAttr(kNameSpaceID_None, nsGkAtoms::href, aHref);
  }
}
 
void 
Area::GetArea(nsIContent** aArea) const
{
  *aArea = mArea;
  NS_IF_ADDREF(*aArea);
}

#include <stdlib.h>

inline PRBool
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
                       PRInt32          aFlags,
                       const char* aMessageName) {
  nsIURI* documentURI = nsnull;
  nsIDocument* doc = aContent->GetOwnerDoc();
  if (doc) {
    documentURI = doc->GetDocumentURI();
  }
  nsContentUtils::ReportToConsole(
     nsContentUtils::eLAYOUT_PROPERTIES,
     aMessageName,
     nsnull,  /* params */
     0, /* params length */
     documentURI,
     PromiseFlatString(NS_LITERAL_STRING("coords=\"") +
                       aCoordsSpec +
                       NS_LITERAL_STRING("\"")), /* source line */
     0, /* line number */
     0, /* column number */
     aFlags,
     "ImageMap");
}

// XXX straight copy from laymap.c
static nscoord* lo_parse_coord_list(char *str, PRInt32* value_cnt)
{
  char *tptr;
  char *n_str;
  PRInt32 i, cnt;
  PRInt32 *value_list;

  /*
   * Nothing in an empty list
   */
  *value_cnt = 0;
  if (!str || *str == '\0')
  {
    return nsnull;
  }

  /*
   * Skip beginning whitespace, all whitespace is empty list.
   */
  n_str = str;
  while (is_space(*n_str))
  {
    n_str++;
  }
  if (*n_str == '\0')
  {
    return nsnull;
  }

  /*
   * Make a pass where any two numbers separated by just whitespace
   * are given a comma separator.  Count entries while passing.
   */
  cnt = 0;
  while (*n_str != '\0')
  {
    PRBool has_comma;

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
    has_comma = PR_FALSE;
    while (is_space(*tptr) || *tptr == ',')
    {
      if (*tptr == ',')
      {
        if (has_comma == PR_FALSE)
        {
          has_comma = PR_TRUE;
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
    if ((*tptr == '\0')&&(has_comma == PR_FALSE))
    {
      break;
    }
    /*
     * Else if the separator is all whitespace, and this is not the
     * end of the string, add a comma to the separator.
     */
    else if (has_comma == PR_FALSE)
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
  value_list = new nscoord[cnt];
  if (!value_list)
  {
    return nsnull;
  }

  /*
   * Second pass to copy integer values into list.
   */
  tptr = str;
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

  *value_cnt = cnt;
  return value_list;
}

void Area::ParseCoords(const nsAString& aSpec)
{
  char* cp = ToNewCString(aSpec);
  if (cp) {
    mCoords = lo_parse_coord_list(cp, &mNumCoords);
    NS_Free(cp);
  }
}

void Area::HasFocus(PRBool aHasFocus)
{
  mHasFocus = aHasFocus;
}

//----------------------------------------------------------------------

class DefaultArea : public Area {
public:
  DefaultArea(nsIContent* aArea);

  virtual PRBool IsInside(nscoord x, nscoord y) const;
  virtual void Draw(nsPresContext* aCX,
                    nsIRenderingContext& aRC);
  virtual void GetRect(nsPresContext* aCX, nsRect& aRect);
};

DefaultArea::DefaultArea(nsIContent* aArea)
  : Area(aArea)
{
}

PRBool DefaultArea::IsInside(nscoord x, nscoord y) const
{
  return PR_TRUE;
}

void DefaultArea::Draw(nsPresContext* aCX, nsIRenderingContext& aRC)
{
}

void DefaultArea::GetRect(nsPresContext* aCX, nsRect& aRect)
{
}

//----------------------------------------------------------------------

class RectArea : public Area {
public:
  RectArea(nsIContent* aArea);

  virtual void ParseCoords(const nsAString& aSpec);
  virtual PRBool IsInside(nscoord x, nscoord y) const;
  virtual void Draw(nsPresContext* aCX,
                    nsIRenderingContext& aRC);
  virtual void GetRect(nsPresContext* aCX, nsRect& aRect);
};

RectArea::RectArea(nsIContent* aArea)
  : Area(aArea)
{
}

void RectArea::ParseCoords(const nsAString& aSpec)
{
  Area::ParseCoords(aSpec);

  PRBool saneRect = PR_TRUE;
  PRInt32 flag = nsIScriptError::warningFlag;
  if (mNumCoords >= 4) {
    if (mCoords[0] > mCoords[2]) {
      // x-coords in reversed order
      nscoord x = mCoords[2];
      mCoords[2] = mCoords[0];
      mCoords[0] = x;
      saneRect = PR_FALSE;
    }
  
    if (mCoords[1] > mCoords[3]) {
      // y-coords in reversed order
      nscoord y = mCoords[3];
      mCoords[3] = mCoords[1];
      mCoords[1] = y;
      saneRect = PR_FALSE;
    }

    if (mNumCoords > 4) {
      // Someone missed the concept of a rect here
      saneRect = PR_FALSE;
    }
  } else {
    saneRect = PR_FALSE;
    flag = nsIScriptError::errorFlag;
  }

  if (!saneRect) {
    logMessage(mArea, aSpec, flag, "ImageMapRectBoundsError");
  }
}

PRBool RectArea::IsInside(nscoord x, nscoord y) const
{
  if (mNumCoords >= 4) {       // Note: > is for nav compatability
    nscoord x1 = mCoords[0];
    nscoord y1 = mCoords[1];
    nscoord x2 = mCoords[2];
    nscoord y2 = mCoords[3];
    NS_ASSERTION(x1 <= x2 && y1 <= y2,
                 "Someone screwed up RectArea::ParseCoords");
    if ((x >= x1) && (x <= x2) && (y >= y1) && (y <= y2)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void RectArea::Draw(nsPresContext* aCX, nsIRenderingContext& aRC)
{
  if (mHasFocus) {
    if (mNumCoords >= 4) {
      nscoord x1 = nsPresContext::CSSPixelsToAppUnits(mCoords[0]);
      nscoord y1 = nsPresContext::CSSPixelsToAppUnits(mCoords[1]);
      nscoord x2 = nsPresContext::CSSPixelsToAppUnits(mCoords[2]);
      nscoord y2 = nsPresContext::CSSPixelsToAppUnits(mCoords[3]);
      NS_ASSERTION(x1 <= x2 && y1 <= y2,
                   "Someone screwed up RectArea::ParseCoords");
      aRC.DrawLine(x1, y1, x1, y2);
      aRC.DrawLine(x1, y2, x2, y2);
      aRC.DrawLine(x1, y1, x2, y1);
      aRC.DrawLine(x2, y1, x2, y2);
    }
  }
}

void RectArea::GetRect(nsPresContext* aCX, nsRect& aRect)
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
  PolyArea(nsIContent* aArea);

  virtual void ParseCoords(const nsAString& aSpec);
  virtual PRBool IsInside(nscoord x, nscoord y) const;
  virtual void Draw(nsPresContext* aCX,
                    nsIRenderingContext& aRC);
  virtual void GetRect(nsPresContext* aCX, nsRect& aRect);
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

PRBool PolyArea::IsInside(nscoord x, nscoord y) const
{
  if (mNumCoords >= 6) {
    PRInt32 intersects = 0;
    nscoord wherex = x;
    nscoord wherey = y;
    PRInt32 totalv = mNumCoords / 2;
    PRInt32 totalc = totalv * 2;
    nscoord xval = mCoords[totalc - 2];
    nscoord yval = mCoords[totalc - 1];
    PRInt32 end = totalc;
    PRInt32 pointer = 1;

    if ((yval >= wherey) != (mCoords[pointer] >= wherey))
      if ((xval >= wherex) == (mCoords[0] >= wherex))
        intersects += (xval >= wherex) ? 1 : 0;
      else
        intersects += ((xval - (yval - wherey) *
                        (mCoords[0] - xval) /
                        (mCoords[pointer] - yval)) >= wherex) ? 1 : 0;

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
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void PolyArea::Draw(nsPresContext* aCX, nsIRenderingContext& aRC)
{
  if (mHasFocus) {
    if (mNumCoords >= 6) {
      nscoord x0 = nsPresContext::CSSPixelsToAppUnits(mCoords[0]);
      nscoord y0 = nsPresContext::CSSPixelsToAppUnits(mCoords[1]);
      nscoord x1, y1;
      for (PRInt32 i = 2; i < mNumCoords; i += 2) {
        x1 = nsPresContext::CSSPixelsToAppUnits(mCoords[i]);
        y1 = nsPresContext::CSSPixelsToAppUnits(mCoords[i+1]);
        aRC.DrawLine(x0, y0, x1, y1);
        x0 = x1;
        y0 = y1;
      }
      x1 = nsPresContext::CSSPixelsToAppUnits(mCoords[0]);
      y1 = nsPresContext::CSSPixelsToAppUnits(mCoords[1]);
      aRC.DrawLine(x0, y0, x1, y1);
    }
  }
}

void PolyArea::GetRect(nsPresContext* aCX, nsRect& aRect)
{
  if (mNumCoords >= 6) {
    nscoord x1, x2, y1, y2, xtmp, ytmp;
    x1 = x2 = nsPresContext::CSSPixelsToAppUnits(mCoords[0]);
    y1 = y2 = nsPresContext::CSSPixelsToAppUnits(mCoords[1]);
    for (PRInt32 i = 2; i < mNumCoords; i += 2) {
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
  CircleArea(nsIContent* aArea);

  virtual void ParseCoords(const nsAString& aSpec);
  virtual PRBool IsInside(nscoord x, nscoord y) const;
  virtual void Draw(nsPresContext* aCX,
                    nsIRenderingContext& aRC);
  virtual void GetRect(nsPresContext* aCX, nsRect& aRect);
};

CircleArea::CircleArea(nsIContent* aArea)
  : Area(aArea)
{
}

void CircleArea::ParseCoords(const nsAString& aSpec)
{
  Area::ParseCoords(aSpec);

  PRBool wrongNumberOfCoords = PR_FALSE;
  PRInt32 flag = nsIScriptError::warningFlag;
  if (mNumCoords >= 3) {
    if (mCoords[2] < 0) {
      logMessage(mArea,
                 aSpec,
                 nsIScriptError::errorFlag,
                 "ImageMapCircleNegativeRadius");
    }
  
    if (mNumCoords > 3) {
      wrongNumberOfCoords = PR_TRUE;
    }
  } else {
    wrongNumberOfCoords = PR_TRUE;
    flag = nsIScriptError::errorFlag;
  }

  if (wrongNumberOfCoords) {
    logMessage(mArea,
               aSpec,
               flag,
               "ImageMapCircleWrongNumberOfCoords");
  }
}

PRBool CircleArea::IsInside(nscoord x, nscoord y) const
{
  // Note: > is for nav compatability
  if (mNumCoords >= 3) {
    nscoord x1 = mCoords[0];
    nscoord y1 = mCoords[1];
    nscoord radius = mCoords[2];
    if (radius < 0) {
      return PR_FALSE;
    }
    nscoord dx = x1 - x;
    nscoord dy = y1 - y;
    nscoord dist = (dx * dx) + (dy * dy);
    if (dist <= (radius * radius)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void CircleArea::Draw(nsPresContext* aCX, nsIRenderingContext& aRC)
{
  if (mHasFocus) {
    if (mNumCoords >= 3) {
      nscoord x1 = nsPresContext::CSSPixelsToAppUnits(mCoords[0]);
      nscoord y1 = nsPresContext::CSSPixelsToAppUnits(mCoords[1]);
      nscoord radius = nsPresContext::CSSPixelsToAppUnits(mCoords[2]);
      if (radius < 0) {
        return;
      }
      nscoord x = x1 - radius;
      nscoord y = y1 - radius;
      nscoord w = 2 * radius;
      aRC.DrawEllipse(x, y, w, w);
    }
  }
}

void CircleArea::GetRect(nsPresContext* aCX, nsRect& aRect)
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
  mPresShell(nsnull),
  mImageFrame(nsnull),
  mContainsBlockContents(PR_FALSE)
{
}

nsImageMap::~nsImageMap()
{
  NS_ASSERTION(mAreas.Count() == 0, "Destroy was not called");
}

NS_IMPL_ISUPPORTS4(nsImageMap,
                   nsIMutationObserver,
                   nsIDOMFocusListener,
                   nsIDOMEventListener,
                   nsIImageMap)

NS_IMETHODIMP
nsImageMap::GetBoundsForAreaContent(nsIContent *aContent, 
                                   nsPresContext* aPresContext, 
                                   nsRect& aBounds)
{
  // Find the Area struct associated with this content node, and return bounds
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    if (area->mArea == aContent) {
      area->GetRect(aPresContext, aBounds);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

void
nsImageMap::FreeAreas()
{
  nsFrameManager *frameManager = mPresShell->FrameManager();

  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    frameManager->RemoveAsPrimaryFrame(area->mArea, mImageFrame);

    nsCOMPtr<nsIContent> areaContent;
    area->GetArea(getter_AddRefs(areaContent));
    if (areaContent) {
      nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(areaContent));
      if (rec) {
        rec->RemoveEventListenerByIID(this, NS_GET_IID(nsIDOMFocusListener));
      }
    }
    delete area;
  }
  mAreas.Clear();
}

nsresult
nsImageMap::Init(nsIPresShell* aPresShell, nsIFrame* aImageFrame, nsIDOMHTMLMapElement* aMap)
{
  NS_PRECONDITION(nsnull != aMap, "null ptr");
  if (nsnull == aMap) {
    return NS_ERROR_NULL_POINTER;
  }
  mPresShell = aPresShell;
  mImageFrame = aImageFrame;

  mMap = do_QueryInterface(aMap);
  NS_ASSERTION(mMap, "aMap is not an nsIContent!");
  mMap->AddMutationObserver(this);

  // "Compile" the areas in the map into faster access versions
  return UpdateAreas();
}


nsresult
nsImageMap::SearchForAreas(nsIContent* aParent, PRBool& aFoundArea,
                           PRBool& aFoundAnchor)
{
  nsresult rv = NS_OK;
  PRUint32 i, n = aParent->GetChildCount();

  // Look for <area> or <a> elements. We'll use whichever type we find first.
  for (i = 0; i < n; i++) {
    nsIContent *child = aParent->GetChildAt(i);

    if (child->IsNodeOfType(nsINode::eHTML)) {
      // If we haven't determined that the map element contains an
      // <a> element yet, then look for <area>.
      if (!aFoundAnchor && child->Tag() == nsGkAtoms::area) {
        aFoundArea = PR_TRUE;
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
      if (!aFoundArea && child->Tag() == nsGkAtoms::a) {
        aFoundAnchor = PR_TRUE;
        rv = AddArea(child);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    
    if (child->IsNodeOfType(nsINode::eELEMENT)) {
      mContainsBlockContents = PR_TRUE;
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

  PRBool foundArea = PR_FALSE;
  PRBool foundAnchor = PR_FALSE;
  mContainsBlockContents = PR_FALSE;

  return SearchForAreas(mMap, foundArea, foundAnchor);
}

nsresult
nsImageMap::AddArea(nsIContent* aArea)
{
  nsAutoString coords;
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::_empty, &nsGkAtoms::rect, &nsGkAtoms::rectangle,
     &nsGkAtoms::poly, &nsGkAtoms::polygon, &nsGkAtoms::circle,
     &nsGkAtoms::circ, &nsGkAtoms::_default, nsnull};

  aArea->GetAttr(kNameSpaceID_None, nsGkAtoms::coords, coords);

  Area* area;
  switch (aArea->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::shape,
                                 strings, eIgnoreCase)) {
    case nsIContent::ATTR_MISSING:
    case 0:
    case 1:
    case 2:
      area = new RectArea(aArea);
      break;
    case 3:
    case 4:
      area = new PolyArea(aArea);
      break;
    case 5:
    case 6:
      area = new CircleArea(aArea);
      break;
    case 7:
      area = new DefaultArea(aArea);
      break;
    default:
      // Unknown area type; bail
      return NS_OK;
  }
  if (!area)
    return NS_ERROR_OUT_OF_MEMORY;

  //Add focus listener to track area focus changes
  nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(aArea));
  if (rec) {
    rec->AddEventListenerByIID(this, NS_GET_IID(nsIDOMFocusListener));
  }

  mPresShell->FrameManager()->SetPrimaryFrameFor(aArea, mImageFrame);
  aArea->SetMayHaveFrame(PR_TRUE);
  NS_ASSERTION(aArea->MayHaveFrame(), "SetMayHaveFrame failed?");

  area->ParseCoords(coords);
  mAreas.AppendElement(area);
  return NS_OK;
}

PRBool
nsImageMap::IsInside(nscoord aX, nscoord aY,
                     nsIContent** aContent) const
{
  NS_ASSERTION(mMap, "Not initialized");
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    if (area->IsInside(aX, aY)) {
      area->GetArea(aContent);

      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

void
nsImageMap::Draw(nsPresContext* aCX, nsIRenderingContext& aRC)
{
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    area->Draw(aCX, aRC);
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
nsImageMap::AttributeChanged(nsIDocument* aDocument,
                             nsIContent*  aContent,
                             PRInt32      aNameSpaceID,
                             nsIAtom*     aAttribute,
                             PRInt32      aModType)
{
  // If the parent of the changing content node is our map then update
  // the map.  But only do this if the node is an HTML <area> or <a>
  // and the attribute that's changing is "shape" or "coords" -- those
  // are the only cases we care about.
  if ((aContent->NodeInfo()->Equals(nsGkAtoms::area) ||
       aContent->NodeInfo()->Equals(nsGkAtoms::a)) &&
      aContent->IsNodeOfType(nsINode::eHTML) &&
      aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::shape ||
       aAttribute == nsGkAtoms::coords)) {
    MaybeUpdateAreas(aContent->GetParent());
  }
}

void
nsImageMap::ContentAppended(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            PRInt32     aNewIndexInContainer)
{
  MaybeUpdateAreas(aContainer);
}

void
nsImageMap::ContentInserted(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer)
{
  MaybeUpdateAreas(aContainer);
}

void
nsImageMap::ContentRemoved(nsIDocument *aDocument,
                           nsIContent* aContainer,
                           nsIContent* aChild,
                           PRInt32 aIndexInContainer)
{
  MaybeUpdateAreas(aContainer);
}

nsresult
nsImageMap::Focus(nsIDOMEvent* aEvent)
{
  return ChangeFocus(aEvent, PR_TRUE);
}

nsresult
nsImageMap::Blur(nsIDOMEvent* aEvent)
{
  return ChangeFocus(aEvent, PR_FALSE);
}

nsresult
nsImageMap::ChangeFocus(nsIDOMEvent* aEvent, PRBool aFocus) {
  //Set which one of our areas changed focus
  nsCOMPtr<nsIDOMEventTarget> target;
  if (NS_SUCCEEDED(aEvent->GetTarget(getter_AddRefs(target))) && target) {
    nsCOMPtr<nsIContent> targetContent(do_QueryInterface(target));
    if (targetContent) {
      PRInt32 i, n = mAreas.Count();
      for (i = 0; i < n; i++) {
        Area* area = (Area*) mAreas.ElementAt(i);
        nsCOMPtr<nsIContent> areaContent;
        area->GetArea(getter_AddRefs(areaContent));
        if (areaContent) {
          if (areaContent.get() == targetContent.get()) {
            //Set or Remove internal focus
            area->HasFocus(aFocus);
            //Now invalidate the rect
            nsCOMPtr<nsIDocument> doc = targetContent->GetDocument();
            //This check is necessary to see if we're still attached to the doc
            if (doc) {
              nsIPresShell *presShell = doc->GetShellAt(0);
              if (presShell) {
                nsIFrame* imgFrame = presShell->GetPrimaryFrameFor(targetContent);
                if (imgFrame) {
                  nsPresContext *presContext = presShell->GetPresContext();
                  if (presContext) {
                    nsRect dmgRect;
                    area->GetRect(presContext, dmgRect);
                    imgFrame->Invalidate(dmgRect, PR_TRUE);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return NS_OK;
}

nsresult
nsImageMap::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

void
nsImageMap::Destroy(void)
{
  FreeAreas();
  mMap->RemoveMutationObserver(this);
}
