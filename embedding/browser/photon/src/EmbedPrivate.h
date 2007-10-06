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
 * Christopher Blizzard.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Brian Edmond <briane@qnx.com>
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

#include <nsCOMPtr.h>
#include <nsString.h>
#include <nsIWebNavigation.h>
#include <nsIURIFixup.h>
#include <nsISHistory.h>
#include "nsIExternalHelperAppService.h"
// for our one function that gets the EmbedPrivate via the chrome
// object.
#include <nsIWebBrowserChrome.h>
#include <nsIPrintSettings.h>
#include <nsIAppShell.h>
#include <nsPIDOMEventTarget.h>
#include <nsVoidArray.h>
#include <nsClipboard.h>
// for profiles
#include <nsIPref.h>

#include <Pt.h>

class EmbedProgress;
class EmbedWindow;
class EmbedContentListener;
class EmbedEventListener;
class EmbedStream;
class EmbedPrintListener;

class nsPIDOMWindow;

class EmbedPrivate {

 public:

  EmbedPrivate();
  ~EmbedPrivate();

  nsresult    Init            (PtWidget_t *aOwningWidget);
  nsresult    Setup           ();
  void        Unrealize       (void);
  void        Show            (void);
  void        Hide            (void);
  void        Position        (PRUint32 aX, PRUint32 aY);
  void        Size            (PRUint32 aWidth, PRUint32 aHeight);
  void        Destroy         (void);
  void        SetURI          (const char *aURI);
  void        LoadCurrentURI  (void);
  void        Stop  (void);
  void        Reload(int32_t flags);
  void        Back  (void);
  void        Forward  (void);
  void 		  ScrollUp(int amount);
  void 		  ScrollDown(int amount);
  void 		  ScrollLeft(int amount);
  void 		  ScrollRight(int amount);
  void        Cut  (int ig);
  void        Copy  (int ig);
  void        Paste  (int ig);
  void        SelectAll  (void);
  void        Clear  (void);
  int    	  SaveAs(char *fname,char *dirname);
  void 		  Print(PpPrintContext_t *pc);
  PRBool 	  CanGoBack();
  PRBool 	  CanGoForward();
  nsIPref 	  *GetPrefs();

  nsresult OpenStream         (const char *aBaseURI, const char *aContentType);
  nsresult AppendToStream     (const char *aData, PRInt32 aLen);
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


#if 0
/*
  not used - we handle focus in/out activate/deactivate in
  child_getting_focus/child_losing_focus methods
  of the PtMozilla widget class
*/

  // these let the widget code know when the toplevel window gets and
  // looses focus.
  void        TopLevelFocusIn (void);
  void        TopLevelFocusOut(void);

  // these are when the widget itself gets focus in and focus out
  // events
  void        ChildFocusIn (void);
  void        ChildFocusOut(void);
#endif


  PtWidget_t                   *mOwningWidget;

  // all of the objects that we own
  EmbedWindow                   *mWindow;
  nsCOMPtr<nsISupports>          mWindowGuard;
  EmbedProgress                 *mProgress;
  nsCOMPtr<nsISupports>          mProgressGuard;
  EmbedContentListener          *mContentListener;
  nsCOMPtr<nsISupports>          mContentListenerGuard;
  EmbedEventListener            *mEventListener;
  nsCOMPtr<nsISupports>          mEventListenerGuard;
  EmbedStream                   *mStream;
  nsCOMPtr<nsISupports>          mStreamGuard;
  EmbedPrintListener             *mPrint;
  nsCOMPtr<nsISupports>          mPrintGuard;

  nsCOMPtr<nsIWebNavigation>     mNavigation;
  nsCOMPtr<nsISHistory>          mSessionHistory;

  // our event target
  nsCOMPtr<nsPIDOMEventTarget>   mEventTarget;

  // the currently loaded uri
  nsString                       mURI;

  nsCOMPtr<nsIPrintSettings> m_PrintSettings;

  // the number of widgets that have been created
  static PRUint32                sWidgetCount;
  // the path to components
  static char                   *sCompPath;
  // the appshell we have created
  static nsIAppShell            *sAppShell;
  // for profiles
  static nsIPref                *sPrefs;
	static nsVoidArray            *sWindowList;
  // for clipboard input group setting
  static nsClipboard *sClipboard;

  // chrome mask
  PRUint32                       mChromeMask;
  // is this a chrome window?
  PRBool                         mIsChrome;
  // has the chrome finished loading?
  PRBool                         mChromeLoaded;
  // saved window ID for reparenting later
  PtWidget_t                     *mMozWindowWidget;

  // this will get the PIDOMWindow for this widget
  nsresult        GetPIDOMWindow   (nsPIDOMWindow **aPIWin);
 private:

  // is the chrome listener attached yet?
  PRBool                         mListenersAttached;

  void GetListener    (void);
  void AttachListeners(void);
  void DetachListeners(void);
	void PrintHeaderFooter_FormatSpecialCodes(const char *original, nsString& aNewStr);

};

#endif /* __EmbedPrivate_h */
