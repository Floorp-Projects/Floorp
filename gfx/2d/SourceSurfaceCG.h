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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#pragma once

#include <ApplicationServices/ApplicationServices.h>

#include "2D.h"

namespace mozilla {
namespace gfx {

class DrawTargetCG;

class SourceSurfaceCG : public SourceSurface
{
public:
  SourceSurfaceCG() {}
  SourceSurfaceCG(CGImageRef aImage) : mImage(aImage) {}
  ~SourceSurfaceCG();

  virtual SurfaceType GetType() const { return SURFACE_COREGRAPHICS_IMAGE; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;
  virtual TemporaryRef<DataSourceSurface> GetDataSurface();

  CGImageRef GetImage() { return mImage; }

  bool InitFromData(unsigned char *aData,
                    const IntSize &aSize,
                    int32_t aStride,
                    SurfaceFormat aFormat);

private:
  CGImageRef mImage;

  /* It might be better to just use the bitmap info from the CGImageRef to
   * deduce the format to save space in SourceSurfaceCG,
   * for now we just store it in mFormat */
  SurfaceFormat mFormat;
};

class DataSourceSurfaceCG : public DataSourceSurface
{
public:
  DataSourceSurfaceCG() {}
  DataSourceSurfaceCG(CGImageRef aImage);
  ~DataSourceSurfaceCG();

  virtual SurfaceType GetType() const { return SURFACE_DATA; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const { return FORMAT_B8G8R8A8; }

  CGImageRef GetImage() { return mImage; }

  bool InitFromData(unsigned char *aData,
                    const IntSize &aSize,
                    int32_t aStride,
                    SurfaceFormat aFormat);

  virtual unsigned char *GetData();

  virtual int32_t Stride() { return CGImageGetBytesPerRow(mImage); }


private:
  CGContextRef mCg;
  CGImageRef mImage;
  //XXX: we don't need to store mData we can just get it from the CGContext
  void *mData;
  /* It might be better to just use the bitmap info from the CGImageRef to
   * deduce the format to save space in SourceSurfaceCG,
   * for now we just store it in mFormat */
};

class SourceSurfaceCGBitmapContext : public DataSourceSurface
{
public:
  SourceSurfaceCGBitmapContext(DrawTargetCG *);
  ~SourceSurfaceCGBitmapContext();

  virtual SurfaceType GetType() const { return SURFACE_COREGRAPHICS_CGCONTEXT; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const { return FORMAT_B8G8R8A8; }

  CGImageRef GetImage() { EnsureImage(); return mImage; }

  virtual unsigned char *GetData() { return static_cast<unsigned char*>(mData); }

  virtual int32_t Stride() { return mStride; }

private:
  //XXX: do the other backends friend their DrawTarget?
  friend class DrawTargetCG;
  void DrawTargetWillChange();
  void EnsureImage() const;

  DrawTargetCG *mDrawTarget;
  CGContextRef mCg;
  mutable CGImageRef mImage;
  void *mData;
  int32_t mStride;
  IntSize mSize;
};


}
}
