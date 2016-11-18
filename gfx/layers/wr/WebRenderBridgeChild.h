/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_WebRenderBridgeChild_h
#define mozilla_layers_WebRenderBridgeChild_h

#include "mozilla/layers/PWebRenderBridgeChild.h"

namespace mozilla {

namespace widget {
class CompositorWidget;
}

namespace layers {

class WebRenderBridgeParent;

class WebRenderBridgeChild final : public PWebRenderBridgeChild
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebRenderBridgeChild)

public:
  explicit WebRenderBridgeChild(const uint64_t& aPipelineId);

  void AddWebRenderCommand(const WebRenderCommand& aCmd);

  bool DPBegin(uint32_t aWidth, uint32_t aHeight);
  void DPEnd();
protected:
  ~WebRenderBridgeChild() {}

  nsTArray<WebRenderCommand> mCommands;
  bool mIsInTransaction;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_WebRenderBridgeChild_h
