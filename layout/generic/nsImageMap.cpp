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
#include "nsImageMap.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsVoidArray.h"
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
#include "nsHTMLAtoms.h"
#include "nsIDOMEventReceiver.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsFrameManager.h"
#include "nsIViewManager.h"
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

MOZ_DECL_CTOR_COUNTER(Area)

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
    mArea->GetAttr(kNameSpaceID_None, nsHTMLAtoms::href, aHref);
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
                       const PRUnichar* aMessageName) {
  nsresult rv;
  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return;
  nsCOMPtr<nsIScriptError> errorObject =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return;
  nsCOMPtr<nsIStringBundleService> stringBundleService =
    do_GetService(kCStringBundleServiceCID, &rv);
  if (NS_FAILED(rv))
    return;
  nsCOMPtr<nsIStringBundle> bundle;
  rv = stringBundleService->CreateBundle(
         "chrome://global/locale/layout_errors.properties",
         getter_AddRefs(bundle));
  if (NS_FAILED(rv))
    return;
  nsXPIDLString errorText;
  rv =
    bundle->FormatStringFromName(aMessageName,
                                 nsnull, 0,
                                 getter_Copies(errorText));
  if (NS_FAILED(rv))
    return;

  // XXX GetOwnerDocument
  nsINodeInfo *nodeInfo = aContent->GetNodeInfo();
  NS_ASSERTION(nodeInfo, "Element with no nodeinfo");

  nsIDocument* doc = nsContentUtils::GetDocument(nodeInfo);
  nsCAutoString urlSpec;
  if (doc) {
    nsIURI *uri = doc->GetDocumentURI();
    if (uri) {
      uri->GetSpec(urlSpec);
    }
  }
  rv = errorObject->Init(errorText.get(),
                         NS_ConvertUTF8toUCS2(urlSpec).get(), /* file name */
                         PromiseFlatString(NS_LITERAL_STRING("coords=\"") +
                                           aCoordsSpec +
                                           NS_LITERAL_STRING("\"")).get(), /* source line */
                         0, /* line number */
                         0, /* column number */
                         aFlags,
                         "ImageMap");
  if (NS_FAILED(rv))
    return;

  consoleService->LogMessage(errorObject);
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
    nsCRT::free(cp);
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
    logMessage(mArea, aSpec, flag, NS_LITERAL_STRING("ImageMapRectBoundsError").get());
  }
}

PRBool RectArea::IsInside(nscoord x, nscoord y) const
{
  if (mNumCoords >= 4) {       // Note: > is for nav compatabilty
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
      float p2t;
      p2t = aCX->PixelsToTwips();
      nscoord x1 = NSIntPixelsToTwips(mCoords[0], p2t);
      nscoord y1 = NSIntPixelsToTwips(mCoords[1], p2t);
      nscoord x2 = NSIntPixelsToTwips(mCoords[2], p2t);
      nscoord y2 = NSIntPixelsToTwips(mCoords[3], p2t);
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
    float p2t;
    p2t = aCX->PixelsToTwips();
    nscoord x1 = NSIntPixelsToTwips(mCoords[0], p2t);
    nscoord y1 = NSIntPixelsToTwips(mCoords[1], p2t);
    nscoord x2 = NSIntPixelsToTwips(mCoords[2], p2t);
    nscoord y2 = NSIntPixelsToTwips(mCoords[3], p2t);
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
                 NS_LITERAL_STRING("ImageMapPolyOddNumberOfCoords").get());
    }
  } else {
    logMessage(mArea,
               aSpec,
               nsIScriptError::errorFlag,
               NS_LITERAL_STRING("ImageMapPolyWrongNumberOfCoords").get());
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
      float p2t;
      p2t = aCX->PixelsToTwips();
      nscoord x0 = NSIntPixelsToTwips(mCoords[0], p2t);
      nscoord y0 = NSIntPixelsToTwips(mCoords[1], p2t);
      nscoord x1, y1;
      for (PRInt32 i = 2; i < mNumCoords; i += 2) {
        x1 = NSIntPixelsToTwips(mCoords[i], p2t);
        y1 = NSIntPixelsToTwips(mCoords[i+1], p2t);
        aRC.DrawLine(x0, y0, x1, y1);
        x0 = x1;
        y0 = y1;
      }
      x1 = NSIntPixelsToTwips(mCoords[0], p2t);
      y1 = NSIntPixelsToTwips(mCoords[1], p2t);
      aRC.DrawLine(x0, y0, x1, y1);
    }
  }
}

void PolyArea::GetRect(nsPresContext* aCX, nsRect& aRect)
{
  if (mNumCoords >= 6) {
    float p2t;
    p2t = aCX->PixelsToTwips();
    nscoord x1, x2, y1, y2, xtmp, ytmp;
    x1 = x2 = NSIntPixelsToTwips(mCoords[0], p2t);
    y1 = y2 = NSIntPixelsToTwips(mCoords[1], p2t);
    for (PRInt32 i = 2; i < mNumCoords; i += 2) {
      xtmp = NSIntPixelsToTwips(mCoords[i], p2t);
      ytmp = NSIntPixelsToTwips(mCoords[i+1], p2t);
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
                 NS_LITERAL_STRING("ImageMapCircleNegativeRadius").get());
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
               NS_LITERAL_STRING("ImageMapCircleWrongNumberOfCoords").get());
  }
}

PRBool CircleArea::IsInside(nscoord x, nscoord y) const
{
  // Note: > is for nav compatabilty
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
      float p2t;
      p2t = aCX->PixelsToTwips();
      nscoord x1 = NSIntPixelsToTwips(mCoords[0], p2t);
      nscoord y1 = NSIntPixelsToTwips(mCoords[1], p2t);
      nscoord radius = NSIntPixelsToTwips(mCoords[2], p2t);
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
    float p2t;
    p2t = aCX->PixelsToTwips();
    nscoord x1 = NSIntPixelsToTwips(mCoords[0], p2t);
    nscoord y1 = NSIntPixelsToTwips(mCoords[1], p2t);
    nscoord radius = NSIntPixelsToTwips(mCoords[2], p2t);
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
  mDocument(nsnull),
  mContainsBlockContents(PR_FALSE)
{
}

nsImageMap::~nsImageMap()
{
  //Remove all our focus listeners
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    nsCOMPtr<nsIContent> areaContent;
    area->GetArea(getter_AddRefs(areaContent));
    if (areaContent) {
      nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(areaContent));
      if (rec) {
        rec->RemoveEventListenerByIID(this, NS_GET_IID(nsIDOMFocusListener));
      }
    }
  }

  FreeAreas();
  if (mDocument) {
    mDocument->RemoveObserver(this);
  }
}

NS_IMPL_ISUPPORTS4(nsImageMap,
                   nsIDocumentObserver,
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
    frameManager->SetPrimaryFrameFor(area->mArea, nsnull);
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

  nsresult rv;
  mMap = do_QueryInterface(aMap, &rv);
  NS_ASSERTION(mMap, "aMap is not an nsIHTMLContent!");
  mDocument = mMap->GetDocument();
  if (mDocument) {
    mDocument->AddObserver(this);
  }

  // "Compile" the areas in the map into faster access versions
  rv = UpdateAreas();
  return rv;
}


nsresult
nsImageMap::UpdateAreasForBlock(nsIContent* aParent, PRBool* aFoundAnchor)
{
  nsresult rv = NS_OK;
  PRUint32 i, n = aParent->GetChildCount();

  for (i = 0; (i < n) && NS_SUCCEEDED(rv); i++) {
    nsIContent *child = aParent->GetChildAt(i);

    nsCOMPtr<nsIDOMHTMLAnchorElement> area = do_QueryInterface(child);
    if (area) {
      *aFoundAnchor = PR_TRUE;
      rv = AddArea(child);
    }
    else {
      rv = UpdateAreasForBlock(child, aFoundAnchor);
    }
  }
  
  return rv;
}

nsresult
nsImageMap::UpdateAreas()
{
  // Get rid of old area data
  FreeAreas();

  PRUint32 i, n = mMap->GetChildCount();
  PRBool containsBlock = PR_FALSE, containsArea = PR_FALSE;

  for (i = 0; i < n; i++) {
    nsIContent *child = mMap->GetChildAt(i);

    // Only look at elements and not text, comments, etc.
    if (!child->IsContentOfType(nsIContent::eHTML))
      continue;

    // First check if this map element contains an AREA element.
    // If so, we only look for AREA elements
    if (!containsBlock) {
      nsCOMPtr<nsIDOMHTMLAreaElement> area = do_QueryInterface(child);
      if (area) {
        containsArea = PR_TRUE;
        AddArea(child);
      }
    }
      
    // If we haven't determined that the map element contains an
    // AREA element yet, the look for a block element with children
    // that are anchors.
    if (!containsArea) {
      UpdateAreasForBlock(child, &containsBlock);

      if (containsBlock)
        mContainsBlockContents = PR_TRUE;
    }
  }

  return NS_OK;
}

nsresult
nsImageMap::AddArea(nsIContent* aArea)
{
  nsAutoString shape, coords;
  aArea->GetAttr(kNameSpaceID_None, nsHTMLAtoms::shape, shape);
  aArea->GetAttr(kNameSpaceID_None, nsHTMLAtoms::coords, coords);

  //Add focus listener to track area focus changes
  nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(aArea));
  if (rec) {
    rec->AddEventListenerByIID(this, NS_GET_IID(nsIDOMFocusListener));
  }

  mPresShell->FrameManager()->SetPrimaryFrameFor(aArea, mImageFrame);

  Area* area;
  if (shape.IsEmpty() ||
      shape.LowerCaseEqualsLiteral("rect") ||
      shape.LowerCaseEqualsLiteral("rectangle")) {
    area = new RectArea(aArea);
  }
  else if (shape.LowerCaseEqualsLiteral("poly") ||
           shape.LowerCaseEqualsLiteral("polygon")) {
    area = new PolyArea(aArea);
  }
  else if (shape.LowerCaseEqualsLiteral("circle") ||
           shape.LowerCaseEqualsLiteral("circ")) {
    area = new CircleArea(aArea);
  }
  else if (shape.LowerCaseEqualsLiteral("default")) {
    area = new DefaultArea(aArea);
  }
  else {
    // Unknown area type; bail
    return NS_OK;
  }
  if (!area)
    return NS_ERROR_OUT_OF_MEMORY;
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

PRBool
nsImageMap::IsInside(nscoord aX, nscoord aY) const
{
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    if (area->IsInside(aX, aY)) {
      nsAutoString href;
      area->GetHREF(href);
      if (!href.IsEmpty()) {
        return PR_TRUE;
      }
      else {
        //We need to return here so we don't hit an overlapping map area
        return PR_FALSE;
      }
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

PRBool
nsImageMap::IsAncestorOf(nsIContent* aContent,
                         nsIContent* aAncestorContent)
{
  for (nsIContent *a = aContent->GetParent(); a; a = a->GetParent())
    if (a == aAncestorContent)
      return PR_TRUE;

  return PR_FALSE;
}

void
nsImageMap::MaybeUpdateAreas(nsIContent *aContent)
{
  if (aContent == mMap || 
      (mContainsBlockContents && IsAncestorOf(aContent, mMap))) {
    UpdateAreas();
  }
}

void
nsImageMap::AttributeChanged(nsIDocument *aDocument,
                             nsIContent*  aContent,
                             PRInt32      aNameSpaceID,
                             nsIAtom*     aAttribute,
                             PRInt32      aModType)
{
  // If the parent of the changing content node is our map then update
  // the map.
  MaybeUpdateAreas(aContent->GetParent());
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
                nsIFrame* imgFrame;
                if (NS_SUCCEEDED(presShell->GetPrimaryFrameFor(targetContent, &imgFrame)) && imgFrame) {
                  nsCOMPtr<nsPresContext> presContext;
                  if (NS_SUCCEEDED(presShell->GetPresContext(getter_AddRefs(presContext))) && presContext) {
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
  //Remove all our focus listeners
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    nsCOMPtr<nsIContent> areaContent;
    area->GetArea(getter_AddRefs(areaContent));
    if (areaContent) {
      nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(areaContent));
      if (rec) {
        rec->RemoveEventListenerByIID(this, NS_GET_IID(nsIDOMFocusListener));
      }
    }
  }
}
