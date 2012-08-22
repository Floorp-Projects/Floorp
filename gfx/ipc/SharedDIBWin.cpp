/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedDIBWin.h"
#include "gfxAlphaRecovery.h"
#include "nsMathUtils.h"
#include "nsDebug.h"

namespace mozilla {
namespace gfx {

static const uint32_t kBytesPerPixel = 4;
static const uint32_t kByteAlign = 1 << gfxAlphaRecovery::GoodAlignmentLog2();
static const uint32_t kHeaderBytes =
  (sizeof(BITMAPV4HEADER) + kByteAlign - 1) & ~(kByteAlign - 1);

SharedDIBWin::SharedDIBWin() :
    mSharedHdc(nullptr)
  , mSharedBmp(nullptr)
  , mOldObj(nullptr)
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
SharedDIBWin::Create(HDC aHdc, uint32_t aWidth, uint32_t aHeight,
                     bool aTransparent)
{
  Close();

  // create the offscreen shared dib
  BITMAPV4HEADER bmih;
  uint32_t size = SetupBitmapHeader(aWidth, aHeight, aTransparent, &bmih);

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
SharedDIBWin::Attach(Handle aHandle, uint32_t aWidth, uint32_t aHeight,
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

uint32_t
SharedDIBWin::SetupBitmapHeader(uint32_t aWidth, uint32_t aHeight,
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

  return (kHeaderBytes + (-aHeader->bV4Height * aHeader->bV4Width * kBytesPerPixel));
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
                                  kHeaderBytes);
  if (!mSharedBmp)
    return NS_ERROR_FAILURE;

  mOldObj = SelectObject(mSharedHdc, mSharedBmp);

  return NS_OK;
}


} // gfx
} // mozilla
