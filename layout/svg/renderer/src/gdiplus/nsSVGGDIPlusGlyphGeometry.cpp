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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#include <windows.h>

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

#include <Gdiplus.h>
using namespace Gdiplus;

#include "nsCOMPtr.h"
#include "nsSVGGDIPlusGlyphGeometry.h"
#include "nsISVGRendererGlyphGeometry.h"
#include "nsISVGGDIPlusCanvas.h"
#include "nsIDOMSVGMatrix.h"
#include "nsSVGGDIPlusRegion.h"
#include "nsISVGGDIPlusRegion.h"
#include "nsISVGRendererRegion.h"
#include "nsISVGGlyphGeometrySource.h"
#include "nsPromiseFlatString.h"
#include "nsSVGGDIPlusGlyphMetrics.h"
#include "nsISVGGDIPlusGlyphMetrics.h"
#include "nsPresContext.h"
#include "nsMemory.h"
#include "nsSVGGDIPlusGradient.h"

/**
 * \addtogroup gdiplus_renderer GDI+ Rendering Engine
 * @{
 */
//////////////////////////////////////////////////////////////////////
/**
 * Helper class used by nsSVGGDIPlusGlyphGeometry
 *
 * We section the text into highlighted & non-highlighted regions and use
 * this helper class to iterate over them.
 */
class SectionIterator
{
public:
  SectionIterator(nsISVGGlyphGeometrySource *src);

  void First() { mPointer=0; }
  void Next() { ++mPointer; }
  PRBool IsEnd() { return mPointer>=mCount; }

  PRBool IsOnlySection() { return mCount==1; }

  PRBool IsHighlighted() {
    NS_ASSERTION(mPointer<mCount, "iterator out of range");
    return mSections[mPointer].highlighted;
  }
  
  const WCHAR *GetSectionPtr() {
    NS_ASSERTION(mPointer<mCount, "iterator out of range");
    return PromiseFlatString(mText).get()+mSections[mPointer].offset;
  }
  
  PRUint32 GetLength() {
    NS_ASSERTION(mPointer<mCount, "iterator out of range");
    return mSections[mPointer].length;
  }

  float GetAdvance() {
    NS_ASSERTION(mPointer<mCount, "iterator out of range");
    return mSections[mPointer].bounds.X;
  }

  const RectF& GetBounds() {
    NS_ASSERTION(mPointer<mCount, "iterator out of range");
    return mSections[mPointer].bounds;
  }

  nscolor GetHighlightForeground(){ return mHighlightForeground; }
  nscolor GetHighlightBackground(){ return mHighlightBackground; }
  
private:
  struct Section{
    PRUint32 offset;
    PRUint32 length;
    PRBool highlighted;
    RectF bounds;
  };

  Section mSections[3];
  nscolor mHighlightForeground;
  nscolor mHighlightBackground;
  int mCount;
  int mPointer;
  nsString mText;
};

/** @} */

SectionIterator::SectionIterator(nsISVGGlyphGeometrySource *src)
    : mCount(0), mPointer(0)
{
  NS_ASSERTION(src, "no glyphgeometrysource");
  
  src->GetCharacterData(mText);
  PRUint32 length = mText.Length();
  PRUint32 roffset,rcount;

  PRBool hasHighlight;
  src->GetHasHighlight(&hasHighlight);
  if (!hasHighlight) {
    // no highlight. this is the 'normal' case
    // we have only one section
    roffset=0;
    rcount=0;
  }
  else {
    src->GetHighlight(&roffset, &rcount,
                      &mHighlightForeground,
                      &mHighlightBackground);
  }
  
  // create the sections. There are between 1-3 sections.
  
  if (roffset>0) {
    mSections[mCount].offset = 0;
    mSections[mCount].length = min(length, roffset);
    mSections[mCount].highlighted = PR_FALSE;
    ++mCount;
  }
  
  if (rcount>0 && roffset<length) {
    mSections[mCount].offset = roffset;
    mSections[mCount].length = min(rcount, length-roffset);
    mSections[mCount].highlighted = PR_TRUE;
    ++mCount;
  }

  if (roffset+rcount<length) {
    mSections[mCount].offset = roffset+rcount;
    mSections[mCount].length = length-mSections[mCount].offset;
    mSections[mCount].highlighted = PR_FALSE;
    ++mCount;
  }

  // calculate the bounds of the sections
  if (!mCount) return;
  
  nsCOMPtr<nsISVGGDIPlusGlyphMetrics> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    src->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics) return;
  }

  if (mCount==1) {
    // Use GetBoundingRect() when there is only one section. This is
    // the normal case and faster than calling getSubBoundingRect(),
    // since the glyph metrics object caches the bb.
    mSections[0].bounds = *metrics->GetBoundingRect();
    return;
  }
  // else
  // generic case
  for (int i=0; i<mCount; ++i) {
    metrics->GetSubBoundingRect(mSections[i].offset, mSections[i].length,
                                &mSections[i].bounds);
  }
    
}

/**
 * \addtogroup gdiplus_renderer GDI+ Rendering Engine
 * @{
 */
////////////////////////////////////////////////////////////////////////
/**
 *  GDI+ glyph geometry implementation
 */
class nsSVGGDIPlusGlyphGeometry : public nsISVGRendererGlyphGeometry
{
protected:
  friend nsresult NS_NewSVGGDIPlusGlyphGeometry(nsISVGRendererGlyphGeometry **result,
                                                nsISVGGlyphGeometrySource *src);

  nsSVGGDIPlusGlyphGeometry();
  ~nsSVGGDIPlusGlyphGeometry();
  nsresult Init(nsISVGGlyphGeometrySource* src);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRendererGlyphGeometry interface:
  NS_DECL_NSISVGRENDERERGLYPHGEOMETRY
  
protected:
  void DrawFill(Graphics* g, Brush& b, nsISVGGradient *aGrad,
                const WCHAR* start, INT length, float x, float y);
  void GetGlobalTransform(Matrix *matrix);
  void UpdateStroke();
  void UpdateRegions(); // update covered region & hit-test region
  
  nsCOMPtr<nsISVGGlyphGeometrySource> mSource;
  nsCOMPtr<nsISVGRendererRegion> mCoveredRegion;
  Region *mHitTestRegion;
  GraphicsPath *mStroke;
};

/** @} */

//----------------------------------------------------------------------
// implementation:

nsSVGGDIPlusGlyphGeometry::nsSVGGDIPlusGlyphGeometry()
    : mStroke(nsnull), mHitTestRegion(nsnull)
{
}

nsSVGGDIPlusGlyphGeometry::~nsSVGGDIPlusGlyphGeometry()
{
  if (mStroke) delete mStroke;
  if (mHitTestRegion) delete mHitTestRegion;
}

nsresult
nsSVGGDIPlusGlyphGeometry::Init(nsISVGGlyphGeometrySource* src)
{
  mSource = src;
  return NS_OK;
}


nsresult
NS_NewSVGGDIPlusGlyphGeometry(nsISVGRendererGlyphGeometry **result,
                              nsISVGGlyphGeometrySource *src)
{
  *result = nsnull;
  
  nsSVGGDIPlusGlyphGeometry* gg = new nsSVGGDIPlusGlyphGeometry();
  if (!gg) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(gg);

  nsresult rv = gg->Init(src);

  if (NS_FAILED(rv)) {
    NS_RELEASE(gg);
    return rv;
  }
  
  *result = gg;
  return rv;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGGDIPlusGlyphGeometry)
NS_IMPL_RELEASE(nsSVGGDIPlusGlyphGeometry)

NS_INTERFACE_MAP_BEGIN(nsSVGGDIPlusGlyphGeometry)
  NS_INTERFACE_MAP_ENTRY(nsISVGRendererGlyphGeometry)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


//----------------------------------------------------------------------
// nsISVGRendererGlyphGeometry methods:

static void gradCBPath(Graphics *gfx, Brush *brush, void *cbStruct)
{
  GraphicsPath *path = (GraphicsPath *)cbStruct;
  gfx->FillPath(brush, path);
}

/** Implements void render(in nsISVGRendererCanvas canvas); */
NS_IMETHODIMP
nsSVGGDIPlusGlyphGeometry::Render(nsISVGRendererCanvas *canvas)
{
  // make sure that we have constructed our render objects
  if (!mCoveredRegion) {
    nsCOMPtr<nsISVGRendererRegion> region;
    Update(nsISVGGeometrySource::UPDATEMASK_ALL, getter_AddRefs(region));
  }

  PRBool hasFill = PR_FALSE, hasStroke = PR_FALSE;
  PRUint16 filltype, stroketype;

  mSource->GetFillPaintType(&filltype);
  if (filltype != nsISVGGeometrySource::PAINT_TYPE_NONE)
    hasFill = PR_TRUE;

  mSource->GetStrokePaintType(&stroketype);
  if (stroketype != nsISVGGeometrySource::PAINT_TYPE_NONE && mStroke)
    hasStroke = PR_TRUE;

  if (!hasFill && !hasStroke) return NS_OK; // nothing to paint
  
  SectionIterator sections(mSource);
  if (sections.IsEnd()) return NS_OK; // nothing to paint

  nsCOMPtr<nsISVGGDIPlusCanvas> gdiplusCanvas = do_QueryInterface(canvas);
  NS_ASSERTION(gdiplusCanvas, "wrong svg canvas for geometry!");
  if (!gdiplusCanvas) return NS_ERROR_FAILURE;
  gdiplusCanvas->GetGraphics()->SetSmoothingMode(SmoothingModeAntiAlias);
  //gdiplusCanvas->GetGraphics()->SetPixelOffsetMode(PixelOffsetModeHalf);
  
  nsAutoString text;
  mSource->GetCharacterData(text);

  float x,y;
  mSource->GetX(&x);
  mSource->GetY(&y);
  
  // iterate over all sections
  while (!sections.IsEnd()) {
    
    if (sections.IsHighlighted()) {
      nscolor bg = sections.GetHighlightBackground();
      SolidBrush brush(Color(NS_GET_R(bg), NS_GET_G(bg), NS_GET_B(bg)));

      GraphicsState state = gdiplusCanvas->GetGraphics()->Save();
      Matrix m;
      GetGlobalTransform(&m);
      gdiplusCanvas->GetGraphics()->MultiplyTransform(&m);
      
      gdiplusCanvas->GetGraphics()->FillRectangle(&brush, sections.GetBounds().X+x,
                                                  sections.GetBounds().Y+y,
                                                  sections.GetBounds().Width,
                                                  sections.GetBounds().Height);
      gdiplusCanvas->GetGraphics()->Restore(state);
    }

    if (hasFill) {
      nscolor color;
      if (!sections.IsHighlighted())
        mSource->GetFillPaint(&color);
      else
        color = sections.GetHighlightForeground();
      
      float opacity;
      mSource->GetFillOpacity(&opacity);

      SolidBrush brush(Color((BYTE)(opacity*255),NS_GET_R(color),NS_GET_G(color),NS_GET_B(color)));

      nsCOMPtr<nsISVGGradient> aGrad;
      if (filltype != nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR)
        mSource->GetFillGradient(getter_AddRefs(aGrad));

      DrawFill(gdiplusCanvas->GetGraphics(), brush, aGrad,
               sections.GetSectionPtr(), sections.GetLength(), sections.GetAdvance()+x, y);
    }

    if (hasStroke) {
      nscolor color;
      if (!sections.IsHighlighted())
        mSource->GetStrokePaint(&color);
      else
        color = sections.GetHighlightForeground();
      
      float opacity;
      mSource->GetStrokeOpacity(&opacity);

      nsCOMPtr<nsISVGGradient> aGrad;
      if (filltype != nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR)
        mSource->GetStrokeGradient(getter_AddRefs(aGrad));

      SolidBrush brush(Color((BYTE)(opacity*255), NS_GET_R(color), NS_GET_G(color), NS_GET_B(color)));

      if (sections.IsOnlySection() && !sections.IsHighlighted()) {
        // this is the 'normal' case
        if (filltype == nsISVGGeometrySource::PAINT_TYPE_SOLID_COLOR) {
          gdiplusCanvas->GetGraphics()->FillPath(&brush, mStroke);
        } else {
          nsCOMPtr<nsISVGRendererRegion> region;
          GetCoveredRegion(getter_AddRefs(region));
          nsCOMPtr<nsISVGGDIPlusRegion> aRegion = do_QueryInterface(region);
          nsCOMPtr<nsIDOMSVGMatrix> ctm;
          mSource->GetCanvasTM(getter_AddRefs(ctm));
          
          GDIPlusGradient(aRegion, aGrad, ctm,
                          gdiplusCanvas->GetGraphics(),
                          gradCBPath, mStroke);
        }
      }
      else {
        // There is more than one section, so we need to clip our cached mStroke
        // XXX For now we just paint the stroke as in the unhighlighted case...
        gdiplusCanvas->GetGraphics()->FillPath(&brush, mStroke);
        // ... and paint a fill on top if the section is highlighted:
        if (sections.IsHighlighted())
          DrawFill(gdiplusCanvas->GetGraphics(), brush, aGrad,
                   sections.GetSectionPtr(), sections.GetLength(), sections.GetAdvance()+x, y);
      }
      
    }      
    
    sections.Next();      
  }
  
  return NS_OK;
}

/** Implements nsISVGRendererRegion update(in unsigned long updatemask); */
NS_IMETHODIMP
nsSVGGDIPlusGlyphGeometry::Update(PRUint32 updatemask, nsISVGRendererRegion **_retval)
{
  *_retval = nsnull;

  const unsigned long strokemask =
    nsISVGGlyphMetricsSource::UPDATEMASK_FONT           |
    nsISVGGlyphMetricsSource::UPDATEMASK_CHARACTER_DATA |
    nsISVGGlyphGeometrySource::UPDATEMASK_METRICS       |
    nsISVGGlyphGeometrySource::UPDATEMASK_X             |
    nsISVGGlyphGeometrySource::UPDATEMASK_Y             |
    nsISVGGeometrySource::UPDATEMASK_STROKE_PAINT_TYPE  |
    nsISVGGeometrySource::UPDATEMASK_STROKE_WIDTH       |
    nsISVGGeometrySource::UPDATEMASK_STROKE_LINECAP     |
    nsISVGGeometrySource::UPDATEMASK_STROKE_LINEJOIN    |
    nsISVGGeometrySource::UPDATEMASK_STROKE_MITERLIMIT  |
    nsISVGGeometrySource::UPDATEMASK_STROKE_DASH_ARRAY  |
    nsISVGGeometrySource::UPDATEMASK_STROKE_DASHOFFSET  |
    nsISVGGeometrySource::UPDATEMASK_CANVAS_TM;
  
  const unsigned long regionsmask =
    nsISVGGlyphGeometrySource::UPDATEMASK_METRICS |
    nsISVGGlyphGeometrySource::UPDATEMASK_X       |
    nsISVGGlyphGeometrySource::UPDATEMASK_Y       |
    nsISVGGeometrySource::UPDATEMASK_CANVAS_TM;

  
  nsCOMPtr<nsISVGRendererRegion> regionBefore = mCoveredRegion;

    
  if ((updatemask & strokemask)!=0) {
    UpdateStroke();
  }

  if ((updatemask & regionsmask)!=0) {
    UpdateRegions();
    if (regionBefore) {
      regionBefore->Combine(mCoveredRegion, _retval);
    }
    else {
      *_retval = mCoveredRegion;
      NS_IF_ADDREF(*_retval);
    }
  }
  else if (updatemask != nsISVGGeometrySource::UPDATEMASK_NOTHING) {
    // region hasn't changed, but something has. so invalidate whole area:
    *_retval = mCoveredRegion;
    NS_IF_ADDREF(*_retval);
  }    
      
  return NS_OK;
}

/** Implements nsISVGRendererRegion getCoveredRegion(); */
NS_IMETHODIMP
nsSVGGDIPlusGlyphGeometry::GetCoveredRegion(nsISVGRendererRegion **_retval)
{
  *_retval = mCoveredRegion;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

/** Implements boolean containsPoint(in float x, in float y); */
NS_IMETHODIMP
nsSVGGDIPlusGlyphGeometry::ContainsPoint(float x, float y, PRBool *_retval)
{
  *_retval = PR_FALSE;

   if (mHitTestRegion && mHitTestRegion->IsVisible(x,y)) {
     *_retval = PR_TRUE;
   }

  return NS_OK;
}

//----------------------------------------------------------------------
//

struct gradCallbackStruct {
  const WCHAR *start;
  INT length;
  nsISVGGDIPlusGlyphMetrics *metrics;
  float x, y;
  StringFormat *stringformat;
};

static void gradCBString(Graphics *gfx, Brush *brush, void *cbStruct)
{
  gradCallbackStruct *info = (gradCallbackStruct *)cbStruct;
  gfx->DrawString(info->start, info->length, info->metrics->GetFont(),
                  PointF(info->x, info->y), info->stringformat, brush);
}

void
nsSVGGDIPlusGlyphGeometry::DrawFill(Graphics* g, Brush& b, nsISVGGradient *aGrad,
                                    const WCHAR* start, INT length,
                                    float x, float y)
{
  nsCOMPtr<nsISVGGDIPlusGlyphMetrics> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    mSource->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics) return;
  }

  GraphicsState state = g->Save();
  
  Matrix m;
  GetGlobalTransform(&m);
  g->MultiplyTransform(&m);
  g->SetTextRenderingHint(metrics->GetTextRenderingHint());

  StringFormat stringFormat(StringFormat::GenericTypographic());
  stringFormat.SetFormatFlags(stringFormat.GetFormatFlags() |
                              StringFormatFlagsMeasureTrailingSpaces);
  stringFormat.SetLineAlignment(StringAlignmentNear);
  
  if (aGrad) {
    nsCOMPtr<nsISVGRendererRegion> region;
    GetCoveredRegion(getter_AddRefs(region));
    nsCOMPtr<nsISVGGDIPlusRegion> aRegion = do_QueryInterface(region);
    nsCOMPtr<nsIDOMSVGMatrix> ctm;
    mSource->GetCanvasTM(getter_AddRefs(ctm));
    
    gradCallbackStruct cb;
    cb.start = start;
    cb.length = length;
    cb.metrics = metrics;
    cb.x = x;  cb.y = y;
    cb.stringformat = &stringFormat;
    
    GDIPlusGradient(aRegion, aGrad, ctm, g, gradCBString, &cb);
  }
  else
    g->DrawString(start, length, metrics->GetFont(), PointF(x,y),
                  &stringFormat, &b);
  
  g->Restore(state);
}

void
nsSVGGDIPlusGlyphGeometry::GetGlobalTransform(Matrix *matrix)
{
  nsCOMPtr<nsIDOMSVGMatrix> ctm;
  mSource->GetCanvasTM(getter_AddRefs(ctm));
  NS_ASSERTION(ctm, "graphic source didn't specify a ctm");
  
  float m[6];
  float val;
  ctm->GetA(&val);
  m[0] = val;
  
  ctm->GetB(&val);
  m[1] = val;
  
  ctm->GetC(&val);  
  m[2] = val;  
  
  ctm->GetD(&val);  
  m[3] = val;  
  
  ctm->GetE(&val);
  m[4] = val;
  
  ctm->GetF(&val);
  m[5] = val;

  matrix->SetElements(m[0],m[1],m[2],m[3],m[4],m[5]);
}

void
nsSVGGDIPlusGlyphGeometry::UpdateStroke()
{
  if (mStroke) {
    delete mStroke;
    mStroke = nsnull;
  }

  PRUint16 type;
  mSource->GetStrokePaintType(&type);
  if (type == nsISVGGeometrySource::PAINT_TYPE_NONE)
    return;

  float width;
  mSource->GetStrokeWidth(&width);
  
  if (width==0.0f) return;

  nsCOMPtr<nsISVGGDIPlusGlyphMetrics> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    mSource->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics) return;
  }

  
  mStroke = new GraphicsPath();
  if (!mStroke) {
    NS_ERROR("couldn't construct graphicspath");
    return;
  }

  FontFamily fontFamily;
  metrics->GetFont()->GetFamily(&fontFamily);
  
  nsAutoString text;
  mSource->GetCharacterData(text);

  float x,y;
  mSource->GetX(&x);
  mSource->GetY(&y);

  
  mStroke->AddString(PromiseFlatString(text).get(), -1, &fontFamily, metrics->GetFont()->GetStyle(),
                     metrics->GetFont()->GetSize(),
                     PointF(x,y), StringFormat::GenericTypographic());
  
  // gdi+ seems to widen paths to >1 width unit. If our pen width is
  // smaller, we need to widen an appropriately enlarged path and then
  // scale it down after widening:
  PRBool scaleToUnitPen = width<1.0f;
  
  Pen pen(Color(), scaleToUnitPen ? 1.0f : width);

  // set linecap style
  PRUint16 capStyle;
  LineCap lcap;
  DashCap dcap;
  mSource->GetStrokeLinecap(&capStyle);
  switch (capStyle) {
    case nsISVGGeometrySource::STROKE_LINECAP_BUTT:
      lcap = LineCapFlat;
      dcap = DashCapFlat;
      break;
    case nsISVGGeometrySource::STROKE_LINECAP_ROUND:
      lcap = LineCapRound;
      dcap = DashCapRound;
      break;
    case nsISVGGeometrySource::STROKE_LINECAP_SQUARE:
      lcap = LineCapSquare;
      dcap = DashCapFlat;
      break;
  }
  pen.SetLineCap(lcap, lcap, dcap);


  // set linejoin style:
  PRUint16 joinStyle;
  LineJoin join;
  mSource->GetStrokeLinejoin(&joinStyle);
  switch(joinStyle) {
    case nsISVGGeometrySource::STROKE_LINEJOIN_MITER:
      join = LineJoinMiterClipped;
      break;
    case nsISVGGeometrySource::STROKE_LINEJOIN_ROUND:
      join = LineJoinRound;
      break;
    case nsISVGGeometrySource::STROKE_LINEJOIN_BEVEL:
      join = LineJoinBevel;
      break;
  }
  pen.SetLineJoin(join);
  
  // set miterlimit:
  float miterlimit;
  mSource->GetStrokeMiterlimit(&miterlimit);
  pen.SetMiterLimit(miterlimit);

  // set pen dashpattern
  float *dashArray;
  PRUint32 count;
  mSource->GetStrokeDashArray(&dashArray, &count);
  if (count>0) {
    if (!scaleToUnitPen) {
      for (PRUint32 i=0; i<count; ++i)
        dashArray[i]/=width;
    }
    
    pen.SetDashPattern(dashArray, count);
    nsMemory::Free(dashArray);

    float offset;
    mSource->GetStrokeDashoffset(&offset);
    if (!scaleToUnitPen)
      offset/=width;
    pen.SetDashOffset(offset);
  }

  Matrix m;
  GetGlobalTransform(&m);

  if (scaleToUnitPen) {
    Matrix enlarge(1/width, 0, 0, 1/width, 0, 0);
    mStroke->Transform(&enlarge);
    m.Scale(width,width);
  }

  
  mStroke->Widen(&pen);
  mStroke->Transform(&m);
//  mStroke->Outline();
}

void
nsSVGGDIPlusGlyphGeometry::UpdateRegions()
{
  if (mHitTestRegion) {
    delete mHitTestRegion;
    mHitTestRegion = nsnull;
  }

  nsCOMPtr<nsISVGGDIPlusGlyphMetrics> metrics;
  {
    nsCOMPtr<nsISVGRendererGlyphMetrics> xpmetrics;
    mSource->GetMetrics(getter_AddRefs(xpmetrics));
    metrics = do_QueryInterface(xpmetrics);
    NS_ASSERTION(metrics, "wrong metrics object!");
    if (!metrics) return;
  }
  
  // the hit-test region is the bounding box transformed into our
  // local coord system:
  mHitTestRegion = new Region(*metrics->GetBoundingRect());

  if (!mHitTestRegion) {
    NS_ERROR("could not construct region");
    return;
  }
  
  float x,y;
  mSource->GetX(&x);
  mSource->GetY(&y);
  
  mHitTestRegion->Translate(x, y);

  Matrix m;
  GetGlobalTransform(&m);
  
  mHitTestRegion->Transform(&m);

  
  // clone the covered region from the hit-test region:

  nsCOMPtr<nsPresContext> presContext;
  mSource->GetPresContext(getter_AddRefs(presContext));

  NS_NewSVGGDIPlusClonedRegion(getter_AddRefs(mCoveredRegion),
                               mHitTestRegion,
                               presContext);
}

