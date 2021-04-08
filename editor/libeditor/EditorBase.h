/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorBase_h
#define mozilla_EditorBase_h

#include "mozilla/Assertions.h"          // for MOZ_ASSERT, etc.
#include "mozilla/EditAction.h"          // for EditAction and EditSubAction
#include "mozilla/EditorDOMPoint.h"      // for EditorDOMPoint
#include "mozilla/EventForwards.h"       // for InputEventTargetRanges
#include "mozilla/Maybe.h"               // for Maybe
#include "mozilla/OwningNonNull.h"       // for OwningNonNull
#include "mozilla/TypeInState.h"         // for PropItem, StyleCache
#include "mozilla/RangeBoundary.h"       // for RawRangeBoundary, RangeBoundary
#include "mozilla/SelectionState.h"      // for RangeUpdater, etc.
#include "mozilla/StyleSheet.h"          // for StyleSheet
#include "mozilla/TransactionManager.h"  // for TransactionManager
#include "mozilla/WeakPtr.h"             // for WeakPtr
#include "mozilla/dom/DataTransfer.h"    // for dom::DataTransfer
#include "mozilla/dom/HTMLBRElement.h"   // for dom::HTMLBRElement
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Text.h"
#include "nsAtom.h"    // for nsAtom, nsStaticAtom
#include "nsCOMPtr.h"  // for already_AddRefed, nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"
#include "nsIContentInlines.h"       // for nsINode::IsEditable()
#include "nsIEditor.h"               // for nsIEditor, etc.
#include "nsIFrame.h"                // for nsBidiLevel
#include "nsISelectionController.h"  // for nsISelectionController constants
#include "nsISelectionListener.h"    // for nsISelectionListener
#include "nsISupportsImpl.h"         // for EditorBase::Release, etc.
#include "nsIWeakReferenceUtils.h"   // for nsWeakPtr
#include "nsLiteralString.h"         // for NS_LITERAL_STRING
#include "nsPIDOMWindow.h"           // for nsPIDOMWindowInner, etc.
#include "nsString.h"                // for nsCString
#include "nsTArray.h"                // for nsTArray and nsAutoTArray
#include "nsWeakReference.h"         // for nsSupportsWeakReference
#include "nscore.h"                  // for nsresult, nsAString, etc.

class mozInlineSpellChecker;
class nsAtom;
class nsCaret;
class nsIContent;
class nsIDocumentStateListener;
class nsIEditActionListener;
class nsIEditorObserver;
class nsINode;
class nsIPrincipal;
class nsISupports;
class nsITransferable;
class nsITransaction;
class nsITransactionListener;
class nsIWidget;
class nsRange;

namespace mozilla {
class AlignStateAtSelection;
class AutoRangeArray;
class AutoSelectionRestorer;
class AutoTopLevelEditSubActionNotifier;
class AutoTransactionBatch;
class AutoTransactionsConserveSelection;
class AutoUpdateViewBatch;
class ChangeAttributeTransaction;
class CompositionTransaction;
class CreateElementTransaction;
class CSSEditUtils;
class DeleteNodeTransaction;
class DeleteRangeTransaction;
class DeleteTextTransaction;
class EditActionResult;
class EditAggregateTransaction;
class EditorEventListener;
class EditTransactionBase;
class ErrorResult;
class HTMLEditor;
class HTMLEditUtils;
class IMEContentObserver;
class InsertNodeTransaction;
class InsertTextTransaction;
class JoinNodeTransaction;
class ListElementSelectionState;
class ListItemElementSelectionState;
class ParagraphStateAtSelection;
class PlaceholderTransaction;
class PresShell;
class ReplaceTextTransaction;
class SplitNodeResult;
class SplitNodeTransaction;
class TextComposition;
class TextEditor;
class TextInputListener;
class TextServicesDocument;
class TypeInState;
class WhiteSpaceVisibilityKeeper;

template <typename NodeType>
class CreateNodeResultBase;
typedef CreateNodeResultBase<dom::Element> CreateElementResult;

namespace dom {
class AbstractRange;
class DataTransfer;
class Document;
class DragEvent;
class Element;
class EventTarget;
class HTMLBRElement;
}  // namespace dom

namespace widget {
struct IMEState;
}  // namespace widget

/**
 * Implementation of an editor object.  it will be the controller/focal point
 * for the main editor services. i.e. the GUIManager, publishing, transaction
 * manager, event interfaces. the idea for the event interfaces is to have them
 * delegate the actual commands to the editor independent of the XPFE
 * implementation.
 */
class EditorBase : public nsIEditor,
                   public nsISelectionListener,
                   public nsSupportsWeakReference {
 public:
  /****************************************************************************
   * NOTE: DO NOT MAKE YOUR NEW METHODS PUBLIC IF they are called by other
   *       classes under libeditor except EditorEventListener and
   *       HTMLEditorEventListener because each public method which may fire
   *       eEditorInput event will need to instantiate new stack class for
   *       managing input type value of eEditorInput and cache some objects
   *       for smarter handling.  In other words, when you add new root
   *       method to edit the DOM tree, you can make your new method public.
   ****************************************************************************/

  typedef dom::Document Document;
  typedef dom::Element Element;
  typedef dom::Selection Selection;
  typedef dom::Text Text;

  enum class EditorType { Text, HTML };

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(EditorBase, nsIEditor)

  // nsIEditor methods
  NS_DECL_NSIEDITOR

  // nsISelectionListener method
  NS_DECL_NSISELECTIONLISTENER

  /**
   * The default constructor. This should suffice. the setting of the
   * interfaces is done after the construction of the editor class.
   */
  EditorBase();

  bool IsTextEditor() const { return !mIsHTMLEditorClass; }
  bool IsHTMLEditor() const { return mIsHTMLEditorClass; }

  /**
   * Init is to tell the implementation of nsIEditor to begin its services
   * @param aDoc          The dom document interface being observed
   * @param aRoot         This is the root of the editable section of this
   *                      document. If it is null then we get root
   *                      from document body.
   * @param aSelCon       this should be used to get the selection location
   *                      (will be null for HTML editors)
   * @param aFlags        A bitmask of flags for specifying the behavior
   *                      of the editor.
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult Init(Document& doc, Element* aRoot,
                                           nsISelectionController* aSelCon,
                                           uint32_t aFlags,
                                           const nsAString& aInitialValue);

  /**
   * PostCreate should be called after Init, and is the time that the editor
   * tells its documentStateObservers that the document has been created.
   */
  MOZ_CAN_RUN_SCRIPT nsresult PostCreate();

  /**
   * PreDestroy is called before the editor goes away, and gives the editor a
   * chance to tell its documentStateObservers that the document is going away.
   * @param aDestroyingFrames set to true when the frames being edited
   * are being destroyed (so there is no need to modify any nsISelections,
   * nor is it safe to do so)
   */
  MOZ_CAN_RUN_SCRIPT virtual void PreDestroy(bool aDestroyingFrames);

  bool IsInitialized() const { return !!mDocument; }
  bool Destroyed() const { return mDidPreDestroy; }

  Document* GetDocument() const { return mDocument; }
  nsPIDOMWindowOuter* GetWindow() const;
  nsPIDOMWindowInner* GetInnerWindow() const;

  /**
   * MayHaveMutationEventListeners() returns true when the window may have
   * mutation event listeners.
   *
   * @param aMutationEventType  One or multiple of NS_EVENT_BITS_MUTATION_*.
   * @return                    true if the editor is an HTMLEditor instance,
   *                            and at least one of NS_EVENT_BITS_MUTATION_* is
   *                            set to the window or in debug build.
   */
  bool MayHaveMutationEventListeners(
      uint32_t aMutationEventType = 0xFFFFFFFF) const {
    if (IsTextEditor()) {
      // DOM mutation event listeners cannot catch the changes of
      // <input type="text"> nor <textarea>.
      return false;
    }
#ifdef DEBUG
    // On debug build, this should always return true for testing complicated
    // path without mutation event listeners because when mutation event
    // listeners do not touch the DOM, editor needs to run as there is no
    // mutation event listeners.
    return true;
#else   // #ifdef DEBUG
    nsPIDOMWindowInner* window = GetInnerWindow();
    return window ? window->HasMutationListeners(aMutationEventType) : false;
#endif  // #ifdef DEBUG #else
  }

  /**
   * MayHaveBeforeInputEventListenersForTelemetry() returns true when the
   * window may have or have had one or more `beforeinput` event listeners.
   * Note that this may return false even if there is a `beforeinput`.
   * See nsPIDOMWindowInner::HasBeforeInputEventListenersForTelemetry()'s
   * comment for the detail.
   */
  bool MayHaveBeforeInputEventListenersForTelemetry() const {
    if (const nsPIDOMWindowInner* window = GetInnerWindow()) {
      return window->HasBeforeInputEventListenersForTelemetry();
    }
    return false;
  }

  /**
   * MutationObserverHasObservedNodeForTelemetry() returns true when a node in
   * the window may have been observed by the web apps with a mutation observer
   * (i.e., `MutationObserver.observe()` called by chrome script and addon's
   * script does not make this returns true).
   * Note that this may return false even if there is a node observed by
   * a MutationObserver.  See
   * nsPIDOMWindowInner::MutationObserverHasObservedNodeForTelemetry()'s comment
   * for the detail.
   */
  bool MutationObserverHasObservedNodeForTelemetry() const {
    if (const nsPIDOMWindowInner* window = GetInnerWindow()) {
      return window->MutationObserverHasObservedNodeForTelemetry();
    }
    return false;
  }

  PresShell* GetPresShell() const;
  nsPresContext* GetPresContext() const;
  already_AddRefed<nsCaret> GetCaret() const;

  already_AddRefed<nsIWidget> GetWidget();

  nsISelectionController* GetSelectionController() const;

  nsresult GetSelection(SelectionType aSelectionType,
                        Selection** aSelection) const;

  Selection* GetSelection(
      SelectionType aSelectionType = SelectionType::eNormal) const {
    if (aSelectionType == SelectionType::eNormal &&
        IsEditActionDataAvailable()) {
      return &SelectionRef();
    }
    nsISelectionController* sc = GetSelectionController();
    if (!sc) {
      return nullptr;
    }
    Selection* selection = sc->GetSelection(ToRawSelectionType(aSelectionType));
    return selection;
  }

  /**
   * Fast non-refcounting editor root element accessor
   */
  Element* GetRoot() const { return mRootElement; }

  /**
   * Likewise, but gets the text control element instead of the root for
   * plaintext editors.
   */
  Element* GetExposedRoot() const;

  /**
   * Set or unset TextInputListener.  If setting non-nullptr when the editor
   * already has a TextInputListener, this will crash in debug build.
   */
  void SetTextInputListener(TextInputListener* aTextInputListener);

  /**
   * Set or unset IMEContentObserver.  If setting non-nullptr when the editor
   * already has an IMEContentObserver, this will crash in debug build.
   */
  void SetIMEContentObserver(IMEContentObserver* aIMEContentObserver);

  /**
   * Returns current composition.
   */
  TextComposition* GetComposition() const;

  /**
   * Get preferred IME status of current widget.
   */
  virtual nsresult GetPreferredIMEState(widget::IMEState* aState);

  /**
   * Returns true if there is composition string and not fixed.
   */
  bool IsIMEComposing() const;

  /**
   * Commit composition if there is.
   * Note that when there is a composition, this requests to commit composition
   * to native IME.  Therefore, when there is composition, this can do anything.
   * For example, the editor instance, the widget or the process itself may
   * be destroyed.
   */
  nsresult CommitComposition();

  /**
   * ToggleTextDirection() toggles text-direction of the root element.
   *
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  ToggleTextDirectionAsAction(nsIPrincipal* aPrincipal = nullptr);

  /**
   * SwitchTextDirectionTo() sets the text-direction of the root element to
   * LTR or RTL.
   */
  enum class TextDirection {
    eLTR,
    eRTL,
  };
  MOZ_CAN_RUN_SCRIPT void SwitchTextDirectionTo(TextDirection aTextDirection);

  /**
   * Finalizes selection and caret for the editor.
   */
  nsresult FinalizeSelection();

  /**
   * Returns true if selection is in an editable element and both the range
   * start and the range end are editable.  E.g., even if the selection range
   * includes non-editable elements, returns true when one of common ancestors
   * of the range start and the range end is editable.  Otherwise, false.
   */
  bool IsSelectionEditable();

  /**
   * Returns number of undo or redo items.
   */
  size_t NumberOfUndoItems() const {
    return mTransactionManager ? mTransactionManager->NumberOfUndoItems() : 0;
  }
  size_t NumberOfRedoItems() const {
    return mTransactionManager ? mTransactionManager->NumberOfRedoItems() : 0;
  }

  /**
   * Returns number of maximum undo/redo transactions.
   */
  int32_t NumberOfMaximumTransactions() const {
    return mTransactionManager
               ? mTransactionManager->NumberOfMaximumTransactions()
               : 0;
  }

  /**
   * Returns true if this editor can store transactions for undo/redo.
   */
  bool IsUndoRedoEnabled() const {
    return mTransactionManager &&
           mTransactionManager->NumberOfMaximumTransactions();
  }

  /**
   * Return true if it's possible to undo/redo right now.
   */
  bool CanUndo() const {
    return IsUndoRedoEnabled() && NumberOfUndoItems() > 0;
  }
  bool CanRedo() const {
    return IsUndoRedoEnabled() && NumberOfRedoItems() > 0;
  }

  /**
   * Enables or disables undo/redo feature.  Returns true if it succeeded,
   * otherwise, e.g., we're undoing or redoing, returns false.
   */
  bool EnableUndoRedo(int32_t aMaxTransactionCount = -1) {
    if (!mTransactionManager) {
      mTransactionManager = new TransactionManager();
    }
    return mTransactionManager->EnableUndoRedo(aMaxTransactionCount);
  }
  bool DisableUndoRedo() {
    if (!mTransactionManager) {
      return true;
    }
    return mTransactionManager->DisableUndoRedo();
  }
  bool ClearUndoRedo() {
    if (!mTransactionManager) {
      return true;
    }
    return mTransactionManager->ClearUndoRedo();
  }

  /**
   * Adds or removes transaction listener to or from the transaction manager.
   * Note that TransactionManager does not check if the listener is in the
   * array.  So, caller of AddTransactionListener() needs to manage if it's
   * already been registered to the transaction manager.
   */
  bool AddTransactionListener(nsITransactionListener& aListener) {
    if (!mTransactionManager) {
      return false;
    }
    return mTransactionManager->AddTransactionListener(aListener);
  }
  bool RemoveTransactionListener(nsITransactionListener& aListener) {
    if (!mTransactionManager) {
      return false;
    }
    return mTransactionManager->RemoveTransactionListener(aListener);
  }

  MOZ_CAN_RUN_SCRIPT virtual nsresult HandleKeyPressEvent(
      WidgetKeyboardEvent* aKeyboardEvent);

  virtual dom::EventTarget* GetDOMEventTarget() const = 0;

  /**
   * Similar to the setter for wrapWidth, but just sets the editor
   * internal state without actually changing the content being edited
   * to wrap at that column.  This should only be used by callers who
   * are sure that their content is already set up correctly.
   */
  void SetWrapColumn(int32_t aWrapColumn) { mWrapColumn = aWrapColumn; }

  /**
   * Accessor methods to flags.
   */
  uint32_t Flags() const { return mFlags; }

  MOZ_CAN_RUN_SCRIPT nsresult AddFlags(uint32_t aFlags) {
    const uint32_t kOldFlags = Flags();
    const uint32_t kNewFlags = (kOldFlags | aFlags);
    if (kNewFlags == kOldFlags) {
      return NS_OK;
    }
    return SetFlags(kNewFlags);  // virtual call and may be expensive.
  }
  MOZ_CAN_RUN_SCRIPT nsresult RemoveFlags(uint32_t aFlags) {
    const uint32_t kOldFlags = Flags();
    const uint32_t kNewFlags = (kOldFlags & ~aFlags);
    if (kNewFlags == kOldFlags) {
      return NS_OK;
    }
    return SetFlags(kNewFlags);  // virtual call and may be expensive.
  }
  MOZ_CAN_RUN_SCRIPT nsresult AddAndRemoveFlags(uint32_t aAddingFlags,
                                                uint32_t aRemovingFlags) {
    MOZ_ASSERT(!(aAddingFlags & aRemovingFlags),
               "Same flags are specified both adding and removing");
    const uint32_t kOldFlags = Flags();
    const uint32_t kNewFlags = ((kOldFlags | aAddingFlags) & ~aRemovingFlags);
    if (kNewFlags == kOldFlags) {
      return NS_OK;
    }
    return SetFlags(kNewFlags);  // virtual call and may be expensive.
  }

  bool IsPlaintextEditor() const {
    return (mFlags & nsIEditor::eEditorPlaintextMask) != 0;
  }

  bool IsSingleLineEditor() const {
    return (mFlags & nsIEditor::eEditorSingleLineMask) != 0;
  }

  bool IsPasswordEditor() const {
    return (mFlags & nsIEditor::eEditorPasswordMask) != 0;
  }

  // FYI: Both IsRightToLeft() and IsLeftToRight() may return false if
  //      the editor inherits the content node's direction.
  bool IsRightToLeft() const {
    return (mFlags & nsIEditor::eEditorRightToLeft) != 0;
  }
  bool IsLeftToRight() const {
    return (mFlags & nsIEditor::eEditorLeftToRight) != 0;
  }

  bool IsReadonly() const {
    return (mFlags & nsIEditor::eEditorReadonlyMask) != 0;
  }

  bool IsInputFiltered() const {
    return (mFlags & nsIEditor::eEditorFilterInputMask) != 0;
  }

  bool IsMailEditor() const {
    return (mFlags & nsIEditor::eEditorMailMask) != 0;
  }

  bool IsWrapHackEnabled() const {
    return (mFlags & nsIEditor::eEditorEnableWrapHackMask) != 0;
  }

  bool IsFormWidget() const {
    return (mFlags & nsIEditor::eEditorWidgetMask) != 0;
  }

  bool NoCSS() const { return (mFlags & nsIEditor::eEditorNoCSSMask) != 0; }

  bool IsInteractionAllowed() const {
    return (mFlags & nsIEditor::eEditorAllowInteraction) != 0;
  }

  bool ShouldSkipSpellCheck() const {
    return (mFlags & nsIEditor::eEditorSkipSpellCheck) != 0;
  }

  bool IsTabbable() const {
    return IsSingleLineEditor() || IsPasswordEditor() || IsFormWidget() ||
           IsInteractionAllowed();
  }

  bool HasIndependentSelection() const { return !!mSelectionController; }

  bool IsModifiable() const { return !IsReadonly(); }

  /**
   * IsInEditSubAction() return true while the instance is handling an edit
   * sub-action.  Otherwise, false.
   */
  bool IsInEditSubAction() const { return mIsInEditSubAction; }

  /**
   * SuppressDispatchingInputEvent() suppresses or unsuppresses dispatching
   * "input" event.
   */
  void SuppressDispatchingInputEvent(bool aSuppress) {
    mDispatchInputEvent = !aSuppress;
  }

  /**
   * IsSuppressingDispatchingInputEvent() returns true if the editor stops
   * dispatching input event.  Otherwise, false.
   */
  bool IsSuppressingDispatchingInputEvent() const {
    return !mDispatchInputEvent;
  }

  /**
   * Returns true if markNodeDirty() has any effect.  Returns false if
   * markNodeDirty() is a no-op.
   */
  bool OutputsMozDirty() const {
    // Return true for Composer (!IsInteractionAllowed()) or mail
    // (IsMailEditor()), but false for webpages.
    return !IsInteractionAllowed() || IsMailEditor();
  }

  /**
   * Get the focused content, if we're focused.  Returns null otherwise.
   */
  virtual nsIContent* GetFocusedContent() const;

  /**
   * Get the focused content for the argument of some IMEStateManager's
   * methods.
   */
  virtual nsIContent* GetFocusedContentForIME() const;

  /**
   * Whether the aGUIEvent should be handled by this editor or not.  When this
   * returns false, The aGUIEvent shouldn't be handled on this editor,
   * i.e., The aGUIEvent should be handled by another inner editor or ancestor
   * elements.
   */
  virtual bool IsAcceptableInputEvent(WidgetGUIEvent* aGUIEvent) const;

  /**
   * FindSelectionRoot() returns a selection root of this editor when aNode
   * gets focus.  aNode must be a content node or a document node.  When the
   * target isn't a part of this editor, returns nullptr.  If this is for
   * designMode, you should set the document node to aNode except that an
   * element in the document has focus.
   */
  virtual Element* FindSelectionRoot(nsINode* aNode) const;

  /**
   * This method has to be called by EditorEventListener::Focus.
   * All actions that have to be done when the editor is focused needs to be
   * added here.
   */
  MOZ_CAN_RUN_SCRIPT void OnFocus(nsINode& aFocusEventTargetNode);

  /** Resyncs spellchecking state (enabled/disabled).  This should be called
   * when anything that affects spellchecking state changes, such as the
   * spellcheck attribute value.
   */
  void SyncRealTimeSpell();

  /**
   * This method re-initializes the selection and caret state that are for
   * current editor state. When editor session is destroyed, it always reset
   * selection state even if this has no focus.  So if destroying editor,
   * we have to call this method for focused editor to set selection state.
   */
  MOZ_CAN_RUN_SCRIPT void ReinitializeSelection(Element& aElement);

  /**
   * InsertTextAsAction() inserts aStringToInsert at selection.
   * Although this method is implementation of nsIEditor.insertText(),
   * this treats the input is an edit action.  If you'd like to insert text
   * as part of edit action, you probably should use InsertTextAsSubAction().
   *
   * @param aStringToInsert     The string to insert.
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  MOZ_CAN_RUN_SCRIPT nsresult InsertTextAsAction(
      const nsAString& aStringToInsert, nsIPrincipal* aPrincipal = nullptr);

  /**
   * DeleteSelectionAsAction() removes selection content or content around
   * caret with transactions.  This should be used for handling it as an
   * edit action.  If you'd like to remove selection for preparing to insert
   * something, you probably should use DeleteSelectionAsSubAction().
   *
   * @param aDirectionAndAmount How much range should be removed.
   * @param aStripWrappers      Whether the parent blocks should be removed
   *                            when they become empty.
   * @param aPrincipal          Set subject principal if it may be called by
   *                            JS.  If set to nullptr, will be treated as
   *                            called by system.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  DeleteSelectionAsAction(nsIEditor::EDirection aDirectionAndAmount,
                          nsIEditor::EStripWrappers aStripWrappers,
                          nsIPrincipal* aPrincipal = nullptr);

 protected:  // May be used by friends.
  class AutoEditActionDataSetter;

  /**
   * TopLevelEditSubActionData stores temporary data while we're handling
   * top-level edit sub-action.
   */
  struct MOZ_STACK_CLASS TopLevelEditSubActionData final {
    friend class AutoEditActionDataSetter;

    // If we have created a new block element, set to it.
    RefPtr<Element> mNewBlockElement;

    // Set selected range before edit.  Then, RangeUpdater keep modifying
    // the range while we're changing the DOM tree.
    RefPtr<RangeItem> mSelectedRange;

    // Computing changed range while we're handling sub actions.
    RefPtr<nsRange> mChangedRange;

    // XXX In strict speaking, mCachedInlineStyles isn't enough to cache inline
    //     styles because inline style can be specified with "style" attribute
    //     and/or CSS in <style> elements or CSS files.  So, we need to look
    //     for better implementation about this.
    // FYI: Initialization cost of AutoStyleCacheArray is expensive and it is
    //      not used by TextEditor so that we should construct it only when
    //      we're an HTMLEditor.
    Maybe<AutoStyleCacheArray> mCachedInlineStyles;

    // If we tried to delete selection, set to true.
    bool mDidDeleteSelection;

    // If we have explicitly set selection inter line, set to true.
    // `AfterEdit()` or something shouldn't overwrite it in such case.
    bool mDidExplicitlySetInterLine;

    // If we have deleted non-collapsed range set to true, there are only 2
    // cases for now:
    //   - non-collapsed range was selected.
    //   - selection was collapsed in a text node and a Unicode character
    //     was removed.
    bool mDidDeleteNonCollapsedRange;

    // If we have deleted parent empty blocks, set to true.
    bool mDidDeleteEmptyParentBlocks;

    // If we're a contenteditable editor, we temporarily increase edit count
    // of the document between `BeforeEdit()` and `AfterEdit()`.  I.e., if
    // we increased the count in `BeforeEdit()`, we need to decrease it in
    // `AfterEdit()`, however, the document may be changed to designMode or
    // non-editable.  Therefore, we need to store with this whether we need
    // to restore it.
    bool mRestoreContentEditableCount;

    // If we explicitly normalized whitespaces around the changed range,
    // set to true.
    bool mDidNormalizeWhitespaces;

    /**
     * The following methods modifies some data of this struct and
     * `EditSubActionData` struct.  Currently, these are required only
     * by `HTMLEditor`.  Therefore, for cutting the runtime cost of
     * `TextEditor`, these methods should be called only by `HTMLEditor`.
     * But it's fine to use these methods in `TextEditor` if necessary.
     * If so, you need to call `DidDeleteText()` and `DidInsertText()`
     * from `SetTextNodeWithoutTransaction()`.
     */
    void DidCreateElement(EditorBase& aEditorBase, Element& aNewElement);
    void DidInsertContent(EditorBase& aEditorBase, nsIContent& aNewContent);
    void WillDeleteContent(EditorBase& aEditorBase,
                           nsIContent& aRemovingContent);
    void DidSplitContent(EditorBase& aEditorBase,
                         nsIContent& aExistingRightContent,
                         nsIContent& aNewLeftContent);
    void WillJoinContents(EditorBase& aEditorBase, nsIContent& aLeftContent,
                          nsIContent& aRightContent);
    void DidJoinContents(EditorBase& aEditorBase, nsIContent& aLeftContent,
                         nsIContent& aRightContent);
    void DidInsertText(EditorBase& aEditorBase,
                       const EditorRawDOMPoint& aInsertionBegin,
                       const EditorRawDOMPoint& aInsertionEnd);
    void DidDeleteText(EditorBase& aEditorBase,
                       const EditorRawDOMPoint& aStartInTextNode);
    void WillDeleteRange(EditorBase& aEditorBase,
                         const EditorRawDOMPoint& aStart,
                         const EditorRawDOMPoint& aEnd);

   private:
    void Clear() {
      mDidExplicitlySetInterLine = false;
      // We don't need to clear other members which are referred only when the
      // editor is an HTML editor anymore.  Note that if `mSelectedRange` is
      // non-nullptr, that means that we're in `HTMLEditor`.
      if (!mSelectedRange) {
        return;
      }
      mNewBlockElement = nullptr;
      mSelectedRange->Clear();
      mChangedRange->Reset();
      if (mCachedInlineStyles.isSome()) {
        mCachedInlineStyles->Clear();
      }
      mDidDeleteSelection = false;
      mDidDeleteNonCollapsedRange = false;
      mDidDeleteEmptyParentBlocks = false;
      mRestoreContentEditableCount = false;
      mDidNormalizeWhitespaces = false;
    }

    /**
     * Extend mChangedRange to include `aNode`.
     */
    nsresult AddNodeToChangedRange(const HTMLEditor& aHTMLEditor,
                                   nsINode& aNode);

    /**
     * Extend mChangedRange to include `aPoint`.
     */
    nsresult AddPointToChangedRange(const HTMLEditor& aHTMLEditor,
                                    const EditorRawDOMPoint& aPoint);

    /**
     * Extend mChangedRange to include `aStart` and `aEnd`.
     */
    nsresult AddRangeToChangedRange(const HTMLEditor& aHTMLEditor,
                                    const EditorRawDOMPoint& aStart,
                                    const EditorRawDOMPoint& aEnd);

    TopLevelEditSubActionData() = default;
    TopLevelEditSubActionData(const TopLevelEditSubActionData& aOther) = delete;
  };

  struct MOZ_STACK_CLASS EditSubActionData final {
    uint32_t mJoinedLeftNodeLength;

    // While this is set to false, TopLevelEditSubActionData::mChangedRange
    // shouldn't be modified since in some cases, modifying it in the setter
    // itself may be faster.  Note that we should affect this only for current
    // edit sub action since mutation event listener may edit different range.
    bool mAdjustChangedRangeFromListener;

   private:
    void Clear() {
      mJoinedLeftNodeLength = 0;
      mAdjustChangedRangeFromListener = true;
    }

    friend EditorBase;
  };

 protected:  // AutoEditActionDataSetter, this shouldn't be accessed by friends.
  /**
   * SettingDataTransfer enum class is used to specify whether DataTransfer
   * should be initialized with or without format.  For example, when user
   * uses Accel + Shift + V to paste text without format, DataTransfer should
   * have only plain/text data to make web apps treat it without format.
   */
  enum class SettingDataTransfer {
    eWithFormat,
    eWithoutFormat,
  };

  /**
   * AutoEditActionDataSetter grabs some necessary objects for handling any
   * edit actions and store the edit action what we're handling.  When this is
   * created, its pointer is set to the mEditActionData, and this guarantees
   * the lifetime of grabbing objects until it's destroyed.
   */
  class MOZ_STACK_CLASS AutoEditActionDataSetter final {
   public:
    // NOTE: aPrincipal will be used when we implement "beforeinput" event.
    //       It's set only when maybe we shouldn't dispatch it because of
    //       called by JS.  I.e., if this is nullptr, we can always dispatch
    //       it.
    AutoEditActionDataSetter(const EditorBase& aEditorBase,
                             EditAction aEditAction,
                             nsIPrincipal* aPrincipal = nullptr);
    ~AutoEditActionDataSetter();

    void UpdateEditAction(EditAction aEditAction) {
      MOZ_ASSERT(!mHasTriedToDispatchBeforeInputEvent,
                 "It's too late to update EditAction since this may have "
                 "already dispatched a beforeinput event");
      mEditAction = aEditAction;
    }

    /**
     * CanHandle() or CanHandleAndHandleBeforeInput() must be called
     * immediately after creating the instance.  If caller does not need to
     * handle "beforeinput" event or caller needs to set additional information
     * the events later, use the former.  Otherwise, use the latter.  If caller
     * uses the former, it's required to call MaybeDispatchBeforeInputEvent() by
     * itself.
     *
     */
    [[nodiscard]] bool CanHandle() const {
#ifdef DEBUG
      mHasCanHandleChecked = true;
#endif  // #ifdefn DEBUG
      return mSelection && mEditorBase.IsInitialized();
    }
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
    CanHandleAndMaybeDispatchBeforeInputEvent() {
      if (NS_WARN_IF(!CanHandle())) {
        return NS_ERROR_NOT_INITIALIZED;
      }
      return MaybeDispatchBeforeInputEvent();
    }

    /**
     * MaybeDispatchBeforeInputEvent() considers whether this instance needs to
     * dispatch "beforeinput" event or not.  Then,
     * mHasTriedToDispatchBeforeInputEvent is set to true.
     *
     * @param aDeleteDirectionAndAmount
     *                  If `MayEditActionDeleteAroundCollapsedSelection(
     *                  mEditAction)` returns true, this must be set.
     *                  Otherwise, don't set explicitly.
     * @return          If this method actually dispatches "beforeinput" event
     *                  and it's canceled, returns
     *                  NS_ERROR_EDITOR_ACTION_CANCELED.
     */
    [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult MaybeDispatchBeforeInputEvent(
        nsIEditor::EDirection aDeleteDirectionAndAmount = nsIEditor::eNone);

    /**
     * MarkAsBeforeInputHasBeenDispatched() should be called only when updating
     * the DOM occurs asynchronously from user input (e.g., inserting blob
     * object which is loaded asynchronously) and `beforeinput` has already
     * been dispatched (always should be so).
     */
    void MarkAsBeforeInputHasBeenDispatched() {
      MOZ_ASSERT(!HasTriedToDispatchBeforeInputEvent());
      MOZ_ASSERT(mEditAction == EditAction::ePaste ||
                 mEditAction == EditAction::ePasteAsQuotation ||
                 mEditAction == EditAction::eDrop);
      mHasTriedToDispatchBeforeInputEvent = true;
    }

    /**
     * ShouldAlreadyHaveHandledBeforeInputEventDispatching() returns true if the
     * edit action requires to handle "beforeinput" event but not yet dispatched
     * it nor considered as not dispatched it and can dispatch it when this is
     * called.
     */
    bool ShouldAlreadyHaveHandledBeforeInputEventDispatching() const {
      return !HasTriedToDispatchBeforeInputEvent() &&
             NeedsBeforeInputEventHandling(mEditAction) &&
             IsBeforeInputEventEnabled() /* &&
              // If we still need to dispatch a clipboard event, we should
              // dispatch it first, then, we need to dispatch beforeinput
              // event later.
              !NeedsToDispatchClipboardEvent()*/
          ;
    }

    /**
     * HasTriedToDispatchBeforeInputEvent() returns true if the instance's
     * MaybeDispatchBeforeInputEvent() has already been called.
     */
    bool HasTriedToDispatchBeforeInputEvent() const {
      return mHasTriedToDispatchBeforeInputEvent;
    }

    bool IsCanceled() const { return mBeforeInputEventCanceled; }

    /**
     * Returns a `Selection` for normal selection.  The lifetime is guaranteed
     * during alive this instance in the stack.
     */
    MOZ_KNOWN_LIVE Selection& SelectionRef() const {
      MOZ_ASSERT(!mSelection ||
                 (mSelection->GetType() == SelectionType::eNormal));
      return *mSelection;
    }

    nsIPrincipal* GetPrincipal() const { return mPrincipal; }
    EditAction GetEditAction() const { return mEditAction; }

    template <typename PT, typename CT>
    void SetSpellCheckRestartPoint(const EditorDOMPointBase<PT, CT>& aPoint) {
      MOZ_ASSERT(aPoint.IsSet());
      // We should store only container and offset because new content may
      // be inserted before referring child.
      // XXX Shouldn't we compare whether aPoint is before
      //     mSpellCheckRestartPoint if it's set.
      mSpellCheckRestartPoint =
          EditorDOMPoint(aPoint.GetContainer(), aPoint.Offset());
    }
    void ClearSpellCheckRestartPoint() { mSpellCheckRestartPoint.Clear(); }
    const EditorDOMPoint& GetSpellCheckRestartPoint() const {
      return mSpellCheckRestartPoint;
    }

    void SetData(const nsAString& aData) {
      MOZ_ASSERT(!mHasTriedToDispatchBeforeInputEvent,
                 "It's too late to set data since this may have already "
                 "dispatched a beforeinput event");
      mData = aData;
    }
    const nsString& GetData() const { return mData; }

    void SetColorData(const nsAString& aData);

    /**
     * InitializeDataTransfer(DataTransfer*) sets mDataTransfer to
     * aDataTransfer.  In this case, aDataTransfer should not be read/write
     * because it'll be set to InputEvent.dataTransfer and which should be
     * read-only.
     */
    void InitializeDataTransfer(dom::DataTransfer* aDataTransfer);
    /**
     * InitializeDataTransfer(nsITransferable*) creates new DataTransfer
     * instance, initializes it with aTransferable and sets mDataTransfer to
     * it.
     */
    void InitializeDataTransfer(nsITransferable* aTransferable);
    /**
     * InitializeDataTransfer(const nsAString&) creates new DataTransfer
     * instance, initializes it with aString and sets mDataTransfer to it.
     */
    void InitializeDataTransfer(const nsAString& aString);
    /**
     * InitializeDataTransferWithClipboard() creates new DataTransfer instance,
     * initializes it with clipboard and sets mDataTransfer to it.
     */
    void InitializeDataTransferWithClipboard(
        SettingDataTransfer aSettingDataTransfer, int32_t aClipboardType);
    dom::DataTransfer* GetDataTransfer() const { return mDataTransfer; }

    /**
     * AppendTargetRange() appends aTargetRange to target ranges.  This should
     * be used only by edit action handlers which do not want to set target
     * ranges to selection ranges.
     */
    void AppendTargetRange(dom::StaticRange& aTargetRange);

    /**
     * Make dispatching `beforeinput` forcibly non-cancelable.
     */
    void MakeBeforeInputEventNonCancelable() {
      mMakeBeforeInputEventNonCancelable = true;
    }

    /**
     * NotifyOfDispatchingClipboardEvent() is called after dispatching
     * a clipboard event.
     */
    void NotifyOfDispatchingClipboardEvent() {
      MOZ_ASSERT(NeedsToDispatchClipboardEvent());
      MOZ_ASSERT(!mHasTriedToDispatchClipboardEvent);
      mHasTriedToDispatchClipboardEvent = true;
    }

    void Abort() { mAborted = true; }
    bool IsAborted() const { return mAborted; }

    void SetTopLevelEditSubAction(EditSubAction aEditSubAction,
                                  EDirection aDirection = eNone) {
      mTopLevelEditSubAction = aEditSubAction;
      TopLevelEditSubActionDataRef().Clear();
      switch (mTopLevelEditSubAction) {
        case EditSubAction::eInsertNode:
        case EditSubAction::eCreateNode:
        case EditSubAction::eSplitNode:
        case EditSubAction::eInsertText:
        case EditSubAction::eInsertTextComingFromIME:
        case EditSubAction::eSetTextProperty:
        case EditSubAction::eRemoveTextProperty:
        case EditSubAction::eRemoveAllTextProperties:
        case EditSubAction::eSetText:
        case EditSubAction::eInsertLineBreak:
        case EditSubAction::eInsertParagraphSeparator:
        case EditSubAction::eCreateOrChangeList:
        case EditSubAction::eIndent:
        case EditSubAction::eOutdent:
        case EditSubAction::eSetOrClearAlignment:
        case EditSubAction::eCreateOrRemoveBlock:
        case EditSubAction::eMergeBlockContents:
        case EditSubAction::eRemoveList:
        case EditSubAction::eCreateOrChangeDefinitionListItem:
        case EditSubAction::eInsertElement:
        case EditSubAction::eInsertQuotation:
        case EditSubAction::eInsertQuotedText:
        case EditSubAction::ePasteHTMLContent:
        case EditSubAction::eInsertHTMLSource:
        case EditSubAction::eSetPositionToAbsolute:
        case EditSubAction::eSetPositionToStatic:
        case EditSubAction::eDecreaseZIndex:
        case EditSubAction::eIncreaseZIndex:
          MOZ_ASSERT(aDirection == eNext);
          mDirectionOfTopLevelEditSubAction = eNext;
          break;
        case EditSubAction::eJoinNodes:
        case EditSubAction::eDeleteText:
          MOZ_ASSERT(aDirection == ePrevious);
          mDirectionOfTopLevelEditSubAction = ePrevious;
          break;
        case EditSubAction::eUndo:
        case EditSubAction::eRedo:
        case EditSubAction::eComputeTextToOutput:
        case EditSubAction::eCreatePaddingBRElementForEmptyEditor:
        case EditSubAction::eNone:
          MOZ_ASSERT(aDirection == eNone);
          mDirectionOfTopLevelEditSubAction = eNone;
          break;
        case EditSubAction::eReplaceHeadWithHTMLSource:
          // NOTE: Not used with AutoTopLevelEditSubActionNotifier.
          mDirectionOfTopLevelEditSubAction = eNone;
          break;
        case EditSubAction::eDeleteNode:
        case EditSubAction::eDeleteSelectedContent:
          // Unfortunately, eDeleteNode and eDeleteSelectedContent is used with
          // any direction.  We might have specific sub-action for each
          // direction, but there are some points referencing
          // eDeleteSelectedContent so that we should keep storing direction
          // as-is for now.
          mDirectionOfTopLevelEditSubAction = aDirection;
          break;
      }
    }
    EditSubAction GetTopLevelEditSubAction() const {
      MOZ_ASSERT(CanHandle());
      return mTopLevelEditSubAction;
    }
    EDirection GetDirectionOfTopLevelEditSubAction() const {
      return mDirectionOfTopLevelEditSubAction;
    }

    const TopLevelEditSubActionData& TopLevelEditSubActionDataRef() const {
      return mParentData ? mParentData->TopLevelEditSubActionDataRef()
                         : mTopLevelEditSubActionData;
    }
    TopLevelEditSubActionData& TopLevelEditSubActionDataRef() {
      return mParentData ? mParentData->TopLevelEditSubActionDataRef()
                         : mTopLevelEditSubActionData;
    }

    const EditSubActionData& EditSubActionDataRef() const {
      return mEditSubActionData;
    }
    EditSubActionData& EditSubActionDataRef() { return mEditSubActionData; }

    SelectionState& SavedSelectionRef() {
      return mParentData ? mParentData->SavedSelectionRef() : mSavedSelection;
    }
    const SelectionState& SavedSelectionRef() const {
      return mParentData ? mParentData->SavedSelectionRef() : mSavedSelection;
    }

    RangeUpdater& RangeUpdaterRef() {
      return mParentData ? mParentData->RangeUpdaterRef() : mRangeUpdater;
    }
    const RangeUpdater& RangeUpdaterRef() const {
      return mParentData ? mParentData->RangeUpdaterRef() : mRangeUpdater;
    }

    void UpdateSelectionCache(Selection& aSelection) {
      MOZ_ASSERT(aSelection.GetType() == SelectionType::eNormal);

      AutoEditActionDataSetter* actionData = this;
      while (actionData) {
        if (actionData->mSelection) {
          actionData->mSelection = &aSelection;
        }
        actionData = actionData->mParentData;
      }
    }

   private:
    bool IsBeforeInputEventEnabled() const;

    static bool NeedsBeforeInputEventHandling(EditAction aEditAction) {
      MOZ_ASSERT(aEditAction != EditAction::eNone);
      switch (aEditAction) {
        case EditAction::eNone:
        // If we're not handling edit action, we don't need to handle
        // "beforeinput" event.
        case EditAction::eNotEditing:
        // If raw level transaction API is used, the API user needs to handle
        // both "beforeinput" event and "input" event if it's necessary.
        case EditAction::eUnknown:
        // Hiding/showing password affects only layout so that we don't need
        // to handle beforeinput event for it.
        case EditAction::eHidePassword:
        // We don't need to dispatch "beforeinput" event before
        // "compositionstart".
        case EditAction::eStartComposition:
        // We don't need to let web apps know the mode change.
        case EditAction::eEnableOrDisableCSS:
        case EditAction::eEnableOrDisableAbsolutePositionEditor:
        case EditAction::eEnableOrDisableResizer:
        case EditAction::eEnableOrDisableInlineTableEditingUI:
        // We don't need to let contents in chrome's editor to know the size
        // change.
        case EditAction::eSetWrapWidth:
        // While resizing or moving element, we update only shadow, i.e.,
        // don't touch to the DOM in content.  Therefore, we don't need to
        // dispatch "beforeinput" event.
        case EditAction::eResizingElement:
        case EditAction::eMovingElement:
        // Perhaps, we don't need to dispatch "beforeinput" event for
        // padding `<br>` element for empty editor because it's internal
        // handling and it should be occurred by another change.
        case EditAction::eCreatePaddingBRElementForEmptyEditor:
          return false;
        default:
          return true;
      }
    }

    bool NeedsToDispatchClipboardEvent() const {
      if (mHasTriedToDispatchClipboardEvent) {
        return false;
      }
      switch (mEditAction) {
        case EditAction::ePaste:
        case EditAction::ePasteAsQuotation:
        case EditAction::eCut:
        case EditAction::eCopy:
          return true;
        default:
          return false;
      }
    }

    EditorBase& mEditorBase;
    RefPtr<Selection> mSelection;
    nsCOMPtr<nsIPrincipal> mPrincipal;
    // EditAction may be nested, for example, a command may be executed
    // from mutation event listener which is run while editor changes
    // the DOM tree.  In such case, we need to handle edit action separately.
    AutoEditActionDataSetter* mParentData;

    // Cached selection for AutoSelectionRestorer.
    SelectionState mSavedSelection;

    // Utility class object for maintaining preserved ranges.
    RangeUpdater mRangeUpdater;

    // The data should be set to InputEvent.data.
    nsString mData;

    // The dataTransfer should be set to InputEvent.dataTransfer.
    RefPtr<dom::DataTransfer> mDataTransfer;

    // They are used for result of InputEvent.getTargetRanges() of beforeinput.
    OwningNonNullStaticRangeArray mTargetRanges;

    // Start point where spell checker should check from.  This is used only
    // by TextEditor.
    EditorDOMPoint mSpellCheckRestartPoint;

    // Different from mTopLevelEditSubAction, its data should be stored only
    // in the most ancestor AutoEditActionDataSetter instance since we don't
    // want to pay the copying cost and sync cost.
    TopLevelEditSubActionData mTopLevelEditSubActionData;

    // Different from mTopLevelEditSubActionData, this stores temporaly data
    // for current edit sub action.
    EditSubActionData mEditSubActionData;

    EditAction mEditAction;

    // Different from its data, you can refer "current" AutoEditActionDataSetter
    // instance's mTopLevelEditSubAction member since it's copied from the
    // parent instance at construction and it's always cleared before this
    // won't be overwritten and cleared before destruction.
    EditSubAction mTopLevelEditSubAction;

    EDirection mDirectionOfTopLevelEditSubAction;

    bool mAborted;

    // Set to true when this handles "beforeinput" event dispatching.  Note
    // that even if "beforeinput" event shouldn't be dispatched for this,
    // instance, this is set to true when it's considered.
    bool mHasTriedToDispatchBeforeInputEvent;
    // Set to true if "beforeinput" event was dispatched and it's canceled.
    bool mBeforeInputEventCanceled;
    // Set to true if `beforeinput` event must not be cancelable even if
    // its inputType is defined as cancelable by the standards.
    bool mMakeBeforeInputEventNonCancelable;
    // Set to true when the edit action handler tries to dispatch a clipboard
    // event.
    bool mHasTriedToDispatchClipboardEvent;

#ifdef DEBUG
    mutable bool mHasCanHandleChecked = false;
#endif  // #ifdef DEBUG

    AutoEditActionDataSetter() = delete;
    AutoEditActionDataSetter(const AutoEditActionDataSetter& aOther) = delete;
  };

  void UpdateEditActionData(const nsAString& aData) {
    mEditActionData->SetData(aData);
  }

  void NotifyOfDispatchingClipboardEvent() {
    MOZ_ASSERT(mEditActionData);
    mEditActionData->NotifyOfDispatchingClipboardEvent();
  }

 protected:  // May be called by friends.
  /****************************************************************************
   * Some friend classes are allowed to call the following protected methods.
   * However, those methods won't prepare caches of some objects which are
   * necessary for them.  So, if you call them from friend classes, you need
   * to make sure that AutoEditActionDataSetter is created.
   ****************************************************************************/

  bool IsEditActionCanceled() const {
    MOZ_ASSERT(mEditActionData);
    return mEditActionData->IsCanceled();
  }

  bool ShouldAlreadyHaveHandledBeforeInputEventDispatching() const {
    MOZ_ASSERT(mEditActionData);
    return mEditActionData
        ->ShouldAlreadyHaveHandledBeforeInputEventDispatching();
  }

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult MaybeDispatchBeforeInputEvent() {
    MOZ_ASSERT(mEditActionData);
    return mEditActionData->MaybeDispatchBeforeInputEvent();
  }

  void MarkAsBeforeInputHasBeenDispatched() {
    MOZ_ASSERT(mEditActionData);
    return mEditActionData->MarkAsBeforeInputHasBeenDispatched();
  }

  bool HasTriedToDispatchBeforeInputEvent() const {
    return mEditActionData &&
           mEditActionData->HasTriedToDispatchBeforeInputEvent();
  }

  bool IsEditActionDataAvailable() const {
    return mEditActionData && mEditActionData->CanHandle();
  }

  bool IsTopLevelEditSubActionDataAvailable() const {
    return mEditActionData && !!GetTopLevelEditSubAction();
  }

  bool IsEditActionAborted() const {
    MOZ_ASSERT(mEditActionData);
    return mEditActionData->IsAborted();
  }

  /**
   * SelectionRef() returns cached normal Selection.  This is pretty faster than
   * EditorBase::GetSelection() if available.
   * Note that this never crash unless public methods ignore the result of
   * AutoEditActionDataSetter::CanHandle() and keep handling edit action but any
   * methods should stop handling edit action if it returns false.
   */
  MOZ_KNOWN_LIVE Selection& SelectionRef() const {
    MOZ_ASSERT(mEditActionData);
    MOZ_ASSERT(mEditActionData->SelectionRef().GetType() ==
               SelectionType::eNormal);
    return mEditActionData->SelectionRef();
  }

  nsIPrincipal* GetEditActionPrincipal() const {
    MOZ_ASSERT(mEditActionData);
    return mEditActionData->GetPrincipal();
  }

  /**
   * GetEditAction() returns EditAction which is being handled.  If some
   * edit actions are nested, this returns the innermost edit action.
   */
  EditAction GetEditAction() const {
    return mEditActionData ? mEditActionData->GetEditAction()
                           : EditAction::eNone;
  }

  /**
   * GetInputEventData() returns inserting or inserted text value with
   * current edit action.  The result is proper for InputEvent.data value.
   */
  const nsString& GetInputEventData() const {
    return mEditActionData ? mEditActionData->GetData() : VoidString();
  }

  /**
   * GetInputEventDataTransfer() returns inserting or inserted transferable
   * content with current edit action.  The result is proper for
   * InputEvent.dataTransfer value.
   */
  dom::DataTransfer* GetInputEventDataTransfer() const {
    return mEditActionData ? mEditActionData->GetDataTransfer() : nullptr;
  }

  /**
   * GetTopLevelEditSubAction() returns the top level edit sub-action.
   * For example, if selected content is being replaced with inserted text,
   * while removing selected content, the top level edit sub-action may be
   * EditSubAction::eDeleteSelectedContent.  However, while inserting new
   * text, the top level edit sub-action may be EditSubAction::eInsertText.
   * So, this result means what we are doing right now unless you're looking
   * for a case which the method is called via mutation event listener or
   * selectionchange event listener which are fired while handling the edit
   * sub-action.
   */
  EditSubAction GetTopLevelEditSubAction() const {
    return mEditActionData ? mEditActionData->GetTopLevelEditSubAction()
                           : EditSubAction::eNone;
  }

  /**
   * GetDirectionOfTopLevelEditSubAction() returns direction which user
   * intended for doing the edit sub-action.
   */
  EDirection GetDirectionOfTopLevelEditSubAction() const {
    return mEditActionData
               ? mEditActionData->GetDirectionOfTopLevelEditSubAction()
               : eNone;
  }

  /**
   * SavedSelection() returns reference to saved selection which are
   * stored by AutoSelectionRestorer.
   */
  SelectionState& SavedSelectionRef() {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->SavedSelectionRef();
  }
  const SelectionState& SavedSelectionRef() const {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->SavedSelectionRef();
  }

  RangeUpdater& RangeUpdaterRef() {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->RangeUpdaterRef();
  }
  const RangeUpdater& RangeUpdaterRef() const {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->RangeUpdaterRef();
  }

  template <typename PT, typename CT>
  void SetSpellCheckRestartPoint(const EditorDOMPointBase<PT, CT>& aPoint) {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->SetSpellCheckRestartPoint(aPoint);
  }

  void ClearSpellCheckRestartPoint() {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->ClearSpellCheckRestartPoint();
  }

  const EditorDOMPoint& GetSpellCheckRestartPoint() const {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->GetSpellCheckRestartPoint();
  }

  const TopLevelEditSubActionData& TopLevelEditSubActionDataRef() const {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->TopLevelEditSubActionDataRef();
  }
  TopLevelEditSubActionData& TopLevelEditSubActionDataRef() {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->TopLevelEditSubActionDataRef();
  }

  const EditSubActionData& EditSubActionDataRef() const {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->EditSubActionDataRef();
  }
  EditSubActionData& EditSubActionDataRef() {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->EditSubActionDataRef();
  }

  /**
   * GetCompositionStartPoint() and GetCompositionEndPoint() returns start and
   * end point of composition string if there is.  Otherwise, returns non-set
   * DOM point.
   */
  EditorRawDOMPoint GetCompositionStartPoint() const;
  EditorRawDOMPoint GetCompositionEndPoint() const;

  /**
   * IsSelectionRangeContainerNotContent() returns true if one of container
   * of selection ranges is not a content node, i.e., a Document node.
   */
  bool IsSelectionRangeContainerNotContent() const;

  /**
   * InsertTextAsSubAction() inserts aStringToInsert at selection.  This
   * should be used for handling it as an edit sub-action.
   *
   * @param aStringToInsert     The string to insert.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  InsertTextAsSubAction(const nsAString& aStringToInsert);

  /**
   * InsertTextWithTransaction() inserts aStringToInsert to aPointToInsert or
   * better insertion point around it.  If aPointToInsert isn't in a text node,
   * this method looks for the nearest point in a text node with
   * FindBetterInsertionPoint().  If there is no text node, this creates
   * new text node and put aStringToInsert to it.
   *
   * @param aDocument       The document of this editor.
   * @param aStringToInsert The string to insert.
   * @param aPointToInser   The point to insert aStringToInsert.
   *                        Must be valid DOM point.
   * @param aPointAfterInsertedString
   *                        The point after inserted aStringToInsert.
   *                        So, when this method actually inserts string,
   *                        this is set to a point in the text node.
   *                        Otherwise, this may be set to aPointToInsert.
   * @return                When this succeeds to insert the string or
   *                        does nothing during composition, returns NS_OK.
   *                        Otherwise, an error code.
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult InsertTextWithTransaction(
      Document& aDocument, const nsAString& aStringToInsert,
      const EditorRawDOMPoint& aPointToInsert,
      EditorRawDOMPoint* aPointAfterInsertedString = nullptr);

  /**
   * InsertTextIntoTextNodeWithTransaction() inserts aStringToInsert into
   * aOffset of aTextNode with transaction.
   *
   * @param aStringToInsert     String to be inserted.
   * @param aPointToInsert      The insertion point.
   * @param aSuppressIME        true if it's not a part of IME composition.
   *                            E.g., adjusting white-spaces during composition.
   *                            false, otherwise.
   */
  MOZ_CAN_RUN_SCRIPT nsresult InsertTextIntoTextNodeWithTransaction(
      const nsAString& aStringToInsert,
      const EditorDOMPointInText& aPointToInsert, bool aSuppressIME = false);

  /**
   * SetTextNodeWithoutTransaction() is optimized path to set new value to
   * the text node directly and without transaction.  This is used when
   * setting `<input>.value` and `<textarea>.value`.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  SetTextNodeWithoutTransaction(const nsAString& aString, Text& aTextNode);

  /**
   * DeleteNodeWithTransaction() removes aContent from the DOM tree.
   *
   * @param aContent    The node which will be removed form the DOM tree.
   */
  MOZ_CAN_RUN_SCRIPT nsresult DeleteNodeWithTransaction(nsIContent& aContent);

  /**
   * InsertNodeWithTransaction() inserts aContentToInsert before the child
   * specified by aPointToInsert.
   *
   * @param aContentToInsert    The node to be inserted.
   * @param aPointToInsert      The insertion point of aContentToInsert.
   *                            If this refers end of the container, the
   *                            transaction will append the node to the
   *                            container.  Otherwise, will insert the node
   *                            before child node referred by this.
   */
  MOZ_CAN_RUN_SCRIPT nsresult InsertNodeWithTransaction(
      nsIContent& aContentToInsert, const EditorDOMPoint& aPointToInsert);

  /**
   * InsertPaddingBRElementForEmptyLastLineWithTransaction() creates a padding
   * <br> element with setting flags to NS_PADDING_FOR_EMPTY_LAST_LINE and
   * inserts it around aPointToInsert.
   *
   * @param aPointToInsert      The DOM point where should be <br> node inserted
   *                            before.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT CreateElementResult
  InsertPaddingBRElementForEmptyLastLineWithTransaction(
      const EditorDOMPoint& aPointToInsert);

  /**
   * CloneAttributesWithTransaction() clones all attributes from
   * aSourceElement to aDestElement after removing all attributes in
   * aDestElement.
   */
  MOZ_CAN_RUN_SCRIPT void CloneAttributesWithTransaction(
      Element& aDestElement, Element& aSourceElement);

  /**
   * CloneAttributeWithTransaction() copies aAttribute of aSourceElement to
   * aDestElement.  If aSourceElement doesn't have aAttribute, this removes
   * aAttribute from aDestElement.
   *
   * @param aAttribute          Attribute name to be cloned.
   * @param aDestElement        Element node which will be set aAttribute or
   *                            whose aAttribute will be removed.
   * @param aSourceElement      Element node which provides the value of
   *                            aAttribute in aDestElement.
   */
  MOZ_CAN_RUN_SCRIPT nsresult CloneAttributeWithTransaction(
      nsAtom& aAttribute, Element& aDestElement, Element& aSourceElement);

  /**
   * RemoveAttributeWithTransaction() removes aAttribute from aElement.
   *
   * @param aElement        Element node which will lose aAttribute.
   * @param aAttribute      Attribute name to be removed from aElement.
   */
  MOZ_CAN_RUN_SCRIPT nsresult
  RemoveAttributeWithTransaction(Element& aElement, nsAtom& aAttribute);

  MOZ_CAN_RUN_SCRIPT virtual nsresult RemoveAttributeOrEquivalent(
      Element* aElement, nsAtom* aAttribute, bool aSuppressTransaction) = 0;

  /**
   * SetAttributeWithTransaction() sets aAttribute of aElement to aValue.
   *
   * @param aElement        Element node which will have aAttribute.
   * @param aAttribute      Attribute name to be set.
   * @param aValue          Attribute value be set to aAttribute.
   */
  MOZ_CAN_RUN_SCRIPT nsresult SetAttributeWithTransaction(
      Element& aElement, nsAtom& aAttribute, const nsAString& aValue);

  MOZ_CAN_RUN_SCRIPT virtual nsresult SetAttributeOrEquivalent(
      Element* aElement, nsAtom* aAttribute, const nsAString& aValue,
      bool aSuppressTransaction) = 0;

  /**
   * Method to replace certain CreateElementNS() calls.
   *
   * @param aTag        Tag you want.
   */
  already_AddRefed<Element> CreateHTMLContent(const nsAtom* aTag);

  /**
   * Creates text node which is marked as "maybe modified frequently" and
   * "maybe masked" if this is a password editor.
   */
  already_AddRefed<nsTextNode> CreateTextNode(const nsAString& aData);

  /**
   * DoInsertText(), DoDeleteText(), DoReplaceText() and DoSetText() are
   * wrapper of `CharacterData::InsertData()`, `CharacterData::DeleteData()`,
   * `CharacterData::ReplaceData()` and `CharacterData::SetData()`.
   */
  MOZ_CAN_RUN_SCRIPT void DoInsertText(dom::Text& aText, uint32_t aOffset,
                                       const nsAString& aStringToInsert,
                                       ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void DoDeleteText(dom::Text& aText, uint32_t aOffset,
                                       uint32_t aCount, ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void DoReplaceText(dom::Text& aText, uint32_t aOffset,
                                        uint32_t aCount,
                                        const nsAString& aStringToInsert,
                                        ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void DoSetText(dom::Text& aText,
                                    const nsAString& aStringToSet,
                                    ErrorResult& aRv);

  /**
   * Create an element node whose name is aTag at before aPointToInsert.  When
   * this succeed to create an element node, this sets aPointToInsert to the
   * new element because the relation of child and offset may be broken.
   * If the caller needs to collapse the selection to next to the new element
   * node, it should call |aPointToInsert.AdvanceOffset()| after calling this.
   *
   * @param aTag            The element name to create.
   * @param aPointToInsert  The insertion point of new element.  If this refers
   *                        end of the container or after, the transaction
   *                        will append the element to the container.
   *                        Otherwise, will insert the element before the
   *                        child node referred by this.
   * @return                The created new element node.
   */
  MOZ_CAN_RUN_SCRIPT already_AddRefed<Element> CreateNodeWithTransaction(
      nsAtom& aTag, const EditorDOMPoint& aPointToInsert);

  /**
   * DeleteTextWithTransaction() removes text in the range from aTextNode.
   *
   * @param aTextNode           The text node which should be modified.
   * @param aOffset             Start offset of removing text in aTextNode.
   * @param aLength             Length of removing text.
   */
  MOZ_CAN_RUN_SCRIPT nsresult DeleteTextWithTransaction(dom::Text& aTextNode,
                                                        uint32_t aOffset,
                                                        uint32_t aLength);

  /**
   * EnsureNoPaddingBRElementForEmptyEditor() removes padding <br> element
   * for empty editor if there is.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  EnsureNoPaddingBRElementForEmptyEditor();

  /**
   * MaybeCreatePaddingBRElementForEmptyEditor() creates padding <br> element
   * for empty editor if there is no children.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  MaybeCreatePaddingBRElementForEmptyEditor();

  /**
   * MarkElementDirty() sets a special dirty attribute on the element.
   * Usually this will be called immediately after creating a new node.
   *
   * @param aElement    The element for which to insert formatting.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult MarkElementDirty(Element& aElement);

  MOZ_CAN_RUN_SCRIPT nsresult
  DoTransactionInternal(nsITransaction* aTransaction);

  /**
   * Get the previous node.
   */
  nsIContent* GetPreviousNode(const EditorRawDOMPoint& aPoint) const {
    return GetPreviousNodeInternal(aPoint, false, true, false);
  }
  nsIContent* GetPreviousElementOrText(const EditorRawDOMPoint& aPoint) const {
    return GetPreviousNodeInternal(aPoint, false, false, false);
  }
  nsIContent* GetPreviousEditableNode(const EditorRawDOMPoint& aPoint) const {
    return GetPreviousNodeInternal(aPoint, true, true, false);
  }
  nsIContent* GetPreviousNodeInBlock(const EditorRawDOMPoint& aPoint) const {
    return GetPreviousNodeInternal(aPoint, false, true, true);
  }
  nsIContent* GetPreviousElementOrTextInBlock(
      const EditorRawDOMPoint& aPoint) const {
    return GetPreviousNodeInternal(aPoint, false, false, true);
  }
  nsIContent* GetPreviousEditableNodeInBlock(
      const EditorRawDOMPoint& aPoint) const {
    return GetPreviousNodeInternal(aPoint, true, true, true);
  }
  nsIContent* GetPreviousNode(const nsINode& aNode) const {
    return GetPreviousNodeInternal(aNode, false, true, false);
  }
  nsIContent* GetPreviousElementOrText(const nsINode& aNode) const {
    return GetPreviousNodeInternal(aNode, false, false, false);
  }
  nsIContent* GetPreviousEditableNode(const nsINode& aNode) const {
    return GetPreviousNodeInternal(aNode, true, true, false);
  }
  nsIContent* GetPreviousNodeInBlock(const nsINode& aNode) const {
    return GetPreviousNodeInternal(aNode, false, true, true);
  }
  nsIContent* GetPreviousElementOrTextInBlock(const nsINode& aNode) const {
    return GetPreviousNodeInternal(aNode, false, false, true);
  }
  nsIContent* GetPreviousEditableNodeInBlock(const nsINode& aNode) const {
    return GetPreviousNodeInternal(aNode, true, true, true);
  }

  /**
   * Get the next node.
   *
   * Note that methods taking EditorRawDOMPoint behavior includes the
   * child at offset as search target.  E.g., following code causes infinite
   * loop.
   *
   * EditorRawDOMPoint point(aEditableNode);
   * while (nsIContent* content = GetNextEditableNode(point)) {
   *   // Do something...
   *   point.Set(content);
   * }
   *
   * Following code must be you expected:
   *
   * while (nsIContent* content = GetNextEditableNode(point)) {
   *   // Do something...
   *   DebugOnly<bool> advanced = point.Advanced();
   *   MOZ_ASSERT(advanced);
   *   point.Set(point.GetChild());
   * }
   *
   * On the other hand, the methods taking nsINode behavior must be what
   * you want.  They start to search the result from next node of the given
   * node.
   */
  template <typename PT, typename CT>
  nsIContent* GetNextNode(const EditorDOMPointBase<PT, CT>& aPoint) const {
    return GetNextNodeInternal(aPoint, false, true, false);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextElementOrText(
      const EditorDOMPointBase<PT, CT>& aPoint) const {
    return GetNextNodeInternal(aPoint, false, false, false);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextEditableNode(
      const EditorDOMPointBase<PT, CT>& aPoint) const {
    return GetNextNodeInternal(aPoint, true, true, false);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextNodeInBlock(
      const EditorDOMPointBase<PT, CT>& aPoint) const {
    return GetNextNodeInternal(aPoint, false, true, true);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextElementOrTextInBlock(
      const EditorDOMPointBase<PT, CT>& aPoint) const {
    return GetNextNodeInternal(aPoint, false, false, true);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextEditableNodeInBlock(
      const EditorDOMPointBase<PT, CT>& aPoint) const {
    return GetNextNodeInternal(aPoint, true, true, true);
  }
  nsIContent* GetNextNode(const nsINode& aNode) const {
    return GetNextNodeInternal(aNode, false, true, false);
  }
  nsIContent* GetNextElementOrText(const nsINode& aNode) const {
    return GetNextNodeInternal(aNode, false, false, false);
  }
  nsIContent* GetNextEditableNode(const nsINode& aNode) const {
    return GetNextNodeInternal(aNode, true, true, false);
  }
  nsIContent* GetNextNodeInBlock(const nsINode& aNode) const {
    return GetNextNodeInternal(aNode, false, true, true);
  }
  nsIContent* GetNextElementOrTextInBlock(const nsINode& aNode) const {
    return GetNextNodeInternal(aNode, false, false, true);
  }
  nsIContent* GetNextEditableNodeInBlock(const nsINode& aNode) const {
    return GetNextNodeInternal(aNode, true, true, true);
  }

  /**
   * Returns true if aNode is our root node.
   */
  bool IsRoot(const nsINode* inNode) const;
  bool IsEditorRoot(const nsINode* aNode) const;

  /**
   * Returns true if aNode is a descendant of our root node.
   */
  bool IsDescendantOfRoot(const nsINode* inNode) const;
  bool IsDescendantOfEditorRoot(const nsINode* aNode) const;

  /**
   * Counts number of editable child nodes.
   */
  uint32_t CountEditableChildren(nsINode* aNode);

  /**
   * Find the deep first and last children.
   */
  nsINode* GetFirstEditableNode(nsINode* aRoot);

  /**
   * Returns true when inserting text should be a part of current composition.
   */
  bool ShouldHandleIMEComposition() const;

  /**
   * GetNodeAtRangeOffsetPoint() returns the node at this position in a range,
   * assuming that the container is the node itself if it's a text node, or
   * the node's parent otherwise.
   */
  static nsIContent* GetNodeAtRangeOffsetPoint(nsINode* aContainer,
                                               int32_t aOffset) {
    return GetNodeAtRangeOffsetPoint(RawRangeBoundary(aContainer, aOffset));
  }
  static nsIContent* GetNodeAtRangeOffsetPoint(const RawRangeBoundary& aPoint);

  static EditorRawDOMPoint GetStartPoint(const Selection& aSelection);
  static EditorRawDOMPoint GetEndPoint(const Selection& aSelection);

  static nsresult GetEndChildNode(const Selection& aSelection,
                                  nsIContent** aEndNode);

  /**
   * CollapseSelectionToEnd() collapses the selection to the end of the editor.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult CollapseSelectionToEnd() const;

  /**
   * AllowsTransactionsToChangeSelection() returns true if editor allows any
   * transactions to change Selection.  Otherwise, transactions shouldn't
   * change Selection.
   */
  inline bool AllowsTransactionsToChangeSelection() const {
    return mAllowsTransactionsToChangeSelection;
  }

  /**
   * MakeThisAllowTransactionsToChangeSelection() with true makes this editor
   * allow transactions to change Selection.  Otherwise, i.e., with false,
   * makes this editor not allow transactions to change Selection.
   */
  inline void MakeThisAllowTransactionsToChangeSelection(bool aAllow) {
    mAllowsTransactionsToChangeSelection = aAllow;
  }

  nsresult HandleInlineSpellCheck(
      const EditorDOMPoint& aPreviouslySelectedStart,
      const dom::AbstractRange* aRange = nullptr);

  /**
   * Likewise, but gets the editor's root instead, which is different for HTML
   * editors.
   */
  virtual Element* GetEditorRoot() const;

  /**
   * Whether the editor is active on the DOM window.  Note that when this
   * returns true but GetFocusedContent() returns null, it means that this
   * editor was focused when the DOM window was active.
   */
  virtual bool IsActiveInDOMWindow() const;

  /**
   * FindBetterInsertionPoint() tries to look for better insertion point which
   * is typically the nearest text node and offset in it.
   *
   * @param aPoint      Insertion point which the callers found.
   * @return            Better insertion point if there is.  If not returns
   *                    same point as aPoint.
   */
  EditorRawDOMPoint FindBetterInsertionPoint(
      const EditorRawDOMPoint& aPoint) const;

  /**
   * HideCaret() hides caret with nsCaret::AddForceHide() or may show carent
   * with nsCaret::RemoveForceHide().  This does NOT set visibility of
   * nsCaret.  Therefore, this is stateless.
   */
  void HideCaret(bool aHide);

 protected:  // Edit sub-action handler
  /**
   * AutoCaretBidiLevelManager() computes bidi level of caret, deleting
   * character(s) from aPointAtCaret at construction.  Then, if you'll
   * need to extend the selection, you should calls `UpdateCaretBidiLevel()`,
   * then, this class may update caret bidi level for you if it's required.
   */
  class MOZ_RAII AutoCaretBidiLevelManager final {
   public:
    /**
     * @param aEditorBase         The editor.
     * @param aPointAtCaret       Collapsed `Selection` point.
     * @param aDirectionAndAmount The direction and amount to delete.
     */
    template <typename PT, typename CT>
    AutoCaretBidiLevelManager(const EditorBase& aEditorBase,
                              nsIEditor::EDirection aDirectionAndAmount,
                              const EditorDOMPointBase<PT, CT>& aPointAtCaret);

    /**
     * Failed() returns true if the constructor failed to handle the bidi
     * information.
     */
    bool Failed() const { return mFailed; }

    /**
     * Canceled() returns true if when the caller should stop deleting
     * characters since caret position is not visually adjacent the deleting
     * characters and user does not wand to delete them in that case.
     */
    bool Canceled() const { return mCanceled; }

    /**
     * MaybeUpdateCaretBidiLevel() may update caret bidi level and schedule to
     * paint it if they are necessary.
     */
    void MaybeUpdateCaretBidiLevel(const EditorBase& aEditorBase) const;

   private:
    Maybe<nsBidiLevel> mNewCaretBidiLevel;
    bool mFailed = false;
    bool mCanceled = false;
  };

  /**
   * UndefineCaretBidiLevel() resets bidi level of the caret.
   */
  void UndefineCaretBidiLevel() const;

  /**
   * Flushing pending notifications if nsFrameSelection requires the latest
   * layout information to compute deletion range.  This may destroy the
   * editor instance itself.  When this returns false, don't keep doing
   * anything.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT bool
  FlushPendingNotificationsIfToHandleDeletionWithFrameSelection(
      nsIEditor::EDirection aDirectionAndAmount) const;

  /**
   * DeleteSelectionAsSubAction() removes selection content or content around
   * caret with transactions.  This should be used for handling it as an
   * edit sub-action.
   *
   * @param aDirectionAndAmount How much range should be removed.
   * @param aStripWrappers      Whether the parent blocks should be removed
   *                            when they become empty.  If this instance is
   *                            a TextEditor, Must be nsIEditor::eNoStrip.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  DeleteSelectionAsSubAction(nsIEditor::EDirection aDirectionAndAmount,
                             nsIEditor::EStripWrappers aStripWrappers);

  /**
   * This method handles "delete selection" commands.
   * NOTE: Don't call this method recursively from the helper methods since
   *       when nobody handled it without canceling and returing an error,
   *       this falls it back to `DeleteSelectionWithTransaction()`.
   *
   * @param aDirectionAndAmount Direction of the deletion.
   * @param aStripWrappers      Must be nsIEditor::eNoStrip if this is a
   *                            TextEditor instance.  Otherwise,
   *                            nsIEditor::eStrip is also valid.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT virtual EditActionResult
  HandleDeleteSelection(nsIEditor::EDirection aDirectionAndAmount,
                        nsIEditor::EStripWrappers aStripWrappers) = 0;

 protected:  // Called by helper classes.
  /**
   * OnStartToHandleTopLevelEditSubAction() is called when
   * GetTopLevelEditSubAction() is EditSubAction::eNone and somebody starts to
   * handle aEditSubAction.
   *
   * @param aTopLevelEditSubAction              Top level edit sub action which
   *                                            will be handled soon.
   * @param aDirectionOfTopLevelEditSubAction   Direction of aEditSubAction.
   */
  MOZ_CAN_RUN_SCRIPT virtual void OnStartToHandleTopLevelEditSubAction(
      EditSubAction aTopLevelEditSubAction,
      nsIEditor::EDirection aDirectionOfTopLevelEditSubAction,
      ErrorResult& aRv);

  /**
   * OnEndHandlingTopLevelEditSubAction() is called after
   * SetTopLevelEditSubAction() is handled.
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult OnEndHandlingTopLevelEditSubAction();

  /**
   * OnStartToHandleEditSubAction() and OnEndHandlingEditSubAction() are called
   * when starting to handle an edit sub action and ending handling an edit
   * sub action.
   */
  void OnStartToHandleEditSubAction() { EditSubActionDataRef().Clear(); }
  void OnEndHandlingEditSubAction() { EditSubActionDataRef().Clear(); }

  /**
   * Routines for managing the preservation of selection across
   * various editor actions.
   */
  bool ArePreservingSelection();
  void PreserveSelectionAcrossActions();
  nsresult RestorePreservedSelection();
  void StopPreservingSelection();

  /**
   * (Begin|End)PlaceholderTransaction() are called by AutoPlaceholderBatch.
   * This set of methods are similar to the (Begin|End)Transaction(), but do
   * not use the transaction managers batching feature.  Instead we use a
   * placeholder transaction to wrap up any further transaction while the
   * batch is open.  The advantage of this is that placeholder transactions
   * can later merge, if needed.  Merging is unavailable between transaction
   * manager batches.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void BeginPlaceholderTransaction(
      nsStaticAtom& aTransactionName);
  enum class ScrollSelectionIntoView { No, Yes };
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void EndPlaceholderTransaction(
      ScrollSelectionIntoView aScrollSelectionIntoView);

  void BeginUpdateViewBatch();
  MOZ_CAN_RUN_SCRIPT void EndUpdateViewBatch();

  /**
   * Used by AutoTransactionBatch.  After calling BeginTransactionInternal(),
   * all transactions will be treated as an atomic transaction.  I.e.,
   * two or more transactions are undid once.
   * XXX What's the difference with PlaceholderTransaction? Should we always
   *     use it instead?
   */
  MOZ_CAN_RUN_SCRIPT void BeginTransactionInternal();
  MOZ_CAN_RUN_SCRIPT void EndTransactionInternal();

 protected:  // Shouldn't be used by friend classes
  /**
   * The default destructor. This should suffice. Should this be pure virtual
   * for someone to derive from the EditorBase later? I don't believe so.
   */
  virtual ~EditorBase();

  MOZ_ALWAYS_INLINE EditorType GetEditorType() const {
    return mIsHTMLEditorClass ? EditorType::HTML : EditorType::Text;
  }

  int32_t WrapWidth() const { return mWrapColumn; }

  /**
   * ToGenericNSResult() computes proper nsresult value for the editor users.
   * This should be used only when public methods return result of internal
   * methods.
   */
  static inline nsresult ToGenericNSResult(nsresult aRv) {
    switch (aRv) {
      // If the editor is destroyed while handling an edit action, editor needs
      // to stop handling it.  However, editor throw exception in this case
      // because Chrome does not throw exception even in this case.
      case NS_ERROR_EDITOR_DESTROYED:
        return NS_OK;
      // If editor meets unexpected DOM tree due to modified by mutation event
      // listener, editor needs to stop handling it.  However, editor shouldn't
      // return error for the users because Chrome does not throw exception in
      // this case.
      case NS_ERROR_EDITOR_UNEXPECTED_DOM_TREE:
        return NS_OK;
      // If the editing action is canceled by event listeners, editor needs
      // to stop handling it.  However, editor shouldn't return error for
      // the callers but they should be able to distinguish whether it's
      // canceled or not.  Although it's DOM specific code, let's return
      // DOM_SUCCESS_DOM_NO_OPERATION here.
      case NS_ERROR_EDITOR_ACTION_CANCELED:
        return NS_SUCCESS_DOM_NO_OPERATION;
      // If there is no selection range or editable selection ranges, editor
      // needs to stop handling it.  However, editor shouldn't return error for
      // the callers to avoid throwing exception.  However, they may want to
      // check whether it works or not.  Therefore, we should return
      // NS_SUCCESS_DOM_NO_OPERATION instead.
      case NS_ERROR_EDITOR_NO_EDITABLE_RANGE:
        return NS_SUCCESS_DOM_NO_OPERATION;
      default:
        return aRv;
    }
  }

  /**
   * GetDocumentCharsetInternal() returns charset of the document.
   */
  nsresult GetDocumentCharsetInternal(nsACString& aCharset) const;

  /**
   * SelectAllInternal() should be used instead of SelectAll() in editor
   * because SelectAll() creates AutoEditActionSetter but we should avoid
   * to create it as far as possible.
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult SelectAllInternal();

  nsresult DetermineCurrentDirection();

  /**
   * DispatchInputEvent() dispatches an "input" event synchronously or
   * asynchronously if it's not safe to dispatch.
   */
  MOZ_CAN_RUN_SCRIPT void DispatchInputEvent();

  /**
   * Called after a transaction is done successfully.
   */
  MOZ_CAN_RUN_SCRIPT void DoAfterDoTransaction(nsITransaction* aTransaction);

  /**
   * Called after a transaction is undone successfully.
   */

  MOZ_CAN_RUN_SCRIPT void DoAfterUndoTransaction();

  /**
   * Called after a transaction is redone successfully.
   */
  MOZ_CAN_RUN_SCRIPT void DoAfterRedoTransaction();

  /**
   * Tell the doc state listeners that the doc state has changed.
   */
  enum TDocumentListenerNotification {
    eDocumentCreated,
    eDocumentToBeDestroyed,
    eDocumentStateChanged
  };
  MOZ_CAN_RUN_SCRIPT nsresult
  NotifyDocumentListeners(TDocumentListenerNotification aNotificationType);

  /**
   * Make the given selection span the entire document.
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult SelectEntireDocument() = 0;

  /**
   * Helper method for scrolling the selection into view after
   * an edit operation.
   *
   * Editor methods *should* call this method instead of the versions
   * in the various selection interfaces, since this makes sure that
   * the editor's sync/async settings for reflowing, painting, and scrolling
   * match.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult ScrollSelectionFocusIntoView();

  /**
   * Helper for GetPreviousNodeInternal() and GetNextNodeInternal().
   */
  nsIContent* FindNextLeafNode(const nsINode* aCurrentNode, bool aGoForward,
                               bool bNoBlockCrossing) const;
  nsIContent* FindNode(const nsINode* aCurrentNode, bool aGoForward,
                       bool aEditableNode, bool aFindAnyDataNode,
                       bool bNoBlockCrossing) const;

  /**
   * Get the node immediately previous node of aNode.
   * @param atNode               The node from which we start the search.
   * @param aFindEditableNode    If true, only return an editable node.
   * @param aFindAnyDataNode     If true, may return invisible data node
   *                             like Comment.
   * @param aNoBlockCrossing     If true, don't move across "block" nodes,
   *                             whatever that means.
   * @return                     The node that occurs before aNode in
   *                             the tree, skipping non-editable nodes if
   *                             aFindEditableNode is true.  If there is no
   *                             previous node, returns nullptr.
   */
  nsIContent* GetPreviousNodeInternal(const nsINode& aNode,
                                      bool aFindEditableNode,
                                      bool aFindAnyDataNode,
                                      bool aNoBlockCrossing) const;

  /**
   * And another version that takes a point in DOM tree rather than a node.
   */
  nsIContent* GetPreviousNodeInternal(const EditorRawDOMPoint& aPoint,
                                      bool aFindEditableNode,
                                      bool aFindAnyDataNode,
                                      bool aNoBlockCrossing) const;

  /**
   * Get the node immediately next node of aNode.
   * @param aNode                The node from which we start the search.
   * @param aFindEditableNode    If true, only return an editable node.
   * @param aFindAnyDataNode     If true, may return invisible data node
   *                             like Comment.
   * @param aNoBlockCrossing     If true, don't move across "block" nodes,
   *                             whatever that means.
   * @return                     The node that occurs after aNode in the
   *                             tree, skipping non-editable nodes if
   *                             aFindEditableNode is true.  If there is no
   *                             next node, returns nullptr.
   */
  nsIContent* GetNextNodeInternal(const nsINode& aNode, bool aFindEditableNode,
                                  bool aFindAnyDataNode,
                                  bool aNoBlockCrossing) const;

  /**
   * And another version that takes a point in DOM tree rather than a node.
   */
  nsIContent* GetNextNodeInternal(const EditorRawDOMPoint& aPoint,
                                  bool aFindEditableNode, bool aFindAnyDataNode,
                                  bool aNoBlockCrossing) const;

  virtual nsresult InstallEventListeners();
  virtual void CreateEventListeners();
  virtual void RemoveEventListeners();

  /**
   * Get the input event target. This might return null.
   */
  virtual already_AddRefed<Element> GetInputEventTargetElement() const = 0;

  /**
   * Return true if spellchecking should be enabled for this editor.
   */
  bool GetDesiredSpellCheckState();

  bool CanEnableSpellCheck() {
    // Check for password/readonly/disabled, which are not spellchecked
    // regardless of DOM. Also, check to see if spell check should be skipped
    // or not.
    return !IsPasswordEditor() && !IsReadonly() && !ShouldSkipSpellCheck();
  }

  /**
   * InitializeSelectionAncestorLimit() is called by InitializeSelection().
   * When this is called, each implementation has to call
   * Selection::SetAncestorLimiter() with aAnotherLimit.
   *
   * @param aAncestorLimit      New ancestor limit of Selection.  This always
   *                            has parent node.  So, it's always safe to
   *                            call SetAncestorLimit() with this node.
   */
  virtual void InitializeSelectionAncestorLimit(
      nsIContent& aAncestorLimit) const;

  /**
   * Creates a range with just the supplied node and appends that to the
   * selection.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  AppendNodeToSelectionAsRange(nsINode* aNode);

  /**
   * When you are using AppendNodeToSelectionAsRange(), call this first to
   * start a new selection.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult ClearSelection();

  /**
   * Initializes selection and caret for the editor.  If aEventTarget isn't
   * a host of the editor, i.e., the editor doesn't get focus, this does
   * nothing.
   */
  MOZ_CAN_RUN_SCRIPT nsresult InitializeSelection(nsINode& aFocusEventTarget);

  enum NotificationForEditorObservers {
    eNotifyEditorObserversOfEnd,
    eNotifyEditorObserversOfBefore,
    eNotifyEditorObserversOfCancel
  };
  MOZ_CAN_RUN_SCRIPT void NotifyEditorObservers(
      NotificationForEditorObservers aNotification);

  /**
   * InsertLineBreakAsSubAction() inserts a line break, i.e., \n if it's
   * TextEditor or <br> if it's HTMLEditor.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult InsertLineBreakAsSubAction();

  /**
   * HowToHandleCollapsedRange indicates how collapsed range should be treated.
   */
  enum class HowToHandleCollapsedRange {
    // Ignore collapsed range.
    Ignore,
    // Extend collapsed range for removing previous content.
    ExtendBackward,
    // Extend collapsed range for removing next content.
    ExtendForward,
  };

  static HowToHandleCollapsedRange HowToHandleCollapsedRangeFor(
      nsIEditor::EDirection aDirectionAndAmount) {
    switch (aDirectionAndAmount) {
      case nsIEditor::eNone:
        return HowToHandleCollapsedRange::Ignore;
      case nsIEditor::ePrevious:
        return HowToHandleCollapsedRange::ExtendBackward;
      case nsIEditor::eNext:
        return HowToHandleCollapsedRange::ExtendForward;
      case nsIEditor::ePreviousWord:
      case nsIEditor::eNextWord:
      case nsIEditor::eToBeginningOfLine:
      case nsIEditor::eToEndOfLine:
        // If the amount is word or
        // line,`AutoRangeArray::ExtendAnchorFocusRangeFor()` must have already
        // been extended collapsed ranges before.
        return HowToHandleCollapsedRange::Ignore;
    }
    MOZ_ASSERT_UNREACHABLE("Invalid nsIEditor::EDirection value");
    return HowToHandleCollapsedRange::Ignore;
  }

  /**
   * DeleteSelectionWithTransaction() removes selected content or content
   * around caret with transactions and remove empty inclusive ancestor
   * inline elements of collapsed selection after removing the contents.
   *
   * @param aDirectionAndAmount How much range should be removed.
   * @param aStripWrappers      Whether the parent blocks should be removed
   *                            when they become empty.
   *                            Note that this must be `nsIEditor::eNoStrip`
   *                            if this is a TextEditor because anyway it'll
   *                            be ignored.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  DeleteSelectionWithTransaction(nsIEditor::EDirection aDirectionAndAmount,
                                 nsIEditor::EStripWrappers aStripWrappers);

  /**
   * DeleteRangesWithTransaction() removes content in aRangesToDelete or content
   * around collapsed ranges in aRangesToDelete with transactions and remove
   * empty inclusive ancestor inline elements of collapsed ranges after
   * removing the contents.
   *
   * @param aDirectionAndAmount How much range should be removed.
   * @param aStripWrappers      Whether the parent blocks should be removed
   *                            when they become empty.
   *                            Note that this must be `nsIEditor::eNoStrip`
   *                            if this is a TextEditor because anyway it'll
   *                            be ignored.
   * @param aRangesToDelete     The ranges to delete content.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT nsresult
  DeleteRangesWithTransaction(nsIEditor::EDirection aDirectionAndAmount,
                              nsIEditor::EStripWrappers aStripWrappers,
                              const AutoRangeArray& aRangesToDelete);

  /**
   * Create an aggregate transaction for delete the content in aRangesToDelete.
   * The result may include DeleteNodeTransactions and/or DeleteTextTransactions
   * as its children.
   *
   * @param aHowToHandleCollapsedRange
   *                            How to handle collapsed ranges.
   * @param aRangesToDelete     The ranges to delete content.
   * @return                    If it can remove the content in ranges, returns
   *                            an aggregate transaction which has some
   *                            DeleteNodeTransactions and/or
   *                            DeleteTextTransactions as its children.
   */
  already_AddRefed<EditAggregateTransaction>
  CreateTransactionForDeleteSelection(
      HowToHandleCollapsedRange aHowToHandleCollapsedRange,
      const AutoRangeArray& aRangesToDelete);

  /**
   * Create a transaction for removing the nodes and/or text around
   * aRangeToDelete.
   *
   * @param aCollapsedRange     The range to be removed.  This must be
   *                            collapsed.
   * @param aHowToHandleCollapsedRange
   *                            How to handle aCollapsedRange.  Must
   *                            be HowToHandleCollapsedRange::ExtendBackward or
   *                            HowToHandleCollapsedRange::ExtendForward.
   * @return                    The transaction to remove content around the
   *                            range.  Its type is DeleteNodeTransaction or
   *                            DeleteTextTransaction.
   */
  already_AddRefed<EditTransactionBase> CreateTransactionForCollapsedRange(
      const nsRange& aCollapsedRange,
      HowToHandleCollapsedRange aHowToHandleCollapsedRange);

  /**
   * ComputeInsertedRange() returns actual range modified by inserting string
   * in a text node.  If mutation event listener changed the text data, this
   * returns a range which covers all over the text data.
   */
  Tuple<EditorDOMPointInText, EditorDOMPointInText> ComputeInsertedRange(
      const EditorDOMPointInText& aInsertedPoint,
      const nsAString& aInsertedString) const;

 private:
  nsCOMPtr<nsISelectionController> mSelectionController;
  RefPtr<Document> mDocument;

  AutoEditActionDataSetter* mEditActionData;

  /**
   * SetTextDirectionTo() sets text-direction of the root element.
   * Should use SwitchTextDirectionTo() or ToggleTextDirection() instead.
   * This is a helper class of them.
   */
  nsresult SetTextDirectionTo(TextDirection aTextDirection);

 protected:  // helper classes which may be used by friends
  /**
   * Stack based helper class for calling EditorBase::EndTransactionInternal().
   * NOTE:  This does not suppress multiple input events.  In most cases,
   *        only one "input" event should be fired for an edit action rather
   *        than per edit sub-action.  In such case, you should use
   *        AutoPlaceholderBatch instead.
   */
  class MOZ_RAII AutoTransactionBatch final {
   public:
    MOZ_CAN_RUN_SCRIPT explicit AutoTransactionBatch(EditorBase& aEditorBase)
        : mEditorBase(aEditorBase) {
      MOZ_KnownLive(mEditorBase).BeginTransactionInternal();
    }

    MOZ_CAN_RUN_SCRIPT ~AutoTransactionBatch() {
      MOZ_KnownLive(mEditorBase).EndTransactionInternal();
    }

   protected:
    EditorBase& mEditorBase;
  };

  /**
   * Stack based helper class for batching a collection of transactions inside
   * a placeholder transaction.  Different from AutoTransactionBatch, this
   * notifies editor observers of before/end edit action handling, and
   * dispatches "input" event if it's necessary.
   */
  class MOZ_RAII AutoPlaceholderBatch final {
   public:
    AutoPlaceholderBatch(EditorBase& aEditorBase,
                         ScrollSelectionIntoView aScrollSelectionIntoView)
        : mEditorBase(aEditorBase),
          mScrollSelectionIntoView(aScrollSelectionIntoView) {
      mEditorBase->BeginPlaceholderTransaction(*nsGkAtoms::_empty);
    }

    AutoPlaceholderBatch(EditorBase& aEditorBase,
                         nsStaticAtom& aTransactionName,
                         ScrollSelectionIntoView aScrollSelectionIntoView)
        : mEditorBase(aEditorBase),
          mScrollSelectionIntoView(aScrollSelectionIntoView) {
      mEditorBase->BeginPlaceholderTransaction(aTransactionName);
    }

    ~AutoPlaceholderBatch() {
      mEditorBase->EndPlaceholderTransaction(mScrollSelectionIntoView);
    }

   protected:
    OwningNonNull<EditorBase> mEditorBase;
    ScrollSelectionIntoView mScrollSelectionIntoView;
  };

  /**
   * Stack based helper class for saving/restoring selection.  Note that this
   * assumes that the nodes involved are still around afterwords!
   */
  class MOZ_RAII AutoSelectionRestorer final {
   public:
    /**
     * Constructor responsible for remembering all state needed to restore
     * aSelection.
     */
    explicit AutoSelectionRestorer(EditorBase& aEditorBase);

    /**
     * Destructor restores mSelection to its former state
     */
    ~AutoSelectionRestorer();

    /**
     * Abort() cancels to restore the selection.
     */
    void Abort();

   protected:
    EditorBase* mEditorBase;
  };

  /**
   * AutoEditSubActionNotifier notifies editor of start to handle
   * top level edit sub-action and end handling top level edit sub-action.
   */
  class MOZ_RAII AutoEditSubActionNotifier final {
   public:
    MOZ_CAN_RUN_SCRIPT AutoEditSubActionNotifier(
        EditorBase& aEditorBase, EditSubAction aEditSubAction,
        nsIEditor::EDirection aDirection, ErrorResult& aRv)
        : mEditorBase(aEditorBase), mIsTopLevel(true) {
      // The top level edit sub action has already be set if this is nested call
      // XXX Looks like that this is not aware of unexpected nested edit action
      //     handling via selectionchange event listener or mutation event
      //     listener.
      if (!mEditorBase.GetTopLevelEditSubAction()) {
        MOZ_KnownLive(mEditorBase)
            .OnStartToHandleTopLevelEditSubAction(aEditSubAction, aDirection,
                                                  aRv);
      } else {
        mIsTopLevel = false;
      }
      mEditorBase.OnStartToHandleEditSubAction();
    }

    MOZ_CAN_RUN_SCRIPT ~AutoEditSubActionNotifier() {
      mEditorBase.OnEndHandlingEditSubAction();
      if (mIsTopLevel) {
        MOZ_KnownLive(mEditorBase).OnEndHandlingTopLevelEditSubAction();
      }
    }

   protected:
    EditorBase& mEditorBase;
    bool mIsTopLevel;
  };

  /**
   * Stack based helper class for turning off active selection adjustment
   * by low level transactions
   */
  class MOZ_RAII AutoTransactionsConserveSelection final {
   public:
    explicit AutoTransactionsConserveSelection(EditorBase& aEditorBase)
        : mEditorBase(aEditorBase),
          mAllowedTransactionsToChangeSelection(
              aEditorBase.AllowsTransactionsToChangeSelection()) {
      mEditorBase.MakeThisAllowTransactionsToChangeSelection(false);
    }

    ~AutoTransactionsConserveSelection() {
      mEditorBase.MakeThisAllowTransactionsToChangeSelection(
          mAllowedTransactionsToChangeSelection);
    }

   protected:
    EditorBase& mEditorBase;
    bool mAllowedTransactionsToChangeSelection;
  };

  /***************************************************************************
   * stack based helper class for batching reflow and paint requests.
   */
  class MOZ_RAII AutoUpdateViewBatch final {
   public:
    MOZ_CAN_RUN_SCRIPT explicit AutoUpdateViewBatch(EditorBase& aEditorBase)
        : mEditorBase(aEditorBase) {
      mEditorBase.BeginUpdateViewBatch();
    }

    MOZ_CAN_RUN_SCRIPT ~AutoUpdateViewBatch() {
      MOZ_KnownLive(mEditorBase).EndUpdateViewBatch();
    }

   protected:
    EditorBase& mEditorBase;
  };

 protected:
  enum Tristate { eTriUnset, eTriFalse, eTriTrue };

  // MIME type of the doc we are editing.
  nsString mContentMIMEType;

  RefPtr<mozInlineSpellChecker> mInlineSpellChecker;
  // Reference to text services document for mInlineSpellChecker.
  RefPtr<TextServicesDocument> mTextServicesDocument;

  RefPtr<TransactionManager> mTransactionManager;
  // Cached root node.
  RefPtr<Element> mRootElement;

  // mPaddingBRElementForEmptyEditor should be used for placing caret
  // at proper position when editor is empty.
  RefPtr<dom::HTMLBRElement> mPaddingBRElementForEmptyEditor;

  // The form field as an event receiver.
  nsCOMPtr<dom::EventTarget> mEventTarget;
  RefPtr<EditorEventListener> mEventListener;
  // Strong reference to placeholder for begin/end batch purposes.
  RefPtr<PlaceholderTransaction> mPlaceholderTransaction;
  // Name of placeholder transaction.
  nsStaticAtom* mPlaceholderName;
  // Saved selection state for placeholder transaction batching.
  mozilla::Maybe<SelectionState> mSelState;
  // IME composition this is not null between compositionstart and
  // compositionend.
  RefPtr<TextComposition> mComposition;

  RefPtr<TextInputListener> mTextInputListener;

  RefPtr<IMEContentObserver> mIMEContentObserver;

  // Listens to all low level actions on the doc.
  // Edit action listener is currently used by highlighter of the findbar and
  // the spellchecker.  So, we should reserve only 2 items.
  typedef AutoTArray<OwningNonNull<nsIEditActionListener>, 2>
      AutoActionListenerArray;
  AutoActionListenerArray mActionListeners;
  // Just notify once per high level change.
  // Editor observer is used only by legacy addons for Thunderbird and
  // BlueGriffon.  So, we don't need to reserve the space for them.
  typedef AutoTArray<OwningNonNull<nsIEditorObserver>, 0>
      AutoEditorObserverArray;
  AutoEditorObserverArray mEditorObservers;
  // Listen to overall doc state (dirty or not, just created, etc.).
  // Document state listener is currently used by FinderHighlighter and
  // BlueGriffon so that reserving only one is enough.
  typedef AutoTArray<OwningNonNull<nsIDocumentStateListener>, 1>
      AutoDocumentStateListenerArray;
  AutoDocumentStateListenerArray mDocStateListeners;

  // Number of modifications (for undo/redo stack).
  uint32_t mModCount;
  // Behavior flags. See nsIEditor.idl for the flags we use.
  uint32_t mFlags;

  int32_t mUpdateCount;

  // Nesting count for batching.
  int32_t mPlaceholderBatch;

  int32_t mWrapColumn;
  int32_t mNewlineHandling;
  int32_t mCaretStyle;

  // -1 = not initialized
  int8_t mDocDirtyState;
  // A Tristate value.
  uint8_t mSpellcheckCheckboxState;

  // If true, initialization was succeeded.
  bool mInitSucceeded;
  // If false, transactions should not change Selection even after modifying
  // the DOM tree.
  bool mAllowsTransactionsToChangeSelection;
  // Whether PreDestroy has been called.
  bool mDidPreDestroy;
  // Whether PostCreate has been called.
  bool mDidPostCreate;
  bool mDispatchInputEvent;
  // True while the instance is handling an edit sub-action.
  bool mIsInEditSubAction;
  // Whether caret is hidden forcibly.
  bool mHidingCaret;
  // Whether spellchecker dictionary is initialized after focused.
  bool mSpellCheckerDictionaryUpdated;
  // Whether we are an HTML editor class.
  bool mIsHTMLEditorClass;

  friend class AlignStateAtSelection;
  friend class AutoRangeArray;
  friend class CompositionTransaction;
  friend class CreateElementTransaction;
  friend class CSSEditUtils;
  friend class DeleteNodeTransaction;
  friend class DeleteRangeTransaction;
  friend class DeleteTextTransaction;
  friend class HTMLEditUtils;
  friend class InsertNodeTransaction;
  friend class InsertTextTransaction;
  friend class JoinNodeTransaction;
  friend class ListElementSelectionState;
  friend class ListItemElementSelectionState;
  friend class ParagraphStateAtSelection;
  friend class ReplaceTextTransaction;
  friend class SplitNodeTransaction;
  friend class TypeInState;
  friend class WhiteSpaceVisibilityKeeper;
  friend class WSRunScanner;
  friend class nsIEditor;
};

}  // namespace mozilla

mozilla::EditorBase* nsIEditor::AsEditorBase() {
  return static_cast<mozilla::EditorBase*>(this);
}

const mozilla::EditorBase* nsIEditor::AsEditorBase() const {
  return static_cast<const mozilla::EditorBase*>(this);
}

#endif  // #ifndef mozilla_EditorBase_h
