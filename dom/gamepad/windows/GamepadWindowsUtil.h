/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_GAMEPAD_GAMEPADWINDOWSUTIL_H_
#define DOM_GAMEPAD_GAMEPADWINDOWSUTIL_H_

#include "mozilla/Assertions.h"
#include <utility>
#include <windows.h>

namespace mozilla::dom {

// UniqueHandle - A generic RAII wrapper for handle-like objects
//
// A type that behaves like UniquePtr, but for arbitrary handle types.
// The type, HandleType, only needs to be copyable (like all C primitive types
// and structures). The `Traits` template parameter will define the `HandleType`
// and the following 3 or 4 operations on the basic type:
//
// static HandleType Invalid()
//    Must return an instance of the object that is considered invalid
//    according to `IsValid()`. Note that UniqueHandle doesn't assume that
//    there is a sentinal value that is invalid -- It only cares that
//    IsValid(Invalid()) is false
//
// static bool IsValid(HandleType)
//    Returns true if the handle is valid, false otherwise
//
// static void Close(HandleType)
//    Called on destruction to close the handle (only called if IsValid())
//
// static bool IsEqual(HandleType, HandleType)
//    OPTIONAL -- If you want the UniqueHandle type to support == and !=
//    operators, implement this. It doesn't affect anything else.
//
// (Note that the functions can take the handle by value or by const-ref)
//
// # Example
//
// struct FdHandleTraits {
//   using HandleType = int;
//   static HandleType Invalid() { return -1; }
//   static void Close(HandleType handle) { close(handle); }
//   static bool IsValid(HandleType handle) { return handle >= 0; }
//   static bool IsEqual(HandleType a, HandleType b) { return a == b; }
// };
//
// UniqueHandle<FdHandleTraits> uniqueFile(open(...));
//
template <typename Traits>
class UniqueHandle {
 public:
  using HandleType = typename Traits::HandleType;

  UniqueHandle() : mHandle(Traits::Invalid()) {
    MOZ_ASSERT(!Traits::IsValid(mHandle));
  }
  explicit UniqueHandle(HandleType t) : mHandle(std::move(t)) {
    MOZ_ASSERT(!Traits::IsValid(Traits::Invalid()));
  }

  UniqueHandle(UniqueHandle&& other) : UniqueHandle() { swap(*this, other); }

  ~UniqueHandle() {
    if (Traits::IsValid(mHandle)) {
      Traits::Close(mHandle);
      mHandle = Traits::Invalid();
    }
  }

  UniqueHandle& operator=(UniqueHandle&& other) {
    UniqueHandle temp(std::move(other));
    swap(*this, temp);
    return *this;
  }

  HandleType Release() {
    using std::swap;
    HandleType result = Traits::Invalid();
    swap(result, mHandle);
    return result;
  }

  explicit operator bool() const { return Traits::IsValid(mHandle); }

  HandleType Get() const { return mHandle; }

  friend void swap(UniqueHandle& a, UniqueHandle& b) {
    // Well-behaved types *should* properly handle self-swap, but let's be
    // defensive just in case
    if (&a != &b) {
      using std::swap;
      swap(a.mHandle, b.mHandle);
    }
  }

  // If the traits don't provide an isEqual() function, this will get SFINAE'd
  // out and equality tests won't be supported
  template <typename = decltype(Traits::IsEqual(Traits::Invalid(),
                                                Traits::Invalid()))>
  friend bool operator==(const UniqueHandle& a, const UniqueHandle& b) {
    return Traits::IsEqual(a.mHandle, b.mHandle);
  }

  template <typename = decltype(Traits::IsEqual(Traits::Invalid(),
                                                Traits::Invalid()))>
  friend bool operator!=(const UniqueHandle& a, const UniqueHandle& b) {
    return !(a == b);
  }

  UniqueHandle(const UniqueHandle&) = delete;
  UniqueHandle& operator=(const UniqueHandle&) = delete;

 private:
  HandleType mHandle;
};

// UniqueHandle trait for NT Kernel handles.
// Has `Tag` to create a strong type for each kind of NT handle.
template <typename Tag>
struct NTKernelHandleTraits {
  using HandleType = HANDLE;
  static HandleType Invalid() { return nullptr; }
  static bool IsValid(HandleType handle) { return !!handle; }
  static void Close(HandleType handle) {
    MOZ_ALWAYS_TRUE(::CloseHandle(handle));
  }
  static bool IsEqual(const HandleType& a, const HandleType& b) {
    return a == b;
  }
};

// Create strong handle traits for several different NT handle types
struct NTMutexHandleTag {};
struct NTEventHandleTag {};
struct NTFileMappingHandleTag {};

using NTMutexHandleTraits = NTKernelHandleTraits<NTMutexHandleTag>;
using NTEventHandleTraits = NTKernelHandleTraits<NTEventHandleTag>;
using NTFileMappingHandleTraits = NTKernelHandleTraits<NTFileMappingHandleTag>;

// UniqueHandle trait for NT file mapping
struct NTFileViewHandleTraits {
  using HandleType = HANDLE;
  static void* Invalid() { return nullptr; }
  static bool IsValid(void* p) { return !!p; }
  static void Close(void* p) { MOZ_ALWAYS_TRUE(::UnmapViewOfFile(p)); }
  static bool IsEqual(const void* a, const void* b) { return a == b; }
};

}  // namespace mozilla::dom

#endif  // DOM_GAMEPAD_GAMEPADWINDOWSUTIL_H_
