/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ProxyAccessible_h
#define mozilla_a11y_ProxyAccessible_h

#include "Accessible.h"
#include "mozilla/a11y/ProxyAccessibleBase.h"
#include "mozilla/a11y/Role.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleTypes.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsRect.h"

#include <oleacc.h>

namespace mozilla {
namespace a11y {

class ProxyAccessible : public ProxyAccessibleBase<ProxyAccessible>
{
public:
  ProxyAccessible(uint64_t aID, ProxyAccessible* aParent,
                  DocAccessibleParent* aDoc, role aRole, uint32_t aInterfaces)
    : ProxyAccessibleBase(aID, aParent, aDoc, aRole, aInterfaces)
    , mSafeToRecurse(true)
  {
    MOZ_COUNT_CTOR(ProxyAccessible);
  }

  ~ProxyAccessible()
  {
    MOZ_COUNT_DTOR(ProxyAccessible);
  }

#include "mozilla/a11y/ProxyAccessibleShared.h"

  bool GetCOMInterface(void** aOutAccessible) const;
  void SetCOMInterface(const RefPtr<IAccessible>& aIAccessible)
  {
    if (aIAccessible) {
      mCOMProxy = aIAccessible;
    } else {
      // If we were supposed to be receiving an interface (hence the call to
      // this function), but the interface turns out to be null, then we're
      // broken for some reason.
      mSafeToRecurse = false;
    }
  }

protected:
  explicit ProxyAccessible(DocAccessibleParent* aThisAsDoc)
    : ProxyAccessibleBase(aThisAsDoc)
  { MOZ_COUNT_CTOR(ProxyAccessible); }

private:
  RefPtr<IAccessible> mCOMProxy;
  bool                mSafeToRecurse;
};

}
}

#endif
