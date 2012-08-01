/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginUtilsOSX_h
#define dom_plugins_PluginUtilsOSX_h 1

#include "npapi.h"
#include "nsRect.h"
#include "mozilla/gfx/QuartzSupport.h"

namespace mozilla {
namespace plugins {
namespace PluginUtilsOSX {

// Need to call back into the browser's message loop to process event.
typedef void (*RemoteProcessEvents) (void*);

NPError ShowCocoaContextMenu(void* aMenu, int aX, int aY, void* pluginModule, RemoteProcessEvents remoteEvent);

void InvokeNativeEventLoop();

// Need to call back and send a cocoa draw event to the plugin.
typedef void (*DrawPluginFunc) (CGContextRef, void*, nsIntRect aUpdateRect);

void* GetCGLayer(DrawPluginFunc aFunc, void* aPluginInstance);
void ReleaseCGLayer(void* cgLayer);
void Repaint(void* cgLayer, nsIntRect aRect);

bool SetProcessName(const char* aProcessName);

/*
 * Provides a wrapper around nsCARenderer to manage double buffering
 * without having to unbind nsCARenderer on every surface swaps.
 *
 * The double buffer renderer begins with no initialize surfaces.
 * The buffers can be initialized and cleared individually.
 * Swapping still occurs regardless if the buffers are initialized.
 */
class THEBES_API nsDoubleBufferCARenderer {
public:
  nsDoubleBufferCARenderer() : mCALayer(nullptr) {}
  size_t GetFrontSurfaceWidth();
  size_t GetFrontSurfaceHeight();
  size_t GetBackSurfaceWidth();
  size_t GetBackSurfaceHeight();
  IOSurfaceID GetFrontSurfaceID();

  bool HasBackSurface();
  bool HasFrontSurface();
  bool HasCALayer();

  void SetCALayer(void *aCALayer);
  bool InitFrontSurface(size_t aWidth, size_t aHeight, AllowOfflineRendererEnum aAllowOfflineRenderer);
  void Render();
  void SwapSurfaces();
  void ClearFrontSurface();
  void ClearBackSurface();

private:
  void *mCALayer;
  RefPtr<nsCARenderer> mCARenderer;
  RefPtr<MacIOSurface> mFrontSurface;
  RefPtr<MacIOSurface> mBackSurface;
};

} // namespace PluginUtilsOSX
} // namespace plugins
} // namespace mozilla

#endif //dom_plugins_PluginUtilsOSX_h
