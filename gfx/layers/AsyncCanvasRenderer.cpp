/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncCanvasRenderer.h"

#include "gfxUtils.h"
#include "GLContext.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/layers/CanvasClient.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

AsyncCanvasRenderer::AsyncCanvasRenderer()
  : mWidth(0)
  , mHeight(0)
  , mCanvasClientAsyncID(0)
  , mCanvasClient(nullptr)
{
  MOZ_COUNT_CTOR(AsyncCanvasRenderer);
}

AsyncCanvasRenderer::~AsyncCanvasRenderer()
{
  MOZ_COUNT_DTOR(AsyncCanvasRenderer);
}

void
AsyncCanvasRenderer::NotifyElementAboutAttributesChanged()
{
  class Runnable final : public nsRunnable
  {
  public:
    explicit Runnable(AsyncCanvasRenderer* aRenderer)
      : mRenderer(aRenderer)
    {}

    NS_IMETHOD Run()
    {
      if (mRenderer) {
        dom::HTMLCanvasElement::SetAttrFromAsyncCanvasRenderer(mRenderer);
      }

      return NS_OK;
    }

    void Revoke()
    {
      mRenderer = nullptr;
    }

  private:
    nsRefPtr<AsyncCanvasRenderer> mRenderer;
  };

  nsRefPtr<nsRunnable> runnable = new Runnable(this);
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch a runnable to the main-thread.");
  }
}

void
AsyncCanvasRenderer::SetCanvasClient(CanvasClient* aClient)
{
  mCanvasClient = aClient;
  if (aClient) {
    mCanvasClientAsyncID = aClient->GetAsyncID();
  } else {
    mCanvasClientAsyncID = 0;
  }
}

} // namespace layers
} // namespace mozilla
