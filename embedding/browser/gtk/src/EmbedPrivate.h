/*
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
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#ifndef __EmbedPrivate_h
#define __EmbedPrivate_h

#include <nsCOMPtr.h>
#include <nsIWebNavigation.h>
#include <nsISHistory.h>
// for our one function that gets the EmbedPrivate via the chrome
// object.
#include <nsIWebBrowserChrome.h>
#include <nsIAppShell.h>
#include <nsIDOMEventReceiver.h>
#include <nsVoidArray.h>
// for profiles
#include <nsIPref.h>


#include "gtkmozembedprivate.h"

class EmbedProgress;
class EmbedWindow;
class EmbedContentListener;
class EmbedEventListener;

class EmbedPrivate {

 public:

  EmbedPrivate();
  ~EmbedPrivate();

  nsresult    Init            (GtkMozEmbed *aOwningWidget);
  nsresult    Realize         (void);
  void        Unrealize       (void);
  void        Show            (void);
  void        Hide            (void);
  void        Resize          (PRUint32 aWidth, PRUint32 aHeight);
  void        SetURI          (const char *aURI);
  void        LoadCurrentURI  (void);

  static void PushStartup     (void);
  static void PopStartup      (void);
  static void SetCompPath     (char *aPath);
  static void SetProfilePath  (char *aDir, char *aName);

  // This function will find the specific EmbedPrivate object for a
  // given nsIWebBrowserChrome.
  static EmbedPrivate *FindPrivateForBrowser(nsIWebBrowserChrome *aBrowser);

  // This is an upcall that will come from the progress listener
  // whenever there is a progress change.  We need this so we can
  // attach event listeners.
  void        ContentProgressChange (void);

  GtkMozEmbed                   *mOwningWidget;

  // all of the objects that we own
  EmbedWindow                   *mWindow;
  nsCOMPtr<nsISupports>          mWindowGuard;
  EmbedProgress                 *mProgress;
  nsCOMPtr<nsISupports>          mProgressGuard;
  EmbedContentListener          *mContentListener;
  nsCOMPtr<nsISupports>          mContentListenerGuard;
  EmbedEventListener            *mEventListener;
  nsCOMPtr<nsISupports>          mEventListenerGuard;

  nsCOMPtr<nsIWebNavigation>     mNavigation;
  nsCOMPtr<nsISHistory>          mSessionHistory;

  // our event receiver
  nsCOMPtr<nsIDOMEventReceiver>  mEventReceiver;

  // the currently loaded uri
  nsString                       mURI;

  // the number of widgets that have been created
  static PRUint32                sWidgetCount;
  // the path to components
  static char                   *sCompPath;
  // the appshell we have created
  static nsIAppShell            *sAppShell;
  // has the window creator service been set up?
  static PRBool                  sCreatorInit;
  // the list of all open windows
  static nsVoidArray            *sWindowList;
  // what is our profile path?
  static char                   *sProfileDir;
  static char                   *sProfileName;
  // for profiles
  static nsIPref                *sPrefs;

 private:

  // is the chrome listener attached yet?
  PRBool                         mListenersAttached;

  void GetListener    (void);
  void AttachListeners(void);
  void DetachListeners(void);
  
  static nsresult StartupProfile(void);

};

#endif /* __EmbedPrivate_h */
