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


#ifndef nsInterfaceState_h__
#define nsInterfaceState_h__


#include "nsIDOMSelectionListener.h"
#include "nsIDocumentStateListener.h"
#include "nsIWebShell.h"

class nsIHTMLEditor;

// class responsible for communicating changes in local state back to the UI.
// This is currently somewhat tied to a given XUL UI implementation

class nsInterfaceState : public nsIDOMSelectionListener,
                         public nsIDocumentStateListener
{
public:

                nsInterfaceState();
  virtual       ~nsInterfaceState();
  
  NS_DECL_ISUPPORTS
  
  NS_IMETHOD    Init(nsIHTMLEditor* aEditor, nsIWebShell *aChromeWebShell);

  // nsIDOMSelectionListener interface
  NS_IMETHOD    NotifySelectionChanged();
  
  // nsIDocumentStateListener interface
  NS_IMETHOD  	NotifyDocumentCreated();
  NS_IMETHOD  	NotifyDocumentWillBeDestroyed();
  NS_IMETHOD    NotifyDocumentStateChanged(PRBool aNowDirty);
  
protected:

  enum {
    eStateUninitialized   = -1,
    eStateOff             = PR_FALSE,
    eStateOn              = PR_TRUE
  };
  
  nsresult      SetNodeAttribute(const char* nodeID, const char* attributeName, const nsString& newValue);

  nsresult      UpdateParagraphState(const char* observerName, const char* attributeName, nsString& ioParaFormat);
  nsresult      UpdateListState(const char* observerName);
  nsresult      UpdateTextState(const char* tagName, const char* observerName, const char* attributeName, PRInt8& ioState);
  nsresult      UpdateFontFace(const char* observerName, const char* attributeName, nsString& ioFontString);
  nsresult      UpdateDirtyState(PRBool aNowDirty);
  
  // this class should not hold references to the editor or editorShell. Doing
  // so would result in cirular reference chains.
  
  nsIHTMLEditor*  mEditor;		// the HTML editor
  nsIWebShell*  mWebShell;  // web shell for the chrome area

  // current state
  PRInt8        mBoldState;
  PRInt8        mItalicState;
  PRInt8        mUnderlineState;
  
  PRInt8        mDirtyState;
  
  nsString      mParagraphFormat;
  nsString      mFontString;
  nsString      mListTag;				// contains "" for none, "ol" or "ul"
  
};

extern "C" nsresult NS_NewInterfaceState(nsIHTMLEditor* aEditor, nsIWebShell* aWebShell, nsIDOMSelectionListener** aInstancePtrResult);

#endif // nsInterfaceState_h__
