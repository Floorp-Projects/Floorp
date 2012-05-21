/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* class to notify frames of background and border image loads */

#include "nsStubImageDecoderObserver.h"

class nsIFrame;
class nsIURI;

#include "imgIRequest.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"

/**
 * Image loaders pass notifications for background and border image
 * loading and animation on to the frames.
 *
 * Each frame's image loaders form a linked list.
 */
class nsImageLoader : public nsStubImageDecoderObserver
{
private:
  nsImageLoader(nsIFrame *aFrame, PRUint32 aActions,
                nsImageLoader *aNextLoader);
  virtual ~nsImageLoader();

public:
  /*
   * Flags to specify actions that can be taken for the image at various
   * times. Reflows always occur before redraws. "Decode" refers to one
   * frame being available, whereas "load" refers to all the data being loaded
   * from the network.
   */
  enum {
    ACTION_REDRAW_ON_DECODE = 0x01,
    ACTION_REDRAW_ON_LOAD   = 0x02,
  };
  static already_AddRefed<nsImageLoader>
    Create(nsIFrame *aFrame, imgIRequest *aRequest,
           PRUint32 aActions, nsImageLoader *aNextLoader);

  NS_DECL_ISUPPORTS

  // imgIDecoderObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage);
  NS_IMETHOD OnStopFrame(imgIRequest *aRequest, PRUint32 aFrame);
  NS_IMETHOD OnStopRequest(imgIRequest *aRequest, bool aLastPart);
  NS_IMETHOD OnImageIsAnimated(imgIRequest *aRequest);
  // Do not override OnDataAvailable since background images are not
  // displayed incrementally; they are displayed after the entire image
  // has been loaded.
  // Note: Images referenced by the <img> element are displayed
  // incrementally in nsImageFrame.cpp.

  // imgIContainerObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD FrameChanged(imgIRequest *aRequest,
                          imgIContainer *aContainer,
                          const nsIntRect *aDirtyRect);

  void Destroy();

  imgIRequest *GetRequest() { return mRequest; }
  nsImageLoader *GetNextLoader() { return mNextLoader; }

private:
  nsresult Load(imgIRequest *aImage);
  /* if aDamageRect is nsnull, the whole frame is redrawn. */
  void DoRedraw(const nsRect* aDamageRect);

  nsIFrame *mFrame;
  nsCOMPtr<imgIRequest> mRequest;
  PRUint32 mActions;
  nsRefPtr<nsImageLoader> mNextLoader;

  // This is a boolean flag indicating whether or not the current image request
  // has been registered with the refresh driver.
  bool mRequestRegistered;
};
