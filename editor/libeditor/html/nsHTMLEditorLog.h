/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
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

#ifndef nsHTMLEditorLog_h__
#define nsHTMLEditorLog_h__

#include "nsHTMLEditor.h"
#include "nsIEditorLogging.h"
#include "nsIFileSpec.h"
#include "nsCOMPtr.h"

class nsEditorTxnLog;

/** implementation of a transaction listener object.
 *
 */
class nsHTMLEditorLog : public nsHTMLEditor,
                        public nsIEditorLogging
{
private:

  nsCOMPtr<nsIFileSpec> mFileSpec;
  nsEditorTxnLog       *mEditorTxnLog;
  PRInt32               mLocked;
  PRInt32               mDepth;

public:

  // Interfaces for AddRef, Release, and QueryInterface.
  NS_DECL_ISUPPORTS_INHERITED

           nsHTMLEditorLog();
  virtual ~nsHTMLEditorLog();

  /* nsIHTMLEditor method implementations. */
  NS_IMETHOD SetInlineProperty(nsIAtom *aProperty, 
                               const nsString *aAttribute,
                               const nsString *aValue);
  NS_IMETHOD SetParagraphFormat(const nsString& aParagraphFormat);
  NS_IMETHOD RemoveInlineProperty(nsIAtom *aProperty, const nsString *aAttribute);
  NS_IMETHOD DeleteSelection(nsIEditor::ESelectionCollapseDirection aAction);
  NS_IMETHOD InsertText(const nsString& aStringToInsert);
  NS_IMETHOD InsertBreak();
  NS_IMETHOD Undo(PRUint32 aCount);
  NS_IMETHOD Redo(PRUint32 aCount);
  NS_IMETHOD BeginTransaction();
  NS_IMETHOD EndTransaction();
  NS_IMETHOD SelectAll();
  NS_IMETHOD BeginningOfDocument();
  NS_IMETHOD EndOfDocument();

  NS_IMETHOD Cut();
  NS_IMETHOD Copy();
  NS_IMETHOD Paste();
  NS_IMETHOD PasteAsQuotation();
  NS_IMETHOD PasteAsPlaintextQuotation();
  NS_IMETHOD PasteAsCitedQuotation(const nsString& aCitation);
  NS_IMETHOD InsertAsQuotation(const nsString& aQuotedText);
  NS_IMETHOD InsertAsPlaintextQuotation(const nsString& aQuotedText);
  NS_IMETHOD InsertAsCitedQuotation(const nsString& aQuotedText, const nsString& aCitation);

  NS_IMETHOD ApplyStyleSheet(const nsString& aURL);

  NS_IMETHOD SetBackgroundColor(const nsString& aColor);
  NS_IMETHOD SetBodyAttribute(const nsString& aAttr, const nsString& aValue);
  NS_IMETHOD InsertList(const nsString& aListType);
  NS_IMETHOD Indent(const nsString& aIndent);
  NS_IMETHOD Align(const nsString& aAlign);
  NS_IMETHOD InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection);
  NS_IMETHOD InsertLinkAroundSelection(nsIDOMElement* aAnchorElement);

  NS_IMETHOD StartLogging(nsIFileSpec *aLogFile);
  NS_IMETHOD StopLogging();

  /* nsHTMLEditorLog public methods. */
  nsresult Write(const char *aBuffer);
  nsresult WriteInt(const char *aFormat, PRInt32 aInt);
  nsresult Flush();
  nsresult PrintUnicode(const nsString &aString);
  nsresult PrintSelection();
  nsresult PrintNode(nsIDOMNode *aNode, PRInt32 aDepth=0);
  nsresult PrintElementNode(nsIDOMNode *aNode, PRInt32 aDepth);
  nsresult PrintTextNode(nsIDOMNode *aNode, PRInt32 aDepth);
  nsresult PrintAttributeNode(nsIDOMNode *aNode, PRInt32 aDepth=0);
  nsresult PrintNodeChildren(nsIDOMNode *aNode, PRInt32 aDepth=0);
  nsresult GetNodeTreeOffsets(nsIDOMNode *aNode, PRInt32 **aResult, PRInt32 *aLength);
  nsresult Lock();
  nsresult Unlock();
};

class nsAutoHTMLEditorLogLock
{
  nsHTMLEditorLog *mLog;

public:

  nsAutoHTMLEditorLogLock(nsHTMLEditorLog *aLog)
  {
    mLog = aLog;

    if (mLog)
      mLog->Lock();
  }

  ~nsAutoHTMLEditorLogLock()
  {
    if (mLog)
      mLog->Unlock();
  }
};

#endif // nsHTMLEditorLog_h__
