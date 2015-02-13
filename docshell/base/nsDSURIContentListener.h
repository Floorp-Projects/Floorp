/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDSURIContentListener_h__
#define nsDSURIContentListener_h__

#include "nsCOMPtr.h"
#include "nsIURIContentListener.h"
#include "nsWeakReference.h"

class nsDocShell;
class nsIWebNavigationInfo;
class nsIHttpChannel;
class nsAString;

class nsDSURIContentListener MOZ_FINAL
  : public nsIURIContentListener
  , public nsSupportsWeakReference
{
  friend class nsDocShell;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURICONTENTLISTENER

  nsresult Init();

protected:
  explicit nsDSURIContentListener(nsDocShell* aDocShell);
  virtual ~nsDSURIContentListener();

  void DropDocShellReference()
  {
    mDocShell = nullptr;
    mExistingJPEGRequest = nullptr;
    mExistingJPEGStreamListener = nullptr;
  }

  // Determine if X-Frame-Options allows content to be framed
  // as a subdocument
  bool CheckFrameOptions(nsIRequest* aRequest);
  bool CheckOneFrameOptionsPolicy(nsIHttpChannel* aHttpChannel,
                                  const nsAString& aPolicy);

  enum XFOHeader
  {
    eDENY,
    eSAMEORIGIN,
    eALLOWFROM
  };

  void ReportXFOViolation(nsIDocShellTreeItem* aTopDocShellItem,
                          nsIURI* aThisURI,
                          XFOHeader aHeader);

protected:
  nsDocShell* mDocShell;
  // Hack to handle multipart images without creating a new viewer
  nsCOMPtr<nsIStreamListener> mExistingJPEGStreamListener;
  nsCOMPtr<nsIChannel> mExistingJPEGRequest;

  // Store the parent listener in either of these depending on
  // if supports weak references or not. Proper weak refs are
  // preferred and encouraged!
  nsWeakPtr mWeakParentContentListener;
  nsIURIContentListener* mParentContentListener;

  nsCOMPtr<nsIWebNavigationInfo> mNavInfo;
};

#endif /* nsDSURIContentListener_h__ */
