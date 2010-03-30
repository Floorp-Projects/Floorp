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

#include "ThebesLayerOGL.h"
#include "ContainerLayerOGL.h"
#include "gfxContext.h"

#include "glWrapper.h"

namespace mozilla {
namespace layers {

// Returns true if it's OK to save the contents of aLayer in an
// opaque surface (a surface without an alpha channel).
// If we can use a surface without an alpha channel, we should, because
// it will often make painting of antialiased text faster and higher
// quality.
static PRBool
UseOpaqueSurface(Layer* aLayer)
{
  // If the visible content in the layer is opaque, there is no need
  // for an alpha channel.
  if (aLayer->IsOpaqueContent())
    return PR_TRUE;
  // Also, if this layer is the bottommost layer in a container which
  // doesn't need an alpha channel, we can use an opaque surface for this
  // layer too. Any transparent areas must be covered by something else
  // in the container.
  ContainerLayerOGL* parent =
    static_cast<ContainerLayerOGL*>(aLayer->GetParent());
  return parent && parent->GetFirstChild() == aLayer &&
         UseOpaqueSurface(parent);
}


ThebesLayerOGL::ThebesLayerOGL(LayerManager *aManager)
  : ThebesLayer(aManager, NULL)
  , mTexture(0)
{
  mImplData = static_cast<LayerOGL*>(this);
}

ThebesLayerOGL::~ThebesLayerOGL()
{
  static_cast<LayerManagerOGL*>(mManager)->MakeCurrent();
  if (mTexture) {
    sglWrapper.DeleteTextures(1, &mTexture);
  }
}

void
ThebesLayerOGL::SetVisibleRegion(const nsIntRegion &aRegion)
{
  if (aRegion.GetBounds() == mVisibleRect) {
    return;
  }
  mVisibleRect = aRegion.GetBounds();

  static_cast<LayerManagerOGL*>(mManager)->MakeCurrent();

  if (!mTexture) {
    sglWrapper.GenTextures(1, &mTexture);
  }

  mInvalidatedRect = mVisibleRect;

  sglWrapper.BindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

  sglWrapper.TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  sglWrapper.TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  sglWrapper.TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  sglWrapper.TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  sglWrapper.TexImage2D(LOCAL_GL_TEXTURE_2D,
                        0,
                        LOCAL_GL_RGBA,
                        mVisibleRect.width,
                        mVisibleRect.height,
                        0,
                        LOCAL_GL_BGRA,
                        LOCAL_GL_UNSIGNED_BYTE,
                        NULL);
}

void
ThebesLayerOGL::InvalidateRegion(const nsIntRegion &aRegion)
{
  nsIntRegion invalidatedRegion;
  invalidatedRegion.Or(aRegion, mInvalidatedRect);
  invalidatedRegion.And(invalidatedRegion, mVisibleRect);
  mInvalidatedRect = invalidatedRegion.GetBounds();
}

gfxContext *
ThebesLayerOGL::BeginDrawing(nsIntRegion *aRegion)
{
  if (mInvalidatedRect.IsEmpty()) {
    aRegion->SetEmpty();
    return NULL;
  }
  if (!mTexture) {
    aRegion->SetEmpty();
    return NULL;
  }
  *aRegion = mInvalidatedRect;

  if (UseOpaqueSurface(this)) {
    mSoftwareSurface = new gfxImageSurface(gfxIntSize(mInvalidatedRect.width,
                                                      mInvalidatedRect.height),
                                           gfxASurface::ImageFormatRGB24);
  } else {
    mSoftwareSurface = new gfxImageSurface(gfxIntSize(mInvalidatedRect.width,
                                                      mInvalidatedRect.height),
                                           gfxASurface::ImageFormatARGB32);
  }

  mContext = new gfxContext(mSoftwareSurface);
  mContext->Translate(gfxPoint(-mInvalidatedRect.x, -mInvalidatedRect.y));
  return mContext.get();
}

void
ThebesLayerOGL::EndDrawing()
{
  static_cast<LayerManagerOGL*>(mManager)->MakeCurrent();

  sglWrapper.BindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
  sglWrapper.TexSubImage2D(LOCAL_GL_TEXTURE_2D,
                           0,
                           mInvalidatedRect.x - mVisibleRect.x,
                           mInvalidatedRect.y - mVisibleRect.y,
                           mInvalidatedRect.width,
                           mInvalidatedRect.height,
                           LOCAL_GL_BGRA,
                           LOCAL_GL_UNSIGNED_BYTE,
                           mSoftwareSurface->Data());

  mSoftwareSurface = NULL;
  mContext = NULL;
}

void
ThebesLayerOGL::CopyFrom(ThebesLayer* aSource,
                           const nsIntRegion& aRegion,
                           const nsIntPoint& aDelta)
{
  // XXX - Roc says this is going away in the API. Ignore it for now.
}

LayerOGL::LayerType
ThebesLayerOGL::GetType()
{
  return TYPE_THEBES;
}

const nsIntRect&
ThebesLayerOGL::GetVisibleRect()
{
  return mVisibleRect;
}

void
ThebesLayerOGL::RenderLayer(int aPreviousFrameBuffer)
{
  if (!mTexture) {
    return;
  }

  float quadTransform[4][4];
  /*
   * Matrix to transform the <0.0,0.0>, <1.0,1.0> quad to the correct position
   * and size.
   */
  memset(&quadTransform, 0, sizeof(quadTransform));
  quadTransform[0][0] = (float)GetVisibleRect().width;
  quadTransform[1][1] = (float)GetVisibleRect().height;
  quadTransform[2][2] = 1.0f;
  quadTransform[3][0] = (float)GetVisibleRect().x;
  quadTransform[3][1] = (float)GetVisibleRect().y;
  quadTransform[3][3] = 1.0f;

  RGBLayerProgram *program = 
    static_cast<LayerManagerOGL*>(mManager)->GetRGBLayerProgram();

  program->Activate();
  program->SetLayerQuadTransform(&quadTransform[0][0]);
  program->SetLayerOpacity(GetOpacity());
  program->SetLayerTransform(&mTransform._11);
  program->Apply();

  sglWrapper.BindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

  sglWrapper.DrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
}

const nsIntRect&
ThebesLayerOGL::GetInvalidatedRect()
{
  return mInvalidatedRect;
}

Layer*
ThebesLayerOGL::GetLayer()
{
  return this;
}

PRBool
ThebesLayerOGL::IsEmpty()
{
  return !mTexture;
}

} /* layers */
} /* mozilla */
