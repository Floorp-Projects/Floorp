/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_SharedDIBSurface_h
#define mozilla_gfx_SharedDIBSurface_h

#include "gfxImageSurface.h"
#include "SharedDIBWin.h"

#include <windows.h>

namespace mozilla {
namespace gfx {

/**
 * A SharedDIBSurface owns an underlying SharedDIBWin.
 */
class SharedDIBSurface : public gfxImageSurface
{
public:
  typedef base::SharedMemoryHandle Handle;

  SharedDIBSurface() { }
  ~SharedDIBSurface() { }

  /**
   * Create this image surface backed by shared memory.
   */
  bool Create(HDC adc, uint32_t aWidth, uint32_t aHeight, bool aTransparent);

  /**
   * Attach this surface to shared memory from another process.
   */
  bool Attach(Handle aHandle, uint32_t aWidth, uint32_t aHeight,
              bool aTransparent);

  /**
   * After drawing to a surface via GDI, GDI must be flushed before the bitmap
   * is valid.
   */
  void Flush() { ::GdiFlush(); }

  HDC GetHDC() { return mSharedDIB.GetHDC(); }

  nsresult ShareToProcess(base::ProcessHandle aChildProcess, Handle* aChildHandle) {
    return mSharedDIB.ShareToProcess(aChildProcess, aChildHandle);
  }

  static bool IsSharedDIBSurface(gfxASurface* aSurface);

private:
  SharedDIBWin mSharedDIB;

  void InitSurface(uint32_t aWidth, uint32_t aHeight, bool aTransparent);
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_SharedDIBSurface_h
