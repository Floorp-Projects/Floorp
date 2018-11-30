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

  T Get() const { return mValue; }

 private:
  T mValue;
};

}  // namespace mozilla

#endif  // mozilla_RustCell_h
