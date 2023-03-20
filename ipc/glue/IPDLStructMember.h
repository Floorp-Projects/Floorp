/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_IPDLStructMember_h
#define mozilla_ipc_IPDLStructMember_h

#include <type_traits>
#include <utility>
#include "mozilla/Attributes.h"

namespace mozilla::ipc {

// For types which are trivially default constructible, like `int`, force the
// value constructor by wrapping the type in this struct.  This is an
// implementation detail which will be hidden by the generated IPDL compiler
// code.
template <typename T>
struct IPDLStructMemberWrapper {
  template <typename... Args>
  MOZ_IMPLICIT IPDLStructMemberWrapper(Args&&... aArgs)
      : mVal(std::forward<Args>(aArgs)...) {}
  MOZ_IMPLICIT operator T&() { return mVal; }
  MOZ_IMPLICIT operator const T&() const { return mVal; }
  T mVal{};
};

template <typename T>
using IPDLStructMember =
    std::conditional_t<std::is_trivially_default_constructible_v<T>,
                       IPDLStructMemberWrapper<T>, T>;

}  // namespace mozilla::ipc

#endif  // mozilla_ipc_IPDLStructMember_h
