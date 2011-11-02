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
 *   Matt Woodrow <mwoodrow@mozilla.com>
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

#ifndef MOZILLA_GFX_SOURCESURFACESKIA_H_
#define MOZILLA_GFX_SOURCESURFACESKIA_H_

#include "2D.h"
#include <vector>
#include "skia/SkCanvas.h"
#include "skia/SkBitmap.h"

namespace mozilla {
namespace gfx {

class DrawTargetSkia;

class SourceSurfaceSkia : public DataSourceSurface
{
public:
  SourceSurfaceSkia();
  ~SourceSurfaceSkia();

  virtual SurfaceType GetType() const { return SURFACE_SKIA; }
  virtual IntSize GetSize() const;
  virtual SurfaceFormat GetFormat() const;

  SkBitmap& GetBitmap() { return mBitmap; }

  bool InitFromData(unsigned char* aData,
                    const IntSize &aSize,
                    int32_t aStride,
                    SurfaceFormat aFormat);

  /**
   * If aOwner is NULL, we make a copy of the pixel data in the bitmap, 
   * otherwise we just reference this data until DrawTargetWillChange is called.
   */
  bool InitWithBitmap(const SkBitmap& aBitmap,
                      SurfaceFormat aFormat,
                      DrawTargetSkia* aOwner);


  virtual unsigned char *GetData();

  virtual int32_t Stride() { return mStride; }

private:
  friend class DrawTargetSkia;

  void DrawTargetWillChange();
  void MarkIndependent();

  SkBitmap mBitmap;
  SurfaceFormat mFormat;
  IntSize mSize;
  int32_t mStride;
  DrawTargetSkia* mDrawTarget;
};

}
}

#endif /* MOZILLA_GFX_SOURCESURFACESKIA_H_ */
