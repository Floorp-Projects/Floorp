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
#include "nsITimer.h"

class nsDocShell;
class nsIInterfaceRequestor;
class nsIWebNavigationInfo;
class nsPIDOMWindowOuter;

// Helper Class to eventually close an already opened window
class MaybeCloseWindowHelper final : public nsITimerCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  explicit MaybeCloseWindowHelper(
      mozilla::dom::BrowsingContext* aContentContext);

  /**
   * Closes the provided window async (if mShouldCloseWindow is true) and
   * returns a valid browsingContext to be used instead as parent for dialogs or
   * similar things.
   * In case mShouldCloseWindow is true, the returned BrowsingContext will be
   * the window's opener (or original cross-group opener in the case of a
   * `noopener` popup).
   */
  mozilla::dom::BrowsingContext* MaybeCloseWindow();

  void SetShouldCloseWindow(bool aShouldCloseWindow);

 protected:
  ~MaybeCloseWindowHelper();

 private:
  already_AddRefed<mozilla::dom::BrowsingContext> ChooseNewBrowsingContext(
      mozilla::dom::BrowsingContext* aBC);

  /**
   * The dom window associated to handle content.
   */
  RefPtr<mozilla::dom::BrowsingContext> mBrowsingContext;

  /**
   * Used to close the window on a timer, to avoid any exceptions that are
   * thrown if we try to close the window before it's fully loaded.
   */
  RefPtr<mozilla::dom::BrowsingContext> mBCToClose;
  nsCOMPtr<nsITimer> mTimer;

  /**
   * This is set based on whether the channel indicates that a new window
   * was opened, e.g. for a download, or was blocked. If so, then we
   * close it.
   */
  bool mShouldCloseWindow;
};

class nsDSURIContentListener final : public nsIURIContentListener,
                                     public nsSupportsWeakReference {
  friend class nsDocShell;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURICONTENTLISTENER

 protected:
  explicit nsDSURIContentListener(nsDocShell* aDocShell);
  virtual ~nsDSURIContentListener();

  void DropDocShellReference() {
    mDocShell = nullptr;
    mExistingJPEGRequest = nullptr;
    mExistingJPEGStreamListener = nullptr;
  }

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
};

#endif /* nsDSURIContentListener_h__ */
