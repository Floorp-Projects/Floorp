/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MimeType_h
#define mozilla_dom_MimeType_h

#include "mozilla/TextUtils.h"
#include "mozilla/UniquePtr.h"
#include "nsTHashMap.h"
#include "nsTArray.h"

template <typename char_type>
struct HashKeyType;
template <>
struct HashKeyType<char16_t> {
  typedef nsStringHashKey HashType;
};
template <>
struct HashKeyType<char> {
  typedef nsCStringHashKey HashType;
};

template <typename char_type>
class TMimeType final {
 private:
  class ParameterValue : public nsTString<char_type> {
   public:
    bool mRequiresQuoting;

    ParameterValue() : mRequiresQuoting(false) {}
  };

  nsTString<char_type> mType;
  nsTString<char_type> mSubtype;
  nsTHashMap<typename HashKeyType<char_type>::HashType, ParameterValue>
      mParameters;
  nsTArray<nsTString<char_type>> mParameterNames;

 public:
  TMimeType(const nsTSubstring<char_type>& aType,
            const nsTSubstring<char_type>& aSubtype)
      : mType(aType), mSubtype(aSubtype) {}

  static mozilla::UniquePtr<TMimeType<char_type>> Parse(
      const nsTSubstring<char_type>& aStr);

  void Serialize(nsTSubstring<char_type>& aStr) const;

  // Returns the `<mType>/<mSubtype>`
  void GetFullType(nsTSubstring<char_type>& aStr) const;

  // @param aName - the name of the parameter
  // @return true if the parameter name is found, false otherwise.
  bool HasParameter(const nsTSubstring<char_type>& aName) const;

  // @param aName - the name of the parameter
  // @param aOutput - will hold the value of the parameter (quoted if necessary)
  // @param aAppend - if true, the method will append to the string;
  //                  otherwise the string is truncated before appending.
  // @return true if the parameter name is found, false otherwise.
  bool GetParameterValue(const nsTSubstring<char_type>& aName,
                         nsTSubstring<char_type>& aOutput,
                         bool aAppend = false) const;

  // @param aName - the name of the parameter
  // @param aValue - the value of the parameter
  void SetParameterValue(const nsTSubstring<char_type>& aName,
                         const nsTSubstring<char_type>& aValue);
};

using MimeType = TMimeType<char16_t>;
using CMimeType = TMimeType<char>;

#endif  // mozilla_dom_MimeType_h
