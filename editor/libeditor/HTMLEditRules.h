/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HTMLEditRules_h
#define HTMLEditRules_h

#include "TypeInState.h"
#include "mozilla/EditorDOMPoint.h"  // for EditorDOMPoint
#include "mozilla/SelectionState.h"
#include "mozilla/TextEditRules.h"
#include "mozilla/TypeInState.h"  // for AutoStyleCacheArray
#include "nsCOMPtr.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nscore.h"

class nsAtom;
class nsINode;
class nsRange;

namespace mozilla {

class EditActionResult;
class HTMLEditor;
class SplitNodeResult;
class TextEditor;
enum class EditSubAction : int32_t;

namespace dom {
class Document;
class Element;
class Selection;
}  // namespace dom

/**
 * Same as TextEditRules, any methods which may modify the DOM tree or
 * Selection should be marked as MOZ_MUST_USE and return nsresult directly
 * or with simple class like EditActionResult.  And every caller of them
 * has to check whether the result is NS_ERROR_EDITOR_DESTROYED and if it is,
 * its callers should stop handling edit action since after mutation event
 * listener or selectionchange event listener disables the editor, we should
 * not modify the DOM tree nor Selection anymore.  And also when methods of
 * this class call methods of other classes like HTMLEditor and WSRunObject,
 * they should check whether CanHandleEditAtion() returns false immediately
 * after the calls.  If it returns false, they should return
 * NS_ERROR_EDITOR_DESTROYED.
 */

class HTMLEditRules : public TextEditRules {
 public:
  HTMLEditRules();

  // TextEditRules methods
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult Init(TextEditor* aTextEditor) override;
  virtual nsresult DetachEditor() override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual nsresult BeforeEdit() override;
  MOZ_CAN_RUN_SCRIPT virtual nsresult AfterEdit() override;
  // NOTE: Don't mark WillDoAction() nor DidDoAction() as MOZ_CAN_RUN_SCRIPT
  //       because they are too generic and doing it makes a lot of public
  //       editor methods marked as MOZ_CAN_RUN_SCRIPT too, but some of them
  //       may not causes running script.  So, ideal fix must be that we make
  //       each method callsed by this method public.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual nsresult WillDoAction(EditSubActionInfo& aInfo, bool* aCancel,
                                bool* aHandled) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual nsresult DidDoAction(EditSubActionInfo& aInfo,
                               nsresult aResult) override;
  virtual bool DocumentIsEmpty() const override;

  /**
   * DocumentModified() is called when editor content is changed.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult DocumentModified();

  MOZ_CAN_RUN_SCRIPT
  nsresult GetListState(bool* aMixed, bool* aOL, bool* aUL, bool* aDL);
  MOZ_CAN_RUN_SCRIPT
  nsresult GetListItemState(bool* aMixed, bool* aLI, bool* aDT, bool* aDD);
  MOZ_CAN_RUN_SCRIPT
  nsresult GetAlignment(bool* aMixed, nsIHTMLEditor::EAlignment* aAlign);
  MOZ_CAN_RUN_SCRIPT
  nsresult GetParagraphState(bool* aMixed, nsAString& outFormat);

  void DidCreateNode(Element& aNewElement);
  void DidInsertNode(nsIContent& aNode);
  void WillDeleteNode(nsINode& aChild);
  void DidSplitNode(nsINode& aExistingRightNode, nsINode& aNewLeftNode);
  void WillJoinNodes(nsINode& aLeftNode, nsINode& aRightNode);
  void DidJoinNodes(nsINode& aLeftNode, nsINode& aRightNode);
  void DidInsertText(nsINode& aTextNode, int32_t aOffset,
                     const nsAString& aString);
  void DidDeleteText(nsINode& aTextNode, int32_t aOffset, int32_t aLength);
  void WillDeleteSelection();

 protected:
  virtual ~HTMLEditRules() = default;

  HTMLEditor& HTMLEditorRef() const {
    MOZ_ASSERT(mData);
    return mData->HTMLEditorRef();
  }

  /**
   * Called after deleting selected content.
   * This method removes unnecessary empty nodes and/or inserts <br> if
   * necessary.
   */
  MOZ_CAN_RUN_SCRIPT MOZ_MUST_USE nsresult DidDeleteSelection();

  /**
   * Called before changing absolute positioned element to static positioned.
   * This method actually changes the position property of nearest absolute
   * positioned element.  Therefore, this might cause destroying the HTML
   * editor.
   *
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillRemoveAbsolutePosition(bool* aCancel,
                                                   bool* aHandled);

  /**
   * Called before changing z-index.
   * This method actually changes z-index of nearest absolute positioned
   * element relatively.  Therefore, this might cause destroying the HTML
   * editor.
   *
   * @param aChange             Amount to change z-index.
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillRelativeChangeZIndex(int32_t aChange, bool* aCancel,
                                                 bool* aHandled);

  /**
   * Called before changing an element to absolute positioned.
   * mNewBlockElement of TopLevelEditSubActionData is set to the target
   * element and if necessary, some ancestor nodes of selection may be split.
   *
   * @param aCancel             Returns true if the operation is canceled.
   * @param aHandled            Returns true if the edit action is handled.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult WillAbsolutePosition(bool* aCancel, bool* aHandled);

  /**
   * PrepareToMakeElementAbsolutePosition() is helper method of
   * WillAbsolutePosition() since in some cases, needs to restore selection
   * with AutoSelectionRestorer.  So, all callers have to check if
   * CanHandleEditAction() still returns true after a call of this method.
   * XXX Should be documented outline of this method.
   *
   * @param aHandled            Returns true if the edit action is handled.
   * @param aTargetElement      Returns target element which should be
   *                            changed to absolute positioned.
   */
  MOZ_CAN_RUN_SCRIPT
  MOZ_MUST_USE nsresult PrepareToMakeElementAbsolutePosition(
      bool* aHandled, RefPtr<Element>* aTargetElement);

  nsresult AppendInnerFormatNodes(nsTArray<OwningNonNull<nsINode>>& aArray,
                                  nsINode* aNode);
  nsresult GetFormatString(nsINode* aNode, nsAString& outFormat);

  MOZ_CAN_RUN_SCRIPT
  nsresult GetParagraphFormatNodes(
      nsTArray<OwningNonNull<nsINode>>& outArrayOfNodes);

  /**
   * DocumentModifiedWorker() is called by DocumentModified() either
   * synchronously or asynchronously.
   */
  MOZ_CAN_RUN_SCRIPT void DocumentModifiedWorker();

 protected:
  HTMLEditor* mHTMLEditor;
  bool mInitialized;
};

}  // namespace mozilla

#endif  // #ifndef HTMLEditRules_h
