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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef MOZILLA_GFX_SOURCESURFACED2DTARGET_H_
#define MOZILLA_GFX_SOURCESURFACED2DTARGET_H_

#include "2D.h"
#include "HelpersD2D.h"
#include <vector>
#include <d3d10_1.h>

namespace mozilla {
namespace gfx {

class DrawTargetD2D;

class SourceSurfaceD2DTarget : public SourceSurface
{
public:
  SourceSurfaceD2DTarget(DrawTargetD2D* aDrawTarget, ID3D10Texture2D* aTexture,
                         SurfaceFormat aFormat);
  ~SourceSurfaceD2DTarget();

  virtual SurfaceType GetType() const { return SURFACE_D2D1_DRAWTARGET; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;
  virtual TemporaryRef<DataSourceSurface> GetDataSurface();

private:
  friend class DrawTargetD2D;
  ID3D10ShaderResourceView *GetSRView();

  // This function is called by the draw target this texture belongs to when
  // it is about to be changed. The texture will be required to make a copy
  // of itself when this happens.
  void DrawTargetWillChange();

  // This will mark the surface as no longer depending on its drawtarget,
  // this may happen on destruction or copying.
  void MarkIndependent();

  ID2D1Bitmap *GetBitmap(ID2D1RenderTarget *aRT);

  RefPtr<ID3D10ShaderResourceView> mSRView;
  RefPtr<ID2D1Bitmap> mBitmap;
  // Non-null if this is a "lazy copy" of the given draw target.
  // Null if we've made a copy. The target is not kept alive, otherwise we'd
  // have leaks since it might keep us alive. If the target is destroyed, it
  // will notify us.
  DrawTargetD2D* mDrawTarget;
  mutable RefPtr<ID3D10Texture2D> mTexture;
  SurfaceFormat mFormat;
};

class DataSourceSurfaceD2DTarget : public DataSourceSurface
{
public:
  DataSourceSurfaceD2DTarget();
  ~DataSourceSurfaceD2DTarget();

  virtual SurfaceType GetType() const { return SURFACE_DATA; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;
  virtual unsigned char *GetData();
  virtual int32_t Stride();

private:
  friend class SourceSurfaceD2DTarget;
  void EnsureMapped();

  mutable RefPtr<ID3D10Texture2D> mTexture;
  SurfaceFormat mFormat;
  D3D10_MAPPED_TEXTURE2D mMap;
  bool mMapped;
};

}
}

#endif /* MOZILLA_GFX_SOURCESURFACED2DTARGET_H_ */
