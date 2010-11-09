/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Bas Schouten <bschouten@mozilla.com>
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

#ifndef GFX_CANVASLAYEROGL_H
#define GFX_CANVASLAYEROGL_H

#include "LayerManagerD3D9.h"
#include "GLContext.h"
#include "gfxASurface.h"

namespace mozilla {
namespace layers {

class THEBES_API CanvasLayerD3D9 :
  public CanvasLayer,
  public LayerD3D9
{
public:
  CanvasLayerD3D9(LayerManagerD3D9 *aManager)
    : CanvasLayer(aManager, NULL),
      LayerD3D9(aManager),
      mDataIsPremultiplied(PR_FALSE),
      mNeedsYFlip(PR_FALSE)
  {
      mImplData = static_cast<LayerD3D9*>(this);
      aManager->deviceManager()->mLayersWithResources.AppendElement(this);
  }

  ~CanvasLayerD3D9();

  // CanvasLayer implementation
  virtual void Initialize(const Data& aData);
  virtual void Updated(const nsIntRect& aRect);

  // LayerD3D9 implementation
  virtual Layer* GetLayer();
  virtual void RenderLayer();
  virtual void CleanResources();
  virtual void LayerManagerDestroyed();

  void CreateTexture();

protected:
  typedef mozilla::gl::GLContext GLContext;

  nsRefPtr<gfxASurface> mSurface;
  nsRefPtr<GLContext> mGLContext;
  nsRefPtr<IDirect3DTexture9> mTexture;

  PRUint32 mCanvasFramebuffer;

  // Indicates whether our texture was obtained through D2D interop.
  bool mIsInteropTexture;

  PRPackedBool mDataIsPremultiplied;
  PRPackedBool mNeedsYFlip;
};

} /* layers */
} /* mozilla */
#endif /* GFX_CANVASLAYERD3D9_H */
