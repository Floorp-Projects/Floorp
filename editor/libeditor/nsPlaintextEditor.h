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
class nsIDOMDocument;
class nsIDOMElement;
class nsIDOMEvent;
class nsIDOMKeyEvent;
class nsIDOMNode;
class nsIDocumentEncoder;
class nsIEditRules;
class nsIOutputStream;
class nsISelectionController;
class nsITransferable;

namespace mozilla {
namespace dom {
class Selection;
} // namespace dom
} // namespace mozilla

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

  /* ------------ nsIPlaintextEditor methods -------------- */
  NS_DECL_NSIPLAINTEXTEDITOR

  /* ------------ nsIEditorMailSupport overrides -------------- */
  NS_DECL_NSIEDITORMAILSUPPORT

  /* ------------ Overrides of nsEditor interface methods -------------- */
  NS_IMETHOD SetAttributeOrEquivalent(nsIDOMElement * aElement,
                                      const nsAString & aAttribute,
                                      const nsAString & aValue,
                                      bool aSuppressTransaction) override;
  NS_IMETHOD RemoveAttributeOrEquivalent(nsIDOMElement * aElement,
                                         const nsAString & aAttribute,
                                         bool aSuppressTransaction) override;

  /** prepare the editor for use */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIContent *aRoot,
                  nsISelectionController *aSelCon, uint32_t aFlags,
                  const nsAString& aValue) override;

  NS_IMETHOD GetDocumentIsEmpty(bool *aDocumentIsEmpty) override;
  NS_IMETHOD GetIsDocumentEditable(bool *aIsDocumentEditable) override;

  NS_IMETHOD DeleteSelection(EDirection aAction,
                             EStripWrappers aStripWrappers) override;

  NS_IMETHOD SetDocumentCharacterSet(const nsACString & characterSet) override;

  NS_IMETHOD Undo(uint32_t aCount) override;
  NS_IMETHOD Redo(uint32_t aCount) override;

  NS_IMETHOD Cut() override;
  NS_IMETHOD CanCut(bool *aCanCut) override;
  NS_IMETHOD Copy() override;
  NS_IMETHOD CanCopy(bool *aCanCopy) override;
  NS_IMETHOD CanDelete(bool *aCanDelete) override;
  NS_IMETHOD Paste(int32_t aSelectionType) override;
  NS_IMETHOD CanPaste(int32_t aSelectionType, bool *aCanPaste) override;
  NS_IMETHOD PasteTransferable(nsITransferable *aTransferable) override;
  NS_IMETHOD CanPasteTransferable(nsITransferable *aTransferable, bool *aCanPaste) override;

  NS_IMETHOD OutputToString(const nsAString& aFormatType,
                            uint32_t aFlags,
                            nsAString& aOutputString) override;

  NS_IMETHOD OutputToStream(nsIOutputStream* aOutputStream,
                            const nsAString& aFormatType,
                            const nsACString& aCharsetOverride,
                            uint32_t aFlags) override;


  /** All editor operations which alter the doc should be prefaced
   *  with a call to StartOperation, naming the action and direction */
  NS_IMETHOD StartOperation(EditAction opID,
                            nsIEditor::EDirection aDirection) override;

  /** All editor operations which alter the doc should be followed
   *  with a call to EndOperation */
  NS_IMETHOD EndOperation() override;

  /** make the given selection span the entire document */
  virtual nsresult SelectEntireDocument(mozilla::dom::Selection* aSelection) override;

  virtual nsresult HandleKeyPressEvent(nsIDOMKeyEvent* aKeyEvent) override;

  virtual already_AddRefed<mozilla::dom::EventTarget> GetDOMEventTarget() override;

  virtual nsresult BeginIMEComposition(mozilla::WidgetCompositionEvent* aEvent) override;
  virtual nsresult UpdateIMEComposition(nsIDOMEvent* aTextEvent) override;

  virtual already_AddRefed<nsIContent> GetInputEventTargetContent() override;

  /* ------------ Utility Routines, not part of public API -------------- */
  NS_IMETHOD TypedText(const nsAString& aString, ETypingAction aAction);

  nsresult InsertTextAt(const nsAString &aStringToInsert,
                        nsIDOMNode *aDestinationNode,
                        int32_t aDestOffset,
                        bool aDoDeleteSelection);

  virtual nsresult InsertFromDataTransfer(mozilla::dom::DataTransfer *aDataTransfer,
                                          int32_t aIndex,
                                          nsIDOMDocument *aSourceDoc,
                                          nsIDOMNode *aDestinationNode,
                                          int32_t aDestOffset,
                                          bool aDoDeleteSelection) override;

  virtual nsresult InsertFromDrop(nsIDOMEvent* aDropEvent) override;

  /**
   * Extends the selection for given deletion operation
   * If done, also update aAction to what's actually left to do after the
   * extension.
   */
  nsresult ExtendSelectionForDelete(mozilla::dom::Selection* aSelection,
                                    nsIEditor::EDirection *aAction);

  // Return true if the data is safe to insert as the source and destination
  // principals match, or we are in a editor context where this doesn't matter.
  // Otherwise, the data must be sanitized first.
  bool IsSafeToInsertData(nsIDOMDocument* aSourceDoc);

  static void GetDefaultEditorPrefs(int32_t &aNewLineHandling,
                                    int32_t &aCaretStyle);

protected:
  virtual  ~nsPlaintextEditor();

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
  already_AddRefed<mozilla::dom::Element>
      CreateBRImpl(nsCOMPtr<nsINode>* aInOutParent, int32_t* aInOutOffset,
                   EDirection aSelect);
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

  enum PasswordFieldAllowed {
    ePasswordFieldAllowed,
    ePasswordFieldNotAllowed
  };
  bool CanCutOrCopy(PasswordFieldAllowed aPasswordFieldAllowed);
  bool FireClipboardEvent(mozilla::EventMessage aEventMessage,
                          int32_t aSelectionType,
                          bool* aActionTaken = nullptr);

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

