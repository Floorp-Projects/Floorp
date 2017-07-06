/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#error This code is NOT for internal Gecko use!
#endif // defined(MOZILLA_INTERNAL_API)

#include "AccessibleHandlerControl.h"

#include "AccessibleHandler.h"

#include "AccessibleEventId.h"

#include "mozilla/Move.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace a11y {

mscom::SingletonFactory<AccessibleHandlerControl> gControlFactory;

namespace detail {

TextChange::TextChange()
  : mIA2UniqueId(0)
  , mIsInsert(false)
  , mText()
{
}

TextChange::TextChange(long aIA2UniqueId, bool aIsInsert,
                       NotNull<IA2TextSegment*> aText)
  : mIA2UniqueId(aIA2UniqueId)
  , mIsInsert(aIsInsert)
  , mText{BSTRCopy(aText->text), aText->start, aText->end}
{
}

TextChange::TextChange(TextChange&& aOther)
  : mText()
{
  *this = Move(aOther);
}

TextChange::TextChange(const TextChange& aOther)
  : mText()
{
  *this = aOther;
}

TextChange&
TextChange::operator=(TextChange&& aOther)
{
  mIA2UniqueId = aOther.mIA2UniqueId;
  mIsInsert = aOther.mIsInsert;
  aOther.mIA2UniqueId = 0;
  ::SysFreeString(mText.text);
  mText = aOther.mText;
  aOther.mText.text = nullptr;
  return *this;
}

TextChange&
TextChange::operator=(const TextChange& aOther)
{
  mIA2UniqueId = aOther.mIA2UniqueId;
  mIsInsert = aOther.mIsInsert;
  ::SysFreeString(mText.text);
  mText = {BSTRCopy(aOther.mText.text), aOther.mText.start, aOther.mText.end};
  return *this;
}

TextChange::~TextChange()
{
  ::SysFreeString(mText.text);
}

HRESULT
TextChange::GetOld(long aIA2UniqueId, NotNull<IA2TextSegment*> aOutOldSegment)
{
  if (mIsInsert || aIA2UniqueId != mIA2UniqueId) {
    return S_OK;
  }

  return SegCopy(*aOutOldSegment, mText);
}

HRESULT
TextChange::GetNew(long aIA2UniqueId, NotNull<IA2TextSegment*> aOutNewSegment)
{
  if (!mIsInsert || aIA2UniqueId != mIA2UniqueId) {
    return S_OK;
  }

  return SegCopy(*aOutNewSegment, mText);
}

/* static */ BSTR
TextChange::BSTRCopy(const BSTR& aIn)
{
  return ::SysAllocStringLen(aIn, ::SysStringLen(aIn));
}

/* static */ HRESULT
TextChange::SegCopy(IA2TextSegment& aDest, const IA2TextSegment& aSrc)
{
  aDest = {BSTRCopy(aSrc.text), aSrc.start, aSrc.end};
  if (aSrc.text && !aDest.text) {
    return E_OUTOFMEMORY;
  }
  if (!::SysStringLen(aDest.text)) {
    return S_FALSE;
  }
  return S_OK;
}

} // namespace detail

HRESULT
AccessibleHandlerControl::Create(AccessibleHandlerControl** aOutObject)
{
  if (!aOutObject) {
    return E_INVALIDARG;
  }

  RefPtr<AccessibleHandlerControl> ctl(new AccessibleHandlerControl());
  ctl.forget(aOutObject);
  return S_OK;
}

AccessibleHandlerControl::AccessibleHandlerControl()
  : mIsRegistered(false)
  , mCacheGen(0)
  , mIA2Proxy(mscom::RegisterProxy(L"ia2marshal.dll"))
  , mHandlerProxy(mscom::RegisterProxy())
{
  MOZ_ASSERT(mIA2Proxy);
}

IMPL_IUNKNOWN1(AccessibleHandlerControl, IHandlerControl)

HRESULT
AccessibleHandlerControl::Invalidate()
{
  ++mCacheGen;
  return S_OK;
}

HRESULT
AccessibleHandlerControl::OnTextChange(long aHwnd, long aIA2UniqueId,
                                       VARIANT_BOOL aIsInsert,
                                       IA2TextSegment* aText)
{
  if (!aText) {
    return E_INVALIDARG;
  }

  mTextChange = detail::TextChange(aIA2UniqueId, aIsInsert, WrapNotNull(aText));
  NotifyWinEvent(aIsInsert ? IA2_EVENT_TEXT_INSERTED : IA2_EVENT_TEXT_REMOVED,
                 reinterpret_cast<HWND>(static_cast<uintptr_t>(aHwnd)),
                 OBJID_CLIENT, aIA2UniqueId);
  return S_OK;
}

HRESULT
AccessibleHandlerControl::GetNewText(long aIA2UniqueId,
                                     NotNull<IA2TextSegment*> aOutNewText)
{
  return mTextChange.GetNew(aIA2UniqueId, aOutNewText);
}

HRESULT
AccessibleHandlerControl::GetOldText(long aIA2UniqueId,
                                     NotNull<IA2TextSegment*> aOutOldText)
{
  return mTextChange.GetOld(aIA2UniqueId, aOutOldText);
}

HRESULT
AccessibleHandlerControl::GetHandlerTypeInfo(ITypeInfo** aOutTypeInfo)
{
  if (!mHandlerProxy) {
    return E_UNEXPECTED;
  }

  return mHandlerProxy->GetTypeInfoForGuid(CLSID_AccessibleHandler,
                                           aOutTypeInfo);
}

HRESULT
AccessibleHandlerControl::Register(NotNull<IGeckoBackChannel*> aGecko)
{
  if (mIsRegistered) {
    return S_OK;
  }

  long pid = static_cast<long>(::GetCurrentProcessId());
  HRESULT hr = aGecko->put_HandlerControl(pid, this);
  mIsRegistered = SUCCEEDED(hr);
  MOZ_ASSERT(mIsRegistered);
  return hr;
}

} // namespace a11y
} // namespace mozilla
