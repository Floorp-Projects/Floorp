/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationAvailability_h
#define mozilla_dom_PresentationAvailability_h

#include "mozilla/DOMEventTargetHelper.h"
#include "nsIPresentationListener.h"

namespace mozilla {
namespace dom {

class PresentationAvailability final : public DOMEventTargetHelper
                                     , public nsIPresentationAvailabilityListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationAvailability,
                                           DOMEventTargetHelper)
  NS_DECL_NSIPRESENTATIONAVAILABILITYLISTENER

  static already_AddRefed<PresentationAvailability> Create(nsPIDOMWindow* aWindow);

  virtual void DisconnectFromOwner() override;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL (public APIs)
  bool Value() const;

  IMPL_EVENT_HANDLER(change);

private:
  explicit PresentationAvailability(nsPIDOMWindow* aWindow);

  ~PresentationAvailability();

  bool Init();

  void Shutdown();

  void UpdateAvailabilityAndDispatchEvent(bool aIsAvailable);

  bool mIsAvailable;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationAvailability_h
