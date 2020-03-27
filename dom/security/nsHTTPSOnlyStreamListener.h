/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTTPSOnlyStreamListener_h___
#define nsHTTPSOnlyStreamListener_h___

#include "nsIStreamListener.h"
#include "nsCOMPtr.h"

/**
 * This event listener gets registered for requests that have been upgraded
 * using the HTTPS-only mode to log failed upgrades to the console.
 */
class nsHTTPSOnlyStreamListener : public nsIStreamListener {
 public:
  // nsISupports methods
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  explicit nsHTTPSOnlyStreamListener(nsIStreamListener* aListener);

 private:
  virtual ~nsHTTPSOnlyStreamListener() = default;

  nsCOMPtr<nsIStreamListener> mListener;
};

#endif /* nsHTTPSOnlyStreamListener_h___ */
