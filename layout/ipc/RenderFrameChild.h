/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RenderFrameChild_h
#define mozilla_dom_RenderFrameChild_h

#include "mozilla/layout/PRenderFrameChild.h"

namespace mozilla {
namespace layout {

class RenderFrameChild : public PRenderFrameChild
{
public:
  RenderFrameChild() {}
  virtual ~RenderFrameChild() {}

  void CancelDefaultPanZoom();

  void Destroy();

protected:
  virtual PLayersChild* AllocPLayers() MOZ_OVERRIDE;
  virtual bool DeallocPLayers(PLayersChild* aLayers) MOZ_OVERRIDE;
};

} // namespace layout
} // namespace mozilla

#endif  // mozilla_dom_RenderFrameChild_h
