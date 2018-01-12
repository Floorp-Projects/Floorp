/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorBase_h
#define mozilla_EditorBase_h

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc.
#include "mozilla/EditorDOMPoint.h"     // for EditorDOMPoint
#include "mozilla/Maybe.h"              // for Maybe
#include "mozilla/OwningNonNull.h"      // for OwningNonNull
#include "mozilla/PresShell.h"          // for PresShell
#include "mozilla/RangeBoundary.h"      // for RawRangeBoundary, RangeBoundary
#include "mozilla/SelectionState.h"     // for RangeUpdater, etc.
#include "mozilla/StyleSheet.h"         // for StyleSheet
#include "mozilla/TextEditRules.h"      // for TextEditRules
#include "mozilla/WeakPtr.h"            // for WeakPtr
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Text.h"
#include "nsCOMPtr.h"                   // for already_AddRefed, nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"                // for nsIDocument
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

class nsAtom;
class nsIContent;
class nsIDOMDocument;
class nsIDOMEvent;
class nsIDOMEventListener;
class nsIDOMEventTarget;
class nsIDOMNode;
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
class nsTransactionManager;

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
class PlaceholderTransaction;
class RemoveStyleSheetTransaction;
class SplitNodeResult;
class SplitNodeTransaction;
class TextComposition;
class TextEditor;
enum class EditAction : int32_t;

namespace dom {
class DataTransfer;
class Element;
class EventTarget;
class Text;
} // namespace dom

namespace widget {
struct IMEState;
} // namespace widget

/**
 * CachedWeakPtr stores a pointer to a class which inherits nsIWeakReference.
 * If the instance of the class has already been destroyed, this returns
 * nullptr.  Otherwise, returns cached pointer.
 * If class T inherits nsISupports a lot, specify Base explicitly for avoiding
 * ambiguous conversion to nsISupports.
 */
template<class T, class Base = nsISupports>
class CachedWeakPtr final
{
public:
  CachedWeakPtr<T, Base>()
    : mCache(nullptr)
  {
  }
  explicit CachedWeakPtr<T, Base>(T* aObject)
  {
    mWeakPtr = do_GetWeakReference(static_cast<Base*>(aObject));
    mCache = aObject;
  }
  explicit CachedWeakPtr<T, Base>(const nsCOMPtr<T>& aOther)
  {
    mWeakPtr = do_GetWeakReference(static_cast<Base*>(aOther.get()));
    mCache = aOther;
  }
  explicit CachedWeakPtr<T, Base>(already_AddRefed<T>& aOther)
  {
    RefPtr<T> other = aOther;
    mWeakPtr = do_GetWeakReference(static_cast<Base*>(other.get()));
    mCache = other;
  }

  CachedWeakPtr<T, Base>& operator=(T* aObject)
  {
    mWeakPtr = do_GetWeakReference(static_cast<Base*>(aObject));
    mCache = aObject;
    return *this;
  }
  CachedWeakPtr<T, Base>& operator=(const nsCOMPtr<T>& aOther)
  {
    mWeakPtr = do_GetWeakReference(static_cast<Base*>(aOther.get()));
    mCache = aOther;
    return *this;
  }
  CachedWeakPtr<T, Base>& operator=(already_AddRefed<T>& aOther)
  {
    RefPtr<T> other = aOther;
    mWeakPtr = do_GetWeakReference(static_cast<Base*>(other.get()));
    mCache = other;
    return *this;
  }

  bool IsAlive() const { return mWeakPtr && mWeakPtr->IsAlive(); }

  explicit operator bool() const { return mWeakPtr; }
  operator T*() const { return get(); }
  T* get() const
  {
    if (mCache && !mWeakPtr->IsAlive()) {
      const_cast<CachedWeakPtr<T, Base>*>(this)->mCache = nullptr;
    }
    return mCache;
  }

private:
  nsWeakPtr mWeakPtr;
  T* MOZ_NON_OWNING_REF mCache;
};

#define kMOZEditorBogusNodeAttrAtom nsGkAtoms::mozeditorbogusnode
#define kMOZEditorBogusNodeValue NS_LITERAL_STRING("TRUE")

/**
 * SplitAtEdges is for EditorBase::SplitNodeDeep(),
 * HTMLEditor::InsertNodeAtPoint()
 */
enum class SplitAtEdges
{
  // EditorBase::SplitNodeDeep() won't split container element nodes at
  // their edges.  I.e., when split point is start or end of container,
  // it won't be split.
  eDoNotCreateEmptyContainer,
  // EditorBase::SplitNodeDeep() always splits containers even if the split
  // point is at edge of a container.  E.g., if split point is start of an
  // inline element, empty inline element is created as a new left node.
  eAllowToCreateEmptyContainer
};

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

protected:
  /**
   * The default destructor. This should suffice. Should this be pure virtual
   * for someone to derive from the EditorBase later? I don't believe so.
   */
  virtual ~EditorBase();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(EditorBase, nsIEditor)

  bool IsInitialized() const { return !!mDocument; }
  already_AddRefed<nsIDOMDocument> GetDOMDocument();
  already_AddRefed<nsIDocument> GetDocument();
  already_AddRefed<nsIPresShell> GetPresShell();
  nsPresContext* GetPresContext()
  {
    RefPtr<nsIPresShell> presShell = GetPresShell();
    return presShell ? presShell->GetPresContext() : nullptr;
  }
  already_AddRefed<nsIWidget> GetWidget();
  nsISelectionController* GetSelectionController() const
  {
    if (mSelectionController) {
      return mSelectionController;
    }
    if (!mDocument) {
      return nullptr;
    }
    nsIPresShell* presShell = mDocument->GetShell();
    if (!presShell) {
      return nullptr;
    }
    nsISelectionController* sc = static_cast<PresShell*>(presShell);
    return sc;
  }
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

  /**
   * InsertTextImpl() inserts aStringToInsert to aPointToInsert or better
   * insertion point around it.  If aPointToInsert isn't in a text node,
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
  virtual nsresult
  InsertTextImpl(nsIDocument& aDocument,
                 const nsAString& aStringToInsert,
                 const EditorRawDOMPoint& aPointToInsert,
                 EditorRawDOMPoint* aPointAfterInsertedString = nullptr);

  nsresult InsertTextIntoTextNodeImpl(const nsAString& aStringToInsert,
                                      Text& aTextNode, int32_t aOffset,
                                      bool aSuppressIME = false);

  nsresult SetTextImpl(Selection& aSelection,
                       const nsAString& aString,
                       Text& aTextNode);

  NS_IMETHOD DeleteSelectionImpl(EDirection aAction,
                                 EStripWrappers aStripWrappers);

  already_AddRefed<Element> DeleteSelectionAndCreateElement(nsAtom& aTag);

  /**
   * Helper routines for node/parent manipulations.
   */
  nsresult DeleteNode(nsINode* aNode);

  /**
   * InsertNode() inserts aContentToInsert before the child specified by
   * aPointToInsert.
   *
   * @param aContentToInsert    The node to be inserted.
   * @param aPointToInsert      The insertion point of aContentToInsert.
   *                            If this refers end of the container, the
   *                            transaction will append the node to the
   *                            container.  Otherwise, will insert the node
   *                            before child node referred by this.
   */
  nsresult InsertNode(nsIContent& aContentToInsert,
                      const EditorRawDOMPoint& aPointToInsert);

  enum ECloneAttributes { eDontCloneAttributes, eCloneAttributes };
  already_AddRefed<Element> ReplaceContainer(Element* aOldContainer,
                                             nsAtom* aNodeType,
                                             nsAtom* aAttribute = nullptr,
                                             const nsAString* aValue = nullptr,
                                             ECloneAttributes aCloneAttributes
                                             = eDontCloneAttributes);
  void CloneAttributes(Element* aDest, Element* aSource);

  nsresult RemoveContainer(nsIContent* aNode);
  already_AddRefed<Element> InsertContainerAbove(nsIContent* aNode,
                                                 nsAtom* aNodeType,
                                                 nsAtom* aAttribute = nullptr,
                                                 const nsAString* aValue =
                                                 nullptr);

  /**
   * SplitNode() creates a transaction to create a new node (left node)
   * identical to an existing node (right node), and split the contents
   * between the same point in both nodes, then, execute the transaction.
   *
   * @param aStartOfRightNode   The point to split.  Its container will be
   *                            the right node, i.e., become the new node's
   *                            next sibling.  And the point will be start
   *                            of the right node.
   * @param aError              If succeed, returns no error.  Otherwise, an
   *                            error.
   */
  already_AddRefed<nsIContent>
  SplitNode(const EditorRawDOMPoint& aStartOfRightNode,
            ErrorResult& aResult);

  nsresult JoinNodes(nsINode& aLeftNode, nsINode& aRightNode);
  nsresult MoveNode(nsIContent* aNode, nsINode* aParent, int32_t aOffset);

  /**
   * MoveAllChildren() moves all children of aContainer to before
   * aPointToInsert.GetChild().
   * See explanation of MoveChildren() for the detail of the behavior.
   *
   * @param aContainer          The container node whose all children should
   *                            be moved.
   * @param aPointToInsert      The insertion point.  The container must not
   *                            be a data node like a text node.
   * @param aError              The result.  If this succeeds to move children,
   *                            returns NS_OK.  Otherwise, an error.
   */
  void MoveAllChildren(nsINode& aContainer,
                       const EditorRawDOMPoint& aPointToInsert,
                       ErrorResult& aError);

  /**
   * MovePreviousSiblings() moves all siblings before aChild (i.e., aChild
   * won't be moved) to before aPointToInsert.GetChild().
   * See explanation of MoveChildren() for the detail of the behavior.
   *
   * @param aChild              The node which is next sibling of the last
   *                            node to be moved.
   * @param aPointToInsert      The insertion point.  The container must not
   *                            be a data node like a text node.
   * @param aError              The result.  If this succeeds to move children,
   *                            returns NS_OK.  Otherwise, an error.
   */
  void MovePreviousSiblings(nsIContent& aChild,
                            const EditorRawDOMPoint& aPointToInsert,
                            ErrorResult& aError);

  /**
   * MoveChildren() moves all children between aFirstChild and aLastChild to
   * before aPointToInsert.GetChild().
   * If some children are moved to different container while this method
   * moves other children, they are just ignored.
   * If the child node referred by aPointToInsert is moved to different
   * container while this method moves children, returns error.
   *
   * @param aFirstChild         The first child which should be moved to
   *                            aPointToInsert.
   * @param aLastChild          The last child which should be moved.  This
   *                            must be a sibling of aFirstChild and it should
   *                            be positioned after aFirstChild in the DOM tree
   *                            order.
   * @param aPointToInsert      The insertion point.  The container must not
   *                            be a data node like a text node.
   * @param aError              The result.  If this succeeds to move children,
   *                            returns NS_OK.  Otherwise, an error.
   */
  void MoveChildren(nsIContent& aFirstChild,
                    nsIContent& aLastChild,
                    const EditorRawDOMPoint& aPointToInsert,
                    ErrorResult& aError);

  nsresult CloneAttribute(nsAtom* aAttribute, Element* aDestElement,
                          Element* aSourceElement);
  nsresult RemoveAttribute(Element* aElement, nsAtom* aAttribute);
  virtual nsresult RemoveAttributeOrEquivalent(Element* aElement,
                                               nsAtom* aAttribute,
                                               bool aSuppressTransaction) = 0;
  nsresult SetAttribute(Element* aElement, nsAtom* aAttribute,
                        const nsAString& aValue);
  virtual nsresult SetAttributeOrEquivalent(Element* aElement,
                                            nsAtom* aAttribute,
                                            const nsAString& aValue,
                                            bool aSuppressTransaction) = 0;

  /**
   * Method to replace certain CreateElementNS() calls.
   *
   * @param aTag        Tag you want.
   */
  already_AddRefed<Element> CreateHTMLContent(nsAtom* aTag);

  /**
   * Creates text node which is marked as "maybe modified frequently".
   */
  static already_AddRefed<nsTextNode> CreateTextNode(nsIDocument& aDocument,
                                                     const nsAString& aData);

  /**
   * IME event handlers.
   */
  virtual nsresult BeginIMEComposition(WidgetCompositionEvent* aEvent);
  virtual nsresult UpdateIMEComposition(
                     WidgetCompositionEvent* aCompositionChangeEvet) = 0;
  void EndIMEComposition();

  /**
   * Commit composition if there is.
   * Note that when there is a composition, this requests to commit composition
   * to native IME.  Therefore, when there is composition, this can do anything.
   * For example, the editor instance, the widget or the process itself may
   * be destroyed.
   */
  nsresult CommitComposition();

  void SwitchTextDirectionTo(uint32_t aDirection);

  RangeUpdater& RangeUpdaterRef() { return mRangeUpdater; }

protected:
  nsresult DetermineCurrentDirection();
  void FireInputEvent();

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
  already_AddRefed<Element> CreateNode(nsAtom* aTag,
                                       const EditorRawDOMPoint& aPointToInsert);

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

  nsresult DeleteText(nsGenericDOMDataNode& aElement,
                      uint32_t aOffset, uint32_t aLength);

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

  // Note that aSelection is optional and can be nullptr.
  nsresult DoTransaction(Selection* aSelection,
                         nsITransaction* aTxn);

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
   * Helper for GetPreviousNodeInternal() and GetNextNodeInternal().
   */
  nsIContent* FindNextLeafNode(nsINode* aCurrentNode,
                               bool aGoForward,
                               bool bNoBlockCrossing);
  nsIContent* FindNode(nsINode* aCurrentNode,
                       bool aGoForward,
                       bool aEditableNode,
                       bool bNoBlockCrossing);

  /**
   * Get the node immediately previous node of aNode.
   * @param atNode               The node from which we start the search.
   * @param aFindEditableNode    If true, only return an editable node.
   * @param aNoBlockCrossing     If true, don't move across "block" nodes,
   *                             whatever that means.
   * @return                     The node that occurs before aNode in
   *                             the tree, skipping non-editable nodes if
   *                             aFindEditableNode is true.  If there is no
   *                             previous node, returns nullptr.
   */
  nsIContent* GetPreviousNodeInternal(nsINode& aNode,
                                      bool aFindEditableNode,
                                      bool aNoBlockCrossing);

  /**
   * And another version that takes a point in DOM tree rather than a node.
   */
  nsIContent* GetPreviousNodeInternal(const EditorRawDOMPoint& aPoint,
                                      bool aFindEditableNode,
                                      bool aNoBlockCrossing);

  /**
   * Get the node immediately next node of aNode.
   * @param aNode                The node from which we start the search.
   * @param aFindEditableNode    If true, only return an editable node.
   * @param aNoBlockCrossing     If true, don't move across "block" nodes,
   *                             whatever that means.
   * @return                     The node that occurs after aNode in the
   *                             tree, skipping non-editable nodes if
   *                             aFindEditableNode is true.  If there is no
   *                             next node, returns nullptr.
   */
  nsIContent* GetNextNodeInternal(nsINode& aNode,
                                  bool aFindEditableNode,
                                  bool bNoBlockCrossing);

  /**
   * And another version that takes a point in DOM tree rather than a node.
   */
  nsIContent* GetNextNodeInternal(const EditorRawDOMPoint& aPoint,
                                  bool aFindEditableNode,
                                  bool aNoBlockCrossing);


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

  nsresult GetSelection(SelectionType aSelectionType,
                        nsISelection** aSelection);

  /**
   * (Begin|End)PlaceholderTransaction() are called by AutoPlaceholderBatch.
   * This set of methods are similar to the (Begin|End)Transaction(), but do
   * not use the transaction managers batching feature.  Instead we use a
   * placeholder transaction to wrap up any further transaction while the
   * batch is open.  The advantage of this is that placeholder transactions
   * can later merge, if needed.  Merging is unavailable between transaction
   * manager batches.
   */
  void BeginPlaceholderTransaction(nsAtom* aTransactionName);
  void EndPlaceholderTransaction();

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
   * SplitNodeImpl() creates a new node (left node) identical to an existing
   * node (right node), and split the contents between the same point in both
   * nodes.
   *
   * @param aStartOfRightNode   The point to split.  Its container will be
   *                            the right node, i.e., become the new node's
   *                            next sibling.  And the point will be start
   *                            of the right node.
   * @param aNewLeftNode        The new node called as left node, so, this
   *                            becomes the container of aPointToSplit's
   *                            previous sibling.
   * @param aError              Must have not already failed.
   *                            If succeed to insert aLeftNode before the
   *                            right node and remove unnecessary contents
   *                            (and collapse selection at end of the left
   *                            node if necessary), returns no error.
   *                            Otherwise, an error.
   */
  void SplitNodeImpl(const EditorDOMPoint& aStartOfRightNode,
                     nsIContent& aNewLeftNode,
                     ErrorResult& aError);

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
   * FYI: aChild must not be being removed from aParent.  In such case, these
   *      methods may return wrong index if aChild doesn't have previous
   *      sibling or next sibling.
   */
  static int32_t GetChildOffset(nsIDOMNode* aChild,
                                nsIDOMNode* aParent);
  static int32_t GetChildOffset(nsINode* aChild,
                                nsINode* aParent);

  /**
   * Set outOffset to the offset of aChild in the parent.
   * Returns the parent of aChild.
   */
  static already_AddRefed<nsIDOMNode> GetNodeLocation(nsIDOMNode* aChild,
                                                      int32_t* outOffset);
  static nsINode* GetNodeLocation(nsINode* aChild, int32_t* aOffset);

  /**
   * Get the previous node.
   */
  nsIContent* GetPreviousNode(const EditorRawDOMPoint& aPoint)
  {
    return GetPreviousNodeInternal(aPoint, false, false);
  }
  nsIContent* GetPreviousEditableNode(const EditorRawDOMPoint& aPoint)
  {
    return GetPreviousNodeInternal(aPoint, true, false);
  }
  nsIContent* GetPreviousNodeInBlock(const EditorRawDOMPoint& aPoint)
  {
    return GetPreviousNodeInternal(aPoint, false, true);
  }
  nsIContent* GetPreviousEditableNodeInBlock(
                const EditorRawDOMPoint& aPoint)
  {
    return GetPreviousNodeInternal(aPoint, true, true);
  }
  nsIContent* GetPreviousNode(nsINode& aNode)
  {
    return GetPreviousNodeInternal(aNode, false, false);
  }
  nsIContent* GetPreviousEditableNode(nsINode& aNode)
  {
    return GetPreviousNodeInternal(aNode, true, false);
  }
  nsIContent* GetPreviousNodeInBlock(nsINode& aNode)
  {
    return GetPreviousNodeInternal(aNode, false, true);
  }
  nsIContent* GetPreviousEditableNodeInBlock(nsINode& aNode)
  {
    return GetPreviousNodeInternal(aNode, true, true);
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
  nsIContent* GetNextNode(const EditorRawDOMPoint& aPoint)
  {
    return GetNextNodeInternal(aPoint, false, false);
  }
  nsIContent* GetNextEditableNode(const EditorRawDOMPoint& aPoint)
  {
    return GetNextNodeInternal(aPoint, true, false);
  }
  nsIContent* GetNextNodeInBlock(const EditorRawDOMPoint& aPoint)
  {
    return GetNextNodeInternal(aPoint, false, true);
  }
  nsIContent* GetNextEditableNodeInBlock(
                const EditorRawDOMPoint& aPoint)
  {
    return GetNextNodeInternal(aPoint, true, true);
  }
  nsIContent* GetNextNode(nsINode& aNode)
  {
    return GetNextNodeInternal(aNode, false, false);
  }
  nsIContent* GetNextEditableNode(nsINode& aNode)
  {
    return GetNextNodeInternal(aNode, true, false);
  }
  nsIContent* GetNextNodeInBlock(nsINode& aNode)
  {
    return GetNextNodeInternal(aNode, false, true);
  }
  nsIContent* GetNextEditableNodeInBlock(nsINode& aNode)
  {
    return GetNextNodeInternal(aNode, true, true);
  }

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
  static inline bool NodeIsType(nsIDOMNode* aNode, nsAtom* aTag)
  {
    return GetTag(aNode) == aTag;
  }

  /**
   * Returns true if aParent can contain a child of type aTag.
   */
  bool CanContain(nsINode& aParent, nsIContent& aChild) const;
  bool CanContainTag(nsINode& aParent, nsAtom& aTag) const;
  bool TagCanContain(nsAtom& aParentTag, nsIContent& aChild) const;
  virtual bool TagCanContainTag(nsAtom& aParentTag, nsAtom& aChildTag) const;

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

  /**
   * returns true if aNode is an editable node.
   */
  bool IsEditable(nsIDOMNode* aNode);
  bool IsEditable(nsINode* aNode)
  {
    NS_ENSURE_TRUE(aNode, false);

    if (!aNode->IsContent() || IsMozEditorBogusNode(aNode) ||
        !IsModifiableNode(aNode)) {
      return false;
    }

    switch (aNode->NodeType()) {
      case nsIDOMNode::ELEMENT_NODE:
        // In HTML editors, if we're dealing with an element, then ask it
        // whether it's editable.
        return mIsHTMLEditorClass ? aNode->IsEditable() : true;
      case nsIDOMNode::TEXT_NODE:
        // Text nodes are considered to be editable by both typed of editors.
        return true;
      default:
        return false;
    }
  }

  /**
   * Returns true if selection is in an editable element and both the range
   * start and the range end are editable.  E.g., even if the selection range
   * includes non-editable elements, returns true when one of common ancestors
   * of the range start and the range end is editable.  Otherwise, false.
   */
  bool IsSelectionEditable();

  /**
   * Returns true if aNode is a MozEditorBogus node.
   */
  bool IsMozEditorBogusNode(nsINode* aNode)
  {
    return aNode && aNode->IsElement() &&
           aNode->AsElement()->AttrValueIs(kNameSpaceID_None,
               kMOZEditorBogusNodeAttrAtom, kMOZEditorBogusNodeValue,
               eCaseMatters);
  }

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
   * Returns number of undo or redo items.  If TransactionManager returns
   * unexpected error, returns -1.
   */
  int32_t NumberOfUndoItems() const;
  int32_t NumberOfRedoItems() const;

  /**
   * From html rules code - migration in progress.
   */
  static nsAtom* GetTag(nsIDOMNode* aNode);

  bool NodesSameType(nsIDOMNode* aNode1, nsIDOMNode* aNode2);
  virtual bool AreNodesSameType(nsIContent* aNode1, nsIContent* aNode2);

  static bool IsTextNode(nsIDOMNode* aNode);
  static bool IsTextNode(nsINode* aNode)
  {
    return aNode->NodeType() == nsIDOMNode::TEXT_NODE;
  }

  /**
   * GetNodeAtRangeOffsetPoint() returns the node at this position in a range,
   * assuming that the container is the node itself if it's a text node, or
   * the node's parent otherwise.
   */
  static nsIContent* GetNodeAtRangeOffsetPoint(nsINode* aContainer,
                                               int32_t aOffset)
  {
    return GetNodeAtRangeOffsetPoint(RawRangeBoundary(aContainer, aOffset));
  }
  static nsIContent* GetNodeAtRangeOffsetPoint(const RawRangeBoundary& aPoint);

  static nsresult GetStartNodeAndOffset(Selection* aSelection,
                                        nsIDOMNode** outStartNode,
                                        int32_t* outStartOffset);
  static nsresult GetStartNodeAndOffset(Selection* aSelection,
                                        nsINode** aStartContainer,
                                        int32_t* aStartOffset);
  static EditorRawDOMPoint GetStartPoint(Selection* aSelection);
  static nsresult GetEndNodeAndOffset(Selection* aSelection,
                                      nsIDOMNode** outEndNode,
                                      int32_t* outEndOffset);
  static nsresult GetEndNodeAndOffset(Selection* aSelection,
                                      nsINode** aEndContainer,
                                      int32_t* aEndOffset);
  static EditorRawDOMPoint GetEndPoint(Selection* aSelection);

  static nsresult GetEndChildNode(Selection* aSelection,
                                  nsIContent** aEndNode);

#if DEBUG_JOE
  static void DumpNode(nsIDOMNode* aNode, int32_t indent = 0);
#endif
  Selection* GetSelection(SelectionType aSelectionType =
                                          SelectionType::eNormal)
  {
    nsISelectionController* sc = GetSelectionController();
    if (!sc) {
      return nullptr;
    }
    Selection* selection = sc->GetDOMSelection(ToRawSelectionType(aSelectionType));
    return selection;
  }

  /**
   * CollapseSelectionToEnd() collapses the selection to the end of the editor.
   */
  nsresult CollapseSelectionToEnd(Selection* aSelection);

  /**
   * Helpers to add a node to the selection.
   * Used by table cell selection methods.
   */
  nsresult CreateRange(nsIDOMNode* aStartContainer, int32_t aStartOffset,
                       nsIDOMNode* aEndContainer, int32_t aEndOffset,
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

  /**
   * SplitNodeDeep() splits aMostAncestorToSplit deeply.
   *
   * @param aMostAncestorToSplit        The most ancestor node which should be
   *                                    split.
   * @param aStartOfDeepestRightNode    The start point of deepest right node.
   *                                    This point must be descendant of
   *                                    aMostAncestorToSplit.
   * @param aSplitAtEdges               Whether the caller allows this to
   *                                    create empty container element when
   *                                    split point is start or end of an
   *                                    element.
   * @return                            SplitPoint() returns split point in
   *                                    aMostAncestorToSplit.  The point must
   *                                    be good to insert something if the
   *                                    caller want to do it.
   */
  SplitNodeResult
  SplitNodeDeep(nsIContent& aMostAncestorToSplit,
                const EditorRawDOMPoint& aDeepestStartOfRightNode,
                SplitAtEdges aSplitAtEdges);

  EditorDOMPoint JoinNodeDeep(nsIContent& aLeftNode,
                              nsIContent& aRightNode);

  nsresult GetString(const nsAString& name, nsAString& value);

  void BeginUpdateViewBatch();
  virtual nsresult EndUpdateViewBatch();

  bool GetShouldTxnSetSelection();

  virtual nsresult HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent);

  nsresult HandleInlineSpellCheck(EditAction action,
                                  Selection* aSelection,
                                  nsINode* previousSelectedNode,
                                  uint32_t previousSelectedOffset,
                                  nsINode* aStartContainer,
                                  uint32_t aStartOffset,
                                  nsINode* aEndContainer,
                                  uint32_t aEndOffset);

  virtual dom::EventTarget* GetDOMEventTarget() = 0;

  /**
   * Fast non-refcounting editor root element accessor
   */
  Element* GetRoot() const { return mRootElement; }

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
  uint32_t Flags() const { return mFlags; }

  nsresult AddFlags(uint32_t aFlags)
  {
    const uint32_t kOldFlags = Flags();
    const uint32_t kNewFlags = (kOldFlags | aFlags);
    if (kNewFlags == kOldFlags) {
      return NS_OK;
    }
    return SetFlags(kNewFlags); // virtual call and may be expensive.
  }
  nsresult RemoveFlags(uint32_t aFlags)
  {
    const uint32_t kOldFlags = Flags();
    const uint32_t kNewFlags = (kOldFlags & ~aFlags);
    if (kNewFlags == kOldFlags) {
      return NS_OK;
    }
    return SetFlags(kNewFlags); // virtual call and may be expensive.
  }
  nsresult AddAndRemoveFlags(uint32_t aAddingFlags, uint32_t aRemovingFlags)
  {
    MOZ_ASSERT(!(aAddingFlags & aRemovingFlags),
               "Same flags are specified both adding and removing");
    const uint32_t kOldFlags = Flags();
    const uint32_t kNewFlags = ((kOldFlags | aAddingFlags) & ~aRemovingFlags);
    if (kNewFlags == kOldFlags) {
      return NS_OK;
    }
    return SetFlags(kNewFlags); // virtual call and may be expensive.
  }

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

  // FYI: Both IsRightToLeft() and IsLeftToRight() may return false if
  //      the editor inherits the content node's direction.
  bool IsRightToLeft() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorRightToLeft) != 0;
  }
  bool IsLeftToRight() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorLeftToRight) != 0;
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
    return !!mSelectionController;
  }

  bool IsModifiable() const
  {
    return !IsReadonly();
  }

  /**
   * IsInEditAction() return true while the instance is handling an edit action.
   * Otherwise, false.
   */
  bool IsInEditAction() const { return mIsInEditAction; }

  /**
   * IsSuppressingDispatchingInputEvent() returns true if the editor stops
   * dispatching input event.  Otherwise, false.
   */
  bool IsSuppressingDispatchingInputEvent() const
  {
    return !mDispatchInputEvent;
  }

  bool Destroyed() const
  {
    return mDidPreDestroy;
  }

  /**
   * GetTransactionManager() returns transaction manager associated with the
   * editor.  This may return nullptr if undo/redo hasn't been enabled.
   */
  already_AddRefed<nsITransactionManager> GetTransactionManager() const;

  /**
   * Get the input event target. This might return null.
   */
  virtual already_AddRefed<nsIContent> GetInputEventTargetContent() = 0;

  /**
   * Get the focused content, if we're focused.  Returns null otherwise.
   */
  virtual nsIContent* GetFocusedContent();

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
   *
   * @param aPoint      Insertion point which the callers found.
   * @return            Better insertion point if there is.  If not returns
   *                    same point as aPoint.
   */
  EditorRawDOMPoint FindBetterInsertionPoint(const EditorRawDOMPoint& aPoint);

  /**
   * HideCaret() hides caret with nsCaret::AddForceHide() or may show carent
   * with nsCaret::RemoveForceHide().  This does NOT set visibility of
   * nsCaret.  Therefore, this is stateless.
   */
  void HideCaret(bool aHide);

private:
  nsCOMPtr<nsISelectionController> mSelectionController;
  nsCOMPtr<nsIDocument> mDocument;

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
  // The form field as an event receiver.
  nsCOMPtr<dom::EventTarget> mEventTarget;
  nsCOMPtr<nsIDOMEventListener> mEventListener;
  // Strong reference to placeholder for begin/end batch purposes.
  RefPtr<PlaceholderTransaction> mPlaceholderTransaction;
  // Name of placeholder transaction.
  nsAtom* mPlaceholderName;
  // Saved selection state for placeholder transaction batching.
  mozilla::Maybe<SelectionState> mSelState;
  // IME composition this is not null between compositionstart and
  // compositionend.
  RefPtr<TextComposition> mComposition;

  RefPtr<TextEditRules> mRules;

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
  int32_t mPlaceholderBatch;
  // The current editor action.
  EditAction mAction;

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
  // Whether we are an HTML editor class.
  bool mIsHTMLEditorClass;

  friend bool NSCanUnload(nsISupports* serviceMgr);
  friend class AutoPlaceholderBatch;
  friend class AutoRules;
  friend class AutoSelectionRestorer;
  friend class AutoTransactionsConserveSelection;
  friend class RangeUpdater;
  friend class nsIEditor;
};

} // namespace mozilla

mozilla::EditorBase*
nsIEditor::AsEditorBase()
{
  return static_cast<mozilla::EditorBase*>(this);
}

const mozilla::EditorBase*
nsIEditor::AsEditorBase() const
{
  return static_cast<const mozilla::EditorBase*>(this);
}

#endif // #ifndef mozilla_EditorBase_h
