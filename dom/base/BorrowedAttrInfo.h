/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BorrowedAttrInfo_h__
#define BorrowedAttrInfo_h__

#include "mozilla/Assertions.h"

class nsAttrName;
class nsAttrValue;

namespace mozilla {
namespace dom {

/**
 * Struct that stores info on an attribute. The name and value must either both
 * be null or both be non-null.
 *
 * Note that, just as the pointers returned by GetAttrNameAt, the pointers that
 * this struct hold are only valid until the element or its attributes are
 * mutated (directly or via script).
 */
struct BorrowedAttrInfo
{
  BorrowedAttrInfo()
    : mName(nullptr)
    , mValue(nullptr)
  {
  }

  BorrowedAttrInfo(const nsAttrName* aName, const nsAttrValue* aValue);

  BorrowedAttrInfo(const BorrowedAttrInfo& aOther);

  const nsAttrName* mName;
  const nsAttrValue* mValue;

  explicit operator bool() const { return mName != nullptr; }
};

} // namespace dom
} // namespace mozilla
#endif
