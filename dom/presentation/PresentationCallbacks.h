/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationCallbacks_h
#define mozilla_dom_PresentationCallbacks_h

#include "mozilla/nsRefPtr.h"
#include "nsCOMPtr.h"
#include "nsIPresentationService.h"
#include "nsString.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class Promise;

class PresentationRequesterCallback final : public nsIPresentationServiceCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSERVICECALLBACK

  PresentationRequesterCallback(nsPIDOMWindow* aWindow,
                                const nsAString& aUrl,
                                const nsAString& aSessionId,
                                Promise* aPromise);

private:
  ~PresentationRequesterCallback();

  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsString mSessionId;
  nsRefPtr<Promise> mPromise;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationCallbacks_h
