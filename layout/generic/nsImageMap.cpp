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
#include "nsImageMap.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsCoord.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIURL.h"
#include "nsXIFConverter.h"
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

class Area {
public:
  Area(const nsString& aBaseURL, const nsString& aHREF,
       const nsString& aTarget, const nsString& aAltText,
       PRBool aSuppress, PRBool aHasURL);
  virtual ~Area();

  void ParseCoords(const nsString& aSpec);

  virtual PRBool IsInside(nscoord x, nscoord y) = 0;
  virtual void Draw(nsIPresContext& aCX,
                    nsIRenderingContext& aRC) = 0;
  virtual void GetShapeName(nsString& aResult) const = 0;

  void ToHTML(nsString& aResult);


  /**
   * Translate the content object into the (XIF) XML Interchange Format
   * XIF is an intermediate form of the content model, the buffer
   * will then be parsed into any number of formats including HTML, TXT, etc.
   */
  virtual void BeginConvertToXIF(nsXIFConverter& aConverter) const;
  virtual void ConvertContentToXIF(nsXIFConverter& aConverter) const;
  virtual void FinishConvertToXIF(nsXIFConverter& aConverter) const;



  const nsString& GetBase() const { return mBase; }
  const nsString& GetHREF() const { return mHREF; }
  const nsString& GetTarget() const { return mTarget; }
  const nsString& GetAltText() const { return mAltText; }
  PRBool GetSuppress() const { return mSuppressFeedback; }

  virtual void SizeOf(nsISizeOfHandler* aHandler) const;

  nsString mBase;
  nsString mHREF;
  nsString mTarget;
  nsString mAltText;
  nscoord* mCoords;
  PRInt32 mNumCoords;
  PRBool mSuppressFeedback;
  PRBool mHasURL;
};

Area::Area(const nsString& aBaseURL, const nsString& aHREF,
           const nsString& aTarget, const nsString& aAltText,
           PRBool aSuppress, PRBool aHasURL)
  : mBase(aBaseURL), mHREF(aHREF), mTarget(aTarget), mAltText(aAltText),
    mSuppressFeedback(aSuppress), mHasURL(aHasURL)
{
  mCoords = nsnull;
  mNumCoords = 0;
}

Area::~Area()
{
  delete [] mCoords;
}

void
Area::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  aHandler->Add((size_t) (- ((PRInt32)sizeof(nsString) * 4) ) );
  mBase.SizeOf(aHandler);
  mHREF.SizeOf(aHandler);
  mTarget.SizeOf(aHandler);
  mAltText.SizeOf(aHandler);
  aHandler->Add(mNumCoords * sizeof(nscoord));
}

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
  delete[] cp;
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

/**
 * Translate the content object into the (XIF) XML Interchange Format
 * XIF is an intermediate form of the content model, the buffer
 * will then be parsed into any number of formats including HTML, TXT, etc.
 */

void Area::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
  nsAutoString  tag("area");
  aConverter.BeginStartTag(tag);


  nsAutoString name("shape");
  nsAutoString shape;
  GetShapeName(shape);
  aConverter.AddAttribute(name,shape);
   

  nsAutoString  coords;
  if (nsnull != mCoords) {
    PRInt32 i, n = mNumCoords;
    for (i = 0; i < n; i++) {
      coords.Append(mCoords[i], 10);
      if (i < n - 1) {
        coords.Append(',');
      }
    }
  }
  name.SetString("coords");
  aConverter.AddAttribute(name,coords);

  name.SetString("href");
  aConverter.AddAttribute(name,mHREF);

  
  if (0 < mTarget.Length()) {
    name.SetString("target");
    aConverter.AddAttribute(name,mTarget);
  }
  if (0 < mAltText.Length()) {
    name.SetString("alt");
    aConverter.AddAttribute(name,mTarget);
  }
  if (mSuppressFeedback) {
    name.SetString("suppress");
    aConverter.AddAttribute(name);
  }
}

void Area::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
  nsAutoString  tag("area");
  aConverter.FinishStartTag(tag,PR_TRUE);
}

void Area::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
  // Nothing needs to be done here, all of the logic
  // is handled in the start and finish methods

}


//----------------------------------------------------------------------

class DefaultArea : public Area {
public:
  DefaultArea(const nsString& aBaseURL, const nsString& aHREF,
              const nsString& aTarget, const nsString& aAltText,
              PRBool aSuppress, PRBool aHasURL);
  ~DefaultArea();

  virtual PRBool IsInside(nscoord x, nscoord y);
  virtual void Draw(nsIPresContext& aCX,
                    nsIRenderingContext& aRC);
  virtual void GetShapeName(nsString& aResult) const;
};

DefaultArea::DefaultArea(const nsString& aBaseURL, const nsString& aHREF,
                         const nsString& aTarget, const nsString& aAltText,
                         PRBool aSuppress, PRBool aHasURL)
  : Area(aBaseURL, aHREF, aTarget, aAltText, aSuppress, aHasURL)
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

void DefaultArea::GetShapeName(nsString& aResult) const
{
  aResult.Append("default");
}

//----------------------------------------------------------------------

class RectArea : public Area {
public:
  RectArea(const nsString& aBaseURL, const nsString& aHREF,
           const nsString& aTarget, const nsString& aAltText,
           PRBool aSuppress, PRBool aHasURL);
  ~RectArea();

  virtual PRBool IsInside(nscoord x, nscoord y);
  virtual void Draw(nsIPresContext& aCX,
                    nsIRenderingContext& aRC);
  virtual void GetShapeName(nsString& aResult) const;
};

RectArea::RectArea(const nsString& aBaseURL, const nsString& aHREF,
                   const nsString& aTarget, const nsString& aAltText,
                   PRBool aSuppress, PRBool aHasURL)
  : Area(aBaseURL, aHREF, aTarget, aAltText, aSuppress, aHasURL)
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
    float p2t;
    aCX.GetPixelsToTwips(&p2t);
    nscoord x1 = NSIntPixelsToTwips(mCoords[0], p2t);
    nscoord y1 = NSIntPixelsToTwips(mCoords[1], p2t);
    nscoord x2 = NSIntPixelsToTwips(mCoords[2], p2t);
    nscoord y2 = NSIntPixelsToTwips(mCoords[3], p2t);
    if ((x1 > x2)|| (y1 > y2)) {
      return;
    }
    aRC.DrawRect(x1, y1, x2 - x1, y2 - y1);
  }
}

void RectArea::GetShapeName(nsString& aResult) const
{
  aResult.Append("rect");
}

//----------------------------------------------------------------------

class PolyArea : public Area {
public:
  PolyArea(const nsString& aBaseURL, const nsString& aHREF,
           const nsString& aTarget, const nsString& aAltText,
           PRBool aSuppress, PRBool aHasURL);
  ~PolyArea();

  virtual PRBool IsInside(nscoord x, nscoord y);
  virtual void Draw(nsIPresContext& aCX,
                    nsIRenderingContext& aRC);
  virtual void GetShapeName(nsString& aResult) const;
};

PolyArea::PolyArea(const nsString& aBaseURL, const nsString& aHREF,
                   const nsString& aTarget, const nsString& aAltText,
                   PRBool aSuppress, PRBool aHasURL)
  : Area(aBaseURL, aHREF, aTarget, aAltText, aSuppress, aHasURL)
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
    float p2t;
    aCX.GetPixelsToTwips(&p2t);
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

void PolyArea::GetShapeName(nsString& aResult) const
{
  aResult.Append("polygon");
}

//----------------------------------------------------------------------

class CircleArea : public Area {
public:
  CircleArea(const nsString& aBaseURL, const nsString& aHREF,
             const nsString& aTarget, const nsString& aAltText,
             PRBool aSuppress, PRBool aHasURL);
  ~CircleArea();

  virtual PRBool IsInside(nscoord x, nscoord y);
  virtual void Draw(nsIPresContext& aCX,
                    nsIRenderingContext& aRC);
  virtual void GetShapeName(nsString& aResult) const;
};

CircleArea::CircleArea(const nsString& aBaseURL, const nsString& aHREF,
                       const nsString& aTarget, const nsString& aAltText,
                       PRBool aSuppress, PRBool aHasURL)
  : Area(aBaseURL, aHREF, aTarget, aAltText, aSuppress, aHasURL)
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
    float p2t;
    aCX.GetPixelsToTwips(&p2t);
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

void CircleArea::GetShapeName(nsString& aResult) const
{
  aResult.Append("circle");
}

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMHTMLElementIID, NS_IDOMHTMLELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLAreaElementIID, NS_IDOMHTMLAREAELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLAnchorElementIID, NS_IDOMHTMLANCHORELEMENT_IID);
static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);

nsImageMap::nsImageMap()
{
  mMap = nsnull;
  mDomMap = nsnull;
  mRefCnt = 1;
  mDocument = nsnull;
  mContainsBlockContents = PR_FALSE;
}

nsImageMap::~nsImageMap()
{
  FreeAreas();
  if (nsnull != mDocument) {
    mDocument->RemoveObserver(NS_STATIC_CAST(nsIDocumentObserver*, this));
    NS_RELEASE(mDocument);
  }
  NS_IF_RELEASE(mDomMap);
  NS_IF_RELEASE(mMap);
}

NS_IMPL_ISUPPORTS(nsImageMap, kIDocumentObserverIID);

void
nsImageMap::FreeAreas()
{
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    delete area;
  }
  mAreas.Clear();
}

nsresult
nsImageMap::Init(nsIDOMHTMLMapElement* aMap)
{
  NS_PRECONDITION(nsnull != aMap, "null ptr");
  if (nsnull == aMap) {
    return NS_ERROR_NULL_POINTER;
  }
  mDomMap = aMap;
  NS_ADDREF(aMap);

  nsresult rv = aMap->QueryInterface(kIContentIID, (void**) &mMap);
  if (NS_SUCCEEDED(rv)) {
    rv = mMap->GetDocument(mDocument);
    if (NS_SUCCEEDED(rv) && (nsnull != mDocument)) {
      mDocument->AddObserver(NS_STATIC_CAST(nsIDocumentObserver*, this));
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
  nsAutoString shape, coords, baseURL, href, target, altText, noHref;
  aArea->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::shape, shape);
  aArea->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::coords, coords);
  aArea->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, href);
  aArea->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::target, target);
  aArea->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::alt, altText);
  PRBool hasURL = (PRBool)(NS_CONTENT_ATTR_HAS_VALUE != aArea->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::nohref, noHref));
  PRBool suppress = PR_FALSE;/* XXX */

  Area* area;
  if ((0 == shape.Length()) || shape.EqualsIgnoreCase("rect")) {
    area = new RectArea(baseURL, href, target, altText, suppress, hasURL);
  } else if (shape.EqualsIgnoreCase("poly") ||
             shape.EqualsIgnoreCase("polygon")) {
    area = new PolyArea(baseURL, href, target, altText, suppress, hasURL);
  } else if (shape.EqualsIgnoreCase("circle")) {
    area = new CircleArea(baseURL, href, target, altText, suppress, hasURL);
  }
  else {
    area = new DefaultArea(baseURL, href, target, altText, suppress, hasURL);
  }
  area->ParseCoords(coords);
  mAreas.AppendElement(area);
  return NS_OK;
}

PRBool
nsImageMap::IsInside(nscoord aX, nscoord aY,
                     nsIURL* aDocURL,
                     nsString& aAbsURL,
                     nsString& aTarget,
                     nsString& aAltText,
                     PRBool* aSuppress)
{
  PRInt32 i, n = mAreas.Count();
  for (i = 0; i < n; i++) {
    Area* area = (Area*) mAreas.ElementAt(i);
    if (area->IsInside(aX, aY) && area->mHasURL) {
      NS_MakeAbsoluteURL(aDocURL, area->mBase, area->mHREF, aAbsURL);
      aTarget = area->mTarget;
      aAltText = area->mAltText;
      *aSuppress = area->mSuppressFeedback;
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
      if ((area->mHREF).Length() > 0) {
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

void
nsImageMap::Draw(nsIPresContext& aCX, nsIRenderingContext& aRC)
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
