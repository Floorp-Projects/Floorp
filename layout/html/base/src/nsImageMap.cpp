/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsImageMap.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsVoidArray.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIURL.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsISizeOfHandler.h"
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
#include "nsIHTMLContent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIFrameManager.h"
#include "nsIViewManager.h"
#include "nsCoord.h"
#include "nsIImageMap.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIStringBundle.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"

static NS_DEFINE_CID(kCStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

class Area {
public:
  Area(nsIContent* aArea, PRBool aSuppress, PRBool aHasURL);
  virtual ~Area();

  virtual void ParseCoords(const nsString& aSpec);

  virtual PRBool IsInside(nscoord x, nscoord y) = 0;
  virtual void Draw(nsIPresContext* aCX,
                    nsIRenderingContext& aRC) = 0;
  virtual void GetRect(nsIPresContext* aCX, nsRect& aRect) = 0;
  virtual void GetShapeName(nsString& aResult) const = 0;

  void HasFocus(PRBool aHasFocus);

  void GetHREF(nsString& aHref) const;
  void GetTarget(nsString& aTarget) const;
  void GetAltText(nsString& aAltText) const;
  PRBool GetSuppress() const { return mSuppressFeedback; }
  PRBool GetHasURL() const { return mHasURL; }
  void GetArea(nsIContent** aArea) const;

#ifdef DEBUG
  virtual void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

  nsCOMPtr<nsIContent> mArea;
  nscoord* mCoords;
  PRInt32 mNumCoords;
  PRPackedBool mSuppressFeedback;
  PRPackedBool mHasURL;
  PRPackedBool mHasFocus;
};

MOZ_DECL_CTOR_COUNTER(Area)

Area::Area(nsIContent* aArea,
           PRBool aSuppress, PRBool aHasURL)
  : mArea(aArea), mSuppressFeedback(aSuppress), mHasURL(aHasURL)
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
Area::GetHREF(nsString& aHref) const
{
  aHref.Truncate();
  if (mArea) {
    mArea->GetAttr(kNameSpaceID_None, nsHTMLAtoms::href, aHref);
  }
}
 
void 
Area::GetTarget(nsString& aTarget) const
{
  aTarget.Truncate();
  if (mArea) {
    mArea->GetAttr(kNameSpaceID_None, nsHTMLAtoms::target, aTarget);
  }
}
 
void 
Area::GetAltText(nsString& aAltText) const
{
  aAltText.Truncate();
  if (mArea) {
    mArea->GetAttr(kNameSpaceID_None, nsHTMLAtoms::alt, aAltText);
  }
}

void 
Area::GetArea(nsIContent** aArea) const
{
  *aArea = mArea;
  NS_IF_ADDREF(*aArea);
}

#ifdef DEBUG
void
Area::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (aResult) {
    PRUint32 sum = sizeof(*this);
    sum += mNumCoords * sizeof(nscoord);
    *aResult = sum;
  }
}
#endif

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

void Area::ParseCoords(const nsString& aSpec)
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
  DefaultArea(nsIContent* aArea, PRBool aSuppress, PRBool aHasURL);
  ~DefaultArea();

  virtual PRBool IsInside(nscoord x, nscoord y);
  virtual void Draw(nsIPresContext* aCX,
                    nsIRenderingContext& aRC);
  virtual void GetRect(nsIPresContext* aCX, nsRect& aRect);
  virtual void GetShapeName(nsString& aResult) const;
};

DefaultArea::DefaultArea(nsIContent* aArea, PRBool aSuppress, PRBool aHasURL)
  : Area(aArea, aSuppress, aHasURL)
{
}

DefaultArea::~DefaultArea()
{
}

PRBool DefaultArea::IsInside(nscoord x, nscoord y)
{
  return PR_TRUE;
}

void DefaultArea::Draw(nsIPresContext* aCX, nsIRenderingContext& aRC)
{
}

void DefaultArea::GetRect(nsIPresContext* aCX, nsRect& aRect)
{
}

void DefaultArea::GetShapeName(nsString& aResult) const
{
  aResult.Append(NS_LITERAL_STRING("default"));
}

//----------------------------------------------------------------------

class RectArea : public Area {
public:
  RectArea(nsIContent* aArea, PRBool aSuppress, PRBool aHasURL);
  ~RectArea();

  virtual void ParseCoords(const nsString& aSpec);
  virtual PRBool IsInside(nscoord x, nscoord y);
  virtual void Draw(nsIPresContext* aCX,
                    nsIRenderingContext& aRC);
  virtual void GetRect(nsIPresContext* aCX, nsRect& aRect);
  virtual void GetShapeName(nsString& aResult) const;
};

RectArea::RectArea(nsIContent* aArea, PRBool aSuppress, PRBool aHasURL)
  : Area(aArea, aSuppress, aHasURL)
{
}

RectArea::~RectArea()
{
}

void RectArea::ParseCoords(const nsString& aSpec)
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
    // Report the error to the console.
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
      bundle->FormatStringFromName(NS_LITERAL_STRING("ImageMapRectBoundsError").get(),
                                   nsnull, 0,
                                   getter_Copies(errorText));
    if (NS_FAILED(rv))
      return;

    nsCOMPtr<nsINodeInfo> nodeInfo;
    mArea->GetNodeInfo(*getter_AddRefs(nodeInfo));
    NS_ASSERTION(nodeInfo, "Element with no nodeinfo");
    
    nsCOMPtr<nsIDocument> doc;
    nodeInfo->GetDocument(*getter_AddRefs(doc));
    nsCAutoString urlSpec;
    if (doc) {
      nsCOMPtr<nsIURI> uri;
      doc->GetDocumentURL(getter_AddRefs(uri));
      if (uri) {
        uri->GetSpec(urlSpec);
      }
    }
    rv = errorObject->Init(errorText.get(),
                           NS_ConvertUTF8toUCS2(urlSpec).get(), /* file name */
                           aSpec.get(), /* source line */
                           0, /* line number */
                           0, /* column number */
                           flag,
                           "ImageMap");
    if (NS_FAILED(rv))
      return;

    consoleService->LogMessage(errorObject);
  }
}

PRBool RectArea::IsInside(nscoord x, nscoord y)
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

void RectArea::Draw(nsIPresContext* aCX, nsIRenderingContext& aRC)
{
  if (mHasFocus) {
    if (mNumCoords >= 4) {
      float p2t;
      aCX->GetPixelsToTwips(&p2t);
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

void RectArea::GetRect(nsIPresContext* aCX, nsRect& aRect)
{
  if (mNumCoords >= 4) {
    float p2t;
    aCX->GetPixelsToTwips(&p2t);
    nscoord x1 = NSIntPixelsToTwips(mCoords[0], p2t);
    nscoord y1 = NSIntPixelsToTwips(mCoords[1], p2t);
    nscoord x2 = NSIntPixelsToTwips(mCoords[2], p2t);
    nscoord y2 = NSIntPixelsToTwips(mCoords[3], p2t);
    NS_ASSERTION(x1 <= x2 && y1 <= y2,
                 "Someone screwed up RectArea::ParseCoords");

    nsRect tmp(x1, y1, x2, y2);
    aRect = tmp;
  }
}

void RectArea::GetShapeName(nsString& aResult) const
{
  aResult.Append(NS_LITERAL_STRING("rect"));
}

//----------------------------------------------------------------------

class PolyArea : public Area {
public:
  PolyArea(nsIContent* aArea, PRBool aSuppress, PRBool aHasURL);
  ~PolyArea();

  virtual PRBool IsInside(nscoord x, nscoord y);
  virtual void Draw(nsIPresContext* aCX,
                    nsIRenderingContext& aRC);
  virtual void GetRect(nsIPresContext* aCX, nsRect& aRect);
  virtual void GetShapeName(nsString& aResult) const;
};

PolyArea::PolyArea(nsIContent* aArea, PRBool aSuppress, PRBool aHasURL)
  : Area(aArea, aSuppress, aHasURL)
{
}

PolyArea::~PolyArea()
{
}

PRBool PolyArea::IsInside(nscoord x, nscoord y)
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

void PolyArea::Draw(nsIPresContext* aCX, nsIRenderingContext& aRC)
{
  if (mHasFocus) {
    if (mNumCoords >= 6) {
      float p2t;
      aCX->GetPixelsToTwips(&p2t);
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

void PolyArea::GetRect(nsIPresContext* aCX, nsRect& aRect)
{
  if (mNumCoords >= 6) {
    float p2t;
    aCX->GetPixelsToTwips(&p2t);
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

    nsRect tmp(x1, y1, x2, y2);
    aRect = tmp;
  }
}

void PolyArea::GetShapeName(nsString& aResult) const
{
  aResult.Append(NS_LITERAL_STRING("polygon"));
}

//----------------------------------------------------------------------

class CircleArea : public Area {
public:
  CircleArea(nsIContent* aArea, PRBool aSuppress, PRBool aHasURL);
  ~CircleArea();

  virtual PRBool IsInside(nscoord x, nscoord y);
  virtual void Draw(nsIPresContext* aCX,
                    nsIRenderingContext& aRC);
  virtual void GetRect(nsIPresContext* aCX, nsRect& aRect);
  virtual void GetShapeName(nsString& aResult) const;
};

CircleArea::CircleArea(nsIContent* aArea, PRBool aSuppress, PRBool aHasURL)
  : Area(aArea, aSuppress, aHasURL)
{
}

CircleArea::~CircleArea()
{
}

PRBool CircleArea::IsInside(nscoord x, nscoord y)
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

void CircleArea::Draw(nsIPresContext* aCX, nsIRenderingContext& aRC)
{
  if (mHasFocus) {
    if (mNumCoords >= 3) {
      float p2t;
      aCX->GetPixelsToTwips(&p2t);
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

void CircleArea::GetRect(nsIPresContext* aCX, nsRect& aRect)
{
  if (mNumCoords >= 3) {
    float p2t;
    aCX->GetPixelsToTwips(&p2t);
    nscoord x1 = NSIntPixelsToTwips(mCoords[0], p2t);
    nscoord y1 = NSIntPixelsToTwips(mCoords[1], p2t);
    nscoord radius = NSIntPixelsToTwips(mCoords[2], p2t);
    if (radius < 0) {
      return;
    }

    nsRect tmp(x1 - radius, y1 - radius, x1 + radius, y1 + radius);
    aRect = tmp;
  }
}

void CircleArea::GetShapeName(nsString& aResult) const
{
  aResult.Append(NS_LITERAL_STRING("circle"));
}

//----------------------------------------------------------------------


nsImageMap::nsImageMap()
{
  NS_INIT_ISUPPORTS();
  mMap = nsnull;
  mDomMap = nsnull;
  mDocument = nsnull;
  mContainsBlockContents = PR_FALSE;
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
  if (nsnull != mDocument) {
    mDocument->RemoveObserver(NS_STATIC_CAST(nsIDocumentObserver*, this));
  }
  NS_IF_RELEASE(mMap);
}

NS_IMPL_ADDREF(nsImageMap)
NS_IMPL_RELEASE(nsImageMap)

NS_IMETHODIMP
nsImageMap::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

  *result = nsnull;
  if (iid.Equals(NS_GET_IID(nsISupports)) ||
      iid.Equals(NS_GET_IID(nsIDocumentObserver))) {
    *result = NS_STATIC_CAST(nsIDocumentObserver*, this);
  }
  else if (iid.Equals(NS_GET_IID(nsIDOMFocusListener)) ||
           iid.Equals(NS_GET_IID(nsIDOMEventListener))) {
    *result = NS_STATIC_CAST(nsIDOMFocusListener*, this);
  }
  else if (iid.Equals(NS_GET_IID(nsIImageMap))) {
    *result = NS_STATIC_CAST(nsIImageMap*, this);
  }
  else {
    return NS_NOINTERFACE;
  }

  NS_ADDREF_THIS();
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::GetBoundsForAreaContent(nsIContent *aContent, 
                                   nsIPresContext* aPresContext, 
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
  nsCOMPtr<nsIFrameManager> frameManager;
  mPresShell->GetFrameManager(getter_AddRefs(frameManager));

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
  mDomMap = aMap;

  nsresult rv = aMap->QueryInterface(NS_GET_IID(nsIContent), (void**) &mMap);
  if (NS_SUCCEEDED(rv)) {
    rv = mMap->GetDocument(mDocument);
    if (NS_SUCCEEDED(rv) && (nsnull != mDocument)) {
      mDocument->AddObserver(NS_STATIC_CAST(nsIDocumentObserver*, this));
      // mDocument is a weak reference, so release the reference we got
      nsIDocument *temp = mDocument;
      NS_RELEASE(temp);
    }
  }

  // "Compile" the areas in the map into faster access versions
  rv = UpdateAreas();
  return rv;
}


nsresult
nsImageMap::UpdateAreasForBlock(nsIContent* aParent, PRBool* aFoundAnchor)
{
  nsresult rv = NS_OK;
  nsIContent* child;
  PRInt32 i, n;
  aParent->ChildCount(n);
  for (i = 0; (i < n) && NS_SUCCEEDED(rv); i++) {
    rv = aParent->ChildAt(i, child);
    if (NS_SUCCEEDED(rv)) {
      nsIDOMHTMLAnchorElement* area;
      rv = child->QueryInterface(NS_GET_IID(nsIDOMHTMLAnchorElement), (void**) &area);
      if (NS_SUCCEEDED(rv)) {
        *aFoundAnchor = PR_TRUE;
        rv = AddArea(child);
        NS_RELEASE(area);
      }
      else {
        rv = UpdateAreasForBlock(child, aFoundAnchor);
      }
      NS_RELEASE(child);
    }
  }
  
  return rv;
}

nsresult
nsImageMap::UpdateAreas()
{
  // Get rid of old area data
  FreeAreas();

  PRInt32 i, n;
  PRBool containsBlock = PR_FALSE, containsArea = PR_FALSE;

  mMap->ChildCount(n);
  for (i = 0; i < n; i++) {
    nsCOMPtr<nsIContent> child;
    mMap->ChildAt(i, *getter_AddRefs(child));

    // Only look at elements and not text, comments, etc.
    nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface(child);
    if (! element)
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
  nsAutoString shape, coords, baseURL, noHref;
  aArea->GetAttr(kNameSpaceID_None, nsHTMLAtoms::shape, shape);
  aArea->GetAttr(kNameSpaceID_None, nsHTMLAtoms::coords, coords);
  PRBool hasURL = (PRBool)(NS_CONTENT_ATTR_HAS_VALUE != aArea->GetAttr(kNameSpaceID_None, nsHTMLAtoms::nohref, noHref));
  PRBool suppress = PR_FALSE;/* XXX */

  //Add focus listener to track area focus changes
  nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(aArea));
  if (rec) {
    rec->AddEventListenerByIID(this, NS_GET_IID(nsIDOMFocusListener));
  }

  nsCOMPtr<nsIFrameManager> frameManager;
  mPresShell->GetFrameManager(getter_AddRefs(frameManager));
  frameManager->SetPrimaryFrameFor(aArea, mImageFrame);

  Area* area;
  if ((0 == shape.Length()) ||
      shape.EqualsIgnoreCase("rect") ||
      shape.EqualsIgnoreCase("rectangle")) {
    area = new RectArea(aArea, suppress, hasURL);
  }
  else if (shape.EqualsIgnoreCase("poly") ||
           shape.EqualsIgnoreCase("polygon")) {
    area = new PolyArea(aArea, suppress, hasURL);
  }
  else if (shape.EqualsIgnoreCase("circle") ||
           shape.EqualsIgnoreCase("circ")) {
    area = new CircleArea(aArea, suppress, hasURL);
  }
  else if (shape.EqualsIgnoreCase("default")) {
    area = new DefaultArea(aArea, suppress, hasURL);
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
                     nsIContent** aContent,
                     nsString& aAbsURL,
                     nsString& aTarget,
                     nsString& aAltText,
                     PRBool* aSuppress)
{
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    if (area->IsInside(aX, aY)) {
      if (area->GetHasURL()) {
        // Set the image loader's source URL and base URL
        nsCOMPtr<nsIURI> baseUri;

        if (mMap) {
          nsCOMPtr<nsIHTMLContent> htmlContent(do_QueryInterface(mMap));
          if (htmlContent) {
            htmlContent->GetBaseURL(*getter_AddRefs(baseUri));
          }
          else {
            nsCOMPtr<nsIDocument> doc;
            mMap->GetDocument(*getter_AddRefs(doc));

            if (doc) {
              // Could just use mDocument here...
              doc->GetBaseURL(*getter_AddRefs(baseUri));
            }
          }
        }

        if (!baseUri) {
          return PR_FALSE;
        }

        nsAutoString href;
        area->GetHREF(href);
        NS_MakeAbsoluteURI(aAbsURL, href, baseUri);
      }

      area->GetTarget(aTarget);
      if (mMap && (aTarget.Length() == 0)) {
        nsCOMPtr<nsIHTMLContent> content(do_QueryInterface(mMap));

        if (content) {
          content->GetBaseTarget(aTarget);
        }
      }

      area->GetAltText(aAltText);
      *aSuppress = area->mSuppressFeedback;

      area->GetArea(aContent);

      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

PRBool
nsImageMap::IsInside(nscoord aX, nscoord aY)
{
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    if (area->IsInside(aX, aY)) {
      nsAutoString href;
      area->GetHREF(href);
      if (href.Length() > 0) {
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
nsImageMap::Draw(nsIPresContext* aCX, nsIRenderingContext& aRC)
{
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    area->Draw(aCX, aRC);
  }
}

NS_IMPL_NSIDOCUMENTOBSERVER_CORE_STUB(nsImageMap)
NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(nsImageMap)
NS_IMPL_NSIDOCUMENTOBSERVER_REFLOW_STUB(nsImageMap)
NS_IMPL_NSIDOCUMENTOBSERVER_STATE_STUB(nsImageMap)
NS_IMPL_NSIDOCUMENTOBSERVER_STYLE_STUB(nsImageMap)


PRBool
nsImageMap::IsAncestorOf(nsIContent* aContent,
                         nsIContent* aAncestorContent)
{
  nsIContent* parent;
  nsresult rv = aContent->GetParent(parent);
  if (NS_SUCCEEDED(rv) && (nsnull != parent)) {
    PRBool rBool;
    if (parent == aAncestorContent) {
      rBool = PR_TRUE;
    }
    else {
      rBool = IsAncestorOf(parent, aAncestorContent);
    }
    NS_RELEASE(parent);
    return rBool;
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsImageMap::ContentChanged(nsIDocument *aDocument,
                           nsIContent* aContent,
                           nsISupports* aSubContent)
{
  // If the parent of the changing content node is our map then update
  // the map.
  nsIContent* parent;
  nsresult rv = aContent->GetParent(parent);
  if (NS_SUCCEEDED(rv) && (nsnull != parent)) {
    if ((parent == mMap) || 
        (mContainsBlockContents && IsAncestorOf(parent, mMap))) {
      UpdateAreas();
    }
    NS_RELEASE(parent);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::AttributeChanged(nsIDocument *aDocument,
                             nsIContent*  aContent,
                             PRInt32      aNameSpaceID,
                             nsIAtom*     aAttribute,
                             PRInt32      aModType, 
                             nsChangeHint aHint)
{
  // If the parent of the changing content node is our map then update
  // the map.
  nsIContent* parent;
  nsresult rv = aContent->GetParent(parent);
  if (NS_SUCCEEDED(rv) && (nsnull != parent)) {
    if ((parent == mMap) || 
        (mContainsBlockContents && IsAncestorOf(parent, mMap))) {
      UpdateAreas();
    }
    NS_RELEASE(parent);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::ContentAppended(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            PRInt32     aNewIndexInContainer)
{
  if ((mMap == aContainer) || 
      (mContainsBlockContents && IsAncestorOf(aContainer, mMap))) {
    UpdateAreas();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::ContentInserted(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer)
{
  if ((mMap == aContainer) ||
      (mContainsBlockContents && IsAncestorOf(aContainer, mMap))) {
    UpdateAreas();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::ContentReplaced(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aOldChild,
                            nsIContent* aNewChild,
                            PRInt32 aIndexInContainer)
{
  if ((mMap == aContainer) ||
      (mContainsBlockContents && IsAncestorOf(aContainer, mMap))) {
    UpdateAreas();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::ContentRemoved(nsIDocument *aDocument,
                           nsIContent* aContainer,
                           nsIContent* aChild,
                           PRInt32 aIndexInContainer)
{
  if ((mMap == aContainer) ||
      (mContainsBlockContents && IsAncestorOf(aContainer, mMap))) {
    UpdateAreas();
  }
  return NS_OK;
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
            nsCOMPtr<nsIDocument> doc;
            //This check is necessary to see if we're still attached to the doc
            if (NS_SUCCEEDED(targetContent->GetDocument(*getter_AddRefs(doc))) && doc) {
              nsCOMPtr<nsIPresShell> presShell;
              doc->GetShellAt(0, getter_AddRefs(presShell));
              if (presShell) {
                nsIFrame* imgFrame;
                if (NS_SUCCEEDED(presShell->GetPrimaryFrameFor(targetContent, &imgFrame)) && imgFrame) {
                  nsCOMPtr<nsIPresContext> presContext;
                  if (NS_SUCCEEDED(presShell->GetPresContext(getter_AddRefs(presContext))) && presContext) {
                    nsRect dmgRect;
                    area->GetRect(presContext, dmgRect);
                    Invalidate(presContext, imgFrame, dmgRect);
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

nsresult
nsImageMap::Invalidate(nsIPresContext* aPresContext, nsIFrame* aFrame, nsRect& aRect)
{
  nsCOMPtr<nsIViewManager> viewManager;
  PRUint32 flags = NS_VMREFRESH_IMMEDIATE;
  nsIView* view;
  nsRect damageRect(aRect);

  aFrame->GetView(aPresContext, &view);
  if (view) {
    view->GetViewManager(*getter_AddRefs(viewManager));
    viewManager->UpdateView(view, damageRect, flags);   
  }
  else {
    nsPoint   offset;

    aFrame->GetOffsetFromView(aPresContext, offset, &view);
    NS_ASSERTION(nsnull != view, "no view");
    damageRect += offset;
    view->GetViewManager(*getter_AddRefs(viewManager));
    viewManager->UpdateView(view, damageRect, flags);
  }
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

#ifdef DEBUG
void
nsImageMap::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (aResult) {
    PRUint32 sum = sizeof(*this);
    PRInt32 i, n = mAreas.Count();
    for (i = 0; i < n; i++) {
      Area* area = (Area*) mAreas.ElementAt(i);
      PRUint32 areaSize;
      area->SizeOf(aHandler, &areaSize);
      sum += areaSize;
    }
    *aResult = sum;
  }
}
#endif
