/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RustTypes_h
#define mozilla_dom_RustTypes_h

#include "nsDebug.h"

#define STATE_HELPERS(Type)                                    \
  using InternalType = decltype(bits);                         \
                                                               \
  constexpr Type() : bits{0} {}                                \
                                                               \
  explicit constexpr Type(InternalType aBits) : bits{aBits} {} \
                                                               \
  bool IsEmpty() const { return bits == 0; }                   \
                                                               \
  bool HasAtLeastOneOfStates(Type aStates) const {             \
    return bool((*this) & aStates);                            \
  }                                                            \
                                                               \
  bool HasState(Type aState) const {                           \
    NS_ASSERTION(!(aState.bits & (aState.bits - 1)),           \
                 "When calling HasState, "                     \
                 "argument has to contain only one state!");   \
    return HasAtLeastOneOfStates(aState);                      \
  }                                                            \
                                                               \
  bool HasAllStates(Type aStates) const {                      \
    return ((*this) & aStates) == aStates;                     \
  }                                                            \
                                                               \
  InternalType GetInternalValue() const { return bits; }

#include "mozilla/dom/GeneratedElementDocumentState.h"

#endif
