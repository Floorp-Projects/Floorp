/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef __nsWindowWatcher_h__
#define __nsWindowWatcher_h__

// {a21bfa01-f349-4394-a84c-8de5cf0737d0}
#define NS_WINDOWWATCHER_CID \
 {0xa21bfa01, 0xf349, 0x4394, {0xa8, 0x4c, 0x8d, 0xe5, 0xcf, 0x7, 0x37, 0xd0}}
#define NS_WINDOWWATCHER_CONTRACTID \
 "@mozilla.org/embedcomp/window-watcher;1"

#include "nsCOMPtr.h"
#include "jspubtd.h"
#include "nsIWindowCreator.h" // for stupid compilers
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsVoidArray.h"

class  nsIURI;
class  nsIDocShellTreeItem;
class  nsIDocShellTreeOwner;
class  nsString;
class  nsWatcherWindowEnumerator;
struct JSContext;
struct JSObject;
struct nsWatcherWindowEntry;
struct PRLock;

class nsWindowWatcher :
      public nsIWindowWatcher,
      public nsPIWindowWatcher
{
friend class nsWatcherWindowEnumerator;

public:
  nsWindowWatcher();
  virtual ~nsWindowWatcher();

  nsresult Init();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIWINDOWWATCHER
  NS_DECL_NSPIWINDOWWATCHER

private:
  PRBool AddEnumerator(nsWatcherWindowEnumerator* inEnumerator);
  PRBool RemoveEnumerator(nsWatcherWindowEnumerator* inEnumerator);

  nsWatcherWindowEntry *FindWindowEntry(nsIDOMWindow *aWindow);
  nsresult RemoveWindow(nsWatcherWindowEntry *inInfo);

  nsresult FindItemWithName(const PRUnichar *aName,
                            nsIDocShellTreeItem **aFoundItem);

  static JSContext *GetJSContextFromWindow(nsIDOMWindow *aWindow);
  static JSContext *GetJSContextFromCallStack();
  static nsresult   URIfromURL(const char *aURL,
                               nsIDOMWindow *aParent,
                               nsIURI **aURI);
  static void       CheckWindowName(nsString& aName);
  static PRUint32   CalculateChromeFlags(const char *aFeatures,
                                         PRBool aFeaturesSpecified,
                                         PRBool aDialog);
  static PRInt32    WinHasOption(const char *aOptions, const char *aName,
                                 PRInt32 aDefault, PRBool *aPresenceFlag);
  static nsresult   ReadyOpenedDocShellItem(nsIDocShellTreeItem *aOpenedItem,
                                            nsIDOMWindow *aParent,
                                            nsIDOMWindow **aOpenedWindow);
  static void       SizeOpenedDocShellItem(nsIDocShellTreeItem *aDocShellItem,
                                           nsIDOMWindow *aParent,
                                           const char *aFeatures,
                                           PRUint32 aChromeFlags);
  static nsresult   AttachArguments(nsIDOMWindow *aWindow,
                                    PRUint32 argc, jsval *argv);
  static nsresult   ConvertSupportsTojsvals(nsIDOMWindow *aWindow,
                                            nsISupports *aArgs,
                                            PRUint32 *aArgc, jsval **aArgv);
  static nsresult   AddSupportsTojsvals(nsISupports *aArg,
                                        JSContext *cx, jsval *aArgv);
  static nsresult   AddInterfaceTojsvals(nsISupports *aArg,
                                      JSContext *cx, jsval *aArgv);
  static void       GetWindowTreeItem(nsIDOMWindow *inWindow,
                                      nsIDocShellTreeItem **outTreeItem);
  static void       GetWindowTreeOwner(nsIDOMWindow *inWindow,
                                       nsIDocShellTreeOwner **outTreeOwner);
  static JSObject  *GetWindowScriptObject(nsIDOMWindow *inWindow);

  nsVoidArray           mEnumeratorList;
  nsWatcherWindowEntry *mOldestWindow;
  nsIDOMWindow         *mActiveWindow;
  PRLock               *mListLock;

  nsCOMPtr<nsIWindowCreator> mWindowCreator;
};

#endif

