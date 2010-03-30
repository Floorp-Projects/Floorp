/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.org>
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

#ifndef GFX_THEBESLAYEROGL_H
#define GFX_THEBESLAYEROGL_H

#include "Layers.h"
#include "LayerManagerOGL.h"
#include "gfxImageSurface.h"


namespace mozilla {
namespace layers {

class ThebesLayerOGL : public ThebesLayer, 
                         public LayerOGL
{
public:
  ThebesLayerOGL(LayerManager *aManager);
  virtual ~ThebesLayerOGL();

  /** Layer implementation */
  void SetVisibleRegion(const nsIntRegion& aRegion);

  /** ThebesLayer implementation */
  void InvalidateRegion(const nsIntRegion& aRegion);

  gfxContext *BeginDrawing(nsIntRegion* aRegionToDraw);

  void EndDrawing();

  void CopyFrom(ThebesLayer* aSource,
                const nsIntRegion& aRegion,
                const nsIntPoint& aDelta);

  /** LayerOGL implementation */
  LayerType GetType();
  Layer* GetLayer();
  virtual PRBool IsEmpty();
  virtual void RenderLayer(int aPreviousFrameBuffer);

  /** ThebesLayerOGL */
  const nsIntRect &GetVisibleRect();
  const nsIntRect &GetInvalidatedRect();

private:
  /** 
   * Visible rectangle, this is used to know the size and position of the quad
   * when doing the rendering of this layer.
   */
  nsIntRect mVisibleRect;
  /**
   * Currently invalidated rectangular area.
   */
  nsIntRect mInvalidatedRect;
  /**
   * Software surface used for this layer's drawing operation. This is created
   * on BeginDrawing() and should be removed on EndDrawing().
   */
  nsRefPtr<gfxImageSurface> mSoftwareSurface;

  /**
   * We hold the reference to the context.
   */
  nsRefPtr<gfxContext> mContext;

  /**
   * OpenGL Texture
   */
  GLuint mTexture;

};

} /* layers */
} /* mozilla */
#endif /* GFX_THEBESLAYEROGL_H */
