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

#ifndef MOZILLA_GFX_HELPERSSKIA_H_
#define MOZILLA_GFX_HELPERSSKIA_H_

#include "2D.h"
#include "skia/SkCanvas.h"

namespace mozilla {
namespace gfx {

static inline SkBitmap::Config
GfxFormatToSkiaConfig(SurfaceFormat format)
{
  switch (format)
  {
    case FORMAT_B8G8R8A8:
      return SkBitmap::kARGB_8888_Config;
    case FORMAT_B8G8R8X8:
      // We probably need to do something here.
      return SkBitmap::kARGB_8888_Config;
    case FORMAT_R5G6B5:
      return SkBitmap::kRGB_565_Config;
    case FORMAT_A8:
      return SkBitmap::kA8_Config;

  }

  return SkBitmap::kARGB_8888_Config;
}

static inline void
GfxMatrixToSkiaMatrix(const Matrix& mat, SkMatrix& retval)
{
    retval.setAll(SkFloatToScalar(mat._11), SkFloatToScalar(mat._21), SkFloatToScalar(mat._31),
                  SkFloatToScalar(mat._12), SkFloatToScalar(mat._22), SkFloatToScalar(mat._32),
                  0, 0, SK_Scalar1);
}

}
}

#endif /* MOZILLA_GFX_HELPERSSKIA_H_ */
