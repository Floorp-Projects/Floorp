/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
#include "nsIPrompt.h"
#include "nsIStreamObserver.h"
#include "nsIDOMDocument.h"
#include "nsVoidArray.h"
#include "nsTextServicesCID.h"
#include "nsIEditorSpellCheck.h"
#include "nsISpellChecker.h"
#include "nsInterfaceState.h"
#include "nsIHTMLEditor.h"
#include "nsIStringBundle.h"
#include "nsICSSStyleSheet.h"

// Parser Observation
#include "nsEditorParserObserver.h"

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
class nsIStyleSheet;

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
    NS_DECL_NSIDOCUMENTLOADEROBSERVER

  protected:
    nsIDOMWindow       *mToolbarWindow;				// weak reference
    nsIDOMWindow       *mContentWindow;				// weak reference

    nsIWebShellWindow  *mWebShellWin;					// weak reference
    nsIWebShell        *mWebShell;						// weak reference

    // The webshell that contains the document being edited.
    // Don't assume that webshell directly contains the document being edited;
    // if we are in a frameset, this assumption is broken.
    nsIWebShell        *mContentAreaWebShell;	// weak reference

  	typedef enum {
  	  eUninitializedEditorType = 0,
  		ePlainTextEditorType = 1,
  		eHTMLTextEditorType = 2
  	} EEditorType;
  	
    nsIPresShell* 	GetPresShellFor(nsIWebShell* aWebShell);
    NS_IMETHOD 			DoEditorMode(nsIWebShell *aWebShell);
    NS_IMETHOD			InstantiateEditor(nsIDOMDocument *aDoc, nsIPresShell *aPresShell);
    NS_IMETHOD			ScrollSelectionIntoView();
    NS_IMETHOD      TransferDocumentStateListeners();
    NS_IMETHOD			RemoveOneProperty(const nsString& aProp, const nsString& aAttr);
    void 						SetButtonImage(nsIDOMNode * aParentNode, PRInt32 aBtnNum, const nsString &aResName);
		NS_IMETHOD  	  PrepareDocumentForEditing(nsIDocumentLoader* aLoader, nsIURI *aUrl);
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

		EEditorType					mEditorType;
		nsString						mEditorTypeString;	// string which describes which editor type will be instantiated (lowercased)
    nsCOMPtr<nsIHTMLEditor>	 	mEditor;						// this can be either an HTML or plain text (or other?) editor

    nsCOMPtr<nsISupports>   mSearchContext;		// context used for search and replace. Owned by the appshell.
    
    nsInterfaceState*         mStateMaintainer;           // we hold the owning ref to this.

    PRInt32 mWrapColumn;      // can't actually set this 'til the editor is created, so we may have to hold on to it for a while

    nsCOMPtr<nsISpellChecker> mSpellChecker;
    nsStringArray   mSuggestedWordList;
    PRInt32         mSuggestedWordIndex;
    NS_IMETHOD      DeleteSuggestedWordList();
    nsStringArray   mDictionaryList;
    PRInt32         mDictionaryIndex;

    PRPackedBool    mCloseWindowWhenLoaded;     // error on load. Close window when loaded
    
    // this is a holding pen for doc state listeners. They will be registered with
    // the editor when that gets created.
    nsCOMPtr<nsISupportsArray> mDocStateListeners;		// contents are nsISupports

    // Get a string from the string bundle file
    void   GetBundleString(const nsString& name, nsString &outString);
    
    // Get the text of the <title> tag
    NS_IMETHOD GetDocumentTitleString(nsString& title);

    // Get the current document title an use it as part of the window title
    // Uses "(Untitled)" for empty title
    NS_IMETHOD UpdateWindowTitle();

private:
    // Pointer to localized strings used for UI
    nsCOMPtr<nsIStringBundle> mStringBundle;
    // Pointer to the EditorContent style sheet we load/unload
    //   for "Edit Mode"/"Browser mode" display
    nsCOMPtr<nsIStyleSheet> mEditModeStyleSheet;
    
    nsEditorParserObserver *mParserObserver;
};

#endif // nsEditorShell_h___
