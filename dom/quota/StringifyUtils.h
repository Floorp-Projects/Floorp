/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_stringifyutils_h__
#define mozilla_dom_quota_stringifyutils_h__

#include "mozilla/ThreadLocal.h"
#include "nsTHashSet.h"
#include "nsLiteralString.h"

namespace mozilla {

// Use these constants for common delimiters. Note that `Stringify` already
// encloses each call to `DoStringify` with `kStringify[Start/End]Instance`.
constexpr auto kStringifyDelimiter = "|"_ns;
constexpr auto kStringifyStartSet = "["_ns;
constexpr auto kStringifyEndSet = "]"_ns;
constexpr auto kStringifyStartInstance = "{"_ns;
constexpr auto kStringifyEndInstance = "}"_ns;

// A Stringifyable class provides a method `Stringify` that returns a string
// representation of the class content.
//
// It's content is just appended by the override of `DoStringify` but
// `Stringifyable` ensures that we won't call `DoStringify` twice for the same
// call and instance. A single `DoStringify` function thus needs not to be
// aware of "should I `Stringify` the content of this member or not", it can
// just do it always.
//
// Stringifyable does not bloat the object by additional members but uses
// thread local memory only when needed.
class Stringifyable {
 public:
  void Stringify(nsACString& aData);

  static void InitTLS();

 private:
  virtual void DoStringify(nsACString& aData) = 0;

  bool IsActive();
  void SetActive(bool aIsActive);

  static MOZ_THREAD_LOCAL(nsTHashSet<Stringifyable*>*)
      sActiveStringifyableInstances;
};

}  // namespace mozilla

#endif  // mozilla_dom_quota_stringifyutils_h__
