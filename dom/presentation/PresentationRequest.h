/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationRequest_h
#define mozilla_dom_PresentationRequest_h

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class Promise;
class PresentationAvailability;

class PresentationRequest final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationRequest,
                                           DOMEventTargetHelper)

  static already_AddRefed<PresentationRequest> Constructor(const GlobalObject& aGlobal,
                                                           const nsAString& aUrl,
                                                           ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL (public APIs)
  already_AddRefed<Promise> Start(ErrorResult& aRv);

  already_AddRefed<Promise> GetAvailability(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(sessionconnect);

private:
  PresentationRequest(nsPIDOMWindow* aWindow,
                      const nsAString& aUrl);

  ~PresentationRequest();

  bool Init();

  nsString mUrl;
  nsRefPtr<PresentationAvailability> mAvailability;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationRequest_h
