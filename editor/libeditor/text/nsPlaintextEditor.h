/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman <glazman@netscape.com>
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

#ifndef nsPlaintextEditor_h__
#define nsPlaintextEditor_h__

#include "nsCOMPtr.h"

#include "nsIPlaintextEditor.h"
#include "nsIEditorMailSupport.h"

#include "nsEditor.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventListener.h"

#include "nsEditRules.h"
#include "nsCycleCollectionParticipant.h"
 
class nsITransferable;
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
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsPlaintextEditor, nsEditor)

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
  NS_DECL_NSIEDITORMAILSUPPORT

  /* ------------ Overrides of nsEditor interface methods -------------- */
  NS_IMETHOD SetAttributeOrEquivalent(nsIDOMElement * aElement,
                                      const nsAString & aAttribute,
                                      const nsAString & aValue,
                                      bool aSuppressTransaction);
  NS_IMETHOD RemoveAttributeOrEquivalent(nsIDOMElement * aElement,
                                         const nsAString & aAttribute,
                                         bool aSuppressTransaction);

  /** prepare the editor for use */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIContent *aRoot, nsISelectionController *aSelCon, PRUint32 aFlags);
  
  NS_IMETHOD GetDocumentIsEmpty(bool *aDocumentIsEmpty);
  NS_IMETHOD GetIsDocumentEditable(bool *aIsDocumentEditable);

  NS_IMETHOD DeleteSelection(EDirection aAction);

  NS_IMETHOD SetDocumentCharacterSet(const nsACString & characterSet);

  NS_IMETHOD Undo(PRUint32 aCount);
  NS_IMETHOD Redo(PRUint32 aCount);

  NS_IMETHOD Cut();
  NS_IMETHOD CanCut(bool *aCanCut);
  NS_IMETHOD Copy();
  NS_IMETHOD CanCopy(bool *aCanCopy);
  NS_IMETHOD Paste(PRInt32 aSelectionType);
  NS_IMETHOD CanPaste(PRInt32 aSelectionType, bool *aCanPaste);
  NS_IMETHOD PasteTransferable(nsITransferable *aTransferable);
  NS_IMETHOD CanPasteTransferable(nsITransferable *aTransferable, bool *aCanPaste);

  NS_IMETHOD CanDrag(nsIDOMEvent *aDragEvent, bool *aCanDrag);
  NS_IMETHOD DoDrag(nsIDOMEvent *aDragEvent);
  NS_IMETHOD InsertFromDrop(nsIDOMEvent* aDropEvent);

  NS_IMETHOD OutputToString(const nsAString& aFormatType,
                            PRUint32 aFlags,
                            nsAString& aOutputString);
                            
  NS_IMETHOD OutputToStream(nsIOutputStream* aOutputStream,
                            const nsAString& aFormatType,
                            const nsACString& aCharsetOverride,
                            PRUint32 aFlags);


  /** All editor operations which alter the doc should be prefaced
   *  with a call to StartOperation, naming the action and direction */
  NS_IMETHOD StartOperation(PRInt32 opID, nsIEditor::EDirection aDirection);

  /** All editor operations which alter the doc should be followed
   *  with a call to EndOperation */
  NS_IMETHOD EndOperation();

  /** make the given selection span the entire document */
  NS_IMETHOD SelectEntireDocument(nsISelection *aSelection);

  virtual nsresult HandleKeyPressEvent(nsIDOMKeyEvent* aKeyEvent);

  virtual already_AddRefed<nsIDOMEventTarget> GetDOMEventTarget();

  virtual nsresult BeginIMEComposition();
  virtual nsresult UpdateIMEComposition(const nsAString &aCompositionString,
                                        nsIPrivateTextRangeList *aTextRange);

  /* ------------ Utility Routines, not part of public API -------------- */
  NS_IMETHOD TypedText(const nsAString& aString, PRInt32 aAction);

  /** Returns the absolute position of the end points of aSelection
   * in the document as a text stream.
   * Invariant: aStartOffset <= aEndOffset.
   */
  nsresult GetTextSelectionOffsets(nsISelection *aSelection,
                                   PRUint32 &aStartOffset, 
                                   PRUint32 &aEndOffset);

  nsresult InsertTextAt(const nsAString &aStringToInsert,
                        nsIDOMNode *aDestinationNode,
                        PRInt32 aDestOffset,
                        bool aDoDeleteSelection);

  /**
   * Extends the selection for given deletion operation
   * If done, also update aAction to what's actually left to do after the
   * extension.
   */
  nsresult ExtendSelectionForDelete(nsISelection* aSelection,
                                    nsIEditor::EDirection *aAction);

  static void GetDefaultEditorPrefs(PRInt32 &aNewLineHandling,
                                    PRInt32 &aCaretStyle);

protected:

  NS_IMETHOD  InitRules();
  void        BeginEditorInit();
  nsresult    EndEditorInit();

  // Helpers for output routines
  NS_IMETHOD GetAndInitDocEncoder(const nsAString& aFormatType,
                                  PRUint32 aFlags,
                                  const nsACString& aCharset,
                                  nsIDocumentEncoder** encoder);

  // key event helpers
  NS_IMETHOD CreateBR(nsIDOMNode *aNode, PRInt32 aOffset, 
                      nsCOMPtr<nsIDOMNode> *outBRNode, EDirection aSelect = eNone);
  NS_IMETHOD CreateBRImpl(nsCOMPtr<nsIDOMNode> *aInOutParent, 
                         PRInt32 *aInOutOffset, 
                         nsCOMPtr<nsIDOMNode> *outBRNode, 
                         EDirection aSelect);
  NS_IMETHOD InsertBR(nsCOMPtr<nsIDOMNode> *outBRNode);

  // factored methods for handling insertion of data from transferables (drag&drop or clipboard)
  NS_IMETHOD PrepareTransferable(nsITransferable **transferable);
  NS_IMETHOD InsertTextFromTransferable(nsITransferable *transferable,
                                        nsIDOMNode *aDestinationNode,
                                        PRInt32 aDestOffset,
                                        bool aDoDeleteSelection);
  virtual nsresult SetupDocEncoder(nsIDocumentEncoder **aDocEncoder);
  virtual nsresult PutDragDataInTransferable(nsITransferable **aTransferable);

  /** shared outputstring; returns whether selection is collapsed and resulting string */
  nsresult SharedOutputString(PRUint32 aFlags, bool* aIsCollapsed, nsAString& aResult);

  /* small utility routine to test the eEditorReadonly bit */
  bool IsModifiable();

  //XXX Kludge: Used to suppress spurious drag/drop events (bug 50703)
  bool     mIgnoreSpuriousDragEvent;
  NS_IMETHOD IgnoreSpuriousDragEvent(bool aIgnoreSpuriousDragEvent) {mIgnoreSpuriousDragEvent = aIgnoreSpuriousDragEvent; return NS_OK;}

  bool CanCutOrCopy();
  bool FireClipboardEvent(PRInt32 aType);

// Data members
protected:

  nsCOMPtr<nsIEditRules>        mRules;
  bool    mWrapToWindow;
  PRInt32 mWrapColumn;
  PRInt32 mMaxTextLength;
  PRInt32 mInitTriggerCounter;
  PRInt32 mNewlineHandling;
  PRInt32 mCaretStyle;

// friends
friend class nsHTMLEditRules;
friend class nsTextEditRules;
friend class nsAutoEditInitRulesTrigger;

};

#endif //nsPlaintextEditor_h__

