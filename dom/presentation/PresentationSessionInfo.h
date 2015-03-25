/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationSessionInfo_h
#define mozilla_dom_PresentationSessionInfo_h

#include "mozilla/nsRefPtr.h"
#include "nsCOMPtr.h"
#include "nsIPresentationListener.h"
#include "nsIPresentationService.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class PresentationSessionInfo
{
public:
  PresentationSessionInfo(const nsAString& aUrl,
                          const nsAString& aSessionId,
                          nsIPresentationServiceCallback* aCallback)
    : mUrl(aUrl)
    , mSessionId(aSessionId)
    , mCallback(aCallback)
  {
    MOZ_ASSERT(!mUrl.IsEmpty());
    MOZ_ASSERT(!mSessionId.IsEmpty());
  }

  const nsAString& GetUrl() const
  {
    return mUrl;
  }

  const nsAString& GetSessionId() const
  {
    return mSessionId;
  }

  void SetCallback(nsIPresentationServiceCallback* aCallback)
  {
    mCallback = aCallback;
  }

  void SetListener(nsIPresentationSessionListener* aListener)
  {
    mListener = aListener;
  }

private:
  nsString mUrl;
  nsString mSessionId;
  nsCOMPtr<nsIPresentationServiceCallback> mCallback;
  nsCOMPtr<nsIPresentationSessionListener> mListener;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationSessionInfo_h
