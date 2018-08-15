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

class MimeType final
{
private:
  class ParameterValue : public nsString
  {
  public:
    bool mRequiresQuoting;

    ParameterValue()
      : mRequiresQuoting(false)
    {}
  };

  nsString mType;
  nsString mSubtype;
  nsDataHashtable<nsStringHashKey, ParameterValue> mParameters;
  nsTArray<nsString> mParameterNames;

public:
  MimeType(const nsAString& aType, const nsAString& aSubtype)
    : mType(aType), mSubtype(aSubtype)
  {}

  static mozilla::UniquePtr<MimeType> Parse(const nsAString& aStr);
  void Serialize(nsAString& aStr) const;
};

#endif // mozilla_dom_MimeType_h
