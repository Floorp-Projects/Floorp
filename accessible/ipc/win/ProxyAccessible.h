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
                  DocAccessibleParent* aDoc, role aRole, uint32_t aInterfaces,
                  const RefPtr<IAccessible>& aIAccessible)
    : ProxyAccessibleBase(aID, aParent, aDoc, aRole, aInterfaces)
    , mCOMProxy(aIAccessible)

  {
    MOZ_COUNT_CTOR(ProxyAccessible);
  }

  ~ProxyAccessible()
  {
    MOZ_COUNT_DTOR(ProxyAccessible);
  }

#include "mozilla/a11y/ProxyAccessibleShared.h"

  bool GetCOMInterface(void** aOutAccessible) const;

protected:
  explicit ProxyAccessible(DocAccessibleParent* aThisAsDoc)
    : ProxyAccessibleBase(aThisAsDoc)
  { MOZ_COUNT_CTOR(ProxyAccessibleBase); }

  void SetCOMInterface(const RefPtr<IAccessible>& aIAccessible)
  { mCOMProxy = aIAccessible; }

private:
  RefPtr<IAccessible> mCOMProxy;
};

}
}

#endif
