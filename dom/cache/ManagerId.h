/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_ManagerId_h
#define mozilla_dom_cache_ManagerId_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/cache/Types.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {
namespace cache {

class ManagerId final
{
public:
  // nsTArray comparator that compares by value instead of pointer values.
  class MOZ_STACK_CLASS Comparator final
  {
  public:
    bool Equals(ManagerId *aA, ManagerId* aB) const { return *aA == *aB; }
    bool LessThan(ManagerId *aA, ManagerId* aB) const { return *aA < *aB; }
  };

  // Main thread only
  static nsresult Create(nsIPrincipal* aPrincipal, ManagerId** aManagerIdOut);

  // Main thread only
  already_AddRefed<nsIPrincipal> Principal() const;

  const nsACString& Origin() const { return mOrigin; }

  bool operator==(const ManagerId& aOther) const
  {
    return mOrigin == aOther.mOrigin &&
           mAppId == aOther.mAppId &&
           mInBrowserElement == aOther.mInBrowserElement;
  }

  bool operator<(const ManagerId& aOther) const
  {
    return mOrigin < aOther.mOrigin ||
           (mOrigin == aOther.mOrigin && mAppId < aOther.mAppId) ||
           (mOrigin == aOther.mOrigin && mAppId == aOther.mAppId &&
            mInBrowserElement < aOther.mInBrowserElement);
  }

private:
  ManagerId(nsIPrincipal* aPrincipal, const nsACString& aOrigin,
            uint32_t aAppId, bool aInBrowserElement);
  ~ManagerId();

  ManagerId(const ManagerId&) = delete;
  ManagerId& operator=(const ManagerId&) = delete;

  // only accessible on main thread
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // immutable to allow threadsfe access
  const nsCString mOrigin;
  const uint32_t mAppId;
  const bool mInBrowserElement;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::cache::ManagerId)
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_ManagerId_h
