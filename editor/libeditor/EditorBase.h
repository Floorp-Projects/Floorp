/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorBase_h
#define mozilla_EditorBase_h

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc.
#include "mozilla/OwningNonNull.h"      // for OwningNonNull
#include "mozilla/SelectionState.h"     // for RangeUpdater, etc.
#include "mozilla/StyleSheet.h"   // for StyleSheet
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/Text.h"
#include "nsCOMPtr.h"                   // for already_AddRefed, nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"
#include "nsIEditor.h"                  // for nsIEditor, etc.
#include "nsIObserver.h"                // for NS_DECL_NSIOBSERVER, etc.
#include "nsIPlaintextEditor.h"         // for nsIPlaintextEditor, etc.
#include "nsISelectionController.h"     // for nsISelectionController constants
#include "nsISupportsImpl.h"            // for EditorBase::Release, etc.
#include "nsIWeakReferenceUtils.h"      // for nsWeakPtr
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nsString.h"                   // for nsCString
#include "nsTArray.h"                   // for nsTArray and nsAutoTArray
#include "nsWeakReference.h"            // for nsSupportsWeakReference
#include "nscore.h"                     // for nsresult, nsAString, etc.

class nsIAtom;
class nsIContent;
class nsIDOMDocument;
class nsIDOMEvent;
class nsIDOMEventListener;
class nsIDOMEventTarget;
class nsIDOMNode;
class nsIDocument;
class nsIDocumentStateListener;
class nsIEditActionListener;
class nsIEditorObserver;
class nsIInlineSpellChecker;
class nsINode;
class nsIPresShell;
class nsISupports;
class nsITransaction;
class nsIWidget;
class nsRange;
class nsString;
class nsTransactionManager;

// This is int32_t instead of int16_t because nsIInlineSpellChecker.idl's
// spellCheckAfterEditorChange is defined to take it as a long.
// XXX EditAction causes unnecessary include of EditorBase from some places.
//     Why don't you move this to nsIEditor.idl?
enum class EditAction : int32_t
{
  ignore = -1,
  none = 0,
  undo,
  redo,
  insertNode,
  createNode,
  deleteNode,
  splitNode,
  joinNode,
  deleteText = 1003,

  // text commands
  insertText         = 2000,
  insertIMEText      = 2001,
  deleteSelection    = 2002,
  setTextProperty    = 2003,
  removeTextProperty = 2004,
  outputText         = 2005,
  setText            = 2006,

  // html only action
  insertBreak         = 3000,
  makeList            = 3001,
  indent              = 3002,
  outdent             = 3003,
  align               = 3004,
  makeBasicBlock      = 3005,
  removeList          = 3006,
  makeDefListItem     = 3007,
  insertElement       = 3008,
  insertQuotation     = 3009,
  htmlPaste           = 3012,
  loadHTML            = 3013,
  resetTextProperties = 3014,
  setAbsolutePosition = 3015,
  removeAbsolutePosition = 3016,
  decreaseZIndex      = 3017,
  increaseZIndex      = 3018
};

inline bool operator!(const EditAction& aOp)
{
  return aOp == EditAction::none;
}

namespace mozilla {
class AddStyleSheetTransaction;
class AutoRules;
class AutoSelectionRestorer;
class AutoTransactionsConserveSelection;
class ChangeAttributeTransaction;
class CompositionTransaction;
class CreateElementTransaction;
class DeleteNodeTransaction;
class DeleteTextTransaction;
class EditAggregateTransaction;
class EditTransactionBase;
class ErrorResult;
class HTMLEditor;
class InsertNodeTransaction;
class InsertTextTransaction;
class JoinNodeTransaction;
class RemoveStyleSheetTransaction;
class SetTextTransaction;
class SplitNodeTransaction;
class TextComposition;
class TextEditor;
struct EditorDOMPoint;

namespace dom {
class DataTransfer;
class Element;
class EventTarget;
class Selection;
class Text;
} // namespace dom

namespace widget {
struct IMEState;
} // namespace widget

#define kMOZEditorBogusNodeAttrAtom nsGkAtoms::mozeditorbogusnode
#define kMOZEditorBogusNodeValue NS_LITERAL_STRING("TRUE")

/**
 * Implementation of an editor object.  it will be the controller/focal point
 * for the main editor services. i.e. the GUIManager, publishing, transaction
 * manager, event interfaces. the idea for the event interfaces is to have them
 * delegate the actual commands to the editor independent of the XPFE
 * implementation.
 */
class EditorBase : public nsIEditor
                 , public nsSupportsWeakReference
{
public:
  typedef dom::Element Element;
  typedef dom::Selection Selection;
  typedef dom::Text Text;

  enum IterDirection
  {
    kIterForward,
    kIterBackward
  };

  /**
   * The default constructor. This should suffice. the setting of the
   * interfaces is done after the construction of the editor class.
   */
  EditorBase();

  virtual TextEditor* AsTextEditor() = 0;
  virtual const TextEditor* AsTextEditor() const = 0;
  virtual HTMLEditor* AsHTMLEditor() = 0;
  virtual const HTMLEditor* AsHTMLEditor() const = 0;

protected:
  /**
   * The default destructor. This should suffice. Should this be pure virtual
   * for someone to derive from the EditorBase later? I don't believe so.
   */
  virtual ~EditorBase();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(EditorBase, nsIEditor)

  already_AddRefed<nsIDOMDocument> GetDOMDocument();
  already_AddRefed<nsIDocument> GetDocument();
  already_AddRefed<nsIPresShell> GetPresShell();
  already_AddRefed<nsIWidget> GetWidget();
  enum NotificationForEditorObservers
  {
    eNotifyEditorObserversOfEnd,
    eNotifyEditorObserversOfBefore,
    eNotifyEditorObserversOfCancel
  };
  void NotifyEditorObservers(NotificationForEditorObservers aNotification);

  // nsIEditor methods
  NS_DECL_NSIEDITOR

public:
  virtual bool IsModifiableNode(nsINode* aNode);

  virtual nsresult InsertTextImpl(const nsAString& aStringToInsert,
                                  nsCOMPtr<nsINode>* aInOutNode,
                                  int32_t* aInOutOffset,
                                  nsIDocument* aDoc);
  nsresult InsertTextIntoTextNodeImpl(const nsAString& aStringToInsert,
                                      Text& aTextNode, int32_t aOffset,
                                      bool aSuppressIME = false);

  nsresult SetTextImpl(const nsAString& aString,
                       Text& aTextNode);

  NS_IMETHOD DeleteSelectionImpl(EDirection aAction,
                                 EStripWrappers aStripWrappers);

  already_AddRefed<Element> DeleteSelectionAndCreateElement(nsIAtom& aTag);

  /**
   * Helper routines for node/parent manipulations.
   */
  nsresult DeleteNode(nsINode* aNode);
  nsresult InsertNode(nsIContent& aNode, nsINode& aParent, int32_t aPosition);
  enum ECloneAttributes { eDontCloneAttributes, eCloneAttributes };
  already_AddRefed<Element> ReplaceContainer(Element* aOldContainer,
                                             nsIAtom* aNodeType,
                                             nsIAtom* aAttribute = nullptr,
                                             const nsAString* aValue = nullptr,
                                             ECloneAttributes aCloneAttributes
                                             = eDontCloneAttributes);
  void CloneAttributes(Element* aDest, Element* aSource);

  nsresult RemoveContainer(nsIContent* aNode);
  already_AddRefed<Element> InsertContainerAbove(nsIContent* aNode,
                                                 nsIAtom* aNodeType,
                                                 nsIAtom* aAttribute = nullptr,
                                                 const nsAString* aValue =
                                                 nullptr);
  nsIContent* SplitNode(nsIContent& aNode, int32_t aOffset,
                        ErrorResult& aResult);
  nsresult JoinNodes(nsINode& aLeftNode, nsINode& aRightNode);
  nsresult MoveNode(nsIContent* aNode, nsINode* aParent, int32_t aOffset);

  nsresult CloneAttribute(nsIAtom* aAttribute, Element* aDestElement,
                          Element* aSourceElement);
  nsresult RemoveAttribute(Element* aElement, nsIAtom* aAttribute);
  virtual nsresult RemoveAttributeOrEquivalent(Element* aElement,
                                               nsIAtom* aAttribute,
                                               bool aSuppressTransaction) = 0;
  nsresult SetAttribute(Element* aElement, nsIAtom* aAttribute,
                        const nsAString& aValue);
  virtual nsresult SetAttributeOrEquivalent(Element* aElement,
                                            nsIAtom* aAttribute,
                                            const nsAString& aValue,
                                            bool aSuppressTransaction) = 0;

  /**
   * Method to replace certain CreateElementNS() calls.
   *
   * @param aTag        Tag you want.
   */
  already_AddRefed<Element> CreateHTMLContent(nsIAtom* aTag);

  /**
   * IME event handlers.
   */
  virtual nsresult BeginIMEComposition(WidgetCompositionEvent* aEvent);
  virtual nsresult UpdateIMEComposition(
                     WidgetCompositionEvent* aCompositionChangeEvet) = 0;
  void EndIMEComposition();

  void SwitchTextDirectionTo(uint32_t aDirection);

protected:
  nsresult DetermineCurrentDirection();
  void FireInputEvent();

  /**
   * Create a transaction for setting aAttribute to aValue on aElement.  Never
   * returns null.
   */
  already_AddRefed<ChangeAttributeTransaction>
    CreateTxnForSetAttribute(Element& aElement, nsIAtom& aAttribute,
                             const nsAString& aValue);

  /**
   * Create a transaction for removing aAttribute on aElement.  Never returns
   * null.
   */
  already_AddRefed<ChangeAttributeTransaction>
    CreateTxnForRemoveAttribute(Element& aElement, nsIAtom& aAttribute);

  /**
   * Create a transaction for creating a new child node of aParent of type aTag.
   */
  already_AddRefed<CreateElementTransaction>
    CreateTxnForCreateElement(nsIAtom& aTag,
                              nsINode& aParent,
                              int32_t aPosition);

  already_AddRefed<Element> CreateNode(nsIAtom* aTag, nsINode* aParent,
                                       int32_t aPosition);

  /**
   * Create a transaction for inserting aNode as a child of aParent.
   */
  already_AddRefed<InsertNodeTransaction>
    CreateTxnForInsertNode(nsIContent& aNode, nsINode& aParent,
                           int32_t aOffset);

  /**
   * Create a transaction for removing aNode from its parent.
   */
  already_AddRefed<DeleteNodeTransaction>
    CreateTxnForDeleteNode(nsINode* aNode);

  /**
   * Create an aggregate transaction for delete selection.  The result may
   * include DeleteNodeTransactions and/or DeleteTextTransactions as its
   * children.
   *
   * @param aAction             The action caused removing the selection.
   * @param aRemovingNode       The node to be removed.
   * @param aOffset             The start offset of the range in aRemovingNode.
   * @param aLength             The length of the range in aRemovingNode.
   * @return                    If it can remove the selection, returns an
   *                            aggregate transaction which has some
   *                            DeleteNodeTransactions and/or
   *                            DeleteTextTransactions as its children.
   */
  already_AddRefed<EditAggregateTransaction>
    CreateTxnForDeleteSelection(EDirection aAction,
                                nsINode** aNode,
                                int32_t* aOffset,
                                int32_t* aLength);

  /**
   * Create a transaction for removing the nodes and/or text in aRange.
   *
   * @param aRangeToDelete      The range to be removed.
   * @param aAction             The action caused removing the range.
   * @param aRemovingNode       The node to be removed.
   * @param aOffset             The start offset of the range in aRemovingNode.
   * @param aLength             The length of the range in aRemovingNode.
   * @return                    The transaction to remove the range.  Its type
   *                            is DeleteNodeTransaction or
   *                            DeleteTextTransaction.
   */
  already_AddRefed<EditTransactionBase>
    CreateTxnForDeleteRange(nsRange* aRangeToDelete,
                            EDirection aAction,
                            nsINode** aRemovingNode,
                            int32_t* aOffset,
                            int32_t* aLength);

  /**
   * Create a transaction for inserting aStringToInsert into aTextNode.  Never
   * returns null.
   */
  already_AddRefed<mozilla::InsertTextTransaction>
    CreateTxnForInsertText(const nsAString& aStringToInsert, Text& aTextNode,
                           int32_t aOffset);

  already_AddRefed<SetTextTransaction>
    CreateTxnForSetText(const nsAString& aString, Text& aTextNode);

  /**
   * Never returns null.
   */
  already_AddRefed<mozilla::CompositionTransaction>
    CreateTxnForComposition(const nsAString& aStringToInsert);

  /**
   * Create a transaction for adding a style sheet.
   */
  already_AddRefed<mozilla::AddStyleSheetTransaction>
    CreateTxnForAddStyleSheet(StyleSheet* aSheet);

  /**
   * Create a transaction for removing a style sheet.
   */
  already_AddRefed<mozilla::RemoveStyleSheetTransaction>
    CreateTxnForRemoveStyleSheet(StyleSheet* aSheet);

  nsresult DeleteText(nsGenericDOMDataNode& aElement,
                      uint32_t aOffset, uint32_t aLength);

  already_AddRefed<DeleteTextTransaction>
    CreateTxnForDeleteText(nsGenericDOMDataNode& aElement,
                           uint32_t aOffset, uint32_t aLength);

  already_AddRefed<DeleteTextTransaction>
    CreateTxnForDeleteCharacter(nsGenericDOMDataNode& aData, uint32_t aOffset,
                                EDirection aDirection);

  already_AddRefed<SplitNodeTransaction>
    CreateTxnForSplitNode(nsIContent& aNode, uint32_t aOffset);

  already_AddRefed<JoinNodeTransaction>
    CreateTxnForJoinNode(nsINode& aLeftNode, nsINode& aRightNode);

  /**
   * This method first deletes the selection, if it's not collapsed.  Then if
   * the selection lies in a CharacterData node, it splits it.  If the
   * selection is at this point collapsed in a CharacterData node, it's
   * adjusted to be collapsed right before or after the node instead (which is
   * always possible, since the node was split).
   */
  nsresult DeleteSelectionAndPrepareToCreateNode();

  /**
   * Called after a transaction is done successfully.
   */
  void DoAfterDoTransaction(nsITransaction *aTxn);

  /**
   * Called after a transaction is undone successfully.
   */

  void DoAfterUndoTransaction();

  /**
   * Called after a transaction is redone successfully.
   */
  void DoAfterRedoTransaction();

  enum TDocumentListenerNotification
  {
    eDocumentCreated,
    eDocumentToBeDestroyed,
    eDocumentStateChanged
  };

  /**
   * Tell the doc state listeners that the doc state has changed.
   */
  nsresult NotifyDocumentListeners(
             TDocumentListenerNotification aNotificationType);

  /**
   * Make the given selection span the entire document.
   */
  virtual nsresult SelectEntireDocument(Selection* aSelection);

  /**
   * Helper method for scrolling the selection into view after
   * an edit operation. aScrollToAnchor should be true if you
   * want to scroll to the point where the selection was started.
   * If false, it attempts to scroll the end of the selection into view.
   *
   * Editor methods *should* call this method instead of the versions
   * in the various selection interfaces, since this version makes sure
   * that the editor's sync/async settings for reflowing, painting, and
   * scrolling match.
   */
  nsresult ScrollSelectionIntoView(bool aScrollToAnchor);

  virtual bool IsBlockNode(nsINode* aNode);

  /**
   * Helper for GetPriorNode() and GetNextNode().
   */
  nsIContent* FindNextLeafNode(nsINode* aCurrentNode,
                               bool aGoForward,
                               bool bNoBlockCrossing);

  virtual nsresult InstallEventListeners();
  virtual void CreateEventListeners();
  virtual void RemoveEventListeners();

  /**
   * Return true if spellchecking should be enabled for this editor.
   */
  bool GetDesiredSpellCheckState();

  bool CanEnableSpellCheck()
  {
    // Check for password/readonly/disabled, which are not spellchecked
    // regardless of DOM. Also, check to see if spell check should be skipped
    // or not.
    return !IsPasswordEditor() && !IsReadonly() && !IsDisabled() &&
           !ShouldSkipSpellCheck();
  }

  /**
   * EnsureComposition() should be called by composition event handlers.  This
   * tries to get the composition for the event and set it to mComposition.
   * However, this may fail because the composition may be committed before
   * the event comes to the editor.
   *
   * @return            true if there is a composition.  Otherwise, for example,
   *                    a composition event handler in web contents moved focus
   *                    for committing the composition, returns false.
   */
  bool EnsureComposition(WidgetCompositionEvent* aCompositionEvent);

  already_AddRefed<nsISelectionController> GetSelectionController();
  nsresult GetSelection(SelectionType aSelectionType,
                        nsISelection** aSelection);

public:
  /**
   * All editor operations which alter the doc should be prefaced
   * with a call to StartOperation, naming the action and direction.
   */
  NS_IMETHOD StartOperation(EditAction opID,
                            nsIEditor::EDirection aDirection);

  /**
   * All editor operations which alter the doc should be followed
   * with a call to EndOperation.
   */
  NS_IMETHOD EndOperation();

  /**
   * Routines for managing the preservation of selection across
   * various editor actions.
   */
  bool ArePreservingSelection();
  void PreserveSelectionAcrossActions(Selection* aSel);
  nsresult RestorePreservedSelection(Selection* aSel);
  void StopPreservingSelection();

  /**
   * SplitNode() creates a new node identical to an existing node, and split
   * the contents between the two nodes
   * @param aExistingRightNode  The node to split.  It will become the new
   *                            node's next sibling.
   * @param aOffset             The offset of aExistingRightNode's
   *                            content|children to do the split at
   * @param aNewLeftNode        The new node resulting from the split, becomes
   *                            aExistingRightNode's previous sibling.
   */
  nsresult SplitNodeImpl(nsIContent& aExistingRightNode,
                         int32_t aOffset,
                         nsIContent& aNewLeftNode);

  /**
   * JoinNodes() takes 2 nodes and merge their content|children.
   * @param aNodeToKeep   The node that will remain after the join.
   * @param aNodeToJoin   The node that will be joined with aNodeToKeep.
   *                      There is no requirement that the two nodes be of the
   *                      same type.
   * @param aParent       The parent of aNodeToKeep
   */
  nsresult JoinNodesImpl(nsINode* aNodeToKeep,
                         nsINode* aNodeToJoin,
                         nsINode* aParent);

  /**
   * Return the offset of aChild in aParent.  Asserts fatally if parent or
   * child is null, or parent is not child's parent.
   */
  static int32_t GetChildOffset(nsIDOMNode* aChild,
                                nsIDOMNode* aParent);

  /**
   * Set outOffset to the offset of aChild in the parent.
   * Returns the parent of aChild.
   */
  static already_AddRefed<nsIDOMNode> GetNodeLocation(nsIDOMNode* aChild,
                                                      int32_t* outOffset);
  static nsINode* GetNodeLocation(nsINode* aChild, int32_t* aOffset);

  /**
   * Returns the number of things inside aNode in the out-param aCount.
   * @param  aNode is the node to get the length of.
   *         If aNode is text, returns number of characters.
   *         If not, returns number of children nodes.
   * @param  aCount [OUT] the result of the above calculation.
   */
  static nsresult GetLengthOfDOMNode(nsIDOMNode *aNode, uint32_t &aCount);

  /**
   * Get the node immediately prior to aCurrentNode.
   * @param aCurrentNode   the node from which we start the search
   * @param aEditableNode  if true, only return an editable node
   * @param aResultNode    [OUT] the node that occurs before aCurrentNode in
   *                             the tree, skipping non-editable nodes if
   *                             aEditableNode is true.  If there is no prior
   *                             node, aResultNode will be nullptr.
   * @param bNoBlockCrossing If true, don't move across "block" nodes,
   *                         whatever that means.
   */
  nsIContent* GetPriorNode(nsINode* aCurrentNode, bool aEditableNode,
                           bool aNoBlockCrossing = false);

  /**
   * And another version that takes a {parent,offset} pair rather than a node.
   */
  nsIContent* GetPriorNode(nsINode* aParentNode,
                           int32_t aOffset,
                           bool aEditableNode,
                           bool aNoBlockCrossing = false);


  /**
   * Get the node immediately after to aCurrentNode.
   * @param aCurrentNode   the node from which we start the search
   * @param aEditableNode  if true, only return an editable node
   * @param aResultNode    [OUT] the node that occurs after aCurrentNode in the
   *                             tree, skipping non-editable nodes if
   *                             aEditableNode is true.  If there is no prior
   *                             node, aResultNode will be nullptr.
   */
  nsIContent* GetNextNode(nsINode* aCurrentNode,
                          bool aEditableNode,
                          bool bNoBlockCrossing = false);

  /**
   * And another version that takes a {parent,offset} pair rather than a node.
   */
  nsIContent* GetNextNode(nsINode* aParentNode,
                          int32_t aOffset,
                          bool aEditableNode,
                          bool aNoBlockCrossing = false);

  /**
   * Helper for GetNextNode() and GetPriorNode().
   */
  nsIContent* FindNode(nsINode* aCurrentNode,
                       bool aGoForward,
                       bool aEditableNode,
                       bool bNoBlockCrossing);
  /**
   * Get the rightmost child of aCurrentNode;
   * return nullptr if aCurrentNode has no children.
   */
  nsIContent* GetRightmostChild(nsINode* aCurrentNode,
                                bool bNoBlockCrossing = false);

  /**
   * Get the leftmost child of aCurrentNode;
   * return nullptr if aCurrentNode has no children.
   */
  nsIContent* GetLeftmostChild(nsINode *aCurrentNode,
                               bool bNoBlockCrossing = false);

  /**
   * Returns true if aNode is of the type implied by aTag.
   */
  static inline bool NodeIsType(nsIDOMNode* aNode, nsIAtom* aTag)
  {
    return GetTag(aNode) == aTag;
  }

  /**
   * Returns true if aParent can contain a child of type aTag.
   */
  bool CanContain(nsINode& aParent, nsIContent& aChild);
  bool CanContainTag(nsINode& aParent, nsIAtom& aTag);
  bool TagCanContain(nsIAtom& aParentTag, nsIContent& aChild);
  virtual bool TagCanContainTag(nsIAtom& aParentTag, nsIAtom& aChildTag);

  /**
   * Returns true if aNode is our root node.
   */
  bool IsRoot(nsIDOMNode* inNode);
  bool IsRoot(nsINode* inNode);
  bool IsEditorRoot(nsINode* aNode);

  /**
   * Returns true if aNode is a descendant of our root node.
   */
  bool IsDescendantOfRoot(nsIDOMNode* inNode);
  bool IsDescendantOfRoot(nsINode* inNode);
  bool IsDescendantOfEditorRoot(nsINode* aNode);

  /**
   * Returns true if aNode is a container.
   */
  virtual bool IsContainer(nsINode* aNode);
  virtual bool IsContainer(nsIDOMNode* aNode);

  /**
   * returns true if aNode is an editable node.
   */
  bool IsEditable(nsIDOMNode* aNode);
  virtual bool IsEditable(nsINode* aNode);

  /**
   * Returns true if aNode is a MozEditorBogus node.
   */
  bool IsMozEditorBogusNode(nsINode* aNode);

  /**
   * Counts number of editable child nodes.
   */
  uint32_t CountEditableChildren(nsINode* aNode);

  /**
   * Find the deep first and last children.
   */
  nsINode* GetFirstEditableNode(nsINode* aRoot);

  /**
   * Returns current composition.
   */
  TextComposition* GetComposition() const;

  /**
   * Returns true if there is composition string and not fixed.
   */
  bool IsIMEComposing() const;

  /**
   * Returns true when inserting text should be a part of current composition.
   */
  bool ShouldHandleIMEComposition() const;

  /**
   * From html rules code - migration in progress.
   */
  static nsresult GetTagString(nsIDOMNode* aNode, nsAString& outString);
  static nsIAtom* GetTag(nsIDOMNode* aNode);

  bool NodesSameType(nsIDOMNode* aNode1, nsIDOMNode* aNode2);
  virtual bool AreNodesSameType(nsIContent* aNode1, nsIContent* aNode2);

  static bool IsTextNode(nsIDOMNode* aNode);
  static bool IsTextNode(nsINode* aNode);

  static nsCOMPtr<nsIDOMNode> GetChildAt(nsIDOMNode* aParent, int32_t aOffset);
  static nsIContent* GetNodeAtRangeOffsetPoint(nsIDOMNode* aParentOrNode,
                                               int32_t aOffset);

  static nsresult GetStartNodeAndOffset(Selection* aSelection,
                                        nsIDOMNode** outStartNode,
                                        int32_t* outStartOffset);
  static nsresult GetStartNodeAndOffset(Selection* aSelection,
                                        nsINode** aStartNode,
                                        int32_t* aStartOffset);
  static nsresult GetEndNodeAndOffset(Selection* aSelection,
                                      nsIDOMNode** outEndNode,
                                      int32_t* outEndOffset);
  static nsresult GetEndNodeAndOffset(Selection* aSelection,
                                      nsINode** aEndNode,
                                      int32_t* aEndOffset);
#if DEBUG_JOE
  static void DumpNode(nsIDOMNode* aNode, int32_t indent = 0);
#endif
  Selection* GetSelection(SelectionType aSelectionType =
                                          SelectionType::eNormal);

  /**
   * Helpers to add a node to the selection.
   * Used by table cell selection methods.
   */
  nsresult CreateRange(nsIDOMNode* aStartParent, int32_t aStartOffset,
                       nsIDOMNode* aEndParent, int32_t aEndOffset,
                       nsRange** aRange);

  /**
   * Creates a range with just the supplied node and appends that to the
   * selection.
   */
  nsresult AppendNodeToSelectionAsRange(nsIDOMNode *aNode);

  /**
   * When you are using AppendNodeToSelectionAsRange(), call this first to
   * start a new selection.
   */
  nsresult ClearSelection();

  nsresult IsPreformatted(nsIDOMNode* aNode, bool* aResult);

  enum class EmptyContainers { no, yes };
  int32_t SplitNodeDeep(nsIContent& aNode, nsIContent& aSplitPointParent,
                        int32_t aSplitPointOffset,
                        EmptyContainers aEmptyContainers =
                          EmptyContainers::yes,
                        nsIContent** outLeftNode = nullptr,
                        nsIContent** outRightNode = nullptr);
  EditorDOMPoint JoinNodeDeep(nsIContent& aLeftNode,
                              nsIContent& aRightNode);

  nsresult GetString(const nsAString& name, nsAString& value);

  void BeginUpdateViewBatch();
  virtual nsresult EndUpdateViewBatch();

  bool GetShouldTxnSetSelection();

  virtual nsresult HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent);

  nsresult HandleInlineSpellCheck(EditAction action,
                                  Selection* aSelection,
                                  nsIDOMNode* previousSelectedNode,
                                  int32_t previousSelectedOffset,
                                  nsIDOMNode* aStartNode,
                                  int32_t aStartOffset,
                                  nsIDOMNode* aEndNode,
                                  int32_t aEndOffset);

  virtual already_AddRefed<dom::EventTarget> GetDOMEventTarget() = 0;

  /**
   * Fast non-refcounting editor root element accessor
   */
  Element* GetRoot();

  /**
   * Likewise, but gets the editor's root instead, which is different for HTML
   * editors.
   */
  virtual Element* GetEditorRoot();

  /**
   * Likewise, but gets the text control element instead of the root for
   * plaintext editors.
   */
  Element* GetExposedRoot();

  /**
   * Accessor methods to flags.
   */
  bool IsPlaintextEditor() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorPlaintextMask) != 0;
  }

  bool IsSingleLineEditor() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorSingleLineMask) != 0;
  }

  bool IsPasswordEditor() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorPasswordMask) != 0;
  }

  bool IsReadonly() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorReadonlyMask) != 0;
  }

  bool IsDisabled() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorDisabledMask) != 0;
  }

  bool IsInputFiltered() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorFilterInputMask) != 0;
  }

  bool IsMailEditor() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorMailMask) != 0;
  }

  bool IsWrapHackEnabled() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorEnableWrapHackMask) != 0;
  }

  bool IsFormWidget() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorWidgetMask) != 0;
  }

  bool NoCSS() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorNoCSSMask) != 0;
  }

  bool IsInteractionAllowed() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorAllowInteraction) != 0;
  }

  bool DontEchoPassword() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorDontEchoPassword) != 0;
  }

  bool ShouldSkipSpellCheck() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorSkipSpellCheck) != 0;
  }

  bool IsTabbable() const
  {
    return IsSingleLineEditor() || IsPasswordEditor() || IsFormWidget() ||
           IsInteractionAllowed();
  }

  bool HasIndependentSelection() const
  {
    return !!mSelConWeak;
  }

  bool IsModifiable() const
  {
    return !IsReadonly();
  }

  /**
   * Get the input event target. This might return null.
   */
  virtual already_AddRefed<nsIContent> GetInputEventTargetContent() = 0;

  /**
   * Get the focused content, if we're focused.  Returns null otherwise.
   */
  virtual already_AddRefed<nsIContent> GetFocusedContent();

  /**
   * Get the focused content for the argument of some IMEStateManager's
   * methods.
   */
  virtual already_AddRefed<nsIContent> GetFocusedContentForIME();

  /**
   * Whether the editor is active on the DOM window.  Note that when this
   * returns true but GetFocusedContent() returns null, it means that this editor was
   * focused when the DOM window was active.
   */
  virtual bool IsActiveInDOMWindow();

  /**
   * Whether the aGUIEvent should be handled by this editor or not.  When this
   * returns false, The aGUIEvent shouldn't be handled on this editor,
   * i.e., The aGUIEvent should be handled by another inner editor or ancestor
   * elements.
   */
  virtual bool IsAcceptableInputEvent(WidgetGUIEvent* aGUIEvent);

  /**
   * FindSelectionRoot() returns a selection root of this editor when aNode
   * gets focus.  aNode must be a content node or a document node.  When the
   * target isn't a part of this editor, returns nullptr.  If this is for
   * designMode, you should set the document node to aNode except that an
   * element in the document has focus.
   */
  virtual already_AddRefed<nsIContent> FindSelectionRoot(nsINode* aNode);

  /**
   * Initializes selection and caret for the editor.  If aEventTarget isn't
   * a host of the editor, i.e., the editor doesn't get focus, this does
   * nothing.
   */
  nsresult InitializeSelection(nsIDOMEventTarget* aFocusEventTarget);

  /**
   * This method has to be called by EditorEventListener::Focus.
   * All actions that have to be done when the editor is focused needs to be
   * added here.
   */
  void OnFocus(nsIDOMEventTarget* aFocusEventTarget);

  /**
   * Used to insert content from a data transfer into the editable area.
   * This is called for each item in the data transfer, with the index of
   * each item passed as aIndex.
   */
  virtual nsresult InsertFromDataTransfer(dom::DataTransfer* aDataTransfer,
                                          int32_t aIndex,
                                          nsIDOMDocument* aSourceDoc,
                                          nsIDOMNode* aDestinationNode,
                                          int32_t aDestOffset,
                                          bool aDoDeleteSelection) = 0;

  virtual nsresult InsertFromDrop(nsIDOMEvent* aDropEvent) = 0;

  /**
   * GetIMESelectionStartOffsetIn() returns the start offset of IME selection in
   * the aTextNode.  If there is no IME selection, returns -1.
   */
  int32_t GetIMESelectionStartOffsetIn(nsINode* aTextNode);

  /**
   * FindBetterInsertionPoint() tries to look for better insertion point which
   * is typically the nearest text node and offset in it.
   */
  void FindBetterInsertionPoint(nsCOMPtr<nsIDOMNode>& aNode,
                                int32_t& aOffset);
  void FindBetterInsertionPoint(nsCOMPtr<nsINode>& aNode,
                                int32_t& aOffset);

  /**
   * HideCaret() hides caret with nsCaret::AddForceHide() or may show carent
   * with nsCaret::RemoveForceHide().  This does NOT set visibility of
   * nsCaret.  Therefore, this is stateless.
   */
  void HideCaret(bool aHide);

protected:
  enum Tristate
  {
    eTriUnset,
    eTriFalse,
    eTriTrue
  };

  // MIME type of the doc we are editing.
  nsCString mContentMIMEType;

  nsCOMPtr<nsIInlineSpellChecker> mInlineSpellChecker;

  RefPtr<nsTransactionManager> mTxnMgr;
  // Cached root node.
  nsCOMPtr<Element> mRootElement;
  // Current IME text node.
  RefPtr<Text> mIMETextNode;
  // The form field as an event receiver.
  nsCOMPtr<dom::EventTarget> mEventTarget;
  nsCOMPtr<nsIDOMEventListener> mEventListener;
  // Weak reference to the nsISelectionController.
  nsWeakPtr mSelConWeak;
  // Weak reference to placeholder for begin/end batch purposes.
  nsWeakPtr mPlaceHolderTxn;
  // Weak reference to the nsIDOMDocument.
  nsWeakPtr mDocWeak;
  // Name of placeholder transaction.
  nsIAtom* mPlaceHolderName;
  // Saved selection state for placeholder transaction batching.
  mozilla::UniquePtr<SelectionState> mSelState;
  // IME composition this is not null between compositionstart and
  // compositionend.
  RefPtr<TextComposition> mComposition;

  // Listens to all low level actions on the doc.
  typedef AutoTArray<OwningNonNull<nsIEditActionListener>, 5>
            AutoActionListenerArray;
  AutoActionListenerArray mActionListeners;
  // Just notify once per high level change.
  typedef AutoTArray<OwningNonNull<nsIEditorObserver>, 3>
            AutoEditorObserverArray;
  AutoEditorObserverArray mEditorObservers;
  // Listen to overall doc state (dirty or not, just created, etc.).
  typedef AutoTArray<OwningNonNull<nsIDocumentStateListener>, 1>
            AutoDocumentStateListenerArray;
  AutoDocumentStateListenerArray mDocStateListeners;

  // Cached selection for AutoSelectionRestorer.
  SelectionState mSavedSel;
  // Utility class object for maintaining preserved ranges.
  RangeUpdater mRangeUpdater;

  // Number of modifications (for undo/redo stack).
  uint32_t mModCount;
  // Behavior flags. See nsIPlaintextEditor.idl for the flags we use.
  uint32_t mFlags;

  int32_t mUpdateCount;

  // Nesting count for batching.
  int32_t mPlaceHolderBatch;
  // The current editor action.
  EditAction mAction;

  // Offset in text node where IME comp string begins.
  uint32_t mIMETextOffset;
  // The Length of the composition string or commit string.  If this is length
  // of commit string, the length is truncated by maxlength attribute.
  uint32_t mIMETextLength;

  // The current direction of editor action.
  EDirection mDirection;
  // -1 = not initialized
  int8_t mDocDirtyState;
  // A Tristate value.
  uint8_t mSpellcheckCheckboxState;

  // Turn off for conservative selection adjustment by transactions.
  bool mShouldTxnSetSelection;
  // Whether PreDestroy has been called.
  bool mDidPreDestroy;
  // Whether PostCreate has been called.
  bool mDidPostCreate;
  bool mDispatchInputEvent;
  // True while the instance is handling an edit action.
  bool mIsInEditAction;
  // Whether caret is hidden forcibly.
  bool mHidingCaret;
  // Whether spellchecker dictionary is initialized after focused.
  bool mSpellCheckerDictionaryUpdated;

  friend bool NSCanUnload(nsISupports* serviceMgr);
  friend class AutoRules;
  friend class AutoSelectionRestorer;
  friend class AutoTransactionsConserveSelection;
  friend class RangeUpdater;
};

} // namespace mozilla

#endif // #ifndef mozilla_EditorBase_h
