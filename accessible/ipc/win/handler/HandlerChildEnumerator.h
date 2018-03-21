/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#error This code is NOT for internal Gecko use!
#endif // defined(MOZILLA_INTERNAL_API)

#ifndef mozilla_a11y_HandlerChildEnumerator_h
#define mozilla_a11y_HandlerChildEnumerator_h

#include "AccessibleHandler.h"
#include "IUnknownImpl.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace a11y {

class HandlerChildEnumerator final : public IEnumVARIANT
{
public:
  explicit HandlerChildEnumerator(AccessibleHandler* aHandler,
                                  IGeckoBackChannel* aGeckoBackChannel);

  DECL_IUNKNOWN

  // IEnumVARIANT
  STDMETHODIMP Clone(IEnumVARIANT** aPpEnum) override;
  STDMETHODIMP Next(ULONG aCelt, VARIANT* aRgVar,
                    ULONG* aPCeltFetched) override;
  STDMETHODIMP Reset() override;
  STDMETHODIMP Skip(ULONG aCelt) override;

private:
  explicit HandlerChildEnumerator(const HandlerChildEnumerator& aEnumerator);
  ~HandlerChildEnumerator();
  void ClearCache();
  HRESULT MaybeCacheChildren();

  RefPtr<AccessibleHandler> mHandler;
  RefPtr<IGeckoBackChannel> mGeckoBackChannel;
  UniquePtr<VARIANT[]> mChildren;
  ULONG mChildCount;
  ULONG mNextChild;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_HandlerChildEnumerator_h
