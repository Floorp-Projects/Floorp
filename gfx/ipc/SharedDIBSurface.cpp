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
 * The Original Code is Mozilla Firefox.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation <http://www.mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "SharedDIBSurface.h"

#include "cairo.h"

namespace mozilla {
namespace gfx {

static const cairo_user_data_key_t SHAREDDIB_KEY = {0};

static const long kBytesPerPixel = 4;

bool
SharedDIBSurface::Create(HDC adc, PRUint32 aWidth, PRUint32 aHeight,
                         bool aTransparent)
{
  nsresult rv = mSharedDIB.Create(adc, aWidth, aHeight, aTransparent);
  if (NS_FAILED(rv) || !mSharedDIB.IsValid())
    return false;

  InitSurface(aWidth, aHeight, aTransparent);
  return true;
}

bool
SharedDIBSurface::Attach(Handle aHandle, PRUint32 aWidth, PRUint32 aHeight,
                         bool aTransparent)
{
  nsresult rv = mSharedDIB.Attach(aHandle, aWidth, aHeight, aTransparent);
  if (NS_FAILED(rv) || !mSharedDIB.IsValid())
    return false;

  InitSurface(aWidth, aHeight, aTransparent);
  return true;
}

void
SharedDIBSurface::InitSurface(PRUint32 aWidth, PRUint32 aHeight,
                              bool aTransparent)
{
  long stride = long(aWidth * kBytesPerPixel);
  unsigned char* data = reinterpret_cast<unsigned char*>(mSharedDIB.GetBits());

  gfxImageFormat format = aTransparent ? ImageFormatARGB32 : ImageFormatRGB24;

  gfxImageSurface::InitWithData(data, gfxIntSize(aWidth, aHeight),
                                stride, format);

  cairo_surface_set_user_data(mSurface, &SHAREDDIB_KEY, this, NULL);
}

bool
SharedDIBSurface::IsSharedDIBSurface(gfxASurface* aSurface)
{
  return aSurface &&
    aSurface->GetType() == gfxASurface::SurfaceTypeImage &&
    aSurface->GetData(&SHAREDDIB_KEY);
}

} // namespace gfx
} // namespace mozilla
