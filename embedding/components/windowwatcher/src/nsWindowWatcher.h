/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
struct JSObject;
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
  nsresult OpenWindowJSInternal(nsIDOMWindow *aParent,
                                const char *aUrl,
                                const char *aName,
                                const char *aFeatures,
                                bool aDialog,
                                nsIArray *argv,
                                bool aCalledFromJS,
                                nsIDOMWindow **_retval);

  static JSContext *GetJSContextFromWindow(nsIDOMWindow *aWindow);
  static JSContext *GetJSContextFromCallStack();
  static nsresult   URIfromURL(const char *aURL,
                               nsIDOMWindow *aParent,
                               nsIURI **aURI);
  
  static PRUint32   CalculateChromeFlags(const char *aFeatures,
                                         bool aFeaturesSpecified,
                                         bool aDialog,
                                         bool aChromeURL,
                                         bool aHasChromeParent);
  static PRInt32    WinHasOption(const char *aOptions, const char *aName,
                                 PRInt32 aDefault, bool *aPresenceFlag);
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

