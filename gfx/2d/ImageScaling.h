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

#ifndef _MOZILLA_GFX_IMAGESCALING_H
#define _MOZILLA_GFX_IMAGESCALING_H

#include "Types.h"

#include <vector>
#include "Point.h"

namespace mozilla {
namespace gfx {

class ImageHalfScaler
{
public:
  ImageHalfScaler(uint8_t *aData, int32_t aStride, const IntSize &aSize)
    : mOrigData(aData), mOrigStride(aStride), mOrigSize(aSize)
    , mDataStorage(NULL)
  {
  }

  ~ImageHalfScaler()
  {
    delete [] mDataStorage;
  }

  void ScaleForSize(const IntSize &aSize);

  uint8_t *GetScaledData() const { return mData; }
  IntSize GetSize() const { return mSize; }
  uint32_t GetStride() const { return mStride; }

private:
  void HalfImage2D(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                   uint8_t *aDest, uint32_t aDestStride);
  void HalfImageVertical(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                         uint8_t *aDest, uint32_t aDestStride);
  void HalfImageHorizontal(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                           uint8_t *aDest, uint32_t aDestStride);

  // This is our SSE2 scaling function. Our destination must always be 16-byte
  // aligned and use a 16-byte aligned stride.
  void HalfImage2D_SSE2(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                        uint8_t *aDest, uint32_t aDestStride);
  void HalfImageVertical_SSE2(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                              uint8_t *aDest, uint32_t aDestStride);
  void HalfImageHorizontal_SSE2(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                                uint8_t *aDest, uint32_t aDestStride);

  void HalfImage2D_C(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                     uint8_t *aDest, uint32_t aDestStride);
  void HalfImageVertical_C(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                           uint8_t *aDest, uint32_t aDestStride);
  void HalfImageHorizontal_C(uint8_t *aSource, int32_t aSourceStride, const IntSize &aSourceSize,
                             uint8_t *aDest, uint32_t aDestStride);

  uint8_t *mOrigData;
  int32_t mOrigStride;
  IntSize mOrigSize;

  uint8_t *mDataStorage;
  // Guaranteed 16-byte aligned
  uint8_t *mData;
  IntSize mSize;
  // Guaranteed 16-byte aligned
  uint32_t mStride;
};

}
}

#endif
