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
 * The Original Code is XMLterm.
 *
 * The Initial Developer of the Original Code is
 * Ramalingam Saravanan.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// nsMsgPrintEngine.h: declaration of nsMsgPrintEngine class
// implementing mozISimpleContainer,
// which provides a DocShell container for use in simple programs
// using the layout engine

#include "nscore.h"
#include "nsCOMPtr.h"

#include "nsIDocShell.h"
#include "nsVoidArray.h"
#include "nsIDocShell.h"
#include "nsIMsgPrintEngine.h"
#include "nsIScriptGlobalObject.h"
#include "nsIStreamListener.h"
#include "nsIWebProgressListener.h"
#include "nsIMsgStatusFeedback.h"
#include "nsIStringBundle.h"
#include "nsIWebBrowserPrint.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "nsIPrintSettings.h"
#include "nsIObserver.h"

// Progress Dialog
#include "nsIPrintProgress.h"
#include "nsIPrintProgressParams.h"
#include "nsIPrintingPromptService.h"

class nsMsgPrintEngine : public nsIMsgPrintEngine,
                         public nsIWebProgressListener,
                         public nsIObserver,
                         public nsSupportsWeakReference {

public:
  nsMsgPrintEngine();
  virtual ~nsMsgPrintEngine();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIMsgPrintEngine interface
  NS_DECL_NSIMSGPRINTENGINE

  // For nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER

  // For nsIObserver
  NS_DECL_NSIOBSERVER

  void PrintMsgWindow();
  NS_IMETHOD  StartNextPrintOperation();

protected:

  PRBool      FirePrintEvent();
  PRBool      FireStartNextEvent();
  NS_IMETHOD  FireThatLoadOperationStartup(nsString *uri);
  NS_IMETHOD  FireThatLoadOperation(nsString *uri);
  void        InitializeDisplayCharset();
  void        SetupObserver();
  nsresult    SetStatusMessage(PRUnichar *aMsgString);
  PRUnichar   *GetString(const PRUnichar *aStringName);
  nsresult    ShowProgressDialog(PRBool aIsForPrinting, PRBool& aDoNotify);

  nsCOMPtr<nsIDocShell>       mDocShell;
  nsCOMPtr<nsIDOMWindowInternal>      mWindow;
  nsCOMPtr<nsIDOMWindowInternal>      mParentWindow;
  PRInt32                     mURICount;
  nsStringArray               mURIArray;
  PRInt32                     mCurrentlyPrintingURI;

  nsCOMPtr<nsIContentViewer>  mContentViewer;
  nsCOMPtr<nsIStringBundle>   mStringBundle;    // String bundles...
  nsCOMPtr<nsIMsgStatusFeedback> mFeedback;     // Tell the user something why don't ya'
  nsCOMPtr<nsIWebBrowserPrint> mWebBrowserPrint;
  nsCOMPtr<nsIPrintSettings>   mPrintSettings;
  nsCOMPtr<nsIDOMWindow>       mMsgDOMWin;
  PRBool                       mIsDoingPrintPreview;
  nsCOMPtr<nsIObserver>        mStartupPPObs;
  PRInt32                      mMsgInx;

  // Progress Dialog
  
  nsCOMPtr<nsIPrintingPromptService> mPrintPromptService;
  nsCOMPtr<nsIWebProgressListener> mPrintProgressListener;
  nsCOMPtr<nsIPrintProgress>       mPrintProgress;
  nsCOMPtr<nsIPrintProgressParams> mPrintProgressParams;
  nsAutoString                     mLoadURI;
};
