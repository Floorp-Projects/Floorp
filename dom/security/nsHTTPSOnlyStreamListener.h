/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTTPSOnlyStreamListener_h___
#define nsHTTPSOnlyStreamListener_h___

#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsIStreamListener.h"

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

  /**
   * Records telemetry about the upgraded request.
   * @param aStatus Request object
   */
  void RecordUpgradeTelemetry(nsIRequest* request, nsresult aStatus);

  /**
   * Logs information to the console if the request failed.
   * @param request Request object
   * @param aStatus Status of request
   */
  void LogUpgradeFailure(nsIRequest* request, nsresult aStatus);

  nsCOMPtr<nsIStreamListener> mListener;
  mozilla::TimeStamp mCreationStart;
};

#endif /* nsHTTPSOnlyStreamListener_h___ */
