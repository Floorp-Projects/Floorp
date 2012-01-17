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
#include "DrawTargetCG.h"
#include "SourceSurfaceCG.h"
#include "Rect.h"

//CG_EXTERN void CGContextSetCompositeOperation (CGContextRef, PrivateCGCompositeMode);

namespace mozilla {
namespace gfx {

static CGRect RectToCGRect(Rect r)
{
  return CGRectMake(r.x, r.y, r.width, r.height);
}

CGBlendMode ToBlendMode(CompositionOp op)
{
  CGBlendMode mode;
  switch (op) {
    case OP_OVER:
      mode = kCGBlendModeNormal;
      break;
    case OP_SOURCE:
      mode = kCGBlendModeCopy;
      break;
    case OP_CLEAR:
      mode = kCGBlendModeClear;
      break;
    case OP_ADD:
      mode = kCGBlendModePlusLighter;
      break;
    case OP_ATOP:
      mode = kCGBlendModeSourceAtop;
      break;
    default:
      mode = kCGBlendModeNormal;
  }
  return mode;
}



DrawTargetCG::DrawTargetCG()
{
}

DrawTargetCG::~DrawTargetCG()
{
}

TemporaryRef<SourceSurface>
DrawTargetCG::Snapshot()
{
  return NULL;
}

TemporaryRef<SourceSurface>
DrawTargetCG::CreateSourceSurfaceFromData(unsigned char *aData,
                                           const IntSize &aSize,
                                           int32_t aStride,
                                           SurfaceFormat aFormat) const
{
  RefPtr<SourceSurfaceCG> newSurf = new SourceSurfaceCG();

  if (!newSurf->InitFromData(aData, aSize, aStride, aFormat)) {
    return NULL;
  }

  return newSurf;
}

TemporaryRef<SourceSurface>
DrawTargetCG::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  return NULL;
}

void
DrawTargetCG::DrawSurface(SourceSurface *aSurface,
                           const Rect &aDest,
                           const Rect &aSource,
                           const DrawOptions &aOptions,
                           const DrawSurfaceOptions &aSurfOptions)
{
  CGImageRef image;
  CGImageRef subimage = NULL;
  if (aSurface->GetType() == COREGRAPHICS_IMAGE) {
    image = static_cast<SourceSurfaceCG*>(aSurface)->GetImage();
    /* we have two options here:
     *  - create a subimage -- this is slower
     *  - fancy things with clip and different dest rects */
    {
      subimage = CGImageCreateWithImageInRect(image, RectToCGRect(aSource));
      image = subimage;
    }

    CGContextDrawImage(mCg, RectToCGRect(aDest), image);

    CGImageRelease(subimage);
  }
}

void
DrawTargetCG::FillRect(const Rect &aRect,
                        const Pattern &aPattern,
                        const DrawOptions &aOptions)
{
  //XXX: it would be nice to hang a CGColor off of the pattern here
  if (aPattern.GetType() == COLOR) {
    Color color = static_cast<const ColorPattern*>(&aPattern)->mColor;
    //XXX: the m prefixes are painful here
    CGContextSetRGBFillColor(mCg, color.mR, color.mG, color.mB, color.mA);
  }

  CGContextSetBlendMode(mCg, ToBlendMode(aOptions.mCompositionOp));
  CGContextFillRect(mCg, RectToCGRect(aRect));
}


bool
DrawTargetCG::Init(const IntSize &aSize)
{
  CGColorSpaceRef cgColorspace;
  cgColorspace = CGColorSpaceCreateDeviceRGB();

  mSize = aSize;

  int bitsPerComponent = 8;
  int stride = mSize.width;

  CGBitmapInfo bitinfo;

  bitinfo = kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedFirst;

  // XXX: mWidth is ugly
  mCg = CGBitmapContextCreate (NULL,
                         mSize.width,
			 mSize.height,
			 bitsPerComponent,
			 stride,
			 cgColorspace,
			 bitinfo);

  CGColorSpaceRelease (cgColorspace);

  return true;
}
}
}
