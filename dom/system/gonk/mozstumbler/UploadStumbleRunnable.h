/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef UPLOADSTUMBLERUNNABLE_H
#define UPLOADSTUMBLERUNNABLE_H

#include "nsIDOMEventListener.h"

class nsIXMLHttpRequest;
class nsIInputStream;

/*
 This runnable is managed by WriteStumbleOnThread only, see that class
 for how this is scheduled.
 */
class UploadStumbleRunnable final : public nsRunnable
{
public:
  explicit UploadStumbleRunnable(nsIInputStream* aUploadInputStream);

  NS_IMETHOD Run() override;
private:
  virtual ~UploadStumbleRunnable() {}
  nsCOMPtr<nsIInputStream> mUploadInputStream;
  nsresult Upload();
};


class UploadEventListener : public nsIDOMEventListener
{
public:
  UploadEventListener(nsIXMLHttpRequest* aXHR);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

protected:
  virtual ~UploadEventListener() {}
  nsCOMPtr<nsIXMLHttpRequest> mXHR;
};

#endif
