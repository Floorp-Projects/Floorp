/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is XMLterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 */

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
#include "nsIPrintListener.h"
#include "nsIMsgStatusFeedback.h"
#include "nsIStringBundle.h"
#include "nsIContentViewerFile.h"

class nsMsgPrintEngine : public nsIMsgPrintEngine, public nsIPrintListener {

public:
  nsMsgPrintEngine();
  virtual ~nsMsgPrintEngine();

  // nsISupports
  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init();

  // nsIMsgPrintEngine interface
  NS_DECL_NSIMSGPRINTENGINE

  // For nsIDocumentLoaderObserver...
  NS_DECL_NSIDOCUMENTLOADEROBSERVER

  // For nIPrintListener
  NS_DECL_NSIPRINTLISTENER

protected:

  NS_IMETHOD  FireThatLoadOperation(nsString *uri);
  NS_IMETHOD  StartNextPrintOperation();
  void        InitializeDisplayCharset();
  void        SetupObserver();
  nsresult    SetStatusMessage(PRUnichar *aMsgString);
  PRUnichar   *GetString(const PRUnichar *aStringName);

  nsCOMPtr<nsIDocShell>       mDocShell;
  nsCOMPtr<nsIDOMWindowInternal>      mWindow;
  PRInt32                     mURICount;
  nsStringArray               mURIArray;
  PRInt32                     mCurrentlyPrintingURI;

  nsCOMPtr<nsIContentViewer>  mContentViewer;
  nsCOMPtr<nsIStringBundle>   mStringBundle;    // String bundles...
  nsCOMPtr<nsIMsgStatusFeedback> mFeedback;     // Tell the user something why don't ya'
  nsCOMPtr<nsIContentViewerFile> mViewerFile;
};
