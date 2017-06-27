/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocElementCreatedNotificationRunner_h
#define nsDocElementCreatedNotificationRunner_h

#include "mozilla/Attributes.h"
#include "nsThreadUtils.h" /* nsRunnable */

#include "nsContentSink.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"

class nsDocElementCreatedNotificationRunner : public mozilla::Runnable
{
public:
  explicit nsDocElementCreatedNotificationRunner(nsIDocument* aDoc)
    : mozilla::Runnable("nsDocElementCreatedNotificationRunner")
    , mDoc(aDoc)
  {
  }

  NS_IMETHOD Run() override
  {
    nsContentSink::NotifyDocElementCreated(mDoc);
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> mDoc;
};

#endif /* nsDocElementCreatedNotificationRunner_h */
