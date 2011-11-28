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

#ifndef __editor_h__
#define __editor_h__

#include "nsCOMPtr.h"
#include "nsWeakReference.h"

#include "nsIEditor.h"
#include "nsIPlaintextEditor.h"
#include "nsIEditorIMESupport.h"
#include "nsIPhonetic.h"

#include "nsIAtom.h"
#include "nsIDOMDocument.h"
#include "nsISelection.h"
#include "nsIDOMCharacterData.h"
#include "nsIPrivateTextRange.h"
#include "nsITransactionManager.h"
#include "nsIComponentManager.h"
#include "nsCOMArray.h"
#include "nsIEditActionListener.h"
#include "nsIEditorObserver.h"
#include "nsIDocumentStateListener.h"
#include "nsIDOMElement.h"
#include "nsSelectionState.h"
#include "nsIEditorSpellCheck.h"
#include "nsIInlineSpellChecker.h"
#include "nsIDOMEventTarget.h"
#include "nsStubMutationObserver.h"
#include "nsIViewManager.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"

class nsIDOMCharacterData;
class nsIDOMRange;
class nsIPresShell;
class ChangeAttributeTxn;
class CreateElementTxn;
class InsertElementTxn;
class DeleteElementTxn;
class InsertTextTxn;
class DeleteTextTxn;
class SplitElementTxn;
class JoinElementTxn;
class EditAggregateTxn;
class IMETextTxn;
class AddStyleSheetTxn;
class RemoveStyleSheetTxn;
class nsIFile;
class nsISelectionController;
class nsIDOMEventTarget;
class nsCSSStyleSheet;
class nsKeyEvent;
class nsIDOMNSEvent;

namespace mozilla {
namespace widget {
struct IMEState;
} // namespace widget
} // namespace mozilla

#define kMOZEditorBogusNodeAttrAtom nsEditProperty::mozEditorBogusNode
#define kMOZEditorBogusNodeValue NS_LITERAL_STRING("TRUE")

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

  enum OperationID
  {
    kOpIgnore = -1,
    kOpNone = 0,
    kOpUndo,
    kOpRedo,
    kOpInsertNode,
    kOpCreateNode,
    kOpDeleteNode,
    kOpSplitNode,
    kOpJoinNode,
    kOpDeleteSelection,
    // text commands
    kOpInsertBreak    = 1000,
    kOpInsertText     = 1001,
    kOpInsertIMEText  = 1002,
    kOpDeleteText     = 1003
  };

  /** The default constructor. This should suffice. the setting of the interfaces is done
   *  after the construction of the editor class.
   */
  nsEditor();
  /** The default destructor. This should suffice. Should this be pure virtual 
   *  for someone to derive from the nsEditor later? I don't believe so.
   */
  virtual ~nsEditor();

//Interfaces for addref and release and queryinterface
//NOTE: Use   NS_DECL_ISUPPORTS_INHERITED in any class inherited from nsEditor
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsEditor,
                                           nsIEditor)

  /* ------------ utility methods   -------------- */
  already_AddRefed<nsIPresShell> GetPresShell();
  void NotifyEditorObservers();

  /* ------------ nsIEditor methods -------------- */
  NS_DECL_NSIEDITOR
  /* ------------ nsIEditorIMESupport methods -------------- */
  NS_DECL_NSIEDITORIMESUPPORT
  
  /* ------------ nsIObserver methods -------------- */
  NS_DECL_NSIOBSERVER

  // nsIPhonetic
  NS_DECL_NSIPHONETIC

public:

  virtual bool IsModifiableNode(nsINode *aNode);
  
  NS_IMETHOD InsertTextImpl(const nsAString& aStringToInsert, 
                               nsCOMPtr<nsIDOMNode> *aInOutNode, 
                               PRInt32 *aInOutOffset,
                               nsIDOMDocument *aDoc);
  nsresult InsertTextIntoTextNodeImpl(const nsAString& aStringToInsert, 
                                      nsIDOMCharacterData *aTextNode, 
                                      PRInt32 aOffset,
                                      bool aSuppressIME = false);
  NS_IMETHOD DeleteSelectionImpl(EDirection aAction);
  NS_IMETHOD DeleteSelectionAndCreateNode(const nsAString& aTag,
                                           nsIDOMNode ** aNewNode);

  /* helper routines for node/parent manipulations */
  nsresult ReplaceContainer(nsIDOMNode *inNode, 
                            nsCOMPtr<nsIDOMNode> *outNode, 
                            const nsAString &aNodeType,
                            const nsAString *aAttribute = nsnull,
                            const nsAString *aValue = nsnull,
                            bool aCloneAttributes = false);

  nsresult RemoveContainer(nsIDOMNode *inNode);
  nsresult InsertContainerAbove(nsIDOMNode *inNode, 
                                nsCOMPtr<nsIDOMNode> *outNode, 
                                const nsAString &aNodeType,
                                const nsAString *aAttribute = nsnull,
                                const nsAString *aValue = nsnull);
  nsresult MoveNode(nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aOffset);

  /* Method to replace certain CreateElementNS() calls. 
     Arguments:
      nsString& aTag          - tag you want
      nsIContent** aContent   - returned Content that was created with above namespace.
  */
  nsresult CreateHTMLContent(const nsAString& aTag, nsIContent** aContent);

  // IME event handlers
  virtual nsresult BeginIMEComposition();
  virtual nsresult UpdateIMEComposition(const nsAString &aCompositionString,
                                        nsIPrivateTextRangeList *aTextRange)=0;
  nsresult EndIMEComposition();

  void SwitchTextDirectionTo(PRUint32 aDirection);

  void BeginKeypressHandling() { mLastKeypressEventWasTrusted = eTriTrue; }
  void BeginKeypressHandling(nsIDOMNSEvent* aEvent);
  void EndKeypressHandling() { mLastKeypressEventWasTrusted = eTriUnset; }

  class FireTrustedInputEvent {
  public:
    explicit FireTrustedInputEvent(nsEditor* aSelf, bool aActive = true)
      : mEditor(aSelf)
      , mShouldAct(aActive && mEditor->mLastKeypressEventWasTrusted == eTriUnset) {
      if (mShouldAct) {
        mEditor->BeginKeypressHandling();
      }
    }
    ~FireTrustedInputEvent() {
      if (mShouldAct) {
        mEditor->EndKeypressHandling();
      }
    }
  private:
    nsEditor* mEditor;
    bool mShouldAct;
  };

protected:
  nsCString mContentMIMEType;       // MIME type of the doc we are editing.

  nsresult DetermineCurrentDirection();

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
                                       nsIDOMNode     *aParent,
                                       PRInt32         aPosition,
                                       CreateElementTxn ** aTxn);

  /** create a transaction for inserting aNode as a child of aParent.
    */
  NS_IMETHOD CreateTxnForInsertElement(nsIDOMNode * aNode,
                                       nsIDOMNode * aParent,
                                       PRInt32      aOffset,
                                       InsertElementTxn ** aTxn);

  /** create a transaction for removing aElement from its parent.
    */
  NS_IMETHOD CreateTxnForDeleteElement(nsIDOMNode * aElement,
                                       DeleteElementTxn ** aTxn);


  NS_IMETHOD CreateTxnForDeleteSelection(EDirection aAction,
                                         EditAggregateTxn ** aTxn,
                                         nsIDOMNode ** aNode,
                                         PRInt32 *aOffset,
                                         PRInt32 *aLength);

  NS_IMETHOD CreateTxnForDeleteInsertionPoint(nsIDOMRange         *aRange, 
                                              EDirection aAction, 
                                              EditAggregateTxn *aTxn,
                                              nsIDOMNode ** aNode,
                                              PRInt32 *aOffset,
                                              PRInt32 *aLength);


  /** create a transaction for inserting aStringToInsert into aTextNode
    * if aTextNode is null, the string is inserted at the current selection.
    */
  NS_IMETHOD CreateTxnForInsertText(const nsAString & aStringToInsert,
                                    nsIDOMCharacterData *aTextNode,
                                    PRInt32 aOffset,
                                    InsertTextTxn ** aTxn);

  NS_IMETHOD CreateTxnForIMEText(const nsAString & aStringToInsert,
                                 IMETextTxn ** aTxn);

  /** create a transaction for adding a style sheet
    */
  NS_IMETHOD CreateTxnForAddStyleSheet(nsCSSStyleSheet* aSheet, AddStyleSheetTxn* *aTxn);

  /** create a transaction for removing a style sheet
    */
  NS_IMETHOD CreateTxnForRemoveStyleSheet(nsCSSStyleSheet* aSheet, RemoveStyleSheetTxn* *aTxn);
  
  NS_IMETHOD DeleteText(nsIDOMCharacterData *aElement,
                        PRUint32             aOffset,
                        PRUint32             aLength);

//  NS_IMETHOD DeleteRange(nsIDOMRange *aRange);

  NS_IMETHOD CreateTxnForDeleteText(nsIDOMCharacterData *aElement,
                                    PRUint32             aOffset,
                                    PRUint32             aLength,
                                    DeleteTextTxn      **aTxn);

  nsresult CreateTxnForDeleteCharacter(nsIDOMCharacterData  *aData,
                                       PRUint32              aOffset,
                                       nsIEditor::EDirection aDirection,
                                       DeleteTextTxn       **aTxn);
	
  NS_IMETHOD CreateTxnForSplitNode(nsIDOMNode *aNode,
                                   PRUint32    aOffset,
                                   SplitElementTxn **aTxn);

  NS_IMETHOD CreateTxnForJoinNode(nsIDOMNode  *aLeftNode,
                                  nsIDOMNode  *aRightNode,
                                  JoinElementTxn **aTxn);

  NS_IMETHOD DeleteSelectionAndPrepareToCreateNode(nsCOMPtr<nsIDOMNode> &parentSelectedNode, 
                                                   PRInt32& offsetOfNewNode);

  // called after a transaction is done successfully
  NS_IMETHOD DoAfterDoTransaction(nsITransaction *aTxn);
  // called after a transaction is undone successfully
  NS_IMETHOD DoAfterUndoTransaction();
  // called after a transaction is redone successfully
  NS_IMETHOD DoAfterRedoTransaction();

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

  // stub.  see comment in source.                     
  virtual bool IsBlockNode(nsIDOMNode *aNode);
  virtual bool IsBlockNode(nsINode *aNode);
  
  // helper for GetPriorNode and GetNextNode
  nsIContent* FindNextLeafNode(nsINode  *aCurrentNode,
                               bool      aGoForward,
                               bool      bNoBlockCrossing,
                               nsIContent *aActiveEditorRoot);

  // Get nsIWidget interface
  nsresult GetWidget(nsIWidget **aWidget);


  // install the event listeners for the editor 
  virtual nsresult InstallEventListeners();

  virtual void CreateEventListeners();

  // unregister and release our event listeners
  virtual void RemoveEventListeners();

  /**
   * Return true if spellchecking should be enabled for this editor.
   */
  bool GetDesiredSpellCheckState();

  nsKeyEvent* GetNativeKeyEvent(nsIDOMKeyEvent* aDOMKeyEvent);

  bool CanEnableSpellCheck()
  {
    // Check for password/readonly/disabled, which are not spellchecked
    // regardless of DOM. Also, check to see if spell check should be skipped or not.
    return !IsPasswordEditor() && !IsReadonly() && !IsDisabled() && !ShouldSkipSpellCheck();
  }

public:

  /** All editor operations which alter the doc should be prefaced
   *  with a call to StartOperation, naming the action and direction */
  NS_IMETHOD StartOperation(PRInt32 opID, nsIEditor::EDirection aDirection);

  /** All editor operations which alter the doc should be followed
   *  with a call to EndOperation */
  NS_IMETHOD EndOperation();

  /** routines for managing the preservation of selection across 
   *  various editor actions */
  bool     ArePreservingSelection();
  nsresult PreserveSelectionAcrossActions(nsISelection *aSel);
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
                         PRInt32     aOffset,
                         nsIDOMNode *aNewLeftNode,
                         nsIDOMNode *aParent);

  /** 
   * JoinNodes() takes 2 nodes and merge their content|children.
   * @param aNodeToKeep   The node that will remain after the join.
   * @param aNodeToJoin   The node that will be joined with aNodeToKeep.
   *                      There is no requirement that the two nodes be of the same type.
   * @param aParent       The parent of aNodeToKeep
   * @param aNodeToKeepIsFirst  if true, the contents|children of aNodeToKeep come before the
   *                            contents|children of aNodeToJoin, otherwise their positions are switched.
   */
  nsresult JoinNodesImpl(nsIDOMNode *aNodeToKeep,
                         nsIDOMNode *aNodeToJoin,
                         nsIDOMNode *aParent,
                         bool        aNodeToKeepIsFirst);

  /**
   *  Set aOffset to the offset of aChild in aParent.  
   *  Returns an error if aChild is not an immediate child of aParent.
   */
  static nsresult GetChildOffset(nsIDOMNode *aChild, 
                                 nsIDOMNode *aParent, 
                                 PRInt32    &aOffset);

  /**
   *  Set aParent to the parent of aChild.
   *  Set aOffset to the offset of aChild in aParent.  
   */
  static nsresult GetNodeLocation(nsIDOMNode *aChild, 
                                 nsCOMPtr<nsIDOMNode> *aParent, 
                                 PRInt32    *aOffset);

  /** returns the number of things inside aNode in the out-param aCount.  
    * @param  aNode is the node to get the length of.  
    *         If aNode is text, returns number of characters. 
    *         If not, returns number of children nodes.
    * @param  aCount [OUT] the result of the above calculation.
    */
  static nsresult GetLengthOfDOMNode(nsIDOMNode *aNode, PRUint32 &aCount);

  /** get the node immediately prior to aCurrentNode
    * @param aCurrentNode   the node from which we start the search
    * @param aEditableNode  if true, only return an editable node
    * @param aResultNode    [OUT] the node that occurs before aCurrentNode in the tree,
    *                       skipping non-editable nodes if aEditableNode is true.
    *                       If there is no prior node, aResultNode will be nsnull.
    * @param bNoBlockCrossing If true, don't move across "block" nodes, whatever that means.
    * @param aActiveEditorRoot If non-null, only return descendants of aActiveEditorRoot.
    */
  nsresult GetPriorNode(nsIDOMNode  *aCurrentNode, 
                        bool         aEditableNode,
                        nsCOMPtr<nsIDOMNode> *aResultNode,
                        bool         bNoBlockCrossing = false,
                        nsIContent  *aActiveEditorRoot = nsnull);

  // and another version that takes a {parent,offset} pair rather than a node
  nsresult GetPriorNode(nsIDOMNode  *aParentNode, 
                        PRInt32      aOffset, 
                        bool         aEditableNode, 
                        nsCOMPtr<nsIDOMNode> *aResultNode,
                        bool         bNoBlockCrossing = false,
                        nsIContent  *aActiveEditorRoot = nsnull);
                       
  /** get the node immediately after to aCurrentNode
    * @param aCurrentNode   the node from which we start the search
    * @param aEditableNode  if true, only return an editable node
    * @param aResultNode    [OUT] the node that occurs after aCurrentNode in the tree,
    *                       skipping non-editable nodes if aEditableNode is true.
    *                       If there is no prior node, aResultNode will be nsnull.
    */
  nsresult GetNextNode(nsIDOMNode  *aCurrentNode, 
                       bool         aEditableNode,
                       nsCOMPtr<nsIDOMNode> *aResultNode,
                       bool         bNoBlockCrossing = false,
                       nsIContent  *aActiveEditorRoot = nsnull);

  // and another version that takes a {parent,offset} pair rather than a node
  nsresult GetNextNode(nsIDOMNode  *aParentNode, 
                       PRInt32      aOffset, 
                       bool         aEditableNode, 
                       nsCOMPtr<nsIDOMNode> *aResultNode,
                       bool         bNoBlockCrossing = false,
                       nsIContent  *aActiveEditorRoot = nsnull);

  // Helper for GetNextNode and GetPriorNode
  nsIContent* FindNode(nsINode *aCurrentNode,
                       bool     aGoForward,
                       bool     aEditableNode,
                       bool     bNoBlockCrossing,
                       nsIContent *aActiveEditorRoot);
  /**
   * Get the rightmost child of aCurrentNode;
   * return nsnull if aCurrentNode has no children.
   */
  already_AddRefed<nsIDOMNode> GetRightmostChild(nsIDOMNode *aCurrentNode, 
                                                 bool        bNoBlockCrossing = false);
  nsIContent* GetRightmostChild(nsINode *aCurrentNode,
                                bool     bNoBlockCrossing = false);

  /**
   * Get the leftmost child of aCurrentNode;
   * return nsnull if aCurrentNode has no children.
   */
  already_AddRefed<nsIDOMNode> GetLeftmostChild(nsIDOMNode  *aCurrentNode, 
                                                bool        bNoBlockCrossing = false);
  nsIContent* GetLeftmostChild(nsINode *aCurrentNode,
                               bool     bNoBlockCrossing = false);

  /** returns true if aNode is of the type implied by aTag */
  static inline bool NodeIsType(nsIDOMNode *aNode, nsIAtom *aTag)
  {
    return GetTag(aNode) == aTag;
  }

  // we should get rid of this method if we can
  static inline bool NodeIsTypeString(nsIDOMNode *aNode, const nsAString &aTag)
  {
    nsIAtom *nodeAtom = GetTag(aNode);
    return nodeAtom && nodeAtom->Equals(aTag);
  }


  /** returns true if aParent can contain a child of type aTag */
  bool CanContainTag(nsIDOMNode* aParent, const nsAString &aTag);
  bool TagCanContain(const nsAString &aParentTag, nsIDOMNode* aChild);
  virtual bool TagCanContainTag(const nsAString &aParentTag, const nsAString &aChildTag);

  /** returns true if aNode is our root node */
  bool IsRootNode(nsIDOMNode *inNode);
  bool IsRootNode(nsINode *inNode);

  /** returns true if aNode is a descendant of our root node */
  bool IsDescendantOfBody(nsIDOMNode *inNode);
  bool IsDescendantOfBody(nsINode *inNode);

  /** returns true if aNode is a container */
  virtual bool IsContainer(nsIDOMNode *aNode);

  /** returns true if aNode is an editable node */
  bool IsEditable(nsIDOMNode *aNode);
  bool IsEditable(nsIContent *aNode);

  virtual bool IsTextInDirtyFrameVisible(nsIContent *aNode);

  /** returns true if aNode is a MozEditorBogus node */
  bool IsMozEditorBogusNode(nsIDOMNode *aNode);
  bool IsMozEditorBogusNode(nsIContent *aNode);

  /** counts number of editable child nodes */
  nsresult CountEditableChildren(nsIDOMNode *aNode, PRUint32 &outCount);
  
  /** Find the deep first and last children. Returned nodes are AddReffed */
  nsresult GetFirstEditableNode(nsIDOMNode *aRoot, nsCOMPtr<nsIDOMNode> *outFirstNode);
#ifdef XXX_DEAD_CODE
  nsresult GetLastEditableNode(nsIDOMNode *aRoot, nsCOMPtr<nsIDOMNode> *outLastNode);
#endif

  nsresult GetIMEBufferLength(PRInt32* length);
  bool     IsIMEComposing();    /* test if IME is in composition state */
  void     SetIsIMEComposing(); /* call this before |IsIMEComposing()| */

  /** from html rules code - migration in progress */
  static nsresult GetTagString(nsIDOMNode *aNode, nsAString& outString);
  static nsIAtom *GetTag(nsIDOMNode *aNode);
  virtual bool NodesSameType(nsIDOMNode *aNode1, nsIDOMNode *aNode2);
  static bool IsTextOrElementNode(nsIDOMNode *aNode);
  static bool IsTextNode(nsIDOMNode *aNode);
  static bool IsTextNode(nsINode *aNode);
  
  static PRInt32 GetIndexOf(nsIDOMNode *aParent, nsIDOMNode *aChild);
  static nsCOMPtr<nsIDOMNode> GetChildAt(nsIDOMNode *aParent, PRInt32 aOffset);
  static nsCOMPtr<nsIDOMNode> GetNodeAtRangeOffsetPoint(nsIDOMNode* aParentOrNode, PRInt32 aOffset);

  static nsresult GetStartNodeAndOffset(nsISelection *aSelection, nsIDOMNode **outStartNode, PRInt32 *outStartOffset);
  static nsresult GetEndNodeAndOffset(nsISelection *aSelection, nsIDOMNode **outEndNode, PRInt32 *outEndOffset);
#if DEBUG_JOE
  static void DumpNode(nsIDOMNode *aNode, PRInt32 indent=0);
#endif

  // Helpers to add a node to the selection. 
  // Used by table cell selection methods
  nsresult CreateRange(nsIDOMNode *aStartParent, PRInt32 aStartOffset,
                       nsIDOMNode *aEndParent, PRInt32 aEndOffset,
                       nsIDOMRange **aRange);

  // Creates a range with just the supplied node and appends that to the selection
  nsresult AppendNodeToSelectionAsRange(nsIDOMNode *aNode);
  // When you are using AppendNodeToSelectionAsRange, call this first to start a new selection
  nsresult ClearSelection();

  nsresult IsPreformatted(nsIDOMNode *aNode, bool *aResult);

  nsresult SplitNodeDeep(nsIDOMNode *aNode, 
                         nsIDOMNode *aSplitPointParent, 
                         PRInt32 aSplitPointOffset,
                         PRInt32 *outOffset,
                         bool    aNoEmptyContainers = false,
                         nsCOMPtr<nsIDOMNode> *outLeftNode = 0,
                         nsCOMPtr<nsIDOMNode> *outRightNode = 0);
  nsresult JoinNodeDeep(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode, nsCOMPtr<nsIDOMNode> *aOutJoinNode, PRInt32 *outOffset); 

  nsresult GetString(const nsAString& name, nsAString& value);

  nsresult BeginUpdateViewBatch(void);
  virtual nsresult EndUpdateViewBatch(void);

  bool GetShouldTxnSetSelection();

  virtual nsresult HandleKeyPressEvent(nsIDOMKeyEvent* aKeyEvent);

  nsresult HandleInlineSpellCheck(PRInt32 action,
                                    nsISelection *aSelection,
                                    nsIDOMNode *previousSelectedNode,
                                    PRInt32 previousSelectedOffset,
                                    nsIDOMNode *aStartNode,
                                    PRInt32 aStartOffset,
                                    nsIDOMNode *aEndNode,
                                    PRInt32 aEndOffset);

  virtual already_AddRefed<nsIDOMEventTarget> GetDOMEventTarget() = 0;

  // Fast non-refcounting editor root element accessor
  nsIDOMElement *GetRoot();

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

  bool UseAsyncUpdate() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorUseAsyncUpdatesMask) != 0;
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
  
  PRBool ShouldSkipSpellCheck() const
  {
    return (mFlags & nsIPlaintextEditor::eEditorSkipSpellCheck) != 0;
  }

  bool IsTabbable() const
  {
    return IsSingleLineEditor() || IsPasswordEditor() || IsFormWidget() ||
           IsInteractionAllowed();
  }

  // Get the focused content, if we're focused.  Returns null otherwise.
  virtual already_AddRefed<nsIContent> GetFocusedContent();

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
  // target isn't a part of this editor, returns NULL.  If this is for
  // designMode, you should set the document node to aNode except that an
  // element in the document has focus.
  virtual already_AddRefed<nsIContent> FindSelectionRoot(nsINode* aNode);

  // Initializes selection and caret for the editor.  If aEventTarget isn't
  // a host of the editor, i.e., the editor doesn't get focus, this does
  // nothing.
  nsresult InitializeSelection(nsIDOMEventTarget* aFocusEventTarget);

  // This method has to be called by nsEditorEventListener::Focus.
  // All actions that have to be done when the editor is focused needs to be
  // added here.
  void OnFocus(nsIDOMEventTarget* aFocusEventTarget);

protected:

  PRUint32        mModCount;		// number of modifications (for undo/redo stack)
  PRUint32        mFlags;		// behavior flags. See nsIPlaintextEditor.idl for the flags we use.

  nsWeakPtr       mSelConWeak;   // weak reference to the nsISelectionController
  PRInt32         mUpdateCount;
  nsIViewManager::UpdateViewBatch mBatch;

  // Spellchecking
  enum Tristate {
    eTriUnset,
    eTriFalse,
    eTriTrue
  }                 mSpellcheckCheckboxState;
  nsCOMPtr<nsIInlineSpellChecker> mInlineSpellChecker;

  nsCOMPtr<nsITransactionManager> mTxnMgr;
  nsWeakPtr         mPlaceHolderTxn;     // weak reference to placeholder for begin/end batch purposes
  nsIAtom          *mPlaceHolderName;    // name of placeholder transaction
  PRInt32           mPlaceHolderBatch;   // nesting count for batching
  nsSelectionState *mSelState;           // saved selection state for placeholder txn batching
  nsSelectionState  mSavedSel;           // cached selection for nsAutoSelectionReset
  nsRangeUpdater    mRangeUpdater;       // utility class object for maintaining preserved ranges
  nsCOMPtr<nsIDOMElement> mRootElement;    // cached root node
  PRInt32           mAction;             // the current editor action
  EDirection        mDirection;          // the current direction of editor action
  
  // data necessary to build IME transactions
  nsCOMPtr<nsIPrivateTextRangeList> mIMETextRangeList; // IME special selection ranges
  nsCOMPtr<nsIDOMCharacterData>     mIMETextNode;      // current IME text node
  PRUint32                          mIMETextOffset;    // offset in text node where IME comp string begins
  PRUint32                          mIMEBufferLength;  // current length of IME comp string
  bool                              mInIMEMode;        // are we inside an IME composition?
  bool                              mIsIMEComposing;   // is IME in composition state?
                                                       // This is different from mInIMEMode. see Bug 98434.

  bool                          mShouldTxnSetSelection;  // turn off for conservative selection adjustment by txns
  bool                          mDidPreDestroy;    // whether PreDestroy has been called
  bool                          mDidPostCreate;    // whether PostCreate has been called
   // various listeners
  nsCOMArray<nsIEditActionListener> mActionListeners;  // listens to all low level actions on the doc
  nsCOMArray<nsIEditorObserver> mEditorObservers;  // just notify once per high level change
  nsCOMArray<nsIDocumentStateListener> mDocStateListeners;// listen to overall doc state (dirty or not, just created, etc)

  PRInt8                        mDocDirtyState;		// -1 = not initialized
  nsWeakPtr        mDocWeak;  // weak reference to the nsIDOMDocument
  // The form field as an event receiver
  nsCOMPtr<nsIDOMEventTarget> mEventTarget;

  nsString* mPhonetic;

 nsCOMPtr<nsIDOMEventListener> mEventListener;

  Tristate mLastKeypressEventWasTrusted;

  friend bool NSCanUnload(nsISupports* serviceMgr);
  friend class nsAutoTxnsConserveSelection;
  friend class nsAutoSelectionReset;
  friend class nsAutoRules;
  friend class nsRangeUpdater;
};


#endif
