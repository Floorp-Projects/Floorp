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

#ifndef GFX_IMAGELAYEROGL_H
#define GFX_IMAGELAYEROGL_H

#include "LayerManagerOGL.h"
#include "ImageLayers.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace layers {

class THEBES_API ImageContainerOGL : public ImageContainer
{
public:
  ImageContainerOGL(LayerManagerOGL *aManager);
  virtual ~ImageContainerOGL() {}

  virtual already_AddRefed<Image> CreateImage(const Image::Format* aFormats,
                                              PRUint32 aNumFormats);

  virtual void SetCurrentImage(Image* aImage);

  virtual already_AddRefed<Image> GetCurrentImage();

  virtual already_AddRefed<gfxASurface> GetCurrentAsSurface(gfxIntSize* aSize);
private:
  typedef mozilla::Mutex Mutex;

  nsRefPtr<Image> mActiveImage;

  Mutex mActiveImageLock;
};

class THEBES_API ImageLayerOGL : public ImageLayer,
                                 public LayerOGL
{
public:
  ImageLayerOGL(LayerManagerOGL *aManager)
    : ImageLayer(aManager, NULL)
  { 
    mImplData = static_cast<LayerOGL*>(this);
  }

  // LayerOGL Implementation
  virtual LayerType GetType();

  virtual Layer* GetLayer();

  virtual void RenderLayer(int aPreviousDestination);
};

class THEBES_API PlanarYCbCrImageOGL : public PlanarYCbCrImage
{
public:
  PlanarYCbCrImageOGL(LayerManagerOGL *aManager);
  virtual ~PlanarYCbCrImageOGL();

  virtual void SetData(const Data &aData);

  /**
   * Upload the data from out mData into our textures. For now we use this to
   * make sure the textures are created and filled on the main thread.
   */
  virtual void AllocateTextures();
  /**
   * XXX
   * Free the textures, we call this from the main thread when we're done
   * drawing this frame. We cannot free this from the constructor since it may
   * be destroyed off the main-thread and might not be able to properly clean
   * up its textures
   */
  virtual void FreeTextures();
  virtual PRBool HasData() { return mHasData; }

  Data mData;
  PRBool mLoaded;
  PRBool mHasData;
  GLuint mTextures[3];
  gfxIntSize mSize;
  LayerManagerOGL *mManager;
};


class THEBES_API CairoImageOGL : public CairoImage
{
public:
  CairoImageOGL(LayerManagerOGL *aManager)
    : CairoImage(NULL)
    , mManager(aManager)
  { }
  ~CairoImageOGL();

  virtual void SetData(const Data &aData);

  GLuint mTexture;
  gfxIntSize mSize;
  LayerManagerOGL *mManager;
};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYEROGL_H */
