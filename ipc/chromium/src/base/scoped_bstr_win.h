/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_BSTR_WIN_H_
#define BASE_SCOPED_BSTR_WIN_H_

#include "base/basictypes.h"  // needed to pick up OS_WIN

#include "base/logging.h"

#include <windows.h>
#include <oleauto.h>

// Manages a BSTR string pointer.
// The class interface is based on scoped_ptr.
class ScopedBstr {
 public:
  ScopedBstr() : bstr_(NULL) {
  }

  // Constructor to create a new BSTR.
  // NOTE: Do not pass a BSTR to this constructor expecting ownership to
  // be transferred - even though it compiles! ;-)
  explicit ScopedBstr(const wchar_t* non_bstr);
  ~ScopedBstr();

  // Give ScopedBstr ownership over an already allocated BSTR or NULL.
  // If you need to allocate a new BSTR instance, use |allocate| instead.
  void Reset(BSTR bstr = NULL);

  // Releases ownership of the BSTR to the caller.
  BSTR Release();

  // Creates a new BSTR from a wide string.
  // If you already have a BSTR and want to transfer ownership to the
  // ScopedBstr instance, call |reset| instead.
  // Returns a pointer to the new BSTR, or NULL if allocation failed.
  BSTR Allocate(const wchar_t* wide_str);

  // Allocates a new BSTR with the specified number of bytes.
  // Returns a pointer to the new BSTR, or NULL if allocation failed.
  BSTR AllocateBytes(int bytes);

  // Sets the allocated length field of the already-allocated BSTR to be
  // |bytes|.  This is useful when the BSTR was preallocated with e.g.
  // SysAllocStringLen or SysAllocStringByteLen (call |AllocateBytes|) and
  // then not all the bytes are being used.
  // Note that if you want to set the length to a specific number of characters,
  // you need to multiply by sizeof(wchar_t).  Oddly, there's no public API to
  // set the length, so we do this ourselves by hand.
  //
  // NOTE: The actual allocated size of the BSTR MUST be >= bytes.
  //  That responsibility is with the caller.
  void SetByteLen(uint32_t bytes);

  // Swap values of two ScopedBstr's.
  void Swap(ScopedBstr& bstr2);

  // Retrieves the pointer address.
  // Used to receive BSTRs as out arguments (and take ownership).
  // The function DCHECKs on the current value being NULL.
  // Usage: GetBstr(bstr.Receive());
  BSTR* Receive();

  // Returns number of chars in the BSTR.
  uint32_t Length() const;

  // Returns the number of bytes allocated for the BSTR.
  uint32_t ByteLength() const;

  operator BSTR() const {
    return bstr_;
  }

 protected:
  BSTR bstr_;

 private:
  // Forbid comparison of ScopedBstr types.  You should never have the same
  // BSTR owned by two different scoped_ptrs.
  bool operator==(const ScopedBstr& bstr2) const;
  bool operator!=(const ScopedBstr& bstr2) const;
  DISALLOW_COPY_AND_ASSIGN(ScopedBstr);
};

// Template class to generate a BSTR from a static wide string
// without touching the heap.  Use this class via the StackBstrVar and
// StackBstr macros.
template <uint32_t string_bytes>
class StackBstrT {
 public:
  // Try to stay as const as we can in an attempt to avoid someone
  // using the class incorrectly (e.g. by supplying a variable instead
  // of a verbatim string.  We also have an assert in the constructor
  // as an extra runtime check since the const-ness only catches one case.
  explicit StackBstrT(const wchar_t* const str) {
    // The BSTR API uses UINT, but we prefer uint32_t.
    // Make sure we'll know about it if these types don't match.
    COMPILE_ASSERT(sizeof(uint32_t) == sizeof(UINT), UintToUint32);
    COMPILE_ASSERT(sizeof(wchar_t) == sizeof(OLECHAR), WcharToOlechar);

    // You shouldn't pass string pointers to this constructor since
    // there's no way for the compiler to calculate the length of the
    // string (string_bytes will be equal to pointer size in those cases).
    DCHECK(lstrlenW(str) == (string_bytes / sizeof(bstr_.str_[0])) - 1) <<
        "not expecting a string pointer";
    memcpy(bstr_.str_, str, string_bytes);
    bstr_.len_ = string_bytes - sizeof(wchar_t);
  }

  operator BSTR() {
    return bstr_.str_;
  }

 protected:
  struct BstrInternal {
    uint32_t len_;
    wchar_t str_[string_bytes / sizeof(wchar_t)];
  } bstr_;
};

// Use this macro to generate an inline BSTR from a wide string.
// This is about 6 times faster than using the SysAllocXxx functions to
// allocate a BSTR and helps with keeping heap fragmentation down.
// Example:
//  DoBstrStuff(StackBstr(L"This is my BSTR"));
// Where DoBstrStuff is:
//  HRESULT DoBstrStuff(BSTR bstr) { ... }
#define StackBstr(str) \
  static_cast<BSTR>(StackBstrT<sizeof(str)>(str))

// If you need a named BSTR variable that's based on a fixed string
// (e.g. if the BSTR is used inside a loop or more than one place),
// use StackBstrVar to declare a variable.
// Example:
//   StackBstrVar(L"my_property", myprop);
//   for (int i = 0; i < objects.length(); ++i)
//     ProcessValue(objects[i].GetProp(myprop));  // GetProp accepts BSTR
#define StackBstrVar(str, var) \
  StackBstrT<sizeof(str)> var(str)

#endif  // BASE_SCOPED_BSTR_WIN_H_
