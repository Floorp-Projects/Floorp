/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

class nsDocElementCreatedNotificationRunner : public nsRunnable
{
public:
  nsDocElementCreatedNotificationRunner(nsIDocument* aDoc)
    : mDoc(aDoc)
  {
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    nsContentSink::NotifyDocElementCreated(mDoc);
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> mDoc;
};

#endif /* nsDocElementCreatedNotificationRunner_h */
