/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationRequest_h
#define mozilla_dom_PresentationRequest_h

#include "mozilla/DOMEventTargetHelper.h"

class nsIDocument;

namespace mozilla {
namespace dom {

class Promise;
class PresentationAvailability;
class PresentationConnection;

class PresentationRequest final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static already_AddRefed<PresentationRequest> Constructor(const GlobalObject& aGlobal,
                                                           const nsAString& aUrl,
                                                           ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL (public APIs)
  already_AddRefed<Promise> Start(ErrorResult& aRv);

  already_AddRefed<Promise> StartWithDevice(const nsAString& aDeviceId,
                                            ErrorResult& aRv);

  already_AddRefed<Promise> Reconnect(const nsAString& aPresentationId,
                                      ErrorResult& aRv);

  already_AddRefed<Promise> GetAvailability(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(connectionavailable);

  nsresult DispatchConnectionAvailableEvent(PresentationConnection* aConnection);

private:
  PresentationRequest(nsPIDOMWindowInner* aWindow,
                      const nsAString& aUrl);

  ~PresentationRequest();

  bool Init();

  void FindOrCreatePresentationConnection(const nsAString& aPresentationId,
                                          Promise* aPromise);

  void FindOrCreatePresentationAvailability(RefPtr<Promise>& aPromise);

  // Implement https://w3c.github.io/webappsec-mixed-content/#categorize-settings-object
  bool IsProhibitMixedSecurityContexts(nsIDocument* aDocument);

  // Implement https://w3c.github.io/webappsec-mixed-content/#a-priori-authenticated-url
  bool IsPrioriAuthenticatedURL(const nsAString& aUrl);

  nsString mUrl;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationRequest_h
