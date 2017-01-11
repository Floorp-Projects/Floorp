/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=99 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_WEBRENDERAPI_H
#define MOZILLA_LAYERS_WEBRENDERAPI_H

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/gfx/webrender.h"
#include "mozilla/layers/WebRenderTypes.h"

namespace mozilla {

namespace widget {
class CompositorWidget;
}

namespace layers {

class CompositorBridgeParentBase;
class RendererOGL;

class WebRenderAPI
{
NS_INLINE_DECL_REFCOUNTING(WebRenderAPI);
public:
  /// Compositor thread only.
  static already_AddRefed<WebRenderAPI> Create(bool aEnableProfiler,
                                               CompositorBridgeParentBase* aBridge,
                                               RefPtr<widget::CompositorWidget>&& aWidget);

  gfx::WindowId GetId() const { return mId; }

protected:
  WebRenderAPI(WrAPI* aRawApi, gfx::WindowId aId)
  : mApi(aRawApi)
  , mId(aId)
  {}

  ~WebRenderAPI();

  WrAPI* mApi;
  gfx::WindowId mId;
};

} // namespace
} // namespace

#endif