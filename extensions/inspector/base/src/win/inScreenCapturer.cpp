/* 
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

#include "inScreenCapturer.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIServiceManager.h"

#include "nsGfxCIID.h"
#include "nsIPresShell.h"
#include "nsIRenderingContext.h"
#include "nsIFrame.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindowInternal.h"
#include "nsIImage.h"
#include "nsIDeviceContext.h" 
#include "nsRect.h"

#include "inIBitmap.h"
#include "inLayoutUtils.h"

///////////////////////////////////////////////////////////////////////////////

inScreenCapturer::inScreenCapturer()
{
  NS_INIT_REFCNT();
}

inScreenCapturer::~inScreenCapturer()
{
}

NS_IMPL_ISUPPORTS1(inScreenCapturer, inIScreenCapturer);

///////////////////////////////////////////////////////////////////////////////
// inIScreenCapturer

NS_IMETHODIMP 
inScreenCapturer::CaptureElement(nsIDOMElement *aElement, inIBitmap **_retval)
{
  if (!aElement) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMWindowInternal> window = inLayoutUtils::GetWindowFor(aElement);
  if (!window) return NS_ERROR_FAILURE;
  nsCOMPtr<nsIPresShell> presShell = inLayoutUtils::GetPresShellFor(window);
  if (!presShell) return NS_ERROR_FAILURE;
  
  // get the dimensions of the element, which is the region we will be copying
  nsIFrame* frame = inLayoutUtils::GetFrameFor(aElement, presShell);
  if (!frame) return NS_ERROR_FAILURE;
  nsRect rect;
  frame->GetRect(rect);
  nsRect screenpos = inLayoutUtils::GetScreenOrigin(aElement);
  rect.x = screenpos.x;
  rect.y = screenpos.y;
  
  // adjust rect for margins
  inLayoutUtils::AdjustRectForMargins(aElement, rect);
  
  // get scale for converting frame dimensions to pixels
  nsCOMPtr<nsIPresContext> pcontext;
  presShell->GetPresContext(getter_AddRefs(pcontext));
  float t2p;
  pcontext->GetTwipsToPixels(&t2p);

  // convert twip values to pixels
  PRInt32 x = NSTwipsToIntPixels(rect.x, t2p);
  PRInt32 y = NSTwipsToIntPixels(rect.y, t2p);
  PRInt32 w = NSTwipsToIntPixels(rect.width, t2p);
  PRInt32 h = NSTwipsToIntPixels(rect.height, t2p);

  // capture that sucker!
  CaptureRegion(window, x, y, w, h, _retval);

  return NS_OK;
}

NS_IMETHODIMP 
inScreenCapturer::CaptureRegion(nsIDOMWindowInternal *aWindow,
                                PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, 
                                inIBitmap **_retval)
{
  // get the native device context
  HDC hdc = ::GetDC(nsnull);
  nsCOMPtr<nsIDeviceContext> dc;

  // determine pixel bit depth
  PRUint32 depth = ::GetDeviceCaps(hdc, COLORRES);

  nsCOMPtr<inIBitmap> bitmap(do_CreateInstance("@mozilla.org/inspector/bitmap;1"));
  if (!bitmap) return NS_ERROR_OUT_OF_MEMORY;
  
  bitmap->Init(aWidth, aHeight, depth);
  PRUint8* bits;
  bitmap->GetBits(&bits);
  
  if (depth == 8) {
    DoCopy8(bits, hdc, aX, aY, aWidth, aHeight);
  } else if (depth == 16) {
    DoCopy16(bits, hdc, aX, aY, aWidth, aHeight);
  } else if (depth == 32 || depth == 24) {
    DoCopy32(bits, hdc, aX, aY, aWidth, aHeight);
  }

  *_retval = bitmap;
  NS_ADDREF(*_retval);
  
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// inScreenCapturer

NS_IMETHODIMP 
inScreenCapturer::DoCopy32(PRUint8* aBits, HDC aHDC, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  PRUint8* bits = aBits;
  for (PRInt32 y = 0; y < aHeight; y++) {
    for (PRInt32 x = 0; x < aWidth; x++) {
      PRUint32 pixel = ::GetPixel(aHDC, aX+x,aY+y);
      *bits = NS_GET_B(pixel);
      *(bits+1) = NS_GET_G(pixel);
      *(bits+2) = NS_GET_R(pixel);
      bits += 3;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
inScreenCapturer::DoCopy8(PRUint8* aBits, HDC aHDC, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
inScreenCapturer::DoCopy16(PRUint8* aBits, HDC aHDC, PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
