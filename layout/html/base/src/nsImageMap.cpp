/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsIImageMap.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsCoord.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIURL.h"

#undef SCALE
#define SCALE(a,b) nscoord((a) * (b))

class Area {
public:
  Area(const nsString& aBaseURL, const nsString& aHREF,
       const nsString& aTarget, const nsString& aAltText,
       PRBool aSuppress);
  virtual ~Area();

  void ParseCoords(const nsString& aSpec);

  virtual PRBool IsInside(nscoord x, nscoord y) = 0;
  virtual void Draw(nsIPresContext& aCX,
                    nsIRenderingContext& aRC) = 0;
  virtual void GetShapeName(nsString& aResult) = 0;

  void ToHTML(nsString& aResult);

  const nsString& GetBase() const { return mBase; }
  const nsString& GetHREF() const { return mHREF; }
  const nsString& GetTarget() const { return mTarget; }
  const nsString& GetAltText() const { return mAltText; }
  PRBool GetSuppress() const { return mSuppressFeedback; }

  nsString mBase;
  nsString mHREF;
  nsString mTarget;
  nsString mAltText;
  nscoord* mCoords;
  PRInt32 mNumCoords;
  PRBool mSuppressFeedback;
};

Area::Area(const nsString& aBaseURL, const nsString& aHREF,
           const nsString& aTarget, const nsString& aAltText,
           PRBool aSuppress)
  : mBase(aBaseURL), mHREF(aHREF), mTarget(aTarget), mAltText(aAltText),
    mSuppressFeedback(aSuppress)
{
  mCoords = nsnull;
  mNumCoords = 0;
}

Area::~Area()
{
  delete [] mCoords;
}

// XXX move into nsCRT
#define XP_IS_SPACE(_ch) \
 (((_ch) == ' ') || ((_ch) == '\t') || ((_ch) == '\r') || ((_ch) == '\n'))

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
  mCoords = lo_parse_coord_list(cp, &mNumCoords);
  delete cp;
}

void Area::ToHTML(nsString& aResult)
{
  aResult.Truncate();
  aResult.Append("<AREA SHAPE=");
  nsAutoString shape;
  GetShapeName(shape);
  aResult.Append(shape);
  aResult.Append(" COORDS=\"");
  if (nsnull != mCoords) {
    PRInt32 i, n = mNumCoords;
    for (i = 0; i < n; i++) {
      aResult.Append(mCoords[i], 10);
      if (i < n - 1) {
        aResult.Append(',');
      }
    }
  }
  aResult.Append("\" HREF=\"");
  aResult.Append(mHREF);
  aResult.Append("\"");
  if (0 < mTarget.Length()) {
    aResult.Append(" TARGET=\"");
    aResult.Append(mTarget);
    aResult.Append("\"");
  }
  if (0 < mAltText.Length()) {
    aResult.Append(" ALT=\"");
    aResult.Append(mAltText);
    aResult.Append("\"");
  }
  if (mSuppressFeedback) {
    aResult.Append(" SUPPRESS");
  }
  aResult.Append('>');
}

//----------------------------------------------------------------------

class DefaultArea : public Area {
public:
  DefaultArea(const nsString& aBaseURL, const nsString& aHREF,
              const nsString& aTarget, const nsString& aAltText,
              PRBool aSuppress);
  ~DefaultArea();

  virtual PRBool IsInside(nscoord x, nscoord y);
  virtual void Draw(nsIPresContext& aCX,
                    nsIRenderingContext& aRC);
  virtual void GetShapeName(nsString& aResult);
};

DefaultArea::DefaultArea(const nsString& aBaseURL, const nsString& aHREF,
                         const nsString& aTarget, const nsString& aAltText,
                         PRBool aSuppress)
  : Area(aBaseURL, aHREF, aTarget, aAltText, aSuppress)
{
}

DefaultArea::~DefaultArea()
{
}

PRBool DefaultArea::IsInside(nscoord x, nscoord y)
{
  return PR_TRUE;
}

void DefaultArea::Draw(nsIPresContext& aCX, nsIRenderingContext& aRC)
{
}

void DefaultArea::GetShapeName(nsString& aResult)
{
  aResult.Append("default");
}

//----------------------------------------------------------------------

class RectArea : public Area {
public:
  RectArea(const nsString& aBaseURL, const nsString& aHREF,
           const nsString& aTarget, const nsString& aAltText,
           PRBool aSuppress);
  ~RectArea();

  virtual PRBool IsInside(nscoord x, nscoord y);
  virtual void Draw(nsIPresContext& aCX,
                    nsIRenderingContext& aRC);
  virtual void GetShapeName(nsString& aResult);
};

RectArea::RectArea(const nsString& aBaseURL, const nsString& aHREF,
                   const nsString& aTarget, const nsString& aAltText,
                   PRBool aSuppress)
  : Area(aBaseURL, aHREF, aTarget, aAltText, aSuppress)
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

void RectArea::Draw(nsIPresContext& aCX, nsIRenderingContext& aRC)
{
  if (mNumCoords >= 4) {
    float p2t = aCX.GetPixelsToTwips();
    nscoord x1 = SCALE(p2t, mCoords[0]);
    nscoord y1 = SCALE(p2t, mCoords[1]);
    nscoord x2 = SCALE(p2t, mCoords[2]);
    nscoord y2 = SCALE(p2t, mCoords[3]);
    if ((x1 > x2)|| (y1 > y2)) {
      return;
    }
    aRC.DrawRect(x1, y1, x2 - x1, y2 - y1);
  }
}

void RectArea::GetShapeName(nsString& aResult)
{
  aResult.Append("rect");
}

//----------------------------------------------------------------------

class PolyArea : public Area {
public:
  PolyArea(const nsString& aBaseURL, const nsString& aHREF,
           const nsString& aTarget, const nsString& aAltText,
           PRBool aSuppress);
  ~PolyArea();

  virtual PRBool IsInside(nscoord x, nscoord y);
  virtual void Draw(nsIPresContext& aCX,
                    nsIRenderingContext& aRC);
  virtual void GetShapeName(nsString& aResult);
};

PolyArea::PolyArea(const nsString& aBaseURL, const nsString& aHREF,
                   const nsString& aTarget, const nsString& aAltText,
                   PRBool aSuppress)
  : Area(aBaseURL, aHREF, aTarget, aAltText, aSuppress)
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

void PolyArea::Draw(nsIPresContext& aCX, nsIRenderingContext& aRC)
{
  if (mNumCoords >= 6) {
    float p2t = aCX.GetPixelsToTwips();
    nscoord x0 = SCALE(p2t, mCoords[0]);
    nscoord y0 = SCALE(p2t, mCoords[1]);
    for (PRInt32 i = 2; i < mNumCoords; i += 2) {
      nscoord x1 = SCALE(p2t, mCoords[i]);
      nscoord y1 = SCALE(p2t, mCoords[i+1]);
      aRC.DrawLine(x0, y0, x1, y1);
      x0 = x1;
      y0 = y1;
    }
    aRC.DrawLine(x0, y0, mCoords[0], mCoords[1]);
  }
}

void PolyArea::GetShapeName(nsString& aResult)
{
  aResult.Append("polygon");
}

//----------------------------------------------------------------------

class CircleArea : public Area {
public:
  CircleArea(const nsString& aBaseURL, const nsString& aHREF,
             const nsString& aTarget, const nsString& aAltText,
             PRBool aSuppress);
  ~CircleArea();

  virtual PRBool IsInside(nscoord x, nscoord y);
  virtual void Draw(nsIPresContext& aCX,
                    nsIRenderingContext& aRC);
  virtual void GetShapeName(nsString& aResult);
};

CircleArea::CircleArea(const nsString& aBaseURL, const nsString& aHREF,
                       const nsString& aTarget, const nsString& aAltText,
                       PRBool aSuppress)
  : Area(aBaseURL, aHREF, aTarget, aAltText, aSuppress)
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

void CircleArea::Draw(nsIPresContext& aCX, nsIRenderingContext& aRC)
{
  if (mNumCoords >= 3) {
    float p2t = aCX.GetPixelsToTwips();
    nscoord x1 = SCALE(p2t, mCoords[0]);
    nscoord y1 = SCALE(p2t, mCoords[1]);
    nscoord radius = SCALE(p2t, mCoords[2]);
    if (radius < 0) {
      return;
    }
    nscoord x = x1 - radius / 2;
    nscoord y = y1 - radius / 2;
    nscoord w = 2 * radius;
    aRC.DrawEllipse(x, y, w, w);
  }
}

void CircleArea::GetShapeName(nsString& aResult)
{
  aResult.Append("circle");
}

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIImageMapIID, NS_IIMAGEMAP_IID);

class ImageMapImpl : public nsIImageMap {
public:
  ImageMapImpl(nsIAtom* aTag);
  ~ImageMapImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD SetName(const nsString& aMapName);

  NS_IMETHOD GetName(nsString& aResult);

  NS_IMETHOD AddArea(const nsString& aBaseURL,
                     const nsString& aShape,
                     const nsString& aCoords,
                     const nsString& aHREF,
                     const nsString& aTarget,
                     const nsString& aAltText,
                     PRBool aSuppress);

  NS_IMETHOD IsInside(nscoord aX, nscoord aY,
                      nsIURL* aDocURL,
                      nsString& aAbsURL,
                      nsString& aTarget,
                      nsString& aAltText,
                      PRBool* aSuppress);

  NS_IMETHOD IsInside(nscoord aX, nscoord aY);

  NS_IMETHOD Draw(nsIPresContext& aCX, nsIRenderingContext& aRC);

  nsString mName;
  nsIAtom* mTag;
  nsVoidArray mAreas;
};

ImageMapImpl::ImageMapImpl(nsIAtom* aTag)
{
  mTag = aTag;
  NS_IF_ADDREF(aTag);
}

ImageMapImpl::~ImageMapImpl()
{
  NS_IF_RELEASE(mTag);
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    delete area;
  }
}

NS_IMPL_ISUPPORTS(ImageMapImpl, kIImageMapIID);

NS_IMETHODIMP ImageMapImpl::SetName(const nsString& aMapName)
{
  mName = aMapName;
  return NS_OK;
}

NS_IMETHODIMP ImageMapImpl::GetName(nsString& aResult) 
{
  aResult = mName;
  return NS_OK;
}

NS_IMETHODIMP ImageMapImpl::AddArea(const nsString& aBaseURL,
                                    const nsString& aShape,
                                    const nsString& aCoords,
                                    const nsString& aHREF,
                                    const nsString& aTarget,
                                    const nsString& aAltText,
                                    PRBool aSuppress)
{
  Area* area;
  if ((0 == aShape.Length()) || aShape.EqualsIgnoreCase("rect")) {
    area = new RectArea(aBaseURL, aHREF, aTarget, aAltText, aSuppress);
  } else if (aShape.EqualsIgnoreCase("poly") ||
             aShape.EqualsIgnoreCase("polygon")) {
    area = new PolyArea(aBaseURL, aHREF, aTarget, aAltText, aSuppress);
  } else if (aShape.EqualsIgnoreCase("circle")) {
    area = new CircleArea(aBaseURL, aHREF, aTarget, aAltText, aSuppress);
  }
  else {
    area = new DefaultArea(aBaseURL, aHREF, aTarget, aAltText, aSuppress);
  }
  area->ParseCoords(aCoords);
  mAreas.AppendElement(area);
  return NS_OK;
}

NS_IMETHODIMP ImageMapImpl::IsInside(nscoord aX, nscoord aY,
                                     nsIURL* aDocURL,
                                     nsString& aAbsURL,
                                     nsString& aTarget,
                                     nsString& aAltText,
                                     PRBool* aSuppress)
{
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    if (area->IsInside(aX, aY)) {
      NS_MakeAbsoluteURL(aDocURL, area->mBase, area->mHREF, aAbsURL);
      aTarget = area->mTarget;
      aAltText = area->mAltText;
      *aSuppress = area->mSuppressFeedback;
      return NS_OK;
    }
  }
  return NS_NOT_INSIDE;
}

NS_IMETHODIMP ImageMapImpl::IsInside(nscoord aX, nscoord aY)
{
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    if (area->IsInside(aX, aY)) {
      return NS_OK;
    }
  }
  return NS_NOT_INSIDE;
}

NS_IMETHODIMP ImageMapImpl::Draw(nsIPresContext& aCX, nsIRenderingContext& aRC)
{
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    area->Draw(aCX, aRC);
  }
  return NS_OK;
}

NS_HTML nsresult NS_NewImageMap(nsIImageMap** aInstancePtrResult,
                                nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  ImageMapImpl* it = new ImageMapImpl(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIImageMapIID, (void**) aInstancePtrResult);
}
