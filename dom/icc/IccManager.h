/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IccManager_h
#define mozilla_dom_IccManager_h

#include "mozilla/DOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArrayHelpers.h"

namespace mozilla {
namespace dom {

class Icc;
class IccListener;

class IccManager final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IccManager, DOMEventTargetHelper)

  explicit IccManager(nsPIDOMWindow* aWindow);

  void
  Shutdown();

  nsresult
  NotifyIccAdd(const nsAString& aIccId);

  nsresult
  NotifyIccRemove(const nsAString& aIccId);

  IMPL_EVENT_HANDLER(iccdetected)
  IMPL_EVENT_HANDLER(iccundetected)

  void
  GetIccIds(nsTArray<nsString>& aIccIds);

  Icc*
  GetIccById(const nsAString& aIccId) const;

  nsPIDOMWindow*
  GetParentObject() const { return GetOwner(); }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  ~IccManager();

private:
  nsTArray<nsRefPtr<IccListener>> mIccListeners;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_IccManager_h
