/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Tom Brinkman <reportbase@gmail.com>
 *  Siarhei Siamashka <siarhei.siamashka@gmail.com>
 *  Timothy B. Terriberry <tterriberry@mozilla.com>
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

#include "ycbcr_to_rgb565.h"

#ifdef HAVE_YCBCR_TO_RGB565

namespace mozilla {

namespace gfx {

# if defined(MOZILLA_MAY_SUPPORT_NEON)

void __attribute((noinline)) yuv42x_to_rgb565_row_neon(uint16 *dst,
                                                       const uint8 *y,
                                                       const uint8 *u,
                                                       const uint8 *v,
                                                       int n,
                                                       int oddflag);

#endif

/*Convert a single pixel from Y'CbCr to RGB565.*/
static PRUint16 yu2rgb565(int y, int u, int v) {
  int r;
  int g;
  int b;
  r = NS_CLAMP((74*y+102*v-14240+256)>>9, 0, 31);
  g = NS_CLAMP((74*y-25*u-52*v+8704+128)>>8, 0, 63);
  b = NS_CLAMP((74*y+129*u-17696+256)>>9, 0, 31);
  return (PRUint16)(r<<11 | g<<5 | b);
}



void yuv_to_rgb565_row_c(uint16 *dst,
                         const uint8 *y,
                         const uint8 *u,
                         const uint8 *v,
                         int x_shift,
                         int pic_x,
                         int pic_width)
{
  int x;
  for (x = 0; x < pic_width; x++)
  {
    dst[x] = yu2rgb565(y[pic_x+x],
                       u[(pic_x+x)>>x_shift],
                       v[(pic_x+x)>>x_shift]);
  }
}

NS_GFX_(void) ConvertYCbCrToRGB565(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* rgb_buf,
                                   int pic_x,
                                   int pic_y,
                                   int pic_width,
                                   int pic_height,
                                   int y_pitch,
                                   int uv_pitch,
                                   int rgb_pitch,
                                   YUVType yuv_type)
{
  int x_shift;
  int y_shift;
  x_shift = yuv_type != YV24;
  y_shift = yuv_type == YV12;
#  ifdef MOZILLA_MAY_SUPPORT_NEON
  if (yuv_type != YV24 && supports_neon())
  {
    for (int i = 0; i < pic_height; i++) {
      int yoffs;
      int uvoffs;
      yoffs = y_pitch * (pic_y+i) + pic_x;
      uvoffs = uv_pitch * ((pic_y+i)>>y_shift) + (pic_x>>x_shift);
      yuv42x_to_rgb565_row_neon((uint16*)(rgb_buf + rgb_pitch * i),
                                y_buf + yoffs,
                                u_buf + uvoffs,
                                v_buf + uvoffs,
                                pic_width,
                                pic_x&x_shift);
    }
  }
  else
#  endif
  {
    for (int i = 0; i < pic_height; i++) {
      int yoffs;
      int uvoffs;
      yoffs = y_pitch * (pic_y+i);
      uvoffs = uv_pitch * ((pic_y+i)>>y_shift);
      yuv_to_rgb565_row_c((uint16*)(rgb_buf + rgb_pitch * i),
                          y_buf + yoffs,
                          u_buf + uvoffs,
                          v_buf + uvoffs,
                          x_shift,
                          pic_x,
                          pic_width);
    }
  }
}

NS_GFX_(bool) IsConvertYCbCrToRGB565Fast(int pic_x,
                                         int pic_y,
                                         int pic_width,
                                         int pic_height,
                                         YUVType yuv_type)
{
#  if defined(MOZILLA_MAY_SUPPORT_NEON)
  return (yuv_type != YV24 && supports_neon());
#  else
  return false;
#  endif
}

} // namespace gfx

} // namespace mozilla

#endif // HAVE_YCBCR_TO_RGB565
