/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BasicSymmetricKeyAlgorithm_h
#define mozilla_dom_BasicSymmetricKeyAlgorithm_h

#include "mozilla/dom/KeyAlgorithm.h"

namespace mozilla {
namespace dom {

class BasicSymmetricKeyAlgorithm : public KeyAlgorithm
{
public:
  BasicSymmetricKeyAlgorithm(nsIGlobalObject* aGlobal, const nsString& aName, uint16_t aLength)
    : KeyAlgorithm(aGlobal, aName)
    , mLength(aLength)
  {}

  ~BasicSymmetricKeyAlgorithm()
  {}

  uint16_t Length() const
  {
    return mLength;
  }

protected:
  uint16_t mLength;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BasicSymmetricKeyAlgorithm_h
