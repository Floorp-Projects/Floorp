/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationSession_h
#define mozilla_dom_PresentationSession_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/PresentationSessionBinding.h"
#include "nsIPresentationListener.h"

namespace mozilla {
namespace dom {

class PresentationSession final : public DOMEventTargetHelper
                                , public nsIPresentationSessionListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationSession,
                                           DOMEventTargetHelper)
  NS_DECL_NSIPRESENTATIONSESSIONLISTENER

  static already_AddRefed<PresentationSession>
    Create(nsPIDOMWindow* aWindow,
           const nsAString& aId,
           PresentationSessionState aState);

  virtual void DisconnectFromOwner() override;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL (public APIs)
  void GetId(nsAString& aId) const;

  PresentationSessionState State() const;

  void Send(const nsAString& aData, ErrorResult& aRv);

  void Close();

  IMPL_EVENT_HANDLER(statechange);
  IMPL_EVENT_HANDLER(message);

private:
  PresentationSession(nsPIDOMWindow* aWindow,
                      const nsAString& aId,
                      PresentationSessionState aState);

  ~PresentationSession();

  bool Init();

  void Shutdown();

  nsresult DispatchStateChangeEvent();

  nsresult DispatchMessageEvent(JS::Handle<JS::Value> aData);

  nsString mId;
  PresentationSessionState mState;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationSession_h
