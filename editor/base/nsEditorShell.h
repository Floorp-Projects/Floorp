/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsEditorShell_h___
#define nsEditorShell_h___

//#include "nsAppCores.h"

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsISupports.h"

#include "nsIEditorShell.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIDOMSelectionListener.h"
#ifdef NECKO
#include "nsIPrompt.h"
#else
#include "nsINetSupport.h"
#endif
#include "nsIStreamObserver.h"
#include "nsIDOMDocument.h"
#include "nsVoidArray.h"
#include "nsTextServicesCID.h"
#include "nsIEditorSpellCheck.h"
#include "nsISpellChecker.h"
#include "nsInterfaceState.h"
#include "nsIHTMLEditor.h"
#include "nsIStringBundle.h"

class nsIBrowserWindow;
class nsIWebShell;
class nsIScriptContext;
class nsIDOMWindow;
class nsIDOMElement;
class nsIDOMNode;
class nsIURI;
class nsIWebShellWindow;
class nsIPresShell;
class nsIOutputStream;
class nsISupportsArray;
class nsIStringBundleService;
class nsIStringBundle;


#define NS_EDITORSHELL_CID                            \
{ /* {} */                                            \
    0x9afff72b, 0xca9a, 0x11d2,                       \
    { 0x96, 0xc9, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } \
}

////////////////////////////////////////////////////////////////////////////////
// nsEditorShell:
////////////////////////////////////////////////////////////////////////////////

class nsEditorShell :   public nsIEditorShell,
                        public nsIEditorSpellCheck,
                        public nsIDocumentLoaderObserver
{
  public:

  	// These must map onto the button-order for nsICommonDialog::Confirm results
    //  which are rather ugly right now (Cancel in the middle!)
    typedef enum {eYes = 0, eCancel = 1, eNo = 2 } EConfirmResult;

    nsEditorShell();
    virtual ~nsEditorShell();

    NS_DECL_ISUPPORTS

    /* Declare all methods in the nsIEditorShell interface */
    NS_DECL_NSIEDITORSHELL

    /* Declare all methods in the nsIEditorSpellCheck interface */
    NS_DECL_NSIEDITORSPELLCHECK

    // nsIDocumentLoaderObserver
    NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand);

#ifndef NECKO
    NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, nsIURI *aUrl, PRInt32 aStatus,
								  nsIDocumentLoaderObserver * aObserver);
#else
    NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus,
                                  nsIDocumentLoaderObserver * aObserver);
#endif // NECKO

#ifndef NECKO
    NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aContentType, 
                           		 nsIContentViewer* aViewer);
#else
    NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, nsIChannel* channel,
                                 nsIContentViewer* aViewer);
#endif // NECKO

#ifndef NECKO
    NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, PRUint32 aProgress, 
                               PRUint32 aProgressMax);
#else
    NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, PRUint32 aProgress,
                               PRUint32 aProgressMax);
#endif // NECKO

#ifndef NECKO
    NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, nsString& aMsg);
#else
    NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsString& aMsg);
#endif // NECKO

#ifndef NECKO
    NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, PRInt32 aStatus);
#else
    NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsresult aStatus);
#endif // NECKO

#ifndef NECKO
    NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,
                                        nsIURI *aURL,
                                        const char *aContentType,
                                        const char *aCommand );
#else
    NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,
                                        nsIChannel* channel,
                                        const char *aContentType,
                                        const char *aCommand );
#endif // NECKO

  protected:
    nsIDOMWindow       *mToolbarWindow;				// weak reference
    nsIDOMWindow       *mContentWindow;				// weak reference

    nsIWebShellWindow  *mWebShellWin;					// weak reference
    nsIWebShell        *mWebShell;						// weak reference
    nsIWebShell        *mContentAreaWebShell;	// weak reference

  	typedef enum {
  	  eUninitializedEditorType = 0,
  		ePlainTextEditorType = 1,
  		eHTMLTextEditorType = 2
  	} EEditorType;
  	
    nsIPresShell* 	GetPresShellFor(nsIWebShell* aWebShell);
    NS_IMETHOD 			DoEditorMode(nsIWebShell *aWebShell);
    NS_IMETHOD	 		ExecuteScript(nsIScriptContext * aContext, const nsString& aScript);
    NS_IMETHOD			InstantiateEditor(nsIDOMDocument *aDoc, nsIPresShell *aPresShell);
    NS_IMETHOD      TransferDocumentStateListeners();
    NS_IMETHOD			RemoveOneProperty(const nsString& aProp, const nsString& aAttr);
    void 						SetButtonImage(nsIDOMNode * aParentNode, PRInt32 aBtnNum, const nsString &aResName);
		NS_IMETHOD			CreateWindowWithURL(const char* urlStr);
		NS_IMETHOD  	  PrepareDocumentForEditing(nsIURI *aUrl);
		NS_IMETHOD      DoFind(PRBool aFindNext);

    void    Alert(const nsString& aTitle, const nsString& aMsg);
    // Bring up a Yes/No dialog WE REALLY NEED A Yes/No/Cancel dialog and would like to set our own caption as well!
    PRBool  Confirm(const nsString& aTitle, const nsString& aQuestion);
    // Return value: No=0, Yes=1, Cancel=2
    // aYesString and aNoString are optional:
    // if null, then "Yes" and "No" are used
    EConfirmResult ConfirmWithCancel(const nsString& aTitle, const nsString& aQuestion,
                                     const nsString *aYesString, const nsString *aNoString);

		// this returns an AddReffed nsIScriptContext. You must relase it.
		nsIScriptContext*  GetScriptContext(nsIDOMWindow * aWin);

    nsString            mEnableScript;     
    nsString            mDisableScript;     

		EEditorType					mEditorType;
		nsString						mEditorTypeString;	// string which describes which editor type will be instantiated (lowercased)
    nsCOMPtr<nsIHTMLEditor>	 	mEditor;						// this can be either an HTML or plain text (or other?) editor

    nsCOMPtr<nsISupports>   mSearchContext;		// context used for search and replace. Owned by the appshell.
    
    nsInterfaceState*    mStateMaintainer;    // we hold the owning ref to this.

    PRInt32 mWrapColumn;      // can't actually set this 'til the editor is created, so we may have to hold on to it for a while

    nsCOMPtr<nsISpellChecker> mSpellChecker;
    nsStringArray   mSuggestedWordList;
    PRInt32         mSuggestedWordIndex;
    NS_IMETHOD      DeleteSuggestedWordList();
    nsStringArray   mDictionaryList;
    PRInt32         mDictionaryIndex;

    // this is a holding pen for doc state listeners. They will be registered with
    // the editor when that gets created.
    nsCOMPtr<nsISupportsArray> mDocStateListeners;		// contents are nsISupports

    // Get a string from the string bundle file
    nsString        GetString(const nsString& name);

private:
    // Pointer to localized strings used for UI
    nsCOMPtr<nsIStringBundle> mStringBundle;
};

#endif // nsEditorShell_h___
