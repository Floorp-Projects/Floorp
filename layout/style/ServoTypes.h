/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoTypes_h
#define mozilla_ServoTypes_h

/*
 * Type definitions used to interact with Servo. This gets included by nsINode,
 * so don't add significant include dependencies to this file.
 */

struct ServoNodeData;
namespace mozilla {

/*
 * Replaced types. These get mapped to associated Servo types in bindgen.
 */

template<typename T>
struct ServoUnsafeCell {
  T value;

  // Ensure that primitive types (i.e. pointers) get zero-initialized.
  ServoUnsafeCell() : value() {};
};

template<typename T>
struct ServoCell {
  ServoUnsafeCell<T> value;
  T Get() const { return value.value; }
  void Set(T arg) { value.value = arg; }
  ServoCell() : value() {};
};

} // namespace mozilla

#endif // mozilla_ServoTypes_h
