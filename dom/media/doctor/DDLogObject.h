/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDLogObject_h_
#define DDLogObject_h_

#include "nsString.h"

namespace mozilla {

// DDLogObject identifies a C++ object by its pointer and its class name (as
// provided in a DDLoggedTypeTrait.)
// Note that a DDLogObject could have the exact same pointer&type as a previous
// one, so extra information is needed to distinguish them, see DDLifetime.
class DDLogObject {
 public:
  // Default-initialization with null pointer.
  DDLogObject() : mTypeName("<unset>"), mPointer(nullptr) {}

  // Construction with given non-null type name and pointer.
  DDLogObject(const char* aTypeName, const void* aPointer)
      : mTypeName(aTypeName), mPointer(aPointer) {
    MOZ_ASSERT(aTypeName);
    MOZ_ASSERT(aPointer);
  }

  // Sets this DDLogObject to an actual object.
  void Set(const char* aTypeName, const void* aPointer) {
    MOZ_ASSERT(aTypeName);
    MOZ_ASSERT(aPointer);
    mTypeName = aTypeName;
    mPointer = aPointer;
  }

  // Object pointer, used for identification purposes only.
  const void* Pointer() const { return mPointer; }

  // Type name. Should only be accessed after non-null pointer initialization.
  const char* TypeName() const {
    MOZ_ASSERT(mPointer);
    return mTypeName;
  }

  bool operator==(const DDLogObject& a) const {
    return mPointer == a.mPointer && (!mPointer || mTypeName == a.mTypeName);
  }

  // Print the type name and pointer, e.g.: "MediaDecoder[136078200]".
  void AppendPrintf(nsCString& mString) const;
  nsCString Printf() const;

 private:
  const char* mTypeName;
  const void* mPointer;
};

}  // namespace mozilla

#endif  // DDLogObject_h_
