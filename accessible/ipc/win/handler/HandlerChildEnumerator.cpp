/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#error This code is NOT for internal Gecko use!
#endif // defined(MOZILLA_INTERNAL_API)

#include "HandlerChildEnumerator.h"
#include "HandlerTextLeaf.h"
#include "mozilla/Assertions.h"

namespace mozilla {
namespace a11y {

HandlerChildEnumerator::HandlerChildEnumerator(AccessibleHandler* aHandler,
    IGeckoBackChannel* aGeckoBackChannel)
  : mHandler(aHandler)
  , mGeckoBackChannel(aGeckoBackChannel)
  , mChildCount(0)
  , mNextChild(0)
{
  MOZ_ASSERT(aHandler);
  MOZ_ASSERT(aGeckoBackChannel);
}

HandlerChildEnumerator::HandlerChildEnumerator(
    const HandlerChildEnumerator& aEnumerator)
  : mHandler(aEnumerator.mHandler)
  , mGeckoBackChannel(aEnumerator.mGeckoBackChannel)
  , mChildCount(aEnumerator.mChildCount)
  , mNextChild(aEnumerator.mNextChild)
{
  if (mChildCount == 0) {
    return;
  }
  mChildren = MakeUnique<VARIANT[]>(mChildCount);
  CopyMemory(mChildren.get(), aEnumerator.mChildren.get(),
             sizeof(VARIANT) * mChildCount);
  for (ULONG index = 0; index < mChildCount; ++index) {
    mChildren[index].pdispVal->AddRef();
  }
}

HandlerChildEnumerator::~HandlerChildEnumerator()
{
  ClearCache();
}

void
HandlerChildEnumerator::ClearCache()
{
  if (!mChildren) {
    return;
  }

  for (ULONG index = 0; index < mChildCount; ++index) {
    mChildren[index].pdispVal->Release();
  }

  mChildren = nullptr;
  mChildCount = 0;
}

HRESULT
HandlerChildEnumerator::MaybeCacheChildren()
{
  if (mChildren) {
    // Already cached.
    return S_OK;
  }

  AccChildData* children;
  HRESULT hr = mGeckoBackChannel->get_AllChildren(&children, &mChildCount);
  if (FAILED(hr)) {
    mChildCount = 0;
    ClearCache();
    return hr;
  }

  HWND hwnd = nullptr;
  hr = mHandler->get_windowHandle(&hwnd);
  MOZ_ASSERT(SUCCEEDED(hr));

  RefPtr<IDispatch> parent;
  hr = mHandler->QueryInterface(IID_IDispatch, getter_AddRefs(parent));
  MOZ_ASSERT(SUCCEEDED(hr));

  mChildren = MakeUnique<VARIANT[]>(mChildCount);
  for (ULONG index = 0; index < mChildCount; ++index) {
    AccChildData& data = children[index];
    VARIANT& child = mChildren[index];
    if (data.mAccessible) {
      RefPtr<IDispatch> disp;
      hr = data.mAccessible->QueryInterface(IID_IDispatch,
                                            getter_AddRefs(disp));
      data.mAccessible->Release();
      MOZ_ASSERT(SUCCEEDED(hr));
      if (FAILED(hr)) {
        child.vt = VT_EMPTY;
        continue;
      }
      child.vt = VT_DISPATCH;
      disp.forget(&child.pdispVal);
    } else {
      // Text leaf.
      RefPtr<IDispatch> leaf(
        new HandlerTextLeaf(parent, index, hwnd, data));
      child.vt = VT_DISPATCH;
      leaf.forget(&child.pdispVal);
    }
  }

  ::CoTaskMemFree(children);
  return S_OK;
}

IMPL_IUNKNOWN_QUERY_HEAD(HandlerChildEnumerator)
IMPL_IUNKNOWN_QUERY_IFACE(IEnumVARIANT)
IMPL_IUNKNOWN_QUERY_TAIL_AGGREGATED(mHandler)

/*** IEnumVARIANT ***/

HRESULT
HandlerChildEnumerator::Clone(IEnumVARIANT** aPpEnum)
{
  RefPtr<HandlerChildEnumerator> newEnum(new HandlerChildEnumerator(*this));
  newEnum.forget(aPpEnum);
  return S_OK;
}

HRESULT
HandlerChildEnumerator::Next(ULONG aCelt, VARIANT* aRgVar,
                             ULONG* aPCeltFetched)
{
  if (!aRgVar || aCelt == 0) {
    return E_INVALIDARG;
  }

  HRESULT hr = MaybeCacheChildren();
  if (FAILED(hr)) {
    return hr;
  }

  for (ULONG index = 0; index < aCelt; ++index) {
    if (mNextChild >= mChildCount) {
      // Less elements remaining than were requested.
      if (aPCeltFetched) {
        *aPCeltFetched = index;
      }
      return S_FALSE;
    }
    aRgVar[index] = mChildren[mNextChild];
    aRgVar[index].pdispVal->AddRef();
    ++mNextChild;
  }
  *aPCeltFetched = aCelt;
  return S_OK;
}

HRESULT
HandlerChildEnumerator::Reset()
{
  mNextChild = 0;
  ClearCache();
  return S_OK;
}

HRESULT
HandlerChildEnumerator::Skip(ULONG aCelt)
{
  mNextChild += aCelt;
  if (mNextChild > mChildCount) {
    // Less elements remaining than the client requested to skip.
    return S_FALSE;
  }
  return S_OK;
}

} // namespace a11y
} // namespace mozilla
