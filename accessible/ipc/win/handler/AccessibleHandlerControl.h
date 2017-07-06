/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#error This code is NOT for internal Gecko use!
#endif // defined(MOZILLA_INTERNAL_API)

#ifndef mozilla_a11y_AccessibleHandlerControl_h
#define mozilla_a11y_AccessibleHandlerControl_h

#include "Factory.h"
#include "HandlerData.h"
#include "IUnknownImpl.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/NotNull.h"

namespace mozilla {
namespace a11y {

namespace detail {

class TextChange final
{
public:
  TextChange();
  TextChange(long aIA2UniqueId, bool aIsInsert, NotNull<IA2TextSegment*> aText);
  TextChange(TextChange&& aOther);
  TextChange(const TextChange& aOther);

  TextChange& operator=(TextChange&& aOther);
  TextChange& operator=(const TextChange& aOther);

  ~TextChange();

  HRESULT GetOld(long aIA2UniqueId, NotNull<IA2TextSegment*> aOutOldSegment);
  HRESULT GetNew(long aIA2UniqueId, NotNull<IA2TextSegment*> aOutNewSegment);

private:
  static BSTR BSTRCopy(const BSTR& aIn);
  static HRESULT SegCopy(IA2TextSegment& aDest, const IA2TextSegment& aSrc);

  long mIA2UniqueId;
  bool mIsInsert;
  IA2TextSegment mText;
};

} // namespace detail

class AccessibleHandlerControl final : public IHandlerControl
{
public:
  static HRESULT Create(AccessibleHandlerControl** aOutObject);

  DECL_IUNKNOWN

  // IHandlerControl
  STDMETHODIMP Invalidate() override;
  STDMETHODIMP OnTextChange(long aHwnd, long aIA2UniqueId,
                            VARIANT_BOOL aIsInsert, IA2TextSegment* aText) override;

  uint32_t GetCacheGen() const
  {
    return mCacheGen;
  }

  HRESULT GetNewText(long aIA2UniqueId, NotNull<IA2TextSegment*> aOutNewText);
  HRESULT GetOldText(long aIA2UniqueId, NotNull<IA2TextSegment*> aOutOldText);

  HRESULT GetHandlerTypeInfo(ITypeInfo** aOutTypeInfo);

  HRESULT Register(NotNull<IGeckoBackChannel*> aGecko);

private:
  AccessibleHandlerControl();
  ~AccessibleHandlerControl() = default;

  bool mIsRegistered;
  uint32_t mCacheGen;
  detail::TextChange mTextChange;
  UniquePtr<mscom::RegisteredProxy> mIA2Proxy;
  UniquePtr<mscom::RegisteredProxy> mHandlerProxy;
};

extern mscom::SingletonFactory<AccessibleHandlerControl> gControlFactory;

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_AccessibleHandlerControl_h
