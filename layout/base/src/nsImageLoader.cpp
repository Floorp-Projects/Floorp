/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsImageLoader.h"

#include "imgILoader.h"

#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"

#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"

#include "imgIContainer.h"

#include "nsIHTMLContent.h"

#include "nsIViewManager.h"

#include "nsIStyleContext.h"

NS_IMPL_ISUPPORTS2(nsImageLoader, imgIDecoderObserver, imgIContainerObserver)

nsImageLoader::nsImageLoader() :
  mFrame(nsnull), mPresContext(nsnull)
{
}

nsImageLoader::~nsImageLoader()
{
  mFrame = nsnull;
  mPresContext = nsnull;

  if (mRequest) {
    mRequest->Cancel(NS_ERROR_FAILURE);
  }
}


void
nsImageLoader::Init(nsIFrame *aFrame, nsIPresContext *aPresContext)
{
  mFrame = aFrame;
  mPresContext = aPresContext;
}

void
nsImageLoader::Destroy()
{
  mFrame = nsnull;
  mPresContext = nsnull;

  if (mRequest) {
    mRequest->Cancel(NS_ERROR_FAILURE);
  }

  mRequest = nsnull;
}

nsresult
nsImageLoader::Load(nsIURI *aURI)
{
  if (!mFrame)
    return NS_ERROR_NOT_INITIALIZED;

  if (!aURI)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsILoadGroup> loadGroup;

  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = mPresContext->GetShell(getter_AddRefs(shell));
  if ((NS_FAILED(rv)) || (!shell)) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocument> doc;
  rv = shell->GetDocument(getter_AddRefs(doc));
  if (NS_FAILED(rv)) return rv;

  // Get the document's loadgroup
  doc->GetDocumentLoadGroup(getter_AddRefs(loadGroup));

  // Get the document URI (for the referrer).
  nsCOMPtr<nsIURI> documentURI;
  doc->GetDocumentURL(getter_AddRefs(documentURI));

  if (mRequest) {
    nsCOMPtr<nsIURI> oldURI;
    mRequest->GetURI(getter_AddRefs(oldURI));
    PRBool eq = PR_FALSE;
    aURI->Equals(oldURI, &eq);
    if (eq) {
      return NS_OK;
    }
  }

  nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1", &rv));
  if (NS_FAILED(rv)) return rv;

  // XXX: initialDocumentURI is NULL!
  return il->LoadImage(aURI, nsnull, documentURI, loadGroup, NS_STATIC_CAST(imgIDecoderObserver *, this), 
                       nsnull, nsIRequest::LOAD_BACKGROUND, nsnull, nsnull, getter_AddRefs(mRequest));
}

                    

NS_IMETHODIMP nsImageLoader::OnStartDecode(imgIRequest *aRequest, nsISupports *aContext)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::OnStartContainer(imgIRequest *aRequest, nsISupports *aContext, imgIContainer *aImage)
{
  if (aImage)
  {
    /* Get requested animation policy from the pres context:
     *   normal = 0
     *   one frame = 1
     *   one loop = 2
     */
    PRUint16 animateMode = imgIContainer::kNormalAnimMode; //default value
    nsresult rv = mPresContext->GetImageAnimationMode(&animateMode);
    if (NS_SUCCEEDED(rv))
      aImage->SetAnimationMode(animateMode);
  }
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::OnStartFrame(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::OnDataAvailable(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame, const nsRect *aRect)
{
  // Background images are not displayed incrementally, they are displayed after the entire 
  // image has been loaded.
  // Note: Images referenced by the <img> element are displayed incrementally in nsImageFrame.cpp
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::OnStopFrame(imgIRequest *aRequest, nsISupports *aContext, gfxIImageFrame *aFrame)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;
  
#ifdef NS_DEBUG
// Make sure the image request status's STATUS_FRAME_COMPLETE flag has been set to ensure
// the image will be painted when invalidated
  if (aRequest) {
   PRUint32 status = imgIRequest::STATUS_ERROR;
   nsresult rv = aRequest->GetImageStatus(&status);
   if (NS_SUCCEEDED(rv)) {
     NS_ASSERTION((status & imgIRequest::STATUS_FRAME_COMPLETE), "imgIRequest::STATUS_FRAME_COMPLETE not set");
   }
  }
#endif

  // Draw the background image
  RedrawDirtyFrame(nsnull);
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::OnStopContainer(imgIRequest *aRequest, nsISupports *aContext, imgIContainer *aImage)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::OnStopDecode(imgIRequest *aRequest, nsISupports *aContext, nsresult status, const PRUnichar *statusArg)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageLoader::FrameChanged(imgIContainer *aContainer, nsISupports *aContext, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  if (!mFrame)
    return NS_ERROR_FAILURE;

  nsRect r(*dirtyRect);

  float p2t;
  mPresContext->GetPixelsToTwips(&p2t);
  r.x = NSIntPixelsToTwips(r.x, p2t);
  r.y = NSIntPixelsToTwips(r.y, p2t);
  r.width = NSIntPixelsToTwips(r.width, p2t);
  r.height = NSIntPixelsToTwips(r.height, p2t);

  RedrawDirtyFrame(&r);

  return NS_OK;
}


void
nsImageLoader::RedrawDirtyFrame(const nsRect* aDamageRect)
{
  // Determine damaged area and tell view manager to redraw it
  nsPoint offset;
  nsRect bounds;
  nsIView* view;

  // NOTE: It is not sufficient to invalidate only the size of the image:
  //       the image may be tiled! 
  //       The best option is to call into the frame, however lacking this
  //       we have to at least invalidate the frame's bounds, hence
  //       as long as we have a frame we'll use its size.
  //

  // Invalidate the entire frame
  // XXX We really only need to invalidate the client area of the frame...    
  mFrame->GetRect(bounds);
  bounds.x = bounds.y = 0;

  // XXX this should be ok, but there is some crappy ass bug causing it not to work
  // XXX seems related to the "body fixup rule" dealing with the canvas and body frames...
#if 0
  // Invalidate the entire frame only if the frame has a tiled background
  // image, otherwise just invalidate the intersection of the frame's bounds
  // with the damaged rect.
  nsCOMPtr<nsIStyleContext> styleContext;
  mFrame->GetStyleContext(getter_AddRefs(styleContext));
  const nsStyleBackground* bg = (const nsStyleBackground*)styleContext->GetStyleData(eStyleStruct_Background);

  if ((bg->mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE) ||
      (bg->mBackgroundRepeat == NS_STYLE_BG_REPEAT_OFF)) {
    // The frame does not have a background image so we are free
    // to invalidate only the intersection of the damage rect and
    // the frame's bounds.

    if (aDamageRect) {
      bounds.IntersectRect(*aDamageRect, bounds);
    }
  }

#endif
  if ((bounds.width > 0) && (bounds.height > 0)) {

    // XXX We should tell the frame the damage area and let it invalidate
    // itself. Add some API calls to nsIFrame to allow a caller to invalidate
    // parts of the frame...
    mFrame->GetView(mPresContext, &view);
    if (!view) {
      mFrame->GetOffsetFromView(mPresContext, offset, &view);
      bounds.x += offset.x;
      bounds.y += offset.y;
    }

    nsCOMPtr<nsIViewManager> vm = nsnull;
    nsresult rv = NS_OK;
    rv = view->GetViewManager(*getter_AddRefs(vm));
    if (NS_SUCCEEDED(rv) && vm) {
      vm->UpdateView(view, bounds, NS_VMREFRESH_NO_SYNC);    
    }
  }

}
