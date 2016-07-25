/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginUtilsOSX_h
#define dom_plugins_PluginUtilsOSX_h 1

#include "npapi.h"
#include "mozilla/gfx/QuartzSupport.h"
#include "nsRect.h"

namespace mozilla {
namespace plugins {
namespace PluginUtilsOSX {

// Need to call back into the browser's message loop to process event.
typedef void (*RemoteProcessEvents) (void*);

NPError ShowCocoaContextMenu(void* aMenu, int aX, int aY, void* pluginModule, RemoteProcessEvents remoteEvent);

void InvokeNativeEventLoop();

// Need to call back and send a cocoa draw event to the plugin.
typedef void (*DrawPluginFunc) (CGContextRef, void*, nsIntRect aUpdateRect);

void* GetCGLayer(DrawPluginFunc aFunc, void* aPluginInstance, double aContentsScaleFactor);
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
class nsDoubleBufferCARenderer {
public:
  nsDoubleBufferCARenderer() : mCALayer(nullptr), mContentsScaleFactor(1.0) {}
  // Returns width in "display pixels".  A "display pixel" is the smallest
  // fully addressable part of a display.  But in HiDPI modes each "display
  // pixel" corresponds to more than one device pixel.  Multiply display pixels
  // by mContentsScaleFactor to get device pixels.
  size_t GetFrontSurfaceWidth();
  // Returns height in "display pixels".  Multiply by
  // mContentsScaleFactor to get device pixels.
  size_t GetFrontSurfaceHeight();
  double GetFrontSurfaceContentsScaleFactor();
  // Returns width in "display pixels".  Multiply by
  // mContentsScaleFactor to get device pixels.
  size_t GetBackSurfaceWidth();
  // Returns height in "display pixels".  Multiply by
  // mContentsScaleFactor to get device pixels.
  size_t GetBackSurfaceHeight();
  double GetBackSurfaceContentsScaleFactor();
  IOSurfaceID GetFrontSurfaceID();

  bool HasBackSurface();
  bool HasFrontSurface();
  bool HasCALayer();

  void SetCALayer(void *aCALayer);
  // aWidth and aHeight are in "display pixels".  Multiply by
  // aContentsScaleFactor to get device pixels.
  bool InitFrontSurface(size_t aWidth, size_t aHeight,
                        double aContentsScaleFactor,
                        AllowOfflineRendererEnum aAllowOfflineRenderer);
  void Render();
  void SwapSurfaces();
  void ClearFrontSurface();
  void ClearBackSurface();

  double GetContentsScaleFactor() { return mContentsScaleFactor; }

private:
  void *mCALayer;
  RefPtr<nsCARenderer> mCARenderer;
  RefPtr<MacIOSurface> mFrontSurface;
  RefPtr<MacIOSurface> mBackSurface;
  double mContentsScaleFactor;
};

} // namespace PluginUtilsOSX
} // namespace plugins
} // namespace mozilla

#endif //dom_plugins_PluginUtilsOSX_h
