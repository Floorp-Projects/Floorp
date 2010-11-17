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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>
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

#include "SharedDIBWin.h"
#include "nsMathUtils.h"
#include "nsDebug.h"

namespace mozilla {
namespace gfx {

static const PRUint32 kBytesPerPixel = 4;

SharedDIBWin::SharedDIBWin() :
    mSharedHdc(nsnull)
  , mSharedBmp(nsnull)
  , mOldObj(nsnull)
{
}

SharedDIBWin::~SharedDIBWin()
{
  Close();
}

nsresult
SharedDIBWin::Close()
{
  if (mSharedHdc && mOldObj)
    ::SelectObject(mSharedHdc, mOldObj);

  if (mSharedHdc)
    ::DeleteObject(mSharedHdc);

  if (mSharedBmp)
    ::DeleteObject(mSharedBmp);

  mSharedHdc = NULL;
  mOldObj = mSharedBmp = NULL;

  SharedDIB::Close();

  return NS_OK;
}

nsresult
SharedDIBWin::Create(HDC aHdc, PRUint32 aWidth, PRUint32 aHeight,
                     bool aTransparent)
{
  Close();

  // create the offscreen shared dib
  BITMAPV4HEADER bmih;
  PRUint32 size = SetupBitmapHeader(aWidth, aHeight, aTransparent, &bmih);

  nsresult rv = SharedDIB::Create(size);
  if (NS_FAILED(rv))
    return rv;

  if (NS_FAILED(SetupSurface(aHdc, &bmih))) {
    Close();
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
SharedDIBWin::Attach(Handle aHandle, PRUint32 aWidth, PRUint32 aHeight,
                     bool aTransparent)
{
  Close();

  BITMAPV4HEADER bmih;
  SetupBitmapHeader(aWidth, aHeight, aTransparent, &bmih);

  nsresult rv = SharedDIB::Attach(aHandle, 0);
  if (NS_FAILED(rv))
    return rv;

  if (NS_FAILED(SetupSurface(NULL, &bmih))) {
    Close();
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

PRUint32
SharedDIBWin::SetupBitmapHeader(PRUint32 aWidth, PRUint32 aHeight,
                                bool aTransparent, BITMAPV4HEADER *aHeader)
{
  // D3D cannot handle an offscreen memory that pitch (SysMemPitch) is negative.
  // So we create top-to-bottom DIB.
  memset((void*)aHeader, 0, sizeof(BITMAPV4HEADER));
  aHeader->bV4Size          = sizeof(BITMAPV4HEADER);
  aHeader->bV4Width         = aWidth;
  aHeader->bV4Height        = -LONG(aHeight); // top-to-buttom DIB
  aHeader->bV4Planes        = 1;
  aHeader->bV4BitCount      = 32;
  aHeader->bV4V4Compression = BI_BITFIELDS;
  aHeader->bV4RedMask       = 0x00FF0000;
  aHeader->bV4GreenMask     = 0x0000FF00;
  aHeader->bV4BlueMask      = 0x000000FF;

  if (aTransparent)
    aHeader->bV4AlphaMask     = 0xFF000000;

  return (sizeof(BITMAPV4HEADER) + (-aHeader->bV4Height * aHeader->bV4Width * kBytesPerPixel));
}

nsresult
SharedDIBWin::SetupSurface(HDC aHdc, BITMAPV4HEADER *aHdr)
{
  mSharedHdc = ::CreateCompatibleDC(aHdc);

  if (!mSharedHdc)
    return NS_ERROR_FAILURE;

  mSharedBmp = ::CreateDIBSection(mSharedHdc,
                                  (BITMAPINFO*)aHdr,
                                  DIB_RGB_COLORS,
                                  &mBitmapBits,
                                  mShMem->handle(),
                                  (unsigned long)sizeof(BITMAPV4HEADER));
  if (!mSharedBmp)
    return NS_ERROR_FAILURE;

  mOldObj = SelectObject(mSharedHdc, mSharedBmp);

  return NS_OK;
}


} // gfx
} // mozilla
