/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Vladimir Vukicevic <vladimir@pobox.com>
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsICanvasRenderingContextInternal_h___
#define nsICanvasRenderingContextInternal_h___

#include "nsISupports.h"
#include "nsIInputStream.h"
#include "nsIDocShell.h"
#include "gfxPattern.h"
#include "mozilla/RefPtr.h"

#define NS_ICANVASRENDERINGCONTEXTINTERNAL_IID \
{ 0xffb42d3c, 0x8281, 0x44c8, \
  { 0xac, 0xba, 0x73, 0x15, 0x31, 0xaa, 0xe5, 0x07 } }

class nsHTMLCanvasElement;
class gfxContext;
class gfxASurface;
class nsIPropertyBag;
class nsDisplayListBuilder;

namespace mozilla {
namespace layers {
class CanvasLayer;
class LayerManager;
}
namespace ipc {
class Shmem;
}
namespace gfx {
class SourceSurface;
}
}

class nsICanvasRenderingContextInternal : public nsISupports {
public:
  typedef mozilla::layers::CanvasLayer CanvasLayer;
  typedef mozilla::layers::LayerManager LayerManager;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICANVASRENDERINGCONTEXTINTERNAL_IID)

  // This method should NOT hold a ref to aParentCanvas; it will be called
  // with nsnull when the element is going away.
  NS_IMETHOD SetCanvasElement(nsHTMLCanvasElement* aParentCanvas) = 0;

  // Sets the dimensions of the canvas, in pixels.  Called
  // whenever the size of the element changes.
  NS_IMETHOD SetDimensions(PRInt32 width, PRInt32 height) = 0;

  NS_IMETHOD InitializeWithSurface(nsIDocShell *docShell, gfxASurface *surface, PRInt32 width, PRInt32 height) = 0;

  // Render the canvas at the origin of the given gfxContext
  NS_IMETHOD Render(gfxContext *ctx, gfxPattern::GraphicsFilter aFilter) = 0;

  // Gives you a stream containing the image represented by this context.
  // The format is given in aMimeTime, for example "image/png".
  //
  // If the image format does not support transparency or aIncludeTransparency
  // is false, alpha will be discarded and the result will be the image
  // composited on black.
  NS_IMETHOD GetInputStream(const char *aMimeType,
                            const PRUnichar *aEncoderOptions,
                            nsIInputStream **aStream) = 0;

  // If this canvas context can be represented with a simple Thebes surface,
  // return the surface.  Otherwise returns an error.
  NS_IMETHOD GetThebesSurface(gfxASurface **surface) = 0;
  
  // This gets an Azure SourceSurface for the canvas, this will be a snapshot
  // of the canvas at the time it was called. This will return null for a
  // non-azure canvas.
  virtual mozilla::TemporaryRef<mozilla::gfx::SourceSurface> GetSurfaceSnapshot() = 0;

  // If this context is opaque, the backing store of the canvas should
  // be created as opaque; all compositing operators should assume the
  // dst alpha is always 1.0.  If this is never called, the context
  // defaults to false (not opaque).
  NS_IMETHOD SetIsOpaque(bool isOpaque) = 0;

  // Invalidate this context and release any held resources, in preperation
  // for possibly reinitializing with SetDimensions/InitializeWithSurface.
  NS_IMETHOD Reset() = 0;

  // Return the CanvasLayer for this context, creating
  // one for the given layer manager if not available.
  virtual already_AddRefed<CanvasLayer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                                       CanvasLayer *aOldLayer,
                                                       LayerManager *aManager) = 0;

  // Return true if the canvas should be forced to be "inactive" to ensure
  // it can be drawn to the screen even if it's too large to be blitted by
  // an accelerated CanvasLayer.
  virtual bool ShouldForceInactiveLayer(LayerManager *aManager) { return false; }

  virtual void MarkContextClean() = 0;

  // Redraw the dirty rectangle of this canvas.
  NS_IMETHOD Redraw(const gfxRect &dirty) = 0;

  // Passes a generic nsIPropertyBag options argument, along with the
  // previous one, if any.  Optional.
  NS_IMETHOD SetContextOptions(nsIPropertyBag *aNewOptions) { return NS_OK; }

  //
  // shmem support
  //

  // If this context can be set to use Mozilla's Shmem segments as its backing
  // store, this will set it to that state. Note that if you have drawn
  // anything into this canvas before changing the shmem state, it will be
  // lost.
  NS_IMETHOD SetIsIPC(bool isIPC) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICanvasRenderingContextInternal,
                              NS_ICANVASRENDERINGCONTEXTINTERNAL_IID)

#endif /* nsICanvasRenderingContextInternal_h___ */
