/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
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

#ifndef nsPlaintextEditor_h__
#define nsPlaintextEditor_h__

#include "nsCOMPtr.h"

#include "nsIPlaintextEditor.h"
#include "nsIEditorMailSupport.h"

#include "nsEditor.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"

#include "nsEditRules.h"
 
class nsIDOMKeyEvent;
class nsITransferable;
class nsIDOMEventReceiver;
class nsIDocumentEncoder;

/**
 * The text editor implementation.
 * Use to edit text document represented as a DOM tree. 
 */
class nsPlaintextEditor : public nsEditor,
                          public nsIPlaintextEditor,
                          public nsIEditorMailSupport
{

public:

// Interfaces for addref and release and queryinterface
// NOTE macro used is for classes that inherit from 
// another class. Only the base class should use NS_DECL_ISUPPORTS
  NS_DECL_ISUPPORTS_INHERITED

  /* below used by TypedText() */
  enum {
    eTypedText,  /* user typed text */
    eTypedBR,    /* user typed shift-enter to get a br */
    eTypedBreak  /* user typed enter */
  };

           nsPlaintextEditor();
  virtual  ~nsPlaintextEditor();

  /* ------------ nsIPlaintextEditor methods -------------- */
  NS_DECL_NSIPLAINTEXTEDITOR

  /* ------------ nsIEditorMailSupport overrides -------------- */
  NS_IMETHOD PasteAsQuotation(PRInt32 aSelectionType);
  NS_IMETHOD InsertAsQuotation(const nsAReadableString& aQuotedText,
                               nsIDOMNode** aNodeInserted);
  NS_IMETHOD PasteAsCitedQuotation(const nsAReadableString& aCitation,
                                   PRInt32 aSelectionType);
  NS_IMETHOD InsertAsCitedQuotation(const nsAReadableString& aQuotedText,
                                    const nsAReadableString& aCitation,
                                    PRBool aInsertHTML,
                                    const nsAReadableString& aCharset,
                                    nsIDOMNode** aNodeInserted);
  NS_IMETHOD Rewrap(PRBool aRespectNewlines);
  NS_IMETHOD StripCites();
  NS_IMETHOD GetEmbeddedObjects(nsISupportsArray** aNodeList);

  /* ------------ nsIEditorIMESupport overrides -------------- */
  
  NS_IMETHOD SetCompositionString(const nsAReadableString & aCompositionString, nsIPrivateTextRangeList * aTextRange, nsTextEventReply * aReply);
  NS_IMETHOD GetReconversionString(nsReconversionEventReply* aReply);

  /* ------------ Overrides of nsEditor interface methods -------------- */
  NS_IMETHOD BeginComposition(nsTextEventReply* aReply);

  /** prepare the editor for use */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell,  nsIContent *aRoot, nsISelectionController *aSelCon, PRUint32 aFlags);
  
  NS_IMETHOD GetDocumentIsEmpty(PRBool *aDocumentIsEmpty);

  NS_IMETHOD DeleteSelection(EDirection aAction);

  NS_IMETHOD SetDocumentCharacterSet(const nsAReadableString & characterSet);

  /** we override this here to install event listeners */
  NS_IMETHOD PostCreate();

  NS_IMETHOD GetFlags(PRUint32 *aFlags);
  NS_IMETHOD SetFlags(PRUint32 aFlags);

  NS_IMETHOD Undo(PRUint32 aCount);
  NS_IMETHOD Redo(PRUint32 aCount);

  NS_IMETHOD Cut();
  NS_IMETHOD CanCut(PRBool *aCanCut);
  NS_IMETHOD Copy();
  NS_IMETHOD CanCopy(PRBool *aCanCopy);
  NS_IMETHOD Paste(PRInt32 aSelectionType);
  NS_IMETHOD CanPaste(PRInt32 aSelectionType, PRBool *aCanPaste);

  NS_IMETHOD CanDrag(nsIDOMEvent *aDragEvent, PRBool *aCanDrag);
  NS_IMETHOD DoDrag(nsIDOMEvent *aDragEvent);
  NS_IMETHOD InsertFromDrop(nsIDOMEvent* aDropEvent);

  NS_IMETHOD OutputToString(nsAWritableString& aOutputString,
                            const nsAReadableString& aFormatType,
                            PRUint32 aFlags);
                            
  NS_IMETHOD OutputToStream(nsIOutputStream* aOutputStream,
                            const nsAReadableString& aFormatType,
                            const nsAReadableString& aCharsetOverride,
                            PRUint32 aFlags);


  /** All editor operations which alter the doc should be prefaced
   *  with a call to StartOperation, naming the action and direction */
  NS_IMETHOD StartOperation(PRInt32 opID, nsIEditor::EDirection aDirection);

  /** All editor operations which alter the doc should be followed
   *  with a call to EndOperation */
  NS_IMETHOD EndOperation();

  /** make the given selection span the entire document */
  NS_IMETHOD SelectEntireDocument(nsISelection *aSelection);

  /* ------------ Utility Routines, not part of public API -------------- */
  NS_IMETHOD TypedText(const nsAReadableString& aString, PRInt32 aAction);

  /** returns the absolute position of the end points of aSelection
    * in the document as a text stream.
    */
  nsresult GetTextSelectionOffsets(nsISelection *aSelection,
                                   PRInt32 &aStartOffset, 
                                   PRInt32 &aEndOffset);

  nsresult GetAbsoluteOffsetsForPoints(nsIDOMNode *aInStartNode,
                                       PRInt32 aInStartOffset,
                                       nsIDOMNode *aInEndNode,
                                       PRInt32 aInEndOffset,
                                       nsIDOMNode *aInCommonParentNode,
                                       PRInt32 &aOutStartOffset, 
                                       PRInt32 &aEndOffset);

protected:

  NS_IMETHOD  InitRules();
  void        BeginEditorInit();
  nsresult    EndEditorInit();
  
  /** install the event listeners for the editor 
    * used to be part of Init, but now broken out into a separate method
    * called by PostCreate, giving the caller the chance to interpose
    * their own listeners before we install our own backstops.
    */
  NS_IMETHOD InstallEventListeners();

  /** returns the layout object (nsIFrame in the real world) for aNode
    * @param aNode          the content to get a frame for
    * @param aLayoutObject  the "primary frame" for aNode, if one exists.  May be null
    * @return NS_OK whether a frame is found or not
    *         an error if some serious error occurs
    */
  NS_IMETHOD GetLayoutObject(nsIDOMNode *aInNode, nsISupports **aOutLayoutObject);
  NS_IMETHOD GetBodyStyleContext(nsIStyleContext** aStyleContext);

  // Helpers for output routines
  NS_IMETHOD GetAndInitDocEncoder(const nsAReadableString& aFormatType,
                                  PRUint32 aFlags,
                                  const nsAReadableString& aCharset,
                                  nsIDocumentEncoder** encoder);

  // key event helpers
  NS_IMETHOD CreateBR(nsIDOMNode *aNode, PRInt32 aOffset, 
                      nsCOMPtr<nsIDOMNode> *outBRNode, EDirection aSelect = eNone);
  NS_IMETHOD CreateBRImpl(nsCOMPtr<nsIDOMNode> *aInOutParent, 
                         PRInt32 *aInOutOffset, 
                         nsCOMPtr<nsIDOMNode> *outBRNode, 
                         EDirection aSelect);
  NS_IMETHOD InsertBR(nsCOMPtr<nsIDOMNode> *outBRNode);

  NS_IMETHOD IsRootTag(nsString &aTag, PRBool &aIsTag);


  // factored methods for handling insertion of data from transferables (drag&drop or clipboard)
  NS_IMETHOD PrepareTransferable(nsITransferable **transferable);
  NS_IMETHOD InsertTextFromTransferable(nsITransferable *transferable);

  /** simple utility to handle any error with event listener allocation or registration */
  void HandleEventListenerError();

  /* small utility routine to test the eEditorReadonly bit */
  PRBool IsModifiable();

  nsresult GetDOMEventReceiver(nsIDOMEventReceiver **aEventReceiver);

  //XXX Kludge: Used to suppress spurious drag/drop events (bug 50703)
  PRBool   mIgnoreSpuriousDragEvent;
  NS_IMETHOD IgnoreSpuriousDragEvent(PRBool aIgnoreSpuriousDragEvent) {mIgnoreSpuriousDragEvent = aIgnoreSpuriousDragEvent; return NS_OK;}

// Data members
protected:

  nsCOMPtr<nsIEditRules>        mRules;
  nsCOMPtr<nsIDOMEventListener> mKeyListenerP;
  nsCOMPtr<nsIDOMEventListener> mMouseListenerP;
  nsCOMPtr<nsIDOMEventListener> mTextListenerP;
  nsCOMPtr<nsIDOMEventListener> mCompositionListenerP;
  nsCOMPtr<nsIDOMEventListener> mDragListenerP;
  nsCOMPtr<nsIDOMEventListener> mFocusListenerP;
  PRBool 	mIsComposing;
  PRInt32 mMaxTextLength;
  PRInt32 mInitTriggerCounter;

// friends
friend class nsHTMLEditRules;
friend class nsTextEditRules;
friend class nsAutoEditInitRulesTrigger;

};

#endif //nsPlaintextEditor_h__

