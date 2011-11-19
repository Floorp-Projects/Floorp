/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* ***** BEGIN LICENSE BLOCK *****
  * Version: MPL 1.1/GPL 2.0/LGPL 2.1
  *
  * The contents of this file are subject to the Mozilla Public License Version
  * 1.1 (the "License"); you may not use this file except in compliance with
  * the License. You may obtain a copy of the License at
  * http://www.mozilla.org/MPL/
  *
  * Software distributed under the License is distributed on an "AS IS" basis,
  * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
  * for the specific language governing rights and limitations under the
  * License.
  *
  * The Original Code is Mozilla.org code.
  *
  * The Initial Developer of the Original Code is
  *   Mozilla Corporation.
  * Portions created by the Initial Developer are Copyright (C) 2010
  * the Initial Developer. All Rights Reserved.
  *
  * Contributor(s):
  *   Benoit Girard <b56girard@gmail.com>
  *
  * Alternatively, the contents of this file may be used under the terms of
  * either of the GNU General Public License Version 2 or later (the "GPL"),
  * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
  * in which case the provisions of the GPL or the LGPL are applicable instead
  * of those above. If you wish to allow use of your version of this file only
  * under the terms of either the GPL or the LGPL, and not to allow others to
  * use your version of this file under the terms of the MPL, indicate your
  * decision by deleting the provisions above and replace them with the notice
  * and other provisions required by the GPL or the LGPL. If you do not delete
  * the provisions above, a recipient may use your version of this file under
  * the terms of any one of the MPL, the GPL or the LGPL.
  *
  * ***** END LICENSE BLOCK ***** */

#ifndef dom_plugins_PluginUtilsOSX_h
#define dom_plugins_PluginUtilsOSX_h 1

#include "npapi.h"
#include "nsRect.h"
#include "nsCoreAnimationSupport.h"

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
  nsDoubleBufferCARenderer() : mCALayer(nsnull) {}
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
  nsRefPtr<nsCARenderer> mCARenderer;
  nsRefPtr<nsIOSurface> mFrontSurface;
  nsRefPtr<nsIOSurface> mBackSurface;
};

} // namespace PluginUtilsOSX
} // namespace plugins
} // namespace mozilla

#endif //dom_plugins_PluginUtilsOSX_h
