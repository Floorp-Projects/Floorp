/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Presentation_h
#define mozilla_dom_Presentation_h

#include "mozilla/DOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class Promise;
class PresentationReceiver;
class PresentationRequest;

class Presentation final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Presentation,
                                           DOMEventTargetHelper)

  static already_AddRefed<Presentation> Create(nsPIDOMWindowInner* aWindow);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL (public APIs)
  void SetDefaultRequest(PresentationRequest* aRequest);

  already_AddRefed<PresentationRequest> GetDefaultRequest() const;

  already_AddRefed<PresentationReceiver> GetReceiver();

private:
  explicit Presentation(nsPIDOMWindowInner* aWindow);

  ~Presentation();

  bool IsInPresentedContent() const;

  RefPtr<PresentationRequest> mDefaultRequest;
  RefPtr<PresentationReceiver> mReceiver;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Presentation_h
