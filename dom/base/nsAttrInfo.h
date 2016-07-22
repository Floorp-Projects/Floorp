/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAttrInfo_h__
#define nsAttrInfo_h__

#include "mozilla/Assertions.h"

class nsAttrName;
class nsAttrValue;

/**
 * Struct that stores info on an attribute. The name and value must
 * either both be null or both be non-null.
 */
struct nsAttrInfo
{
  nsAttrInfo()
    : mName(nullptr)
    , mValue(nullptr)
  {
  }

  nsAttrInfo(const nsAttrName* aName, const nsAttrValue* aValue)
    : mName(aName)
    , mValue(aValue)
  {
    MOZ_ASSERT_IF(aName, aValue);
  }

  nsAttrInfo(const nsAttrInfo& aOther)
    : mName(aOther.mName)
    , mValue(aOther.mValue)
  {
  }

  const nsAttrName* mName;
  const nsAttrValue* mValue;
};

#endif
