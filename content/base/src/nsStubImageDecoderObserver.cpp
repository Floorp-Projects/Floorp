/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStubImageDecoderObserver.h"

NS_IMETHODIMP
nsStubImageDecoderObserver::OnStartRequest(imgIRequest *aRequest)
{
    return NS_OK;
}

NS_IMETHODIMP
nsStubImageDecoderObserver::OnStartDecode(imgIRequest *aRequest)
{
    return NS_OK;
}

NS_IMETHODIMP
nsStubImageDecoderObserver::OnStartContainer(imgIRequest *aRequest,
                                             imgIContainer *aContainer)
{
    return NS_OK;
}

NS_IMETHODIMP
nsStubImageDecoderObserver::OnStartFrame(imgIRequest *aRequest,
                                         PRUint32 aFrame)
{
    return NS_OK;
}

NS_IMETHODIMP
nsStubImageDecoderObserver::OnDataAvailable(imgIRequest *aRequest,
                                            bool aCurrentFrame,
                                            const nsIntRect * aRect)
{
    return NS_OK;
}

NS_IMETHODIMP
nsStubImageDecoderObserver::OnStopFrame(imgIRequest *aRequest,
                                        PRUint32 aFrame)
{
    return NS_OK;
}

NS_IMETHODIMP
nsStubImageDecoderObserver::OnStopContainer(imgIRequest *aRequest,
                                            imgIContainer *aContainer)
{
    return NS_OK;
}

NS_IMETHODIMP
nsStubImageDecoderObserver::OnStopDecode(imgIRequest *aRequest,
                                         nsresult status,
                                         const PRUnichar *statusArg)
{
    return NS_OK;
}

NS_IMETHODIMP
nsStubImageDecoderObserver::OnStopRequest(imgIRequest *aRequest, 
                                          bool aIsLastPart)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsStubImageDecoderObserver::OnDiscard(imgIRequest *aRequest)
{
  return NS_OK;
}

NS_IMETHODIMP
nsStubImageDecoderObserver::OnImageIsAnimated(imgIRequest *aRequest)
{
  return NS_OK;
}

NS_IMETHODIMP
nsStubImageDecoderObserver::FrameChanged(imgIRequest* aRequest,
                                         imgIContainer *aContainer,
                                         const nsIntRect *aDirtyRect)
{
    return NS_OK;
}
