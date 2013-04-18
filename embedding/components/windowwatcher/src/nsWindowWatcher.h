/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWindowWatcher_h__
#define __nsWindowWatcher_h__

// {a21bfa01-f349-4394-a84c-8de5cf0737d0}
#define NS_WINDOWWATCHER_CID \
 {0xa21bfa01, 0xf349, 0x4394, {0xa8, 0x4c, 0x8d, 0xe5, 0xcf, 0x7, 0x37, 0xd0}}

#include "nsCOMPtr.h"
#include "jspubtd.h"
#include "mozilla/Mutex.h"
#include "nsIWindowCreator.h" // for stupid compilers
#include "nsIWindowWatcher.h"
#include "nsIPromptFactory.h"
#include "nsPIWindowWatcher.h"
#include "nsTArray.h"

class  nsIURI;
class  nsIDocShellTreeItem;
class  nsIDocShellTreeOwner;
class  nsIWebBrowserChrome;
class  nsString;
class  nsWatcherWindowEnumerator;
class  nsIScriptContext;
class  nsPromptService;
struct JSContext;
class JSObject;
struct nsWatcherWindowEntry;
struct SizeSpec;

class nsWindowWatcher :
      public nsIWindowWatcher,
      public nsPIWindowWatcher,
      public nsIPromptFactory
{
friend class nsWatcherWindowEnumerator;

public:
  nsWindowWatcher();
  virtual ~nsWindowWatcher();

  nsresult Init();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIWINDOWWATCHER
  NS_DECL_NSPIWINDOWWATCHER
  NS_DECL_NSIPROMPTFACTORY

protected:
  friend class nsPromptService;
  bool AddEnumerator(nsWatcherWindowEnumerator* inEnumerator);
  bool RemoveEnumerator(nsWatcherWindowEnumerator* inEnumerator);

  nsWatcherWindowEntry *FindWindowEntry(nsIDOMWindow *aWindow);
  nsresult RemoveWindow(nsWatcherWindowEntry *inInfo);

  // Get the caller tree item.  Look on the JS stack, then fall back
  // to the parent if there's nothing there.
  already_AddRefed<nsIDocShellTreeItem>
    GetCallerTreeItem(nsIDocShellTreeItem* aParentItem);
  
  // Unlike GetWindowByName this will look for a caller on the JS
  // stack, and then fall back on aCurrentWindow if it can't find one.
  nsresult SafeGetWindowByName(const nsAString& aName,
                               nsIDOMWindow* aCurrentWindow,
                               nsIDOMWindow** aResult);

  // Just like OpenWindowJS, but knows whether it got called via OpenWindowJS
  // (which means called from script) or called via OpenWindow.
  nsresult OpenWindowInternal(nsIDOMWindow *aParent,
                              const char *aUrl,
                              const char *aName,
                              const char *aFeatures,
                              bool aCalledFromJS,
                              bool aDialog,
                              bool aNavigate,
                              nsIArray *argv,
                              nsIDOMWindow **_retval);

  static JSContext *GetJSContextFromWindow(nsIDOMWindow *aWindow);
  static nsresult   URIfromURL(const char *aURL,
                               nsIDOMWindow *aParent,
                               nsIURI **aURI);
  
  static uint32_t   CalculateChromeFlags(nsIDOMWindow *aParent,
                                         const char *aFeatures,
                                         bool aFeaturesSpecified,
                                         bool aDialog,
                                         bool aChromeURL,
                                         bool aHasChromeParent);
  static int32_t    WinHasOption(const char *aOptions, const char *aName,
                                 int32_t aDefault, bool *aPresenceFlag);
  /* Compute the right SizeSpec based on aFeatures */
  static void       CalcSizeSpec(const char* aFeatures, SizeSpec& aResult);
  static nsresult   ReadyOpenedDocShellItem(nsIDocShellTreeItem *aOpenedItem,
                                            nsIDOMWindow *aParent,
                                            bool aWindowIsNew,
                                            nsIDOMWindow **aOpenedWindow);
  static void       SizeOpenedDocShellItem(nsIDocShellTreeItem *aDocShellItem,
                                           nsIDOMWindow *aParent,
                                           const SizeSpec & aSizeSpec);
  static void       GetWindowTreeItem(nsIDOMWindow *inWindow,
                                      nsIDocShellTreeItem **outTreeItem);
  static void       GetWindowTreeOwner(nsIDOMWindow *inWindow,
                                       nsIDocShellTreeOwner **outTreeOwner);

  nsTArray<nsWatcherWindowEnumerator*> mEnumeratorList;
  nsWatcherWindowEntry *mOldestWindow;
  mozilla::Mutex        mListLock;

  nsCOMPtr<nsIWindowCreator> mWindowCreator;
};

#endif

