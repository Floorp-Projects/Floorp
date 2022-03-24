/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* an object with the same layout as a Rust std::cell::Cell<T> */

#ifndef mozilla_RustCell_h
#define mozilla_RustCell_h

namespace mozilla {

/**
 * Object with the same layout as std::cell::Cell<T>.
 *
 * ServoBindings.toml defines a mapping so that generated bindings use a
 * real std::cell::Cell<T>.
 *
 * Note that while the layout matches, this doesn't have the same ABI as a
 * std::cell::Cell<T>, so values of this type can't be passed over FFI
 * functions.
 */
template <typename T>
class RustCell {
 public:
  RustCell() : mValue() {}
  explicit RustCell(T aValue) : mValue(aValue) {}

  T Get() const { return mValue; }
  void Set(T aValue) { mValue = aValue; }

  // That this API doesn't mirror the API of rust's Cell because all values in
  // C++ effectively act like they're wrapped in Cell<...> already, so this type
  // only exists for FFI purposes.
  T* AsPtr() { return &mValue; }
  const T* AsPtr() const { return &mValue; }

 private:
  T mValue;
};

}  // namespace mozilla

#endif  // mozilla_RustCell_h
