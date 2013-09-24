/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layer_RenderFrameUtils_h
#define mozilla_layer_RenderFrameUtils_h

#include "ipc/IPCMessageUtils.h"

namespace mozilla {
namespace layout {

enum ScrollingBehavior {
  /**
   * Use default scrolling behavior, which is synchronous: web content
   * is reflowed and repainted for every scroll or zoom.
   */
  DEFAULT_SCROLLING,
  /**
   * Use asynchronous panning and zooming, in which prerendered
   * content can be translated and scaled independently of the thread
   * painting content, without content reflowing or repainting.
   */
  ASYNC_PAN_ZOOM,
  SCROLLING_BEHAVIOR_SENTINEL
};

} // namespace layout
} // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::layout::ScrollingBehavior>
  : public EnumSerializer<mozilla::layout::ScrollingBehavior,
                          mozilla::layout::DEFAULT_SCROLLING,
                          mozilla::layout::SCROLLING_BEHAVIOR_SENTINEL>
{};

} // namespace IPC

#endif // mozilla_layer_RenderFrameUtils_h
