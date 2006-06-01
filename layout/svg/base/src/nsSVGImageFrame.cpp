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

#include "nsSVGPathGeometryFrame.h"
#include "nsISVGRendererSurface.h"
#include "nsISVGRendererCanvas.h"
#include "nsISVGRenderer.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGAnimPresAspRatio.h"
#include "nsIDOMSVGPresAspectRatio.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "imgIDecoderObserver.h"
#include "nsImageLoadingContent.h"
#include "nsIDOMSVGImageElement.h"
#include "nsSVGElement.h"
#include "nsSVGUtils.h"

#define NS_GET_BIT(rowptr, x) (rowptr[(x)>>3] &  (1<<(7-(x)&0x7)))

class nsSVGImageFrame;

class nsSVGImageListener : public imgIDecoderObserver
{
public:
  nsSVGImageListener(nsSVGImageFrame *aFrame);
  virtual ~nsSVGImageListener();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER

  void SetFrame(nsSVGImageFrame *frame) { mFrame = frame; }

private:
  nsSVGImageFrame *mFrame;
};


class nsSVGImageFrame : public nsSVGPathGeometryFrame
{
protected:
  friend nsIFrame*
  NS_NewSVGImageFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);

  virtual ~nsSVGImageFrame();
  NS_IMETHOD InitSVG();

public:
  nsSVGImageFrame(nsStyleContext* aContext) : nsSVGPathGeometryFrame(aContext) {}

  // nsIFrame interface:
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  // nsISVGPathGeometrySource interface:
  NS_IMETHOD ConstructPath(cairo_t *aCtx);

  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsISVGRendererCanvas* canvas);

  // nsSVGGeometryFrame overload:
  // Lie about our fill/stroke so hit detection works
  virtual nsresult GetStrokePaintType() { return eStyleSVGPaintType_None; }
  virtual nsresult GetFillPaintType() { return eStyleSVGPaintType_Color; }

  // nsISVGPathGeometrySource mthods:
  NS_IMETHOD GetHittestMask(PRUint16 *aHittestMask);
  
  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgImageFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGImage"), aResult);
  }
#endif

private:
  nsCOMPtr<nsIDOMSVGPreserveAspectRatio> mPreserveAspectRatio;

  nsCOMPtr<imgIDecoderObserver> mListener;
  nsCOMPtr<nsISVGRendererSurface> mSurface;

  nsresult ConvertFrame(gfxIImageFrame *aNewFrame);

  friend class nsSVGImageListener;
  PRPackedBool mSurfaceInvalid;
};

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGImageFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGImageElement> Rect = do_QueryInterface(aContent);
  if (!Rect) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGImageFrame for a content element that doesn't support the right interfaces\n");
#endif
    return nsnull;
  }

  return new (aPresShell) nsSVGImageFrame(aContext);
}

nsSVGImageFrame::~nsSVGImageFrame()
{
  // set the frame to null so we don't send messages to a dead object.
  if (mListener) {
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
    if (imageLoader) {
      imageLoader->RemoveObserver(mListener);
    }
    NS_REINTERPRET_CAST(nsSVGImageListener*, mListener.get())->SetFrame(nsnull);
  }
  mListener = nsnull;
}

NS_IMETHODIMP
nsSVGImageFrame::InitSVG()
{
  nsresult rv = nsSVGPathGeometryFrame::InitSVG();
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDOMSVGImageElement> Rect = do_QueryInterface(mContent);
  NS_ASSERTION(Rect,"wrong content element");

  {
    nsCOMPtr<nsIDOMSVGAnimatedPreserveAspectRatio> ratio;
    Rect->GetPreserveAspectRatio(getter_AddRefs(ratio));
    ratio->GetAnimVal(getter_AddRefs(mPreserveAspectRatio));
    NS_ASSERTION(mPreserveAspectRatio, "no preserveAspectRatio");
    if (!mPreserveAspectRatio) return NS_ERROR_FAILURE;
  }

  mSurface = nsnull;
  mSurfaceInvalid = PR_TRUE;

  mListener = new nsSVGImageListener(this);
  if (!mListener) return NS_ERROR_OUT_OF_MEMORY;
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
  NS_ENSURE_TRUE(imageLoader, NS_ERROR_UNEXPECTED);
  imageLoader->AddObserver(mListener);

  return NS_OK; 
}

//----------------------------------------------------------------------
// nsIFrame methods:

NS_IMETHODIMP
nsSVGImageFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                  nsIAtom*        aAttribute,
                                  PRInt32         aModType)
{
   if (aNameSpaceID == kNameSpaceID_None &&
       (aAttribute == nsGkAtoms::x ||
        aAttribute == nsGkAtoms::y ||
        aAttribute == nsGkAtoms::width ||
        aAttribute == nsGkAtoms::height ||
        aAttribute == nsGkAtoms::preserveAspectRatio)) {
     UpdateGraphic();
     return NS_OK;
   }

   return nsSVGPathGeometryFrame::AttributeChanged(aNameSpaceID,
                                                   aAttribute, aModType);
}

//----------------------------------------------------------------------
// nsISVGPathGeometrySource methods:

/* For the purposes of the update/invalidation logic pretend to
   be a rectangle. */
NS_IMETHODIMP nsSVGImageFrame::ConstructPath(cairo_t *aCtx)
{
  float x, y, width, height;

  nsSVGElement *element = NS_STATIC_CAST(nsSVGElement*, mContent);
  element->GetAnimatedLengthValues(&x, &y, &width, &height, nsnull);

  /* In a perfect world, this would be handled by the DOM, and 
     return a DOM exception. */
  if (width == 0 || height == 0)
    return NS_OK;

  cairo_rectangle(aCtx, x, y, width, height);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods:
NS_IMETHODIMP
nsSVGImageFrame::PaintSVG(nsISVGRendererCanvas* canvas)
{
  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;

  if (mSurfaceInvalid) {
    nsCOMPtr<imgIRequest> currentRequest;
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(mContent);
    if (imageLoader)
      imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                              getter_AddRefs(currentRequest));

    nsCOMPtr<imgIContainer> currentContainer;
    if (currentRequest)
      currentRequest->GetImage(getter_AddRefs(currentContainer));

    nsCOMPtr<gfxIImageFrame> currentFrame;
    if (currentContainer) 
      currentContainer->GetCurrentFrame(getter_AddRefs(currentFrame));

    if (currentFrame) {
      ConvertFrame(currentFrame);
      mSurfaceInvalid = PR_FALSE;
    } else {
      return NS_OK;
    }
  }

  if (mSurface) {
    nsCOMPtr<nsIDOMSVGMatrix> ctm;
    GetCanvasTM(getter_AddRefs(ctm));

    float x, y, width, height;
    nsSVGElement *element = NS_STATIC_CAST(nsSVGElement*, mContent);
    element->GetAnimatedLengthValues(&x, &y, &width, &height, nsnull);

    PRUint32 nativeWidth, nativeHeight;
    mSurface->GetWidth(&nativeWidth);
    mSurface->GetHeight(&nativeHeight);

    nsCOMPtr<nsIDOMSVGImageElement> image = do_QueryInterface(mContent);
    nsCOMPtr<nsIDOMSVGAnimatedPreserveAspectRatio> ratio;
    image->GetPreserveAspectRatio(getter_AddRefs(ratio));

    nsCOMPtr<nsIDOMSVGMatrix> trans, ctmXY, fini;
    trans = nsSVGUtils::GetViewBoxTransform(width, height,
                                            0, 0,
                                            nativeWidth, nativeHeight,
                                            ratio);
    ctm->Translate(x, y, getter_AddRefs(ctmXY));
    ctmXY->Multiply(trans, getter_AddRefs(fini));

    if (GetStyleDisplay()->IsScrollableOverflow()) {
      canvas->PushClip();
      canvas->SetClipRect(ctm, x, y, width, height);
    }

    canvas->CompositeSurfaceMatrix(mSurface,
                                   fini,
                                   mStyleContext->GetStyleDisplay()->mOpacity);

    if (GetStyleDisplay()->IsScrollableOverflow())
      canvas->PopClip();
  }

  return NS_OK;
}

nsIAtom *
nsSVGImageFrame::GetType() const
{
  return nsLayoutAtoms::svgImageFrame;
}

nsresult
nsSVGImageFrame::ConvertFrame(gfxIImageFrame *aNewFrame)
{
  PRInt32 width, height;
  aNewFrame->GetWidth(&width);
  aNewFrame->GetHeight(&height);
  
  nsresult rv;
  nsCOMPtr<nsISVGRenderer> renderer;

  nsISVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame)
    return NS_ERROR_FAILURE;
  rv = outerSVGFrame->GetRenderer(getter_AddRefs(renderer));
  if (NS_FAILED(rv))
    return rv;
  rv = renderer->CreateSurface(width, height, getter_AddRefs(mSurface));
  if (NS_FAILED(rv))
    return rv;
  
  PRUint8 *data, *target;
  PRUint32 length;
  PRInt32 stride;
  mSurface->Lock();
  mSurface->GetData(&data, &length, &stride);
  if (!data) {
    mSurface->Unlock();
    return NS_ERROR_FAILURE;
  }
#ifdef MOZ_PLATFORM_IMAGES_BOTTOM_TO_TOP
  stride = -stride;
#endif

  aNewFrame->LockImageData();
  aNewFrame->LockAlphaData();
  
  PRUint8 *rgb, *alpha = nsnull;
  PRUint32 bpr, abpr;
  aNewFrame->GetImageData(&rgb, &length);
  aNewFrame->GetImageBytesPerRow(&bpr);
  if (!rgb) {
    mSurface->Unlock();
    aNewFrame->UnlockImageData();
    aNewFrame->UnlockAlphaData();
    return NS_ERROR_FAILURE;
  }

#ifdef MOZ_CAIRO_GFX
  // cairo gfx already has the data in the order/format - just copy
  memcpy(data, rgb, bpr*height);
#else

  aNewFrame->GetAlphaData(&alpha, &length);
  aNewFrame->GetAlphaBytesPerRow(&abpr);

  // some platforms return 4bpp (OSX and Win32 under some circumstances)
  const PRUint32 bpp = bpr/width;

#ifdef XP_MACOSX
  // pixels on os-x have a lead byte we don't care about (alpha or
  // garbage, depending on the image format) - shift our pointer down
  // one so we can use the rest of the code as-is
  rgb++;
#endif

#if (defined(XP_UNIX) && !defined(XP_MACOSX)) || defined(MOZ_CAIRO_GFX)
#define REVERSE_CHANNELS
#endif

  // cairo/os-x wants ABGR format, GDI+ wants RGBA, cairo/unix wants BGRA
  if (!alpha) {
    for (PRInt32 y=0; y<height; y++) {
      if (stride > 0)
        target = data + stride * y;
      else
        target = data + stride * (1 - height) + stride * y;
      for (PRInt32 x=0; x<width; x++) {
#ifdef XP_MACOSX
        *target++ = 255;
#endif
#ifndef REVERSE_CHANNELS
        *target++ = rgb[y*bpr + bpp*x];
        *target++ = rgb[y*bpr + bpp*x + 1];
        *target++ = rgb[y*bpr + bpp*x + 2];
#else
        *target++ = rgb[y*bpr + bpp*x + 2];
        *target++ = rgb[y*bpr + bpp*x + 1];
        *target++ = rgb[y*bpr + bpp*x];
#endif
#ifndef XP_MACOSX
        *target++ = 255;
#endif
      }
    }
  } else {
    if (abpr >= width) {
      /* 8-bit alpha */
      for (PRInt32 y=0; y<height; y++) {
        if (stride > 0)
          target = data + stride * y;
        else
          target = data + stride * (1 - height) + stride * y;
        for (PRInt32 x=0; x<width; x++) {
          PRUint32 a = alpha[y*abpr + x];
#ifdef XP_MACOSX
          *target++ = a;
#endif
#ifndef REVERSE_CHANNELS
          FAST_DIVIDE_BY_255(*target++, rgb[y*bpr + bpp*x] * a);
          FAST_DIVIDE_BY_255(*target++, rgb[y*bpr + bpp*x + 1] * a);
          FAST_DIVIDE_BY_255(*target++, rgb[y*bpr + bpp*x + 2] * a);
#else
          FAST_DIVIDE_BY_255(*target++, rgb[y*bpr + bpp*x + 2] * a);
          FAST_DIVIDE_BY_255(*target++, rgb[y*bpr + bpp*x + 1] * a);
          FAST_DIVIDE_BY_255(*target++, rgb[y*bpr + bpp*x] * a);
#endif
#ifndef XP_MACOSX
          *target++ = a;
#endif
        }
      }
    } else {
      /* 1-bit alpha */
      for (PRInt32 y=0; y<height; y++) {
        if (stride > 0)
          target = data + stride * y;
        else
          target = data + stride * (1 - height) + stride * y;
        PRUint8 *alphaRow = alpha + y*abpr;
        
        for (PRUint32 x=0; x<width; x++) {
          if (NS_GET_BIT(alphaRow, x)) {
#ifdef XP_MACOSX
            *target++ = 255;
#endif
#ifndef REVERSE_CHANNELS
            *target++ = rgb[y*bpr + bpp*x];
            *target++ = rgb[y*bpr + bpp*x + 1];
            *target++ = rgb[y*bpr + bpp*x + 2];
#else
            *target++ = rgb[y*bpr + bpp*x + 2];
            *target++ = rgb[y*bpr + bpp*x + 1];
            *target++ = rgb[y*bpr + bpp*x];
#endif
#ifndef XP_MACOSX
            *target++ = 255;
#endif
          } else {
            *target++ = 0;
            *target++ = 0;
            *target++ = 0;
            *target++ = 0;
          }
        }
      }
    }
  }

#undef REVERSE_CHANNELS

#endif // MOZ_CAIRO_GFX
  
  mSurface->Unlock();
  aNewFrame->UnlockImageData();
  aNewFrame->UnlockAlphaData();
  
  return NS_OK;
}

// Lie about our fill/stroke so that hit detection works properly

NS_IMETHODIMP
nsSVGImageFrame::GetHittestMask(PRUint16 *aHittestMask)
{
  *aHittestMask=0;

  switch(GetStyleSVG()->mPointerEvents) {
    case NS_STYLE_POINTER_EVENTS_NONE:
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED:
      if (GetStyleVisibility()->IsVisible()) {
        /* XXX: should check pixel transparency */
        *aHittestMask |= HITTEST_MASK_FILL;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_VISIBLEFILL:
    case NS_STYLE_POINTER_EVENTS_VISIBLESTROKE:
    case NS_STYLE_POINTER_EVENTS_VISIBLE:
      if (GetStyleVisibility()->IsVisible()) {
        *aHittestMask |= HITTEST_MASK_FILL;
      }
      break;
    case NS_STYLE_POINTER_EVENTS_PAINTED:
      /* XXX: should check pixel transparency */
      *aHittestMask |= HITTEST_MASK_FILL;
      break;
    case NS_STYLE_POINTER_EVENTS_FILL:
    case NS_STYLE_POINTER_EVENTS_STROKE:
    case NS_STYLE_POINTER_EVENTS_ALL:
      *aHittestMask |= HITTEST_MASK_FILL;
      break;
    default:
      NS_ERROR("not reached");
      break;
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGImageListener implementation

NS_IMPL_ISUPPORTS2(nsSVGImageListener,
                   imgIDecoderObserver,
                   imgIContainerObserver)

nsSVGImageListener::nsSVGImageListener(nsSVGImageFrame *aFrame) :  mFrame(aFrame)
{
}

nsSVGImageListener::~nsSVGImageListener()
{
}

NS_IMETHODIMP nsSVGImageListener::OnStartRequest(imgIRequest *aRequest)
{
  return NS_OK;
}

NS_IMETHODIMP nsSVGImageListener::OnStartDecode(imgIRequest *aRequest)
{
  return NS_OK;
}

NS_IMETHODIMP nsSVGImageListener::OnStartContainer(imgIRequest *aRequest,
                                                   imgIContainer *aImage)
{
  return NS_OK;
}

NS_IMETHODIMP nsSVGImageListener::OnStartFrame(imgIRequest *aRequest,
                                            gfxIImageFrame *aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP nsSVGImageListener::OnDataAvailable(imgIRequest *aRequest,
                                                  gfxIImageFrame *aFrame,
                                                  const nsRect *aRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsSVGImageListener::OnStopFrame(imgIRequest *aRequest,
                                              gfxIImageFrame *aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP nsSVGImageListener::OnStopContainer(imgIRequest *aRequest,
                                                  imgIContainer *aImage)
{
  return NS_OK;
}

NS_IMETHODIMP nsSVGImageListener::OnStopDecode(imgIRequest *aRequest,
                                               nsresult status,
                                               const PRUnichar *statusArg)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  mFrame->mSurfaceInvalid = PR_TRUE;
  mFrame->UpdateGraphic();
  return NS_OK;
}

NS_IMETHODIMP nsSVGImageListener::OnStopRequest(imgIRequest *aRequest,
                                                PRBool aLastPart)
{
  return NS_OK;
}

NS_IMETHODIMP nsSVGImageListener::FrameChanged(imgIContainer *aContainer,
                                               gfxIImageFrame *newframe,
                                               nsRect * dirtyRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  mFrame->mSurfaceInvalid = PR_TRUE;
  mFrame->UpdateGraphic();
  return NS_OK;
}

