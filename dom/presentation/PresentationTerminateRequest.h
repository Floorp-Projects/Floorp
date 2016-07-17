/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationTerminateRequest_h__
#define mozilla_dom_PresentationTerminateRequest_h__

#include "nsIPresentationTerminateRequest.h"
#include "nsCOMPtr.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class PresentationTerminateRequest final : public nsIPresentationTerminateRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONTERMINATEREQUEST

  PresentationTerminateRequest(nsIPresentationDevice* aDevice,
                               const nsAString& aPresentationId,
                               nsIPresentationControlChannel* aControlChannel,
                               bool aIsFromReceiver);

private:
  virtual ~PresentationTerminateRequest();

  nsString mPresentationId;
  nsCOMPtr<nsIPresentationDevice> mDevice;
  nsCOMPtr<nsIPresentationControlChannel> mControlChannel;
  bool mIsFromReceiver;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_PresentationTerminateRequest_h__ */

