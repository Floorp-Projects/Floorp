/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAutoCopyListener_h_
#define nsAutoCopyListener_h_

#include "nsISelectionListener.h"
#include "nsISelectionPrivate.h"
#include "mozilla/Attributes.h"

class nsAutoCopyListener final : public nsISelectionListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISELECTIONLISTENER

  explicit nsAutoCopyListener(int16_t aClipboardID)
    : mCachedClipboard(aClipboardID)
  {}

  void Listen(nsISelectionPrivate *aSelection)
  {
      NS_ASSERTION(aSelection, "Null selection passed to Listen()");
      aSelection->AddSelectionListener(this);
  }

  static nsAutoCopyListener* GetInstance(int16_t aClipboardID)
  {
    if (!sInstance) {
      sInstance = new nsAutoCopyListener(aClipboardID);

      NS_ADDREF(sInstance);
    }

    return sInstance;
  }

  static void Shutdown()
  {
    NS_IF_RELEASE(sInstance);
  }

private:
  ~nsAutoCopyListener() {}

  static nsAutoCopyListener* sInstance;
  int16_t mCachedClipboard;
};

#endif
