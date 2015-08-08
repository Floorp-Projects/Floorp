/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_REFPTRSKIA_H_
#define MOZILLA_GFX_REFPTRSKIA_H_

namespace mozilla {
namespace gfx {

// The following class was imported from Skia, which is under the
// following licence:
//
// Copyright (c) 2011 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

template <typename T> class RefPtrSkia {
public:
  RefPtrSkia() : fObj(NULL) {}
  explicit RefPtrSkia(T* obj) : fObj(obj) { SkSafeRef(fObj); }
  RefPtrSkia(const RefPtrSkia& o) : fObj(o.fObj) { SkSafeRef(fObj); }
  ~RefPtrSkia() { SkSafeUnref(fObj); }

  RefPtrSkia& operator=(const RefPtrSkia& rp) {
    SkRefCnt_SafeAssign(fObj, rp.fObj);
    return *this;
  }
  RefPtrSkia& operator=(T* obj) {
    SkRefCnt_SafeAssign(fObj, obj);
    return *this;
  }

  T* get() const { return fObj; }
  T& operator*() const { return *fObj; }
  T* operator->() const { return fObj; }

  RefPtrSkia& adopt(T* obj) {
    SkSafeUnref(fObj);
    fObj = obj;
    return *this;
  }

  typedef T* RefPtrSkia::*unspecified_bool_type;
  operator unspecified_bool_type() const {
    return fObj ? &RefPtrSkia::fObj : NULL;
  }

private:
  T* fObj;
};

// End of code imported from Skia.

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_REFPTRSKIA_H_ */
