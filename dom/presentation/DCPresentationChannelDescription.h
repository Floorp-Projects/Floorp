/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DCPresentationChannelDescription_h
#define mozilla_dom_DCPresentationChannelDescription_h

#include "nsIPresentationControlChannel.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

// PresentationChannelDescription for Data Channel
class DCPresentationChannelDescription final : public nsIPresentationChannelDescription
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONCHANNELDESCRIPTION

  explicit DCPresentationChannelDescription(const nsAString& aSDP)
    : mSDP(aSDP)
  {
  }

private:
  virtual ~DCPresentationChannelDescription() = default;

  nsString mSDP;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DCPresentationChannelDescription_h
