/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WindowFeatures_h
#define mozilla_dom_WindowFeatures_h

#include "nsString.h"
#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/HashTable.h"   // mozilla::HashMap

#include "nsStringFwd.h"  // nsCString, nsACString, nsAutoCString, nsLiteralCString
#include "nsTStringHasher.h"  // mozilla::DefaultHasher<nsCString>

namespace mozilla {
namespace dom {

// Represents `tokenizedFeatures` in
// https://html.spec.whatwg.org/multipage/window-object.html#concept-window-open-features-tokenize
// with accessor methods for values.
class WindowFeatures {
 public:
  WindowFeatures() = default;

  WindowFeatures(const WindowFeatures& aOther) = delete;
  WindowFeatures& operator=(const WindowFeatures& aOther) = delete;

  WindowFeatures(WindowFeatures&& aOther) = delete;
  WindowFeatures& operator=(WindowFeatures&& aOther) = delete;

  // Tokenizes `aFeatures` and stores the result map in member field.
  // This should be called at the begining, only once.
  //
  // Returns true if successfully tokenized, false otherwise.
  bool Tokenize(const nsACString& aFeatures);

  // Returns true if the `aName` feature is specified.
  template <size_t N>
  bool Exists(const char (&aName)[N]) const {
    MOZ_ASSERT(IsLowerCase(aName));
    nsLiteralCString name(aName);
    return tokenizedFeatures_.has(name);
  }

  // Returns string value of `aName` feature.
  // The feature must exist.
  template <size_t N>
  const nsCString& Get(const char (&aName)[N]) const {
    MOZ_ASSERT(IsLowerCase(aName));
    nsLiteralCString name(aName);
    auto p = tokenizedFeatures_.lookup(name);
    MOZ_ASSERT(p.found());

    return p->value();
  }

  // Returns integer value of `aName` feature.
  // The feature must exist.
  template <size_t N>
  int32_t GetInt(const char (&aName)[N]) const {
    const nsCString& value = Get(aName);
    return ParseIntegerWithFallback(value);
  }

  // Returns bool value of `aName` feature.
  // The feature must exist.
  template <size_t N>
  bool GetBool(const char (&aName)[N]) const {
    const nsCString& value = Get(aName);
    return ParseBool(value);
  }

  // Returns bool value of `aName` feature.
  // If the feature doesn't exist, returns `aDefault`.
  //
  // If `aPresenceFlag` is provided and the feature exists, it's set to `true`.
  // (note that the value isn't overwritten if the feature doesn't exist)
  template <size_t N>
  bool GetBoolWithDefault(const char (&aName)[N], bool aDefault,
                          bool* aPresenceFlag = nullptr) const {
    MOZ_ASSERT(IsLowerCase(aName));
    nsLiteralCString name(aName);
    auto p = tokenizedFeatures_.lookup(name);
    if (p.found()) {
      if (aPresenceFlag) {
        *aPresenceFlag = true;
      }
      return ParseBool(p->value());
    }
    return aDefault;
  }

  // Remove the feature from the map.
  template <size_t N>
  void Remove(const char (&aName)[N]) {
    MOZ_ASSERT(IsLowerCase(aName));
    nsLiteralCString name(aName);
    tokenizedFeatures_.remove(name);
  }

  // Returns true if there was no feature specified, or all features are
  // removed by `Remove`.
  //
  // Note that this can be true even if `aFeatures` parameter of `Tokenize`
  // is not empty, in case it contains no feature with non-empty name.
  bool IsEmpty() const { return tokenizedFeatures_.empty(); }

  // Stringify the map into `aOutput`.
  // The result can be parsed again with `Tokenize`.
  void Stringify(nsAutoCString& aOutput);

 private:
#ifdef DEBUG
  // Returns true if `text` does not contain any character that gets modified by
  // `ToLowerCase`.
  static bool IsLowerCase(const char* text);
#endif

  static int32_t ParseIntegerWithFallback(const nsCString& aValue);
  static bool ParseBool(const nsCString& aValue);

  // A map from feature name to feature value.
  // If value is not provided, it's empty string.
  mozilla::HashMap<nsCString, nsCString> tokenizedFeatures_;
};

}  // namespace dom
}  // namespace mozilla

#endif  // #ifndef mozilla_dom_WindowFeatures_h
