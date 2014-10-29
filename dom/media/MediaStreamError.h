/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaStreamError_h
#define mozilla_dom_MediaStreamError_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsWrapperCache.h"
#include "js/TypeDecls.h"
#include "nsPIDOMWindow.h"
#include "nsRefPtr.h"

#if defined(XP_WIN) && defined(GetMessage)
#undef GetMessage
#endif

namespace mozilla {
namespace dom {

#define MOZILLA_DOM_MEDIASTREAMERROR_IMPLEMENTATION_IID \
{ 0x95fa29aa, 0x0cc2, 0x4698, \
 { 0x9d, 0xa9, 0xf2, 0xeb, 0x03, 0x91, 0x0b, 0xd1 } }

class MediaStreamError;
}

class BaseMediaMgrError
{
  friend class dom::MediaStreamError;
protected:
  BaseMediaMgrError(const nsAString& aName,
                    const nsAString& aMessage,
                    const nsAString& aConstraintName);
  const nsString mName;
  nsString mMessage;
  const nsString mConstraintName;
};

class MediaMgrError MOZ_FINAL : public nsISupports,
                                public BaseMediaMgrError
{
public:
  MediaMgrError(const nsAString& aName,
                const nsAString& aMessage =  EmptyString(),
                const nsAString& aConstraintName =  EmptyString())
  : BaseMediaMgrError(aName, aMessage, aConstraintName) {}

  NS_DECL_THREADSAFE_ISUPPORTS

private:
  ~MediaMgrError() {}
};

namespace dom {
class MediaStreamError MOZ_FINAL : public nsISupports,
                                   public BaseMediaMgrError,
                                   public nsWrapperCache
{
public:
  MediaStreamError(nsPIDOMWindow* aParent,
                   const nsAString& aName,
                   const nsAString& aMessage = EmptyString(),
                   const nsAString& aConstraintName =  EmptyString());

  MediaStreamError(nsPIDOMWindow* aParent,
                   const BaseMediaMgrError& aOther)
  : BaseMediaMgrError(aOther.mName, aOther.mMessage, aOther.mConstraintName)
  , mParent(aParent) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaStreamError)
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOM_MEDIASTREAMERROR_IMPLEMENTATION_IID)

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  nsPIDOMWindow* GetParentObject() const
  {
    return mParent;
  }
  void GetName(nsAString& aName) const;
  void GetMessage(nsAString& aMessage) const;
  void GetConstraintName(nsAString& aConstraintName) const;

private:
  virtual ~MediaStreamError() {}

  nsRefPtr<nsPIDOMWindow> mParent;
};

NS_DEFINE_STATIC_IID_ACCESSOR(MediaStreamError,
                              MOZILLA_DOM_MEDIASTREAMERROR_IMPLEMENTATION_IID)
} // namespace dom
} // namespace mozilla

#endif
