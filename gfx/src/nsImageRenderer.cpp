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

#include "libimg.h"
#include "ilIImageRenderer.h"
#include "nsIImage.h"
#include "nsIRenderingContext.h"
#include "ni_pixmp.h"
#include "il_util.h"
#include "nsGfxCIID.h"
#include "nsIDeviceContext.h"

static NS_DEFINE_IID(kIImageRendererIID, IL_IIMAGERENDERER_IID);

class ImageRendererImpl : public ilIImageRenderer {
public:
  ImageRendererImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD NewPixmap(void* aDisplayContext, 
			                   PRInt32 aWidth, PRInt32 aHeight, 
			                   IL_Pixmap* aImage, IL_Pixmap* aMask);

  NS_IMETHOD UpdatePixmap(void* aDisplayContext, 
			                      IL_Pixmap* aImage, 
			                      PRInt32 aXOffset, PRInt32 aYOffset, 
			                      PRInt32 aWidth, PRInt32 aHeight);


  NS_IMETHOD ControlPixmapBits(void* aDisplayContext, 
				                         IL_Pixmap* aImage, PRUint32 aControlMsg);

  NS_IMETHOD DestroyPixmap(void* aDisplayContext, IL_Pixmap* aImage);
  
  NS_IMETHOD DisplayPixmap(void* aDisplayContext, 
                  			     IL_Pixmap* aImage, IL_Pixmap* aMask, 
                  			     PRInt32 aX, PRInt32 aY, 
                  			     PRInt32 aXOffset, PRInt32 aYOffset, 
                  			     PRInt32 aWidth, PRInt32 aHeight);

  NS_IMETHOD DisplayIcon(void* aDisplayContext, 
			                     PRInt32 aX, PRInt32 aY, PRUint32 aIconNumber);

  NS_IMETHOD GetIconDimensions(void* aDisplayContext, 
                        				 PRInt32 *aWidthPtr, PRInt32 *aHeightPtr, 
                        				 PRUint32 aIconNumber);

  NS_IMETHOD SetImageNaturalDimensions(IL_Pixmap* aImage, PRInt32 naturalwidth, PRInt32 naturalheight);


  NS_IMETHOD SetDecodedRect(IL_Pixmap* aImage, 
			                    PRInt32 x1, PRInt32 y1,
                                PRInt32 x2, PRInt32 y2);


};

NS_IMETHODIMP
ImageRendererImpl::SetImageNaturalDimensions(
                   IL_Pixmap* aImage, 
                   PRInt32 naturalwidth, 
                   PRInt32 naturalheight){

    nsIImage *img = (nsIImage *)aImage->client_data;

    if(img){
        nsresult rv = img->SetNaturalWidth(naturalwidth);
        rv = img->SetNaturalHeight(naturalheight);
    }
    return NS_OK;
}
ImageRendererImpl::ImageRendererImpl()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(ImageRendererImpl, kIImageRendererIID)


NS_IMETHODIMP
ImageRendererImpl::NewPixmap(void* aDisplayContext, 
			                       PRInt32 aWidth, PRInt32 aHeight, 
                      	     IL_Pixmap* aImage, IL_Pixmap* aMask)
{
  nsIDeviceContext *dc = (nsIDeviceContext *)aDisplayContext;
  nsIImage  *img;
  nsresult  rv;
  nsMaskRequirements maskflag;

  static NS_DEFINE_IID(kImageCID, NS_IMAGE_CID);
  static NS_DEFINE_IID(kImageIID, NS_IIMAGE_IID);

 if (!aImage)
    return NS_ERROR_NULL_POINTER;
  
  // initialize in case of failure
  NS_ASSERTION(!aImage->bits, "We have bits already?");
  aImage->bits = nsnull;
  aImage->haveBits = PR_FALSE;
  if (aMask)
  {
    aMask->haveBits = PR_FALSE;
    aMask->bits = nsnull;
  }

  // Create a new image object
  rv = nsComponentManager::CreateInstance(kImageCID, nsnull, kImageIID, (void **)&img);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Have the image match the depth and color space associated with the
  // device.
  // XXX We probably don't want to do that for monomchrome images (e.g., XBM)
  // or one-bit deep GIF images.
  PRInt32 depth;
  IL_ColorSpace *colorSpace;

  rv = dc->GetILColorSpace(colorSpace);
  if (NS_FAILED(rv)) {
    return rv;
  }
  depth = colorSpace->pixmap_depth;

  // Initialize the image object
 
  if(aMask == nsnull) 
    maskflag = nsMaskRequirements_kNoMask; 
  else
    maskflag = nsMaskRequirements_kNeeds1Bit;

  if(aImage->header.alpha_bits == 8)
      maskflag = nsMaskRequirements_kNeeds8Bit;

  rv = img->Init(aWidth, aHeight, depth, maskflag);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Update the pixmap image and mask information

  // Don't get the bits here, because we can't guarantee that this address
  // will still be valid when we start using it. We *must* wait until we're
  // inside a lock/unlocks pixels block before getting the bits address
  // aImage->bits = img->GetBits(); 
  aImage->haveBits = PR_TRUE;

  aImage->client_data = img;  // we don't need to add a ref here, because there's
                              // already one from the call to create the image object
 
  aImage->header.width = aWidth;
  aImage->header.height = aHeight;
  aImage->header.widthBytes = img->GetLineStride();

  if (aMask) {
    // see comment about about getting the bits here
    // aMask->bits = img->GetAlphaBits();
    aMask->haveBits = PR_TRUE;

    aMask->client_data = img;
    // We must add another reference here, because when the mask's pixmap is
    // destroyed it will release a reference
    NS_ADDREF(img);
    aMask->header.width = aWidth;
    aMask->header.height = aHeight;
    aMask->header.widthBytes = img->GetAlphaLineStride();
  }

  // Replace the existing color space with the color space associated
  // with the device.
  IL_ReleaseColorSpace(aImage->header.color_space);
  aImage->header.color_space = colorSpace;

  // XXX Why do we do this on a per-image basis?
  if (8 == depth) {
    IL_ColorMap *cmap = &colorSpace->cmap;
    nsColorMap *nscmap = img->GetColorMap();
    PRUint8 *mapptr = nscmap->Index;
    int i;
                
    for (i=0; i < cmap->num_colors; i++) {
      *mapptr++ = cmap->map[i].red;
      *mapptr++ = cmap->map[i].green;
      *mapptr++ = cmap->map[i].blue;
    }

    img->ImageUpdated(dc, nsImageUpdateFlags_kColorMapChanged, nsnull);
                
    if (aImage->header.transparent_pixel) {
      PRUint8 red, green, blue;
      PRUint8 *lookup_table = (PRUint8 *)aImage->header.color_space->cmap.table;
      red = aImage->header.transparent_pixel->red;
      green = aImage->header.transparent_pixel->green;
      blue = aImage->header.transparent_pixel->blue;
      aImage->header.transparent_pixel->index = lookup_table[((red >> 3) << 10) |
                                                             ((green >> 3) << 5) |
                                                             (blue >> 3)];
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
ImageRendererImpl::UpdatePixmap(void* aDisplayContext, 
				                        IL_Pixmap* aImage, 
				                        PRInt32 aXOffset, PRInt32 aYOffset, 
				                        PRInt32 aWidth, PRInt32 aHeight)
{
  nsIDeviceContext *dc = (nsIDeviceContext *)aDisplayContext;
  nsIImage         *img = (nsIImage *)aImage->client_data;
  nsRect            drect(aXOffset, aYOffset, aWidth, aHeight);

  img->ImageUpdated(dc, nsImageUpdateFlags_kBitsChanged, &drect);
  return NS_OK;
}

NS_IMETHODIMP
ImageRendererImpl::SetDecodedRect(   IL_Pixmap* aImage, 
				                PRInt32 x1, PRInt32 y1,
                                PRInt32 x2, PRInt32 y2)
{

  nsIImage         *img;
  
  if(aImage == NULL)
      return NS_OK;

  img= (nsIImage *)aImage->client_data;
  img->SetDecodedRect(x1, y1, x2, y2);

  return NS_OK;
}


NS_IMETHODIMP
ImageRendererImpl::ControlPixmapBits(void* aDisplayContext, 
				                             IL_Pixmap* aImage, PRUint32 aControlMsg)
{
  nsIDeviceContext *dc = (nsIDeviceContext *)aDisplayContext;
  
  if (!aImage) 
      return NS_ERROR_NULL_POINTER;
  
  nsIImage *img = (nsIImage *)aImage->client_data;
  if (!img) 
      return NS_ERROR_UNEXPECTED;

  PRBool   isMask = aImage->header.is_mask;
  nsresult rv = NS_OK;
      
  switch (aControlMsg)
  {
    case IL_LOCK_BITS:
      rv = img->LockImagePixels(isMask);
      if (NS_FAILED(rv)) 
          return rv;
      // the pixels may have moved, so need to update the bits ptr
      aImage->bits = (isMask) ? img->GetAlphaBits() : img->GetBits();
      break;
      
    case IL_UNLOCK_BITS:
      rv = img->UnlockImagePixels(isMask);
      break;
      
    case IL_RELEASE_BITS:
      rv = img->Optimize(dc);
      break;
      
    default:
      NS_NOTREACHED("Uknown control msg");
  }

  return rv;
}

NS_IMETHODIMP
ImageRendererImpl::DestroyPixmap(void* aDisplayContext, IL_Pixmap* aImage)
{
  nsIImage *img = (nsIImage *)aImage->client_data;

  aImage->client_data = nsnull;
  if (img) {
    NS_RELEASE(img);
  }
  return NS_OK;
}
  

NS_IMETHODIMP
ImageRendererImpl::DisplayPixmap(void* aDisplayContext, 
				                         IL_Pixmap* aImage, IL_Pixmap* aMask, 
                        				 PRInt32 aX, PRInt32 aY, 
                        				 PRInt32 aXOffset, PRInt32 aYOffset, 
                        				 PRInt32 aWidth, PRInt32 aHeight)
{
  // Image library doesn't drive the display process.
  // XXX Why is this part of the API?
      return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP 
ImageRendererImpl::DisplayIcon(void* aDisplayContext, 
			                         PRInt32 aX, PRInt32 aY, PRUint32 aIconNumber)
{
  // XXX Why is this part of the API?
      return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
ImageRendererImpl::GetIconDimensions(void* aDisplayContext, 
				                             PRInt32 *aWidthPtr, PRInt32 *aHeightPtr, 
                        				     PRUint32 aIconNumber)
{
  // XXX Why is this part of the API?
      return NS_ERROR_NOT_IMPLEMENTED;
}

extern "C" NS_GFX_(nsresult)
NS_NewImageRenderer(ilIImageRenderer  **aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  ilIImageRenderer *renderer = new ImageRendererImpl();
  if (renderer == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return renderer->QueryInterface(kIImageRendererIID, (void **)aInstancePtrResult);
}
