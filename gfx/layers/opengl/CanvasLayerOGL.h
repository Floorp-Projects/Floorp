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

#include "mozilla/layers/PLayers.h"
#include "mozilla/layers/ShadowLayers.h"

#include "LayerManagerOGL.h"
#include "gfxASurface.h"

namespace mozilla {
namespace layers {

class THEBES_API CanvasLayerOGL :
  public CanvasLayer,
  public LayerOGL
{
public:
  CanvasLayerOGL(LayerManagerOGL *aManager)
    : CanvasLayer(aManager, NULL),
      LayerOGL(aManager),
      mTexture(0),
      mDelayedUpdates(PR_FALSE)
  { 
      mImplData = static_cast<LayerOGL*>(this);
  }
  ~CanvasLayerOGL() { Destroy(); }

  // CanvasLayer implementation
  virtual void Initialize(const Data& aData);

  // LayerOGL implementation
  virtual void Destroy();
  virtual Layer* GetLayer() { return this; }
  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);

protected:
  void UpdateSurface();

  nsRefPtr<gfxASurface> mCanvasSurface;
  nsRefPtr<GLContext> mCanvasGLContext;
  gl::ShaderProgramType mLayerProgram;

  void MakeTexture();
  GLuint mTexture;

  PRPackedBool mDelayedUpdates;
  PRPackedBool mGLBufferIsPremultiplied;
  PRPackedBool mNeedsYFlip;
};

// NB: eventually we'll have separate shadow canvas2d and shadow
// canvas3d layers, but currently they look the same from the
// perspective of the compositor process
class ShadowCanvasLayerOGL : public ShadowCanvasLayer,
                             public LayerOGL
{
  typedef gl::TextureImage TextureImage;

public:
  ShadowCanvasLayerOGL(LayerManagerOGL* aManager);
  virtual ~ShadowCanvasLayerOGL();

  // CanvasLayer impl
  virtual void Initialize(const Data& aData);
  // This isn't meaningful for shadow canvas.
  virtual void Updated(const nsIntRect&) {}

  // ShadowCanvasLayer impl
  virtual already_AddRefed<gfxSharedImageSurface>
  Swap(gfxSharedImageSurface* aNewFront);

  virtual void DestroyFrontBuffer();

  virtual void Disconnect();

  // LayerOGL impl
  void Destroy();
  Layer* GetLayer();
  virtual void RenderLayer(int aPreviousFrameBuffer,
                           const nsIntPoint& aOffset);

private:
  nsRefPtr<TextureImage> mTexImage;


  // XXX FIXME holding to free
  nsRefPtr<gfxSharedImageSurface> mDeadweight;


};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYEROGL_H */
