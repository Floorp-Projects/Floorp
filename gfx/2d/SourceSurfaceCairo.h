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

#ifndef _MOZILLA_GFX_OP_SOURCESURFACE_CAIRO_H_
#define _MOZILLA_GFX_OP_SOURCESURFACE_CAIRO_H

#include "2D.h"

namespace mozilla {
namespace gfx {

class DrawTargetCairo;

class SourceSurfaceCairo : public SourceSurface
{
public:
  // Create a SourceSurfaceCairo. The surface will not be copied, but simply
  // referenced.
  // If aDrawTarget is non-NULL, it is assumed that this is a snapshot source
  // surface, and we'll call DrawTargetCairo::RemoveSnapshot(this) on it when
  // we're destroyed.
  SourceSurfaceCairo(cairo_surface_t* aSurface, const IntSize& aSize,
                     const SurfaceFormat& aFormat,
                     DrawTargetCairo* aDrawTarget = NULL);
  virtual ~SourceSurfaceCairo();

  virtual SurfaceType GetType() const { return SURFACE_CAIRO; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;
  virtual TemporaryRef<DataSourceSurface> GetDataSurface();

  cairo_surface_t* GetSurface() const;

private: // methods
  friend class DrawTargetCairo;
  void DrawTargetWillChange();
  void MarkIndependent();

private: // data
  IntSize mSize;
  SurfaceFormat mFormat;
  cairo_surface_t* mSurface;
  DrawTargetCairo* mDrawTarget;
};

class DataSourceSurfaceCairo : public DataSourceSurface
{
public:
  DataSourceSurfaceCairo(cairo_surface_t* imageSurf);
  virtual ~DataSourceSurfaceCairo();
  virtual unsigned char *GetData();
  virtual int32_t Stride();

  virtual SurfaceType GetType() const { return SURFACE_CAIRO_IMAGE; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;

  cairo_surface_t* GetSurface() const;

private:
  cairo_surface_t* mImageSurface;
};

}
}

#endif // _MOZILLA_GFX_OP_SOURCESURFACE_CAIRO_H
