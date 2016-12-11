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

// Indicates whether a style resolution should be considered to consume the style
// for the construction of a layout frame, or whether the caller is just
// peeking.
enum class ConsumeStyleBehavior {
  Consume,
  DontConsume,
};

// Indicates whether the Servo style system should expect the style on an element
// to have already been resolved (i.e. via a parallel traversal), or whether it
// may be lazily computed.
enum class LazyComputeBehavior {
  Allow,
  Assert,
};

// Indicates whether the Servo style system should perform normal processing or
// whether it should only process unstyled children of the root and their
// descendants.
enum class TraversalRootBehavior {
  Normal,
  UnstyledChildrenOnly,
};

} // namespace mozilla

#endif // mozilla_ServoTypes_h
