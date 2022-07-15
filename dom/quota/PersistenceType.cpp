/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PersistenceType.h"

#include <utility>
#include "nsIFile.h"
#include "nsLiteralString.h"
#include "nsString.h"

namespace mozilla::dom::quota {

namespace {

constexpr auto kPersistentCString = "persistent"_ns;
constexpr auto kTemporaryCString = "temporary"_ns;
constexpr auto kDefaultCString = "default"_ns;

constexpr auto kPermanentString = u"permanent"_ns;
constexpr auto kTemporaryString = u"temporary"_ns;
constexpr auto kDefaultString = u"default"_ns;

static_assert(PERSISTENCE_TYPE_PERSISTENT == 0 &&
                  PERSISTENCE_TYPE_TEMPORARY == 1 &&
                  PERSISTENCE_TYPE_DEFAULT == 2 &&
                  PERSISTENCE_TYPE_INVALID == 3,
              "Incorrect enum values!");

template <PersistenceType type>
struct PersistenceTypeTraits;

template <>
struct PersistenceTypeTraits<PERSISTENCE_TYPE_PERSISTENT> {
  template <typename T>
  static T To();

  static bool From(const nsACString& aString) {
    return aString == kPersistentCString;
  }

  static bool From(const int32_t aInt32) { return aInt32 == 0; }

  static bool From(nsIFile& aFile) {
    nsAutoString leafName;
    MOZ_ALWAYS_SUCCEEDS(aFile.GetLeafName(leafName));
    return leafName == kPermanentString;
  }
};

template <>
nsLiteralCString
PersistenceTypeTraits<PERSISTENCE_TYPE_PERSISTENT>::To<nsLiteralCString>() {
  return kPersistentCString;
}

template <>
struct PersistenceTypeTraits<PERSISTENCE_TYPE_TEMPORARY> {
  template <typename T>
  static T To();

  static bool From(const nsACString& aString) {
    return aString == kTemporaryCString;
  }

  static bool From(const int32_t aInt32) { return aInt32 == 1; }

  static bool From(nsIFile& aFile) {
    nsAutoString leafName;
    MOZ_ALWAYS_SUCCEEDS(aFile.GetLeafName(leafName));
    return leafName == kTemporaryString;
  }
};

template <>
nsLiteralCString
PersistenceTypeTraits<PERSISTENCE_TYPE_TEMPORARY>::To<nsLiteralCString>() {
  return kTemporaryCString;
}

template <>
struct PersistenceTypeTraits<PERSISTENCE_TYPE_DEFAULT> {
  template <typename T>
  static T To();

  static bool From(const nsACString& aString) {
    return aString == kDefaultCString;
  }

  static bool From(const int32_t aInt32) { return aInt32 == 2; }

  static bool From(nsIFile& aFile) {
    nsAutoString leafName;
    MOZ_ALWAYS_SUCCEEDS(aFile.GetLeafName(leafName));
    return leafName == kDefaultString;
  }
};

template <>
nsLiteralCString
PersistenceTypeTraits<PERSISTENCE_TYPE_DEFAULT>::To<nsLiteralCString>() {
  return kDefaultCString;
}

template <typename T>
Maybe<T> TypeTo_impl(const PersistenceType aPersistenceType) {
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_PERSISTENT:
      return Some(PersistenceTypeTraits<PERSISTENCE_TYPE_PERSISTENT>::To<T>());

    case PERSISTENCE_TYPE_TEMPORARY:
      return Some(PersistenceTypeTraits<PERSISTENCE_TYPE_TEMPORARY>::To<T>());

    case PERSISTENCE_TYPE_DEFAULT:
      return Some(PersistenceTypeTraits<PERSISTENCE_TYPE_DEFAULT>::To<T>());

    default:
      return Nothing();
  }
}

template <typename T>
Maybe<PersistenceType> TypeFrom_impl(T& aData) {
  if (PersistenceTypeTraits<PERSISTENCE_TYPE_PERSISTENT>::From(aData)) {
    return Some(PERSISTENCE_TYPE_PERSISTENT);
  }

  if (PersistenceTypeTraits<PERSISTENCE_TYPE_TEMPORARY>::From(aData)) {
    return Some(PERSISTENCE_TYPE_TEMPORARY);
  }

  if (PersistenceTypeTraits<PERSISTENCE_TYPE_DEFAULT>::From(aData)) {
    return Some(PERSISTENCE_TYPE_DEFAULT);
  }

  return Nothing();
}

void BadPersistenceType() { MOZ_CRASH("Bad persistence type value!"); }

}  // namespace

bool IsValidPersistenceType(const PersistenceType aPersistenceType) {
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_PERSISTENT:
    case PERSISTENCE_TYPE_TEMPORARY:
    case PERSISTENCE_TYPE_DEFAULT:
      return true;

    default:
      return false;
  }
}

bool IsBestEffortPersistenceType(const PersistenceType aPersistenceType) {
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_TEMPORARY:
    case PERSISTENCE_TYPE_DEFAULT:
      return true;

    case PERSISTENCE_TYPE_PERSISTENT:
    case PERSISTENCE_TYPE_INVALID:
    default:
      return false;
  }
}

nsLiteralCString PersistenceTypeToString(
    const PersistenceType aPersistenceType) {
  const auto maybeString = TypeTo_impl<nsLiteralCString>(aPersistenceType);
  if (maybeString.isNothing()) {
    BadPersistenceType();
  }
  return maybeString.value();
}

Maybe<PersistenceType> PersistenceTypeFromString(const nsACString& aString,
                                                 const fallible_t&) {
  return TypeFrom_impl(aString);
}

PersistenceType PersistenceTypeFromString(const nsACString& aString) {
  const auto maybePersistenceType = TypeFrom_impl(aString);
  if (maybePersistenceType.isNothing()) {
    BadPersistenceType();
  }
  return maybePersistenceType.value();
}

Maybe<PersistenceType> PersistenceTypeFromInt32(const int32_t aInt32,
                                                const fallible_t&) {
  return TypeFrom_impl(aInt32);
}

Maybe<PersistenceType> PersistenceTypeFromFile(nsIFile& aFile,
                                               const fallible_t&) {
  return TypeFrom_impl(aFile);
}

}  // namespace mozilla::dom::quota
