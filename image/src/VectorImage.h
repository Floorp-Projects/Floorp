/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_imagelib_VectorImage_h_
#define mozilla_imagelib_VectorImage_h_

#include "Image.h"
#include "nsIStreamListener.h"
#include "nsWeakReference.h"
#include "mozilla/TimeStamp.h"

class imgIDecoderObserver;

namespace mozilla {
namespace layers {
class LayerManager;
class ImageContainer;
}
namespace image {

class SVGDocumentWrapper;
class SVGRootRenderingObserver;

class VectorImage : public Image,
                    public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  // BEGIN NS_DECL_IMGICONTAINER (minus GetAnimationMode/SetAnimationMode)
  // ** Don't edit this chunk except to mirror changes in imgIContainer.idl **
  NS_IMETHOD GetWidth(PRInt32 *aWidth);
  NS_IMETHOD GetHeight(PRInt32 *aHeight);
  NS_IMETHOD GetType(PRUint16 *aType);
  NS_IMETHOD_(PRUint16) GetType(void);
  NS_IMETHOD GetAnimated(bool *aAnimated);
  NS_IMETHOD GetCurrentFrameIsOpaque(bool *aCurrentFrameIsOpaque);
  NS_IMETHOD GetFrame(PRUint32 aWhichFrame, PRUint32 aFlags, gfxASurface **_retval);
  NS_IMETHOD GetImageContainer(mozilla::layers::ImageContainer **_retval) { *_retval = NULL; return NS_OK; }
  NS_IMETHOD CopyFrame(PRUint32 aWhichFrame, PRUint32 aFlags, gfxImageSurface **_retval);
  NS_IMETHOD ExtractFrame(PRUint32 aWhichFrame, const nsIntRect & aRect, PRUint32 aFlags, imgIContainer **_retval);
  NS_IMETHOD Draw(gfxContext *aContext, gfxPattern::GraphicsFilter aFilter, const gfxMatrix & aUserSpaceToImageSpace, const gfxRect & aFill, const nsIntRect & aSubimage, const nsIntSize & aViewportSize, PRUint32 aFlags);
  NS_IMETHOD_(nsIFrame *) GetRootLayoutFrame(void);
  NS_IMETHOD RequestDecode(void);
  NS_IMETHOD LockImage(void);
  NS_IMETHOD UnlockImage(void);
  NS_IMETHOD RequestDiscard(void);
  NS_IMETHOD ResetAnimation(void);
  NS_IMETHOD_(void) RequestRefresh(const mozilla::TimeStamp& aTime);
  // END NS_DECL_IMGICONTAINER

  VectorImage(imgStatusTracker* aStatusTracker = nsnull);
  virtual ~VectorImage();

  // Methods inherited from Image
  nsresult Init(imgIDecoderObserver* aObserver,
                const char* aMimeType,
                const char* aURIString,
                PRUint32 aFlags);
  void GetCurrentFrameRect(nsIntRect& aRect);

  virtual size_t HeapSizeOfSourceWithComputedFallback(nsMallocSizeOfFun aMallocSizeOf) const;
  virtual size_t HeapSizeOfDecodedWithComputedFallback(nsMallocSizeOfFun aMallocSizeOf) const;
  virtual size_t NonHeapSizeOfDecoded() const;
  virtual size_t OutOfProcessSizeOfDecoded() const;

  // Callback for SVGRootRenderingObserver
  void InvalidateObserver();

protected:
  virtual nsresult StartAnimation();
  virtual nsresult StopAnimation();
  virtual bool     ShouldAnimate();

private:
  nsWeakPtr                          mObserver;   //! imgIDecoderObserver
  nsRefPtr<SVGDocumentWrapper>       mSVGDocumentWrapper;
  nsRefPtr<SVGRootRenderingObserver> mRenderingObserver;

  nsIntRect      mRestrictedRegion;       // If we were created by
                                          // ExtractFrame, this is the region
                                          // that we're restricted to using.
                                          // Otherwise, this is ignored.

  nsIntSize      mLastRenderedSize;       // The viewport-size that we've
                                          // most recently passed to
                                          // mSVGDocumentWrapper as its
                                          // viewport-bounds.

  bool           mIsInitialized:1;        // Have we been initalized?
  bool           mIsFullyLoaded:1;        // Has OnStopRequest been called?
  bool           mIsDrawing:1;            // Are we currently drawing?
  bool           mHaveAnimations:1;       // Is our SVG content SMIL-animated?
                                          // (Only set after mIsFullyLoaded.)
  bool           mHaveRestrictedRegion:1; // Are we a restricted-region clone
                                          // created via ExtractFrame?
};

} // namespace image
} // namespace mozilla

#endif // mozilla_imagelib_VectorImage_h_
