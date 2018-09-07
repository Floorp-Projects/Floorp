/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MimeType_h
#define mozilla_dom_MimeType_h

#include "mozilla/TextUtils.h"
#include "mozilla/UniquePtr.h"
#include "nsDataHashtable.h"
#include "nsTArray.h"

template<typename char_type> struct HashKeyType;
template<> struct HashKeyType<char16_t> { typedef nsStringHashKey HashType; };
template<> struct HashKeyType<char> { typedef nsCStringHashKey HashType; };


template <typename char_type>
class TMimeType final
{
private:
  class ParameterValue : public nsTString<char_type>
  {
  public:
    bool mRequiresQuoting;

    ParameterValue()
      : mRequiresQuoting(false)
    {}
  };

  nsTString<char_type> mType;
  nsTString<char_type> mSubtype;
  nsDataHashtable<typename HashKeyType<char_type>::HashType, ParameterValue> mParameters;
  nsTArray<nsTString<char_type>> mParameterNames;

public:
  TMimeType(const nsTSubstring<char_type>& aType, const nsTSubstring<char_type>& aSubtype)
    : mType(aType), mSubtype(aSubtype)
  {}

  static mozilla::UniquePtr<TMimeType<char_type>> Parse(const nsTSubstring<char_type>& aStr);

  void Serialize(nsTSubstring<char_type>& aStr) const;
};

using MimeType = TMimeType<char16_t>;
using CMimeType = TMimeType<char>;

#endif // mozilla_dom_MimeType_h
