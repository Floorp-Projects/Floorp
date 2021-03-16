/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_BufferSize_h
#define vm_BufferSize_h

namespace js {
// Class wrapping an ArrayBuffer or ArrayBufferView byte offset or length.
class BufferSize {
  size_t size_ = 0;

 public:
  explicit BufferSize(size_t size) : size_(size) {}

  size_t get() const { return size_; }

  uint32_t getWasmUint32() const {
    // Exact test on max heap size for the time being: 64K-2 pages
    MOZ_ASSERT(size_ <= UINT32_MAX - 65536 * 2 + 1);
    return size_;
  }
};

}  // namespace js

#endif  // vm_BufferSize_h
