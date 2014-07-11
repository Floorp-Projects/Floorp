/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 * Copyright (c) 2014, Mozilla
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 ** Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 ** Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in
 *  the documentation and/or other materials provided with the
 *  distribution.
 *
 ** Neither the name of Google nor the names of its contributors may
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GMP_VIDEO_PLANE_h_
#define GMP_VIDEO_PLANE_h_

#include "gmp-errors.h"
#include <stdint.h>

// The implementation backing this interface uses shared memory for the
// buffer(s). This means it can only be used by the "owning" process.
// At first the process which created the object owns it. When the object
// is passed to an interface the creator loses ownership and must Destroy()
// the object. Further attempts to use it may fail due to not being able to
// access the underlying buffer(s).
//
// Methods that create or destroy shared memory must be called on the main
// thread. They are marked below.
class GMPPlane {
public:
  // MAIN THREAD ONLY
  // CreateEmptyPlane - set allocated size, actual plane size and stride:
  // If current size is smaller than current size, then a buffer of sufficient
  // size will be allocated.
  virtual GMPErr CreateEmptyPlane(int32_t aAllocatedSize,
                                  int32_t aStride,
                                  int32_t aPlaneSize) = 0;

  // MAIN THREAD ONLY
  // Copy the entire plane data.
  virtual GMPErr Copy(const GMPPlane& aPlane) = 0;

  // MAIN THREAD ONLY
  // Copy buffer: If current size is smaller
  // than current size, then a buffer of sufficient size will be allocated.
  virtual GMPErr Copy(int32_t aSize, int32_t aStride, const uint8_t* aBuffer) = 0;

  // Swap plane data.
  virtual void Swap(GMPPlane& aPlane) = 0;

  // Get allocated size.
  virtual int32_t AllocatedSize() const = 0;

  // Set actual size.
  virtual void ResetSize() = 0;

  // Return true is plane size is zero, false if not.
  virtual bool IsZeroSize() const = 0;

  // Get stride value.
  virtual int32_t Stride() const = 0;

  // Return data pointer.
  virtual const uint8_t* Buffer() const = 0;

  // Overloading with non-const.
  virtual uint8_t* Buffer() = 0;

  // MAIN THREAD ONLY IF OWNING PROCESS
  // Call this when done with the object. This may delete it.
  virtual void Destroy() = 0;
};

#endif // GMP_VIDEO_PLANE_h_
