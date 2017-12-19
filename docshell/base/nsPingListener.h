/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPingListener_h__
#define nsPingListener_h__

#include "nsIStreamListener.h"

#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
class DocGroup;
}
}

class nsIContent;
class nsIDocShell;
class nsILoadGroup;
class nsITimer;
class nsIURI;

class nsPingListener final : public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsPingListener()
  {
  }

  void SetLoadGroup(nsILoadGroup* aLoadGroup) {
    mLoadGroup = aLoadGroup;
  }

  nsresult StartTimeout(mozilla::dom::DocGroup* aDocGroup);

  static void DispatchPings(nsIDocShell* aDocShell,
              nsIContent* aContent,
              nsIURI* aTarget,
              nsIURI* aReferrer,
              uint32_t aReferrerPolicy);
private:
  ~nsPingListener();

  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsITimer> mTimer;
};

#endif /* nsPingListener_h__ */
