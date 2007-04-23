/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
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

#ifndef __EmbedPrivate_h
#define __EmbedPrivate_h

#include "nsCOMPtr.h"
#ifdef MOZILLA_INTERNAL_API
#include "nsString.h"
#else
#include "nsStringAPI.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#endif
#include "nsIWebNavigation.h"
#include "nsISHistory.h"
// for our one function that gets the EmbedPrivate via the chrome
// object.
#include "nsIWebBrowserChrome.h"
#include "nsIAppShell.h"
#include "nsIDOMEventReceiver.h"
#include "nsVoidArray.h"

// app component registration
#include "nsIGenericFactory.h"
#include "nsIComponentRegistrar.h"

#include "nsIDocCharset.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebBrowserFind.h"
// for the focus hacking we need to do
#include "nsIFocusController.h"
// for frames
#include "nsIDOMWindowCollection.h"
#include "gtkmozembedprivate.h"

#include "nsICacheEntryDescriptor.h"

#include "EmbedGtkTools.h"
class EmbedProgress;
class EmbedWindow;
class EmbedContentListener;
class EmbedEventListener;

class nsPIDOMWindow;
class nsIDirectoryServiceProvider;

class EmbedCommon {
 public:
  EmbedCommon() {
  }
  ~EmbedCommon() { }
  static EmbedCommon* GetInstance();
  static void DeleteInstance();
  nsresult    Init(void);
  GtkObject   *mCommon;
  static GtkMozEmbed* GetAnyLiveWidget();
};
class EmbedPrivate {

 public:

  EmbedPrivate();
  ~EmbedPrivate();

  nsresult    Init            (GtkMozEmbed *aOwningWidget);
  nsresult    Realize         (PRBool *aAlreadRealized);
  void        Unrealize       (void);
  void        Show            (void);
  void        Hide            (void);
  void        Resize          (PRUint32 aWidth, PRUint32 aHeight);
  void        Destroy         (void);
  void        SetURI          (const char *aURI);
  void        LoadCurrentURI  (void);
  void        Reload          (PRUint32 reloadFlags);

  void        SetChromeMask   (PRUint32 chromeMask);
  void        ApplyChromeMask ();

  static void PushStartup     (void);
  static void PopStartup      (void);
  static void SetPath         (const char *aPath);
  static void SetCompPath     (const char *aPath);

  static void SetAppComponents (const nsModuleComponentInfo* aComps,
                                int aNumComponents);
  static void SetProfilePath  (const char *aDir, const char *aName);
  static void SetDirectoryServiceProvider (nsIDirectoryServiceProvider * appFileLocProvider);

  nsresult OpenStream         (const char *aBaseURI, const char *aContentType);
  nsresult AppendToStream     (const PRUint8 *aData, PRUint32 aLen);
  nsresult CloseStream        (void);

  // This function will find the specific EmbedPrivate object for a
  // given nsIWebBrowserChrome.
  static EmbedPrivate *FindPrivateForBrowser(nsIWebBrowserChrome *aBrowser);

  // This is an upcall that will come from the progress listener
  // whenever there is a content state change.  We need this so we can
  // attach event listeners.
  void        ContentStateChange    (void);

  // This is an upcall from the progress listener when content is
  // finished loading.  We have this so that if it's chrome content
  // that we can size to content properly and show ourselves if
  // visibility is set.
  void        ContentFinishedLoading(void);

#ifdef MOZ_WIDGET_GTK
  // these let the widget code know when the toplevel window gets and
  // looses focus.
  void        TopLevelFocusIn (void);
  void        TopLevelFocusOut(void);
#endif

  // these are when the widget itself gets focus in and focus out
  // events
  void        ChildFocusIn (void);
  void        ChildFocusOut(void);
  PRBool      ClipBoardAction(GtkMozEmbedClipboard type);
  char*       GetEncoding ();
  nsresult    SetEncoding (const char *encoding);
  PRBool      FindText(const char *exp, PRBool  reverse,
                       PRBool  whole_word, PRBool  case_sensitive,
                       PRBool  restart);
  void        SetScrollTop(PRUint32 aTop);
  nsresult    ScrollToSelectedNode(nsIDOMNode *aDOMNode);
  nsresult    InsertTextToNode(nsIDOMNode *aDOMNode, const char *string);
  nsresult    GetFocusController(nsIFocusController **controller);
  nsresult    GetDOMWindowByNode(nsIDOMNode *aNode, nsIDOMWindow * *aDOMWindow);
  nsresult    GetZoom(PRInt32 *aZoomLevel, nsISupports *aContext = nsnull);
  nsresult    SetZoom(PRInt32 aZoomLevel, nsISupports *aContext = nsnull);
  nsresult    HasFrames(PRUint32 *numberOfFrames);
  nsresult    GetMIMEInfo(const char **aMime, nsIDOMNode *aDOMNode = nsnull);
  nsresult    GetCacheEntry(const char *aStorage,
                            const char *aKeyName,
                            PRUint32 aAccess,
                            PRBool aIsBlocking,
                            nsICacheEntryDescriptor **aDescriptor);
  nsresult    GetSHistoryList(GtkMozHistoryItem **GtkHI,
                               GtkMozEmbedSessionHistory type, gint *count);


#ifdef MOZ_ACCESSIBILITY_ATK
  void *GetAtkObjectForCurrentDocument();
#endif

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
  nsCString                      mPrePath;

  // the number of widgets that have been created
  static PRUint32                sWidgetCount;
  // the path to the GRE
  static char                   *sPath;
  // the path to components
  static char                   *sCompPath;
  // the list of application-specific components to register
  static const nsModuleComponentInfo  *sAppComps;
  static int                     sNumAppComps;
  // the appshell we have created
  static nsIAppShell            *sAppShell;
  // the list of all open windows
  static nsVoidArray            *sWindowList;
  // what is our profile path?
  static nsILocalFile           *sProfileDir;
  static nsISupports            *sProfileLock;

  static nsIDirectoryServiceProvider * sAppFileLocProvider;

  // chrome mask
  PRUint32                       mChromeMask;
  // is this a chrome window?
  PRBool                         mIsChrome;
  // has the chrome finished loading?
  PRBool                         mChromeLoaded;

  // has the network finished loading?
  PRBool                         mLoadFinished;

  // saved window ID for reparenting later
  GtkWidget                     *mMozWindowWidget;
  // has someone called Destroy() on us?
  PRBool                         mIsDestroyed;

  //Open Blocker for Create Window class //Fixme...
  //I just tried to block it on earlier moment
  PRBool                         mOpenBlock;
  PRBool                         mNeedFav;
 private:

  // is the chrome listener attached yet?
  PRBool                         mListenersAttached;
  PRBool                         mDoResizeEmbed;

  void GetListener    (void);
  void AttachListeners(void);
  void DetachListeners(void);

  // this will get the PIDOMWindow for this widget
  nsresult        GetPIDOMWindow   (nsPIDOMWindow **aPIWin);

  static nsresult RegisterAppComponents();

  // offscreen window methods and the offscreen widget
  static void       EnsureOffscreenWindow(void);
  static void       DestroyOffscreenWindow(void);
  static GtkWidget *sOffscreenWindow;
  static GtkWidget *sOffscreenFixed;

};

#endif /* __EmbedPrivate_h */
