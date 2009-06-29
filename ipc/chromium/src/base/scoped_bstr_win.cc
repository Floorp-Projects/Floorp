// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_bstr_win.h"

#include "base/logging.h"


ScopedBstr::ScopedBstr(const wchar_t* non_bstr)
    : bstr_(SysAllocString(non_bstr)) {
}

ScopedBstr::~ScopedBstr() {
  COMPILE_ASSERT(sizeof(ScopedBstr) == sizeof(BSTR), ScopedBstrSize);
  SysFreeString(bstr_);
}

void ScopedBstr::Reset(BSTR bstr) {
  if (bstr != bstr_) {
    // if |bstr_| is NULL, SysFreeString does nothing.
    SysFreeString(bstr_);
    bstr_ = bstr;
  }
}

BSTR ScopedBstr::Release() {
  BSTR bstr = bstr_;
  bstr_ = NULL;
  return bstr;
}

void ScopedBstr::Swap(ScopedBstr& bstr2) {
  BSTR tmp = bstr_;
  bstr_ = bstr2.bstr_;
  bstr2.bstr_ = tmp;
}

BSTR* ScopedBstr::Receive() {
  DCHECK(bstr_ == NULL) << "BSTR leak.";
  return &bstr_;
}

BSTR ScopedBstr::Allocate(const wchar_t* wide_str) {
  Reset(SysAllocString(wide_str));
  return bstr_;
}

BSTR ScopedBstr::AllocateBytes(int bytes) {
  Reset(SysAllocStringByteLen(NULL, bytes));
  return bstr_;
}

void ScopedBstr::SetByteLen(uint32 bytes) {
  DCHECK(bstr_ != NULL) << "attempting to modify a NULL bstr";
  uint32* data = reinterpret_cast<uint32*>(bstr_);
  data[-1] = bytes;
}

uint32 ScopedBstr::Length() const {
  return SysStringLen(bstr_);
}

uint32 ScopedBstr::ByteLength() const {
  return SysStringByteLen(bstr_);
}
