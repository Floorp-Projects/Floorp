/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPlaintextEditor_h__
#define nsPlaintextEditor_h__

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsEditor.h"
#include "nsIEditor.h"
#include "nsIEditorMailSupport.h"
#include "nsIPlaintextEditor.h"
#include "nsISupportsImpl.h"
#include "nscore.h"

class nsIContent;
class nsIDOMDataTransfer;
class nsIDOMDocument;
class nsIDOMElement;
class nsIDOMEvent;
class nsIDOMEventTarget;
class nsIDOMKeyEvent;
class nsIDOMNode;
class nsIDocumentEncoder;
class nsIEditRules;
class nsIOutputStream;
class nsIPrivateTextRangeList;
class nsISelection;
class nsISelectionController;
class nsITransferable;

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
  enum ETypingAction {
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
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIContent *aRoot, nsISelectionController *aSelCon, uint32_t aFlags);
  
  NS_IMETHOD GetDocumentIsEmpty(bool *aDocumentIsEmpty);
  NS_IMETHOD GetIsDocumentEditable(bool *aIsDocumentEditable);

  NS_IMETHOD DeleteSelection(EDirection aAction,
                             EStripWrappers aStripWrappers);

  NS_IMETHOD SetDocumentCharacterSet(const nsACString & characterSet);

  NS_IMETHOD Undo(uint32_t aCount);
  NS_IMETHOD Redo(uint32_t aCount);

  NS_IMETHOD Cut();
  NS_IMETHOD CanCut(bool *aCanCut);
  NS_IMETHOD Copy();
  NS_IMETHOD CanCopy(bool *aCanCopy);
  NS_IMETHOD Paste(int32_t aSelectionType);
  NS_IMETHOD CanPaste(int32_t aSelectionType, bool *aCanPaste);
  NS_IMETHOD PasteTransferable(nsITransferable *aTransferable);
  NS_IMETHOD CanPasteTransferable(nsITransferable *aTransferable, bool *aCanPaste);

  NS_IMETHOD OutputToString(const nsAString& aFormatType,
                            uint32_t aFlags,
                            nsAString& aOutputString);
                            
  NS_IMETHOD OutputToStream(nsIOutputStream* aOutputStream,
                            const nsAString& aFormatType,
                            const nsACString& aCharsetOverride,
                            uint32_t aFlags);


  /** All editor operations which alter the doc should be prefaced
   *  with a call to StartOperation, naming the action and direction */
  NS_IMETHOD StartOperation(EditAction opID,
                            nsIEditor::EDirection aDirection);

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

  virtual already_AddRefed<nsIContent> GetInputEventTargetContent();

  /* ------------ Utility Routines, not part of public API -------------- */
  NS_IMETHOD TypedText(const nsAString& aString, ETypingAction aAction);

  nsresult InsertTextAt(const nsAString &aStringToInsert,
                        nsIDOMNode *aDestinationNode,
                        int32_t aDestOffset,
                        bool aDoDeleteSelection);

  virtual nsresult InsertFromDataTransfer(nsIDOMDataTransfer *aDataTransfer,
                                          int32_t aIndex,
                                          nsIDOMDocument *aSourceDoc,
                                          nsIDOMNode *aDestinationNode,
                                          int32_t aDestOffset,
                                          bool aDoDeleteSelection);

  virtual nsresult InsertFromDrop(nsIDOMEvent* aDropEvent);

  /**
   * Extends the selection for given deletion operation
   * If done, also update aAction to what's actually left to do after the
   * extension.
   */
  nsresult ExtendSelectionForDelete(nsISelection* aSelection,
                                    nsIEditor::EDirection *aAction);

  // Return true if the data is safe to insert as the source and destination
  // principals match, or we are in a editor context where this doesn't matter.
  // Otherwise, the data must be sanitized first.
  bool IsSafeToInsertData(nsIDOMDocument* aSourceDoc);

  static void GetDefaultEditorPrefs(int32_t &aNewLineHandling,
                                    int32_t &aCaretStyle);

protected:

  NS_IMETHOD  InitRules();
  void        BeginEditorInit();
  nsresult    EndEditorInit();

  // Helpers for output routines
  NS_IMETHOD GetAndInitDocEncoder(const nsAString& aFormatType,
                                  uint32_t aFlags,
                                  const nsACString& aCharset,
                                  nsIDocumentEncoder** encoder);

  // key event helpers
  NS_IMETHOD CreateBR(nsIDOMNode *aNode, int32_t aOffset, 
                      nsCOMPtr<nsIDOMNode> *outBRNode, EDirection aSelect = eNone);
  nsresult CreateBRImpl(nsCOMPtr<nsIDOMNode>* aInOutParent,
                        int32_t* aInOutOffset,
                        nsCOMPtr<nsIDOMNode>* outBRNode,
                        EDirection aSelect);
  nsresult InsertBR(nsCOMPtr<nsIDOMNode>* outBRNode);

  // factored methods for handling insertion of data from transferables (drag&drop or clipboard)
  NS_IMETHOD PrepareTransferable(nsITransferable **transferable);
  NS_IMETHOD InsertTextFromTransferable(nsITransferable *transferable,
                                        nsIDOMNode *aDestinationNode,
                                        int32_t aDestOffset,
                                        bool aDoDeleteSelection);

  /** shared outputstring; returns whether selection is collapsed and resulting string */
  nsresult SharedOutputString(uint32_t aFlags, bool* aIsCollapsed, nsAString& aResult);

  /* small utility routine to test the eEditorReadonly bit */
  bool IsModifiable();

  bool CanCutOrCopy();
  bool FireClipboardEvent(int32_t aType, int32_t aSelectionType);

  bool UpdateMetaCharset(nsIDOMDocument* aDocument,
                         const nsACString& aCharacterSet);

// Data members
protected:

  nsCOMPtr<nsIEditRules>        mRules;
  bool    mWrapToWindow;
  int32_t mWrapColumn;
  int32_t mMaxTextLength;
  int32_t mInitTriggerCounter;
  int32_t mNewlineHandling;
  int32_t mCaretStyle;

// friends
friend class nsHTMLEditRules;
friend class nsTextEditRules;
friend class nsAutoEditInitRulesTrigger;

};

#endif //nsPlaintextEditor_h__

