/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsImageMap.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIURL.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsIXIFConverter.h"
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
#include "nsHTMLIIDs.h"
#include "nsIDOMEventReceiver.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIFrameManager.h"
#include "nsIViewManager.h"
#include "nsCoord.h"

class Area {
public:
  Area(nsIContent* aArea, PRBool aSuppress, PRBool aHasURL);
  virtual ~Area();

  void ParseCoords(const nsString& aSpec);

  virtual PRBool IsInside(nscoord x, nscoord y) = 0;
  virtual void Draw(nsIPresContext* aCX,
                    nsIRenderingContext& aRC) = 0;
  virtual void GetRect(nsIPresContext* aCX, nsRect& aRect) = 0;
  virtual void GetShapeName(nsString& aResult) const = 0;

  void ToHTML(nsString& aResult);
  void HasFocus(PRBool aHasFocus);


  /**
   * Translate the content object into the (XIF) XML Interchange Format
   * XIF is an intermediate form of the content model, the buffer
   * will then be parsed into any number of formats including HTML, TXT, etc.
   */
  virtual void BeginConvertToXIF(nsIXIFConverter* aConverter) const;
  virtual void ConvertContentToXIF(nsIXIFConverter* aConverter) const;
  virtual void FinishConvertToXIF(nsIXIFConverter* aConverter) const;


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
  PRBool mSuppressFeedback;
  PRBool mHasURL;
  PRBool mHasFocus;
};

MOZ_DECL_CTOR_COUNTER(Area);

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
    mArea->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::href, aHref);
  }
}
 
void 
Area::GetTarget(nsString& aTarget) const
{
  aTarget.Truncate();
  if (mArea) {
    mArea->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::target, aTarget);
  }
}
 
void 
Area::GetAltText(nsString& aAltText) const
{
  aAltText.Truncate();
  if (mArea) {
    mArea->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::alt, aAltText);
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
#define XP_ATOI(_s) ::atoi(_s)

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
  if ((str == NULL)||(*str == '\0'))
  {
    return((PRInt32 *)NULL);
  }

  /*
   * Skip beginning whitespace, all whitespace is empty list.
   */
  n_str = str;
  while (XP_IS_SPACE(*n_str))
  {
    n_str++;
  }
  if (*n_str == '\0')
  {
    return((PRInt32 *)NULL);
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
    while ((!XP_IS_SPACE(*tptr))&&(*tptr != ',')&&(*tptr != '\0'))
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
    while ((XP_IS_SPACE(*tptr))||(*tptr == ','))
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
 
  *value_cnt = cnt;

  /*
   * Allocate space for the coordinate array.
   */
  value_list = new nscoord[cnt];
  if (value_list == NULL)
  {
    return((PRInt32 *)NULL);
  }

  /*
   * Second pass to copy integer values into list.
   */
  tptr = str;
  for (i=0; i<cnt; i++)
  {
    char *ptr;

    ptr = strchr(tptr, ',');
    if (ptr != NULL)
    {
      *ptr = '\0';
    }
    /*
     * Strip whitespace in front of number because I don't
     * trust atoi to do it on all platforms.
     */
    while (XP_IS_SPACE(*tptr))
    {
      tptr++;
    }
    if (*tptr == '\0')
    {
      value_list[i] = 0;
    }
    else
    {
      value_list[i] = (nscoord)XP_ATOI(tptr);
    }
    if (ptr != NULL)
    {
      *ptr = ',';
      tptr = (char *)(ptr + 1);
    }
  }
  return(value_list);
}

void Area::ParseCoords(const nsString& aSpec)
{
  char* cp = aSpec.ToNewCString();
  if (cp) {
    mCoords = lo_parse_coord_list(cp, &mNumCoords);
    nsCRT::free(cp);
  }
}

void Area::ToHTML(nsString& aResult)
{
  nsAutoString href, target, altText;

  if (mArea) {
    mArea->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::href, href);
    mArea->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::target, target);
    mArea->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::alt, altText);
  }

  aResult.Truncate();
  aResult.AppendWithConversion("<AREA SHAPE=");
  nsAutoString shape;
  GetShapeName(shape);
  aResult.Append(shape);
  aResult.AppendWithConversion(" COORDS=\"");
  if (nsnull != mCoords) {
    PRInt32 i, n = mNumCoords;
    for (i = 0; i < n; i++) {
      aResult.AppendInt(mCoords[i], 10);
      if (i < n - 1) {
        aResult.AppendWithConversion(',');
      }
    }
  }
  aResult.AppendWithConversion("\" HREF=\"");
  aResult.Append(href);
  aResult.AppendWithConversion("\"");
  if (0 < target.Length()) {
    aResult.AppendWithConversion(" TARGET=\"");
    aResult.Append(target);
    aResult.AppendWithConversion("\"");
  }
  if (0 < altText.Length()) {
    aResult.AppendWithConversion(" ALT=\"");
    aResult.Append(altText);
    aResult.AppendWithConversion("\"");
  }
  if (mSuppressFeedback) {
    aResult.AppendWithConversion(" SUPPRESS");
  }
  aResult.AppendWithConversion('>');
}

/**
 * Translate the content object into the (XIF) XML Interchange Format
 * XIF is an intermediate form of the content model, the buffer
 * will then be parsed into any number of formats including HTML, TXT, etc.
 */

void Area::HasFocus(PRBool aHasFocus)
{
  mHasFocus = aHasFocus;
}

void Area::BeginConvertToXIF(nsIXIFConverter* aConverter) const
{
  nsAutoString href, target, altText;

  if (mArea) {
    mArea->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::href, href);
    mArea->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::target, target);
    mArea->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::alt, altText);
  }

  nsAutoString  tag; tag.AssignWithConversion("area");
  aConverter->BeginStartTag(tag);


  nsAutoString name; name.AssignWithConversion("shape");
  nsAutoString shape;
  GetShapeName(shape);
  aConverter->AddAttribute(name,shape);
   

  nsAutoString  coords;
  if (nsnull != mCoords) {
    PRInt32 i, n = mNumCoords;
    for (i = 0; i < n; i++) {
      coords.AppendInt(mCoords[i], 10);
      if (i < n - 1) {
        coords.AppendWithConversion(',');
      }
    }
  }
  name.AssignWithConversion("coords");
  aConverter->AddAttribute(name,coords);

  name.AssignWithConversion("href");
  aConverter->AddAttribute(name,href);

  
  if (0 < target.Length()) {
    name.AssignWithConversion("target");
    aConverter->AddAttribute(name,target);
  }
  if (0 < altText.Length()) {
    name.AssignWithConversion("alt");
    aConverter->AddAttribute(name,altText);
  }
  if (mSuppressFeedback) {
    name.AssignWithConversion("suppress");
    aConverter->AddAttribute(name);
  }
}

void Area::FinishConvertToXIF(nsIXIFConverter* aConverter) const
{
  nsAutoString  tag; tag.AssignWithConversion("area");
  aConverter->FinishStartTag(tag,PR_TRUE, PR_TRUE);
}

void Area::ConvertContentToXIF(nsIXIFConverter* ) const
{
  // Nothing needs to be done here, all of the logic
  // is handled in the start and finish methods

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
  aResult.AppendWithConversion("default");
}

//----------------------------------------------------------------------

class RectArea : public Area {
public:
  RectArea(nsIContent* aArea, PRBool aSuppress, PRBool aHasURL);
  ~RectArea();

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

PRBool RectArea::IsInside(nscoord x, nscoord y)
{
  if (mNumCoords >= 4) {       // Note: > is for nav compatabilty
    nscoord x1 = mCoords[0];
    nscoord y1 = mCoords[1];
    nscoord x2 = mCoords[2];
    nscoord y2 = mCoords[3];
    if ((x1 > x2)|| (y1 > y2)) {
      // Can't be inside a screwed up rect
      return PR_FALSE;
    }
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
      if ((x1 > x2)|| (y1 > y2)) {
        return;
      }
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
    if ((x1 > x2)|| (y1 > y2)) {
      return;
    }

    nsRect tmp(x1, y1, x2, y2);
    aRect = tmp;
  }
}

void RectArea::GetShapeName(nsString& aResult) const
{
  aResult.AppendWithConversion("rect");
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
  aResult.AppendWithConversion("polygon");
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
  aResult.AppendWithConversion("circle");
}

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMHTMLElementIID, NS_IDOMHTMLELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLAreaElementIID, NS_IDOMHTMLAREAELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLAnchorElementIID, NS_IDOMHTMLANCHORELEMENT_IID);
static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);

nsImageMap::nsImageMap()
{
  NS_INIT_REFCNT();
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
  else {
    return NS_NOINTERFACE;
  }

  NS_ADDREF_THIS();
  return NS_OK;
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

  nsresult rv = aMap->QueryInterface(kIContentIID, (void**) &mMap);
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
nsImageMap::UpdateAreasForBlock(nsIContent* aParent)
{
  nsresult rv = NS_OK;
  nsIContent* child;
  PRInt32 i, n;
  aParent->ChildCount(n);
  for (i = 0; (i < n) && NS_SUCCEEDED(rv); i++) {
    rv = aParent->ChildAt(i, child);
    if (NS_SUCCEEDED(rv)) {
      nsIDOMHTMLAnchorElement* area;
      rv = child->QueryInterface(kIDOMHTMLAnchorElementIID, (void**) &area);
      if (NS_SUCCEEDED(rv)) {
        rv = AddArea(child);
        NS_RELEASE(area);
      }
      else {
        rv = UpdateAreasForBlock(child);
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

  nsresult rv = NS_OK;
  nsIContent* child;
  PRInt32 i, n;
  PRBool containsBlock = PR_FALSE, containsArea = PR_FALSE;

  mMap->ChildCount(n);
  for (i = 0; (i < n) && NS_SUCCEEDED(rv); i++) {
    rv = mMap->ChildAt(i, child);
    if (NS_SUCCEEDED(rv)) {
      // Only look at elements and not text, comments, etc.
      nsIDOMHTMLElement* element;
      rv = child->QueryInterface(kIDOMHTMLElementIID, (void**)&element);
      if (NS_FAILED(rv)) {
        rv = NS_OK;
        continue;
      }
      NS_RELEASE(element);

      // First check if this map element contains an AREA element.
      // If so, we only look for AREA elements
      if (!containsBlock) {
        nsIDOMHTMLAreaElement* area;
        rv = child->QueryInterface(kIDOMHTMLAreaElementIID, (void**) &area);
        if (NS_SUCCEEDED(rv)) {
          containsArea = PR_TRUE;
          rv = AddArea(child);
          NS_RELEASE(area);
        }
        else {
          containsBlock = PR_TRUE;
          mContainsBlockContents = PR_TRUE;
          rv = NS_OK;
        }
      }
      
      // If the map element doesn't contain an AREA element as its
      // first element, we make the assumption that it contains
      // block elements and we look for children that are anchors.
      if (!containsArea) {
        rv = UpdateAreasForBlock(child);
      }
      
      NS_RELEASE(child);
    }
  }

  return rv;
}

nsresult
nsImageMap::AddArea(nsIContent* aArea)
{
  nsAutoString shape, coords, baseURL, noHref;
  aArea->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::shape, shape);
  aArea->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::coords, coords);
  PRBool hasURL = (PRBool)(NS_CONTENT_ATTR_HAS_VALUE != aArea->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::nohref, noHref));
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
  else {
    area = new DefaultArea(aArea, suppress, hasURL);
  }
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
        nsresult rv;
        // Set the image loader's source URL and base URL
        nsIURI* baseUri = nsnull;
        nsIHTMLContent* htmlContent;
        if (mMap) {
          rv = mMap->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);
          if (NS_SUCCEEDED(rv)) {
            htmlContent->GetBaseURL(baseUri);
            NS_RELEASE(htmlContent);
          }
          else {
            nsIDocument* doc;
            rv = mMap->GetDocument(doc);
            if (NS_SUCCEEDED(rv)) {
              doc->GetBaseURL(baseUri); // Could just use mDocument here...
              NS_RELEASE(doc);
            }
          }
        }
        if (NS_FAILED(rv) || !baseUri) return PR_FALSE;
        
        nsAutoString href;
        area->GetHREF(href);
        NS_MakeAbsoluteURI(aAbsURL, href, baseUri);

        NS_RELEASE(baseUri);
      }

      area->GetTarget(aTarget);
      if (mMap && (aTarget.Length() == 0)) {
        nsIHTMLContent* content = nsnull;
        nsresult result = mMap->QueryInterface(kIHTMLContentIID, (void**)&content);
        if ((NS_OK == result) && content) {
          content->GetBaseTarget(aTarget);
          NS_RELEASE(content);
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

NS_IMETHODIMP
nsImageMap::BeginUpdate(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::EndUpdate(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::BeginLoad(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::EndLoad(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::EndReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{
  return NS_OK;
}

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
nsImageMap::ContentStatesChanged(nsIDocument* aDocument,
                                 nsIContent* aContent1,
                                 nsIContent* aContent2)
{
  return NS_OK;
}


NS_IMETHODIMP
nsImageMap::AttributeChanged(nsIDocument *aDocument,
                             nsIContent*  aContent,
                             PRInt32      aNameSpaceID,
                             nsIAtom*     aAttribute,
                             PRInt32      aHint)
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

NS_IMETHODIMP
nsImageMap::StyleSheetAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::StyleSheetRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                           nsIStyleSheet* aStyleSheet,
                                           PRBool aDisabled)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::StyleRuleChanged(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet,
                             nsIStyleRule* aStyleRule,
                             PRInt32 aHint)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::StyleRuleAdded(nsIDocument *aDocument,
                           nsIStyleSheet* aStyleSheet,
                           nsIStyleRule* aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::StyleRuleRemoved(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet,
                             nsIStyleRule* aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageMap::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
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
              nsCOMPtr<nsIPresShell> presShell = getter_AddRefs(doc->GetShellAt(0));
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
