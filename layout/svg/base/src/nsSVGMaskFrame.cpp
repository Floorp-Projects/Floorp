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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMSVGMaskElement.h"
#include "nsSVGMaskFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsISVGValueUtils.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsSVGDefsFrame.h"
#include "nsIDOMSVGLength.h"
#include "nsISVGRendererSurface.h"
#include "nsSVGUtils.h"

typedef nsSVGDefsFrame nsSVGMaskFrameBase;

class nsSVGMaskFrame : public nsSVGMaskFrameBase,
                       public nsISVGMaskFrame
{
  friend nsIFrame*
  NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsIContent* aContent);

  virtual ~nsSVGMaskFrame();
  NS_IMETHOD InitSVG();

 public:
  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsISVGMaskFrame interface:
  NS_IMETHOD MaskPaint(nsISVGRendererCanvas* aCanvas,
                       nsISVGRendererSurface* aSurface,
                       nsISVGChildFrame* aParent,
                       nsCOMPtr<nsIDOMSVGMatrix> aMatrix,
                       float aOpacity = 1.0f);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgMaskFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGMask"), aResult);
  }
#endif

 private:
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mMaskUnits;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mMaskContentUnits;
  nsCOMPtr<nsIDOMSVGLength> mX;
  nsCOMPtr<nsIDOMSVGLength> mY;
  nsCOMPtr<nsIDOMSVGLength> mWidth;
  nsCOMPtr<nsIDOMSVGLength> mHeight;

  nsISVGChildFrame *mMaskParent;
  nsCOMPtr<nsIDOMSVGMatrix> mMaskParentMatrix;

  // nsISVGContainerFrame interface:
  already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();
};

NS_INTERFACE_MAP_BEGIN(nsSVGMaskFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGMaskFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGMaskFrameBase)


//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGMaskFrame(nsIPresShell* aPresShell, nsIContent* aContent)
{
  return new (aPresShell) nsSVGMaskFrame;
}

nsresult
NS_GetSVGMaskFrame(nsISVGMaskFrame **aResult,
                   nsIURI *aURI, nsIContent *aContent)
{
  *aResult = nsnull;

  // Get the PresShell
  nsIDocument *myDoc = aContent->GetCurrentDoc();
  if (!myDoc) {
    NS_WARNING("No document for this content!");
    return NS_ERROR_FAILURE;
  }
  nsIPresShell *aPresShell = myDoc->GetShellAt(0);

  // Find the referenced frame
  nsIFrame *cpframe;
  if (!NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&cpframe, aURI, aContent, aPresShell)))
    return NS_ERROR_FAILURE;

  nsIAtom* frameType = cpframe->GetType();
  if (frameType != nsLayoutAtoms::svgMaskFrame)
    return NS_ERROR_FAILURE;

  *aResult = (nsSVGMaskFrame *)cpframe;
  return NS_OK;
}

nsSVGMaskFrame::~nsSVGMaskFrame()
{
  NS_REMOVE_SVGVALUE_OBSERVER(mX);
  NS_REMOVE_SVGVALUE_OBSERVER(mY);
  NS_REMOVE_SVGVALUE_OBSERVER(mWidth);
  NS_REMOVE_SVGVALUE_OBSERVER(mHeight);
  NS_REMOVE_SVGVALUE_OBSERVER(mMaskUnits);
  NS_REMOVE_SVGVALUE_OBSERVER(mMaskContentUnits);
}

NS_IMETHODIMP
nsSVGMaskFrame::InitSVG()
{
  nsresult rv = nsSVGDefsFrame::InitSVG();
  if (NS_FAILED(rv))
    return rv;

  mMaskParentMatrix = nsnull;

  nsCOMPtr<nsIDOMSVGMaskElement> mask = do_QueryInterface(mContent);
  NS_ASSERTION(mask, "wrong content element");

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    mask->GetX(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mX));
    NS_ASSERTION(mX, "no X");
    if (!mX) return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mX);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    mask->GetY(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mY));
    NS_ASSERTION(mY, "no Y");
    if (!mY) return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mY);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    mask->GetWidth(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mWidth));
    NS_ASSERTION(mWidth, "no Width");
    if (!mWidth) return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mWidth);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    mask->GetHeight(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mHeight));
    NS_ASSERTION(mHeight, "no Height");
    if (!mHeight) return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mHeight);
  }

  mask->GetMaskUnits(getter_AddRefs(mMaskUnits));
  NS_ADD_SVGVALUE_OBSERVER(mMaskUnits);

  mask->GetMaskContentUnits(getter_AddRefs(mMaskContentUnits));
  NS_ADD_SVGVALUE_OBSERVER(mMaskContentUnits);

  return NS_OK;
}


/* sRGB -> linearRGB mapping table */
static unsigned char rgb2lin[256] = {
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   2,   2, 
  2,   2,   2,   2,   2,   3,   3,   3, 
  3,   3,   4,   4,   4,   4,   4,   5, 
  5,   5,   5,   6,   6,   6,   6,   7, 
  7,   7,   8,   8,   8,   9,   9,   9, 
 10,  10,  10,  11,  11,  11,  12,  12, 
 13,  13,  13,  14,  14,  15,  15,  16, 
 16,  16,  17,  17,  18,  18,  19,  19, 
 20,  20,  21,  22,  22,  23,  23,  24, 
 24,  25,  26,  26,  27,  27,  28,  29, 
 29,  30,  31,  31,  32,  33,  33,  34, 
 35,  36,  36,  37,  38,  38,  39,  40, 
 41,  42,  42,  43,  44,  45,  46,  47, 
 47,  48,  49,  50,  51,  52,  53,  54, 
 55,  55,  56,  57,  58,  59,  60,  61, 
 62,  63,  64,  65,  66,  67,  68,  70, 
 71,  72,  73,  74,  75,  76,  77,  78, 
 80,  81,  82,  83,  84,  85,  87,  88, 
 89,  90,  92,  93,  94,  95,  97,  98, 
 99, 101, 102, 103, 105, 106, 107, 109, 
110, 112, 113, 114, 116, 117, 119, 120, 
122, 123, 125, 126, 128, 129, 131, 132, 
134, 135, 137, 139, 140, 142, 144, 145, 
147, 148, 150, 152, 153, 155, 157, 159, 
160, 162, 164, 166, 167, 169, 171, 173, 
175, 176, 178, 180, 182, 184, 186, 188, 
190, 192, 193, 195, 197, 199, 201, 203, 
205, 207, 209, 211, 213, 215, 218, 220, 
222, 224, 226, 228, 230, 232, 235, 237, 
239, 241, 243, 245, 248, 250, 252, 255
};

NS_IMETHODIMP
nsSVGMaskFrame::MaskPaint(nsISVGRendererCanvas* aCanvas,
                          nsISVGRendererSurface* aSurface,
                          nsISVGChildFrame* aParent,
                          nsCOMPtr<nsIDOMSVGMatrix> aMatrix,
                          float aOpacity)
{
  nsRect dirty;

  if (NS_FAILED(aCanvas->PushSurface(aSurface)))
    return NS_ERROR_FAILURE;

  {
    nsIFrame *frame;
    CallQueryInterface(aParent, &frame);
    nsCOMPtr<nsIContent> parent = frame->GetContent();

    float x, y, width, height;

    PRUint16 units;
    mMaskUnits->GetAnimVal(&units);

    if (units == nsIDOMSVGMaskElement::SVG_MUNITS_OBJECTBOUNDINGBOX) {

      aParent->SetMatrixPropagation(PR_FALSE);
      aParent->NotifyCanvasTMChanged(PR_TRUE);

      nsCOMPtr<nsIDOMSVGRect> bbox;
      aParent->GetBBox(getter_AddRefs(bbox));

      aParent->SetMatrixPropagation(PR_TRUE);
      aParent->NotifyCanvasTMChanged(PR_TRUE);

      if (!bbox)
        return NS_OK;

#ifdef DEBUG_tor
      bbox->GetX(&x);
      bbox->GetY(&y);
      bbox->GetWidth(&width);
      bbox->GetHeight(&height);

      fprintf(stderr, "mask bbox: %f,%f %fx%f\n", x, y, width, height);
#endif

      bbox->GetX(&x);
      x += nsSVGUtils::ObjectSpace(bbox, mX, nsSVGUtils::X);
      bbox->GetY(&y);
      y += nsSVGUtils::ObjectSpace(bbox, mY, nsSVGUtils::Y);
      width = nsSVGUtils::ObjectSpace(bbox, mWidth, nsSVGUtils::X);
      height = nsSVGUtils::ObjectSpace(bbox, mHeight, nsSVGUtils::Y);
    } else {
      x = nsSVGUtils::UserSpace(parent, mX, nsSVGUtils::X);
      y = nsSVGUtils::UserSpace(parent, mY, nsSVGUtils::Y);
      width = nsSVGUtils::UserSpace(parent, mWidth, nsSVGUtils::X);
      height = nsSVGUtils::UserSpace(parent, mHeight, nsSVGUtils::Y);
    }

#ifdef DEBUG_tor
    fprintf(stderr, "mask clip: %f,%f %fx%f\n", x, y, width, height);
#endif

    aCanvas->SetClipRect(aMatrix, x, y, width, height);
  }

  mMaskParent = aParent,
  mMaskParentMatrix = aMatrix;

  NotifyCanvasTMChanged(PR_TRUE);

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->PaintSVG(aCanvas, dirty, PR_TRUE);
    }
  }

  aCanvas->PopSurface();

  /* Now convert to intensity (sRGB -> linearRGB -> intensity)
     and store in alpha channel */

  PRUint32 width, height, length;
  PRUint8 *data;
  PRInt32 stride;

  aSurface->Lock();
  aSurface->GetWidth(&width);
  aSurface->GetHeight(&height);
  aSurface->GetData(&data, &length, &stride);
  for (PRUint32 y = 0; y < height; y++)
    for (PRUint32 x = 0; x < width; x++) {
      PRUint32 a; 
      float r, g, b, intensity;

      /* un-premultiply and sRGB -> linearRGB conversion */
      a = data[stride * y + 4 * x + 3];
      if (a) {
        b = rgb2lin[(255 * (PRUint32)data[stride * y + 4 * x])     / a] / 255.0;
        g = rgb2lin[(255 * (PRUint32)data[stride * y + 4 * x + 1]) / a] / 255.0;
        r = rgb2lin[(255 * (PRUint32)data[stride * y + 4 * x + 2]) / a] / 255.0;
      } else {
        b = g = r = 0.0f;
      }

      /* linearRGB -> intensity */
      intensity = (r * 0.2125 + g * 0.7154 + b * 0.0721) * (a / 255.0) * aOpacity;

      data[stride * y + 4 * x] = 255;
      data[stride * y + 4 * x + 1] = 255;
      data[stride * y + 4 * x + 2] = 255;
      data[stride * y + 4 * x + 3] = (unsigned char)(intensity * 255);
    }

  aSurface->Unlock();

  return NS_OK;
}

nsIAtom *
nsSVGMaskFrame::GetType() const
{
  return nsLayoutAtoms::svgMaskFrame;
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGMaskFrame::GetCanvasTM()
{
  // startup cycle
  if (!mMaskParentMatrix) {
    NS_ASSERTION(mParent, "null parent");
    nsISVGContainerFrame *containerFrame;
    mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame),
                            (void**)&containerFrame);
    if (!containerFrame) {
      NS_ERROR("invalid parent");
      return nsnull;
    }
    mMaskParentMatrix = containerFrame->GetCanvasTM();
  }

  nsCOMPtr<nsIDOMSVGMatrix> canvasTM = mMaskParentMatrix;

  /* object bounding box? */
  PRUint16 units;
  nsCOMPtr<nsIDOMSVGMaskElement> path = do_QueryInterface(mContent);
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> aEnum;
  path->GetMaskContentUnits(getter_AddRefs(aEnum));
  aEnum->GetAnimVal(&units);

  if (mMaskParent &&
      units == nsIDOMSVGMaskElement::SVG_MUNITS_OBJECTBOUNDINGBOX) {
    nsCOMPtr<nsIDOMSVGRect> rect;
    nsresult rv = mMaskParent->GetBBox(getter_AddRefs(rect));

    if (NS_SUCCEEDED(rv)) {
      float minx, miny, width, height;
      rect->GetX(&minx);
      rect->GetY(&miny);
      rect->GetWidth(&width);
      rect->GetHeight(&height);

      nsCOMPtr<nsIDOMSVGMatrix> tmp, fini;
      canvasTM->Translate(minx, miny, getter_AddRefs(tmp));
      tmp->ScaleNonUniform(width, height, getter_AddRefs(fini));
      canvasTM = fini;
    }
  }

  nsIDOMSVGMatrix* retval = canvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}
