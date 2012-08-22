/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfx_SharedDIBWin_h__
#define gfx_SharedDIBWin_h__

#include <windows.h>

#include "SharedDIB.h"

namespace mozilla {
namespace gfx {

class SharedDIBWin : public SharedDIB
{
public:
  SharedDIBWin();
  ~SharedDIBWin();

  // Allocate a new win32 dib section compatible with an hdc. The dib will
  // be selected into the hdc on return.
  nsresult Create(HDC aHdc, uint32_t aWidth, uint32_t aHeight,
                  bool aTransparent);

  // Wrap a dib section around an existing shared memory object. aHandle should
  // point to a section large enough for the dib's memory, otherwise this call
  // will fail.
  nsresult Attach(Handle aHandle, uint32_t aWidth, uint32_t aHeight,
                  bool aTransparent);

  // Destroy or release resources associated with this dib.
  nsresult Close();

  // Return the HDC of the shared dib.
  HDC GetHDC() { return mSharedHdc; }

  // Return the bitmap bits.
  void* GetBits() { return mBitmapBits; }

private:
  HDC                 mSharedHdc;
  HBITMAP             mSharedBmp;
  HGDIOBJ             mOldObj;
  void*               mBitmapBits;

  uint32_t SetupBitmapHeader(uint32_t aWidth, uint32_t aHeight,
                             bool aTransparent, BITMAPV4HEADER *aHeader);
  nsresult SetupSurface(HDC aHdc, BITMAPV4HEADER *aHdr);
};

} // gfx
} // mozilla

#endif
