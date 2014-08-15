/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __editor_h__
#define __editor_h__

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc.
#include "mozilla/TypedEnum.h"          // for MOZ_BEGIN_ENUM_CLASS, etc.
#include "mozilla/dom/Text.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMArray.h"                 // for nsCOMArray
#include "nsCOMPtr.h"                   // for already_AddRefed, nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsEditProperty.h"             // for nsEditProperty, etc
#include "nsIEditor.h"                  // for nsIEditor::EDirection, etc
#include "nsIEditorIMESupport.h"        // for NS_DECL_NSIEDITORIMESUPPORT, etc
#include "nsIObserver.h"                // for NS_DECL_NSIOBSERVER, etc
#include "nsIPhonetic.h"                // for NS_DECL_NSIPHONETIC, etc
#include "nsIPlaintextEditor.h"         // for nsIPlaintextEditor, etc
#include "nsISupportsImpl.h"            // for nsEditor::Release, etc
#include "nsIWeakReferenceUtils.h"      // for nsWeakPtr
#include "nsLiteralString.h"            // for NS_LITERAL_STRING
#include "nsSelectionState.h"           // for nsRangeUpdater, etc
#include "nsString.h"                   // for nsCString
#include "nsWeakReference.h"            // for nsSupportsWeakReference
#include "nscore.h"                     // for nsresult, nsAString, etc

class AddStyleSheetTxn;
class ChangeAttributeTxn;
class CreateElementTxn;
class DeleteNodeTxn;
class DeleteTextTxn;
class EditAggregateTxn;
class IMETextTxn;
class InsertElementTxn;
class InsertTextTxn;
class JoinElementTxn;
class RemoveStyleSheetTxn;
class SplitElementTxn;
class nsIAtom;
class nsIContent;
class nsIDOMCharacterData;
class nsIDOMDataTransfer;
class nsIDOMDocument;
class nsIDOMElement;
class nsIDOMEvent;
class nsIDOMEventListener;
class nsIDOMEventTarget;
class nsIDOMKeyEvent;
class nsIDOMNode;
class nsIDOMRange;
class nsIDocument;
class nsIDocumentStateListener;
class nsIEditActionListener;
class nsIEditorObserver;
class nsIInlineSpellChecker;
class nsINode;
class nsIPresShell;
class nsISelection;
class nsISupports;
class nsITransaction;
class nsIWidget;
class nsRange;
class nsString;
class nsTransactionManager;

namespace mozilla {
class CSSStyleSheet;
class ErrorResult;
class TextComposition;

namespace dom {
class DataTransfer;
class Element;
class EventTarget;
class Selection;
class Text;
}  // namespace dom
}  // namespace mozilla

namespace mozilla {
namespace widget {
struct IMEState;
} // namespace widget
} // namespace mozilla

#define kMOZEditorBogusNodeAttrAtom nsEditProperty::mozEditorBogusNode
#define kMOZEditorBogusNodeValue NS_LITERAL_STRING("TRUE")

// This is int32_t instead of int16_t because nsIInlineSpellChecker.idl's
// spellCheckAfterEditorChange is defined to take it as a long.
MOZ_BEGIN_ENUM_CLASS(EditAction, int32_t)
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
MOZ_END_ENUM_CLASS(EditAction)

inline bool operator!(const EditAction& aOp)
{
  return aOp == EditAction::none;
}

/** implementation of an editor object.  it will be the controller/focal point 
 *  for the main editor services. i.e. the GUIManager, publishing, transaction 
 *  manager, event interfaces. the idea for the event interfaces is to have them 
 *  delegate the actual commands to the editor independent of the XPFE implementation.
 */
class nsEditor : public nsIEditor,
                 public nsIEditorIMESupport,
                 public nsSupportsWeakReference,
                 public nsIObserver,
                 public nsIPhonetic
{
public:

  enum IterDirection
  {
    kIterForward,
    kIterBackward
  };

  /** The default constructor. This should suffice. the setting of the interfaces is done
   *  after the construction of the editor class.
   */
  nsEditor();

protected:
  /** The default destructor. This should suffice. Should this be pure virtual 
   *  for someone to derive from the nsEditor later? I don't believe so.
   */
  virtual ~nsEditor();

public:
//Interfaces for addref and release and queryinterface
//NOTE: Use   NS_DECL_ISUPPORTS_INHERITED in any class inherited from nsEditor
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsEditor,
                                           nsIEditor)

  /* ------------ utility methods   -------------- */
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

  /* ------------ nsIEditor methods -------------- */
  NS_DECL_NSIEDITOR

  /* ------------ nsIEditorIMESupport methods -------------- */
  NS_DECL_NSIEDITORIMESUPPORT

  /* ------------ nsIObserver methods -------------- */
  NS_DECL_NSIOBSERVER

  // nsIPhonetic
  NS_DECL_NSIPHONETIC

public:

  nsresult MarkNodeDirty(nsINode* aNode);
  virtual bool IsModifiableNode(nsINode *aNode);

  NS_IMETHOD InsertTextImpl(const nsAString& aStringToInsert, 
                               nsCOMPtr<nsIDOMNode> *aInOutNode, 
                               int32_t *aInOutOffset,
                               nsIDOMDocument *aDoc);
  nsresult InsertTextIntoTextNodeImpl(const nsAString& aStringToInsert,
                                      nsINode* aTextNode,
                                      int32_t aOffset,
                                      bool aSuppressIME = false);
  nsresult InsertTextIntoTextNodeImpl(const nsAString& aStringToInsert, 
                                      nsIDOMCharacterData *aTextNode, 
                                      int32_t aOffset,
                                      bool aSuppressIME = false);
  NS_IMETHOD DeleteSelectionImpl(EDirection aAction,
                                 EStripWrappers aStripWrappers);
  NS_IMETHOD DeleteSelectionAndCreateNode(const nsAString& aTag,
                                           nsIDOMNode ** aNewNode);

  /* helper routines for node/parent manipulations */
  nsresult DeleteNode(nsINode* aNode);
  nsresult InsertNode(nsIContent* aContent, nsINode* aParent,
                      int32_t aPosition);
  nsresult ReplaceContainer(nsINode* inNode,
                            mozilla::dom::Element** outNode,
                            const nsAString& aNodeType,
                            const nsAString* aAttribute = nullptr,
                            const nsAString* aValue = nullptr,
                            bool aCloneAttributes = false);
  nsresult ReplaceContainer(nsIDOMNode *inNode, 
                            nsCOMPtr<nsIDOMNode> *outNode, 
                            const nsAString &aNodeType,
                            const nsAString *aAttribute = nullptr,
                            const nsAString *aValue = nullptr,
                            bool aCloneAttributes = false);

  nsresult RemoveContainer(nsINode* aNode);
  nsresult RemoveContainer(nsIDOMNode *inNode);
  nsresult InsertContainerAbove(nsIContent* aNode,
                                mozilla::dom::Element** aOutNode,
                                const nsAString& aNodeType,
                                const nsAString* aAttribute = nullptr,
                                const nsAString* aValue = nullptr);
  nsresult InsertContainerAbove(nsIDOMNode *inNode, 
                                nsCOMPtr<nsIDOMNode> *outNode, 
                                const nsAString &aNodeType,
                                const nsAString *aAttribute = nullptr,
                                const nsAString *aValue = nullptr);
  nsresult JoinNodes(nsINode* aNodeToKeep, nsIContent* aNodeToMove);
  nsresult MoveNode(nsINode* aNode, nsINode* aParent, int32_t aOffset);
  nsresult MoveNode(nsIDOMNode *aNode, nsIDOMNode *aParent, int32_t aOffset);

  /* Method to replace certain CreateElementNS() calls. 
     Arguments:
      nsString& aTag          - tag you want
  */
  already_AddRefed<mozilla::dom::Element>
    CreateHTMLContent(const nsAString& aTag, mozilla::ErrorResult& rv);

  // IME event handlers
  virtual nsresult BeginIMEComposition(mozilla::WidgetCompositionEvent* aEvent);
  virtual nsresult UpdateIMEComposition(nsIDOMEvent* aDOMTextEvent) = 0;
  void EndIMEComposition();

  void SwitchTextDirectionTo(uint32_t aDirection);

protected:
  nsresult DetermineCurrentDirection();
  void FireInputEvent();

  /** create a transaction for setting aAttribute to aValue on aElement
    */
  NS_IMETHOD CreateTxnForSetAttribute(nsIDOMElement *aElement,
                                      const nsAString &  aAttribute,
                                      const nsAString &  aValue,
                                      ChangeAttributeTxn ** aTxn);

  /** create a transaction for removing aAttribute on aElement
    */
  NS_IMETHOD CreateTxnForRemoveAttribute(nsIDOMElement *aElement,
                                         const nsAString &  aAttribute,
                                         ChangeAttributeTxn ** aTxn);

  /** create a transaction for creating a new child node of aParent of type aTag.
    */
  NS_IMETHOD CreateTxnForCreateElement(const nsAString & aTag,
                                       nsIDOMNode      *aParent,
                                       int32_t         aPosition,
                                       CreateElementTxn ** aTxn);

  /** create a transaction for inserting aNode as a child of aParent.
    */
  NS_IMETHOD CreateTxnForInsertElement(nsIDOMNode * aNode,
                                       nsIDOMNode * aParent,
                                       int32_t      aOffset,
                                       InsertElementTxn ** aTxn);

  /** create a transaction for removing aNode from its parent.
    */
  nsresult CreateTxnForDeleteNode(nsINode* aNode, DeleteNodeTxn** aTxn);


  nsresult CreateTxnForDeleteSelection(EDirection aAction,
                                       EditAggregateTxn** aTxn,
                                       nsINode** aNode,
                                       int32_t* aOffset,
                                       int32_t* aLength);

  nsresult CreateTxnForDeleteInsertionPoint(nsRange* aRange, 
                                            EDirection aAction, 
                                            EditAggregateTxn* aTxn,
                                            nsINode** aNode,
                                            int32_t* aOffset,
                                            int32_t* aLength);


  /** create a transaction for inserting aStringToInsert into aTextNode
    * if aTextNode is null, the string is inserted at the current selection.
    */
  NS_IMETHOD CreateTxnForInsertText(const nsAString & aStringToInsert,
                                    nsIDOMCharacterData *aTextNode,
                                    int32_t aOffset,
                                    InsertTextTxn ** aTxn);

  NS_IMETHOD CreateTxnForIMEText(const nsAString & aStringToInsert,
                                 IMETextTxn ** aTxn);

  /** create a transaction for adding a style sheet
    */
  NS_IMETHOD CreateTxnForAddStyleSheet(mozilla::CSSStyleSheet* aSheet,
                                       AddStyleSheetTxn* *aTxn);

  /** create a transaction for removing a style sheet
    */
  NS_IMETHOD CreateTxnForRemoveStyleSheet(mozilla::CSSStyleSheet* aSheet,
                                          RemoveStyleSheetTxn* *aTxn);
  
  NS_IMETHOD DeleteText(nsIDOMCharacterData *aElement,
                        uint32_t             aOffset,
                        uint32_t             aLength);

  inline nsresult DeleteText(mozilla::dom::Text* aText, uint32_t aOffset,
                             uint32_t aLength)
  {
    return DeleteText(static_cast<nsIDOMCharacterData*>(GetAsDOMNode(aText)),
                      aOffset, aLength);
  }

//  NS_IMETHOD DeleteRange(nsIDOMRange *aRange);

  nsresult CreateTxnForDeleteText(nsIDOMCharacterData* aElement,
                                  uint32_t             aOffset,
                                  uint32_t             aLength,
                                  DeleteTextTxn**      aTxn);

  nsresult CreateTxnForDeleteCharacter(nsIDOMCharacterData* aData,
                                       uint32_t             aOffset,
                                       EDirection           aDirection,
                                       DeleteTextTxn**      aTxn);
	
  NS_IMETHOD CreateTxnForSplitNode(nsIDOMNode *aNode,
                                   uint32_t    aOffset,
                                   SplitElementTxn **aTxn);

  NS_IMETHOD CreateTxnForJoinNode(nsIDOMNode  *aLeftNode,
                                  nsIDOMNode  *aRightNode,
                                  JoinElementTxn **aTxn);

  /**
   * This method first deletes the selection, if it's not collapsed.  Then if
   * the selection lies in a CharacterData node, it splits it.  If the
   * selection is at this point collapsed in a CharacterData node, it's
   * adjusted to be collapsed right before or after the node instead (which is
   * always possible, since the node was split).
   */
  nsresult DeleteSelectionAndPrepareToCreateNode();


  // called after a transaction is done successfully
  void DoAfterDoTransaction(nsITransaction *aTxn);
  // called after a transaction is undone successfully
  void DoAfterUndoTransaction();
  // called after a transaction is redone successfully
  void DoAfterRedoTransaction();

  typedef enum {
    eDocumentCreated,
    eDocumentToBeDestroyed,
    eDocumentStateChanged
  } TDocumentListenerNotification;
  
  // tell the doc state listeners that the doc state has changed
  NS_IMETHOD NotifyDocumentListeners(TDocumentListenerNotification aNotificationType);
  
  /** make the given selection span the entire document */
  NS_IMETHOD SelectEntireDocument(nsISelection *aSelection);

  /** helper method for scrolling the selection into view after
   *  an edit operation. aScrollToAnchor should be true if you
   *  want to scroll to the point where the selection was started.
   *  If false, it attempts to scroll the end of the selection into view.
   *
   *  Editor methods *should* call this method instead of the versions
   *  in the various selection interfaces, since this version makes sure
   *  that the editor's sync/async settings for reflowing, painting, and
   *  scrolling match.
   */
  NS_IMETHOD ScrollSelectionIntoView(bool aScrollToAnchor);

  // Convenience method; forwards to IsBlockNode(nsINode*).
  bool IsBlockNode(nsIDOMNode* aNode);
  // stub.  see comment in source.                     
  virtual bool IsBlockNode(nsINode* aNode);
  
  // helper for GetPriorNode and GetNextNode
  nsIContent* FindNextLeafNode(nsINode  *aCurrentNode,
                               bool      aGoForward,
                               bool      bNoBlockCrossing);

  // install the event listeners for the editor 
  virtual nsresult InstallEventListeners();

  virtual void CreateEventListeners();

  // unregister and release our event listeners
  virtual void RemoveEventListeners();

  /**
   * Return true if spellchecking should be enabled for this editor.
   */
  bool GetDesiredSpellCheckState();

  bool CanEnableSpellCheck()
  {
    // Check for password/readonly/disabled, which are not spellchecked
    // regardless of DOM. Also, check to see if spell check should be skipped or not.
    return !IsPasswordEditor() && !IsReadonly() && !IsDisabled() && !ShouldSkipSpellCheck();
  }

  /**
   * EnsureComposition() should be composition event handlers or text event
   * handler.  This tries to get the composition for the event and set it to
   * mComposition.
   */
  void EnsureComposition(mozilla::WidgetGUIEvent* aEvent);

public:

  /** All editor operations which alter the doc should be prefaced
   *  with a call to StartOperation, naming the action and direction */
  NS_IMETHOD StartOperation(EditAction opID,
                            nsIEditor::EDirection aDirection);

  /** All editor operations which alter the doc should be followed
   *  with a call to EndOperation */
  NS_IMETHOD EndOperation();

  /** routines for managing the preservation of selection across 
   *  various editor actions */
  bool     ArePreservingSelection();
  void     PreserveSelectionAcrossActions(mozilla::dom::Selection* aSel);
  nsresult RestorePreservedSelection(nsISelection *aSel);
  void     StopPreservingSelection();

  /** 
   * SplitNode() creates a new node identical to an existing node, and split the contents between the two nodes
   * @param aExistingRightNode   the node to split.  It will become the new node's next sibling.
   * @param aOffset              the offset of aExistingRightNode's content|children to do the split at
   * @param aNewLeftNode         [OUT] the new node resulting from the split, becomes aExistingRightNode's previous sibling.
   * @param aParent              the parent of aExistingRightNode
   */
  nsresult SplitNodeImpl(nsIDOMNode *aExistingRightNode,
                         int32_t     aOffset,
                         nsIDOMNode *aNewLeftNode,
                         nsIDOMNode *aParent);

  /** 
   * JoinNodes() takes 2 nodes and merge their content|children.
   * @param aNodeToKeep   The node that will remain after the join.
   * @param aNodeToJoin   The node that will be joined with aNodeToKeep.
   *                      There is no requirement that the two nodes be of the same type.
   * @param aParent       The parent of aNodeToKeep
   */
  nsresult JoinNodesImpl(nsINode* aNodeToKeep,
                         nsINode* aNodeToJoin,
                         nsINode* aParent);

  /**
   * Return the offset of aChild in aParent.  Asserts fatally if parent or
   * child is null, or parent is not child's parent.
   */
  static int32_t GetChildOffset(nsIDOMNode *aChild,
                                nsIDOMNode *aParent);

  /**
   *  Set outOffset to the offset of aChild in the parent.
   *  Returns the parent of aChild.
   */
  static already_AddRefed<nsIDOMNode> GetNodeLocation(nsIDOMNode* aChild,
                                                      int32_t* outOffset);
  static nsINode* GetNodeLocation(nsINode* aChild, int32_t* aOffset);

  /** returns the number of things inside aNode in the out-param aCount.  
    * @param  aNode is the node to get the length of.  
    *         If aNode is text, returns number of characters. 
    *         If not, returns number of children nodes.
    * @param  aCount [OUT] the result of the above calculation.
    */
  static nsresult GetLengthOfDOMNode(nsIDOMNode *aNode, uint32_t &aCount);

  /** get the node immediately prior to aCurrentNode
    * @param aCurrentNode   the node from which we start the search
    * @param aEditableNode  if true, only return an editable node
    * @param aResultNode    [OUT] the node that occurs before aCurrentNode in the tree,
    *                       skipping non-editable nodes if aEditableNode is true.
    *                       If there is no prior node, aResultNode will be nullptr.
    * @param bNoBlockCrossing If true, don't move across "block" nodes, whatever that means.
    */
  nsresult GetPriorNode(nsIDOMNode  *aCurrentNode, 
                        bool         aEditableNode,
                        nsCOMPtr<nsIDOMNode> *aResultNode,
                        bool         bNoBlockCrossing = false);
  nsIContent* GetPriorNode(nsINode* aCurrentNode, bool aEditableNode,
                           bool aNoBlockCrossing = false);

  // and another version that takes a {parent,offset} pair rather than a node
  nsresult GetPriorNode(nsIDOMNode  *aParentNode, 
                        int32_t      aOffset, 
                        bool         aEditableNode, 
                        nsCOMPtr<nsIDOMNode> *aResultNode,
                        bool         bNoBlockCrossing = false);
  nsIContent* GetPriorNode(nsINode* aParentNode,
                           int32_t aOffset,
                           bool aEditableNode,
                           bool aNoBlockCrossing = false);


  /** get the node immediately after to aCurrentNode
    * @param aCurrentNode   the node from which we start the search
    * @param aEditableNode  if true, only return an editable node
    * @param aResultNode    [OUT] the node that occurs after aCurrentNode in the tree,
    *                       skipping non-editable nodes if aEditableNode is true.
    *                       If there is no prior node, aResultNode will be nullptr.
    */
  nsresult GetNextNode(nsIDOMNode  *aCurrentNode, 
                       bool         aEditableNode,
                       nsCOMPtr<nsIDOMNode> *aResultNode,
                       bool         bNoBlockCrossing = false);
  nsIContent* GetNextNode(nsINode* aCurrentNode,
                          bool aEditableNode,
                          bool bNoBlockCrossing = false);

  // and another version that takes a {parent,offset} pair rather than a node
  nsresult GetNextNode(nsIDOMNode  *aParentNode, 
                       int32_t      aOffset, 
                       bool         aEditableNode, 
                       nsCOMPtr<nsIDOMNode> *aResultNode,
                       bool         bNoBlockCrossing = false);
  nsIContent* GetNextNode(nsINode* aParentNode,
                          int32_t aOffset,
                          bool aEditableNode,
                          bool aNoBlockCrossing = false);

  // Helper for GetNextNode and GetPriorNode
  nsIContent* FindNode(nsINode *aCurrentNode,
                       bool     aGoForward,
                       bool     aEditableNode,
                       bool     bNoBlockCrossing);
  /**
   * Get the rightmost child of aCurrentNode;
   * return nullptr if aCurrentNode has no children.
   */
  nsIDOMNode* GetRightmostChild(nsIDOMNode* aCurrentNode,
                                bool bNoBlockCrossing = false);
  nsIContent* GetRightmostChild(nsINode *aCurrentNode,
                                bool     bNoBlockCrossing = false);

  /**
   * Get the leftmost child of aCurrentNode;
   * return nullptr if aCurrentNode has no children.
   */
  nsIDOMNode* GetLeftmostChild(nsIDOMNode* aCurrentNode,
                               bool bNoBlockCrossing = false);
  nsIContent* GetLeftmostChild(nsINode *aCurrentNode,
                               bool     bNoBlockCrossing = false);

  /** returns true if aNode is of the type implied by aTag */
  static inline bool NodeIsType(nsIDOMNode *aNode, nsIAtom *aTag)
  {
    return GetTag(aNode) == aTag;
  }

  /** returns true if aParent can contain a child of type aTag */
  bool CanContain(nsIDOMNode* aParent, nsIDOMNode* aChild);
  bool CanContainTag(nsIDOMNode* aParent, nsIAtom* aTag);
  bool TagCanContain(nsIAtom* aParentTag, nsIDOMNode* aChild);
  virtual bool TagCanContainTag(nsIAtom* aParentTag, nsIAtom* aChildTag);

  /** returns true if aNode is our root node */
  bool IsRoot(nsIDOMNode* inNode);
  bool IsRoot(nsINode* inNode);
  bool IsEditorRoot(nsINode* aNode);

  /** returns true if aNode is a descendant of our root node */
  bool IsDescendantOfRoot(nsIDOMNode* inNode);
  bool IsDescendantOfRoot(nsINode* inNode);
  bool IsDescendantOfEditorRoot(nsIDOMNode* aNode);
  bool IsDescendantOfEditorRoot(nsINode* aNode);

  /** returns true if aNode is a container */
  virtual bool IsContainer(nsINode* aNode);
  virtual bool IsContainer(nsIDOMNode* aNode);

  /** returns true if aNode is an editable node */
  bool IsEditable(nsIDOMNode *aNode);
  virtual bool IsEditable(nsINode* aNode);

  /** returns true if aNode is a MozEditorBogus node */
  bool IsMozEditorBogusNode(nsINode* aNode);

  /** counts number of editable child nodes */
  uint32_t CountEditableChildren(nsINode* aNode);
  
  /** Find the deep first and last children. */
  nsINode* GetFirstEditableNode(nsINode* aRoot);

  /**
   * Returns current composition.
   */
  mozilla::TextComposition* GetComposition() const;
  /**
   * Returns true if there is composition string and not fixed.
   */
  bool IsIMEComposing() const;

  /** from html rules code - migration in progress */
  static nsresult GetTagString(nsIDOMNode *aNode, nsAString& outString);
  static nsIAtom *GetTag(nsIDOMNode *aNode);

  bool NodesSameType(nsIDOMNode *aNode1, nsIDOMNode *aNode2);
  virtual bool AreNodesSameType(nsIContent* aNode1, nsIContent* aNode2);

  static bool IsTextNode(nsIDOMNode *aNode);
  static bool IsTextNode(nsINode *aNode);
  
  static nsCOMPtr<nsIDOMNode> GetChildAt(nsIDOMNode *aParent, int32_t aOffset);
  static nsCOMPtr<nsIDOMNode> GetNodeAtRangeOffsetPoint(nsIDOMNode* aParentOrNode, int32_t aOffset);

  static nsresult GetStartNodeAndOffset(nsISelection *aSelection, nsIDOMNode **outStartNode, int32_t *outStartOffset);
  static nsresult GetStartNodeAndOffset(mozilla::dom::Selection* aSelection,
                                        nsINode** aStartNode,
                                        int32_t* aStartOffset);
  static nsresult GetEndNodeAndOffset(nsISelection *aSelection, nsIDOMNode **outEndNode, int32_t *outEndOffset);
  static nsresult GetEndNodeAndOffset(mozilla::dom::Selection* aSelection,
                                      nsINode** aEndNode,
                                      int32_t* aEndOffset);
#if DEBUG_JOE
  static void DumpNode(nsIDOMNode *aNode, int32_t indent=0);
#endif
  mozilla::dom::Selection* GetSelection();

  // Helpers to add a node to the selection. 
  // Used by table cell selection methods
  nsresult CreateRange(nsIDOMNode *aStartParent, int32_t aStartOffset,
                       nsIDOMNode *aEndParent, int32_t aEndOffset,
                       nsIDOMRange **aRange);

  // Creates a range with just the supplied node and appends that to the selection
  nsresult AppendNodeToSelectionAsRange(nsIDOMNode *aNode);
  // When you are using AppendNodeToSelectionAsRange, call this first to start a new selection
  nsresult ClearSelection();

  nsresult IsPreformatted(nsIDOMNode *aNode, bool *aResult);

  nsresult SplitNodeDeep(nsIDOMNode *aNode, 
                         nsIDOMNode *aSplitPointParent, 
                         int32_t aSplitPointOffset,
                         int32_t *outOffset,
                         bool    aNoEmptyContainers = false,
                         nsCOMPtr<nsIDOMNode> *outLeftNode = 0,
                         nsCOMPtr<nsIDOMNode> *outRightNode = 0);
  nsresult JoinNodeDeep(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsCOMPtr<nsIDOMNode> *aOutJoinNode, int32_t *outOffset); 

  nsresult GetString(const nsAString& name, nsAString& value);

  void BeginUpdateViewBatch(void);
  virtual nsresult EndUpdateViewBatch(void);

  bool GetShouldTxnSetSelection();

  virtual nsresult HandleKeyPressEvent(nsIDOMKeyEvent* aKeyEvent);

  nsresult HandleInlineSpellCheck(EditAction action,
                                    nsISelection *aSelection,
                                    nsIDOMNode *previousSelectedNode,
                                    int32_t previousSelectedOffset,
                                    nsIDOMNode *aStartNode,
                                    int32_t aStartOffset,
                                    nsIDOMNode *aEndNode,
                                    int32_t aEndOffset);

  virtual already_AddRefed<mozilla::dom::EventTarget> GetDOMEventTarget() = 0;

  // Fast non-refcounting editor root element accessor
  mozilla::dom::Element *GetRoot();

  // Likewise, but gets the editor's root instead, which is different for HTML
  // editors
  virtual mozilla::dom::Element* GetEditorRoot();

  // Likewise, but gets the text control element instead of the root for
  // plaintext editors
  mozilla::dom::Element* GetExposedRoot();

  // Accessor methods to flags
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

  // Get the input event target. This might return null.
  virtual already_AddRefed<nsIContent> GetInputEventTargetContent() = 0;

  // Get the focused content, if we're focused.  Returns null otherwise.
  virtual already_AddRefed<nsIContent> GetFocusedContent();

  // Get the focused content for the argument of some IMEStateManager's
  // methods.
  virtual already_AddRefed<nsIContent> GetFocusedContentForIME();

  // Whether the editor is active on the DOM window.  Note that when this
  // returns true but GetFocusedContent() returns null, it means that this editor was
  // focused when the DOM window was active.
  virtual bool IsActiveInDOMWindow();

  // Whether the aEvent should be handled by this editor or not.  When this
  // returns FALSE, The aEvent shouldn't be handled on this editor,
  // i.e., The aEvent should be handled by another inner editor or ancestor
  // elements.
  virtual bool IsAcceptableInputEvent(nsIDOMEvent* aEvent);

  // FindSelectionRoot() returns a selection root of this editor when aNode
  // gets focus.  aNode must be a content node or a document node.  When the
  // target isn't a part of this editor, returns nullptr.  If this is for
  // designMode, you should set the document node to aNode except that an
  // element in the document has focus.
  virtual already_AddRefed<nsIContent> FindSelectionRoot(nsINode* aNode);

  // Initializes selection and caret for the editor.  If aEventTarget isn't
  // a host of the editor, i.e., the editor doesn't get focus, this does
  // nothing.
  nsresult InitializeSelection(nsIDOMEventTarget* aFocusEventTarget);

  // Finalizes selection and caret for the editor.
  void FinalizeSelection();

  // This method has to be called by nsEditorEventListener::Focus.
  // All actions that have to be done when the editor is focused needs to be
  // added here.
  void OnFocus(nsIDOMEventTarget* aFocusEventTarget);

  // Used to insert content from a data transfer into the editable area.
  // This is called for each item in the data transfer, with the index of
  // each item passed as aIndex.
  virtual nsresult InsertFromDataTransfer(mozilla::dom::DataTransfer *aDataTransfer,
                                          int32_t aIndex,
                                          nsIDOMDocument *aSourceDoc,
                                          nsIDOMNode *aDestinationNode,
                                          int32_t aDestOffset,
                                          bool aDoDeleteSelection) = 0;

  virtual nsresult InsertFromDrop(nsIDOMEvent* aDropEvent) = 0;

  virtual already_AddRefed<nsIDOMNode> FindUserSelectAllNode(nsIDOMNode* aNode) { return nullptr; }

protected:
  enum Tristate {
    eTriUnset,
    eTriFalse,
    eTriTrue
  };
  // Spellchecking
  nsCString mContentMIMEType;       // MIME type of the doc we are editing.

  nsCOMPtr<nsIInlineSpellChecker> mInlineSpellChecker;

  nsRefPtr<nsTransactionManager> mTxnMgr;
  nsCOMPtr<mozilla::dom::Element> mRootElement; // cached root node
  nsCOMPtr<nsIDOMCharacterData>     mIMETextNode;      // current IME text node
  nsCOMPtr<mozilla::dom::EventTarget> mEventTarget; // The form field as an event receiver
  nsCOMPtr<nsIDOMEventListener> mEventListener;
  nsWeakPtr        mSelConWeak;          // weak reference to the nsISelectionController
  nsWeakPtr        mPlaceHolderTxn;      // weak reference to placeholder for begin/end batch purposes
  nsWeakPtr        mDocWeak;             // weak reference to the nsIDOMDocument
  nsIAtom          *mPlaceHolderName;    // name of placeholder transaction
  nsSelectionState *mSelState;           // saved selection state for placeholder txn batching
  nsString         *mPhonetic;
  // IME composition this is not null between compositionstart and
  // compositionend.
  nsRefPtr<mozilla::TextComposition> mComposition;

  // various listeners
  nsCOMArray<nsIEditActionListener> mActionListeners;  // listens to all low level actions on the doc
  nsCOMArray<nsIEditorObserver> mEditorObservers;  // just notify once per high level change
  nsCOMArray<nsIDocumentStateListener> mDocStateListeners;// listen to overall doc state (dirty or not, just created, etc)

  nsSelectionState  mSavedSel;           // cached selection for nsAutoSelectionReset
  nsRangeUpdater    mRangeUpdater;       // utility class object for maintaining preserved ranges

  uint32_t          mModCount;     // number of modifications (for undo/redo stack)
  uint32_t          mFlags;        // behavior flags. See nsIPlaintextEditor.idl for the flags we use.

  int32_t           mUpdateCount;

  int32_t           mPlaceHolderBatch;   // nesting count for batching
  EditAction        mAction;             // the current editor action

  uint32_t          mIMETextOffset;    // offset in text node where IME comp string begins

  EDirection        mDirection;          // the current direction of editor action
  int8_t            mDocDirtyState;      // -1 = not initialized
  uint8_t           mSpellcheckCheckboxState; // a Tristate value

  bool mShouldTxnSetSelection;  // turn off for conservative selection adjustment by txns
  bool mDidPreDestroy;    // whether PreDestroy has been called
  bool mDidPostCreate;    // whether PostCreate has been called
  bool mDispatchInputEvent;

  friend bool NSCanUnload(nsISupports* serviceMgr);
  friend class nsAutoTxnsConserveSelection;
  friend class nsAutoSelectionReset;
  friend class nsAutoRules;
  friend class nsRangeUpdater;
};


#endif
