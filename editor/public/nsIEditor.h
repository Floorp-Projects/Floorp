/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIEditor_h__
#define nsIEditor_h__
#include "nsISupports.h"
#include "nscore.h"
#include "nsIDiskDocument.h"
#include "nsString.h"

#define NS_IEDITOR_IID \
{/* A3C5EE71-742E-11d2-8F2C-006008310194*/ \
0xa3c5ee71, 0x742e, 0x11d2, \
{0x8f, 0x2c, 0x0, 0x60, 0x8, 0x31, 0x1, 0x94} }


class nsIPresShell;
class nsIDOMNode;
class nsIDOMElement;
class nsIDOMDocument;
class nsIDOMSelection;
class nsITransaction;
class nsITransactionManager;
class nsIOutputStream;
class nsIEditActionListener;
class nsIEditorObserver;
class nsIDocumentStateListener;
class nsFileSpec;
class nsISelectionController;
class nsIContent;

class nsIEditor  : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEDITOR_IID; return iid; }

  /* An enum used to describe how to collpase a non-collapsed selection */
  typedef enum {
    eNone,
    eNext,
    ePrevious,
    eNextWord,
    ePreviousWord,
    eToBeginningOfLine,
    eToEndOfLine
  } EDirection;


  /**
   * Init is to tell the implementation of nsIEditor to begin its services
   * @param aDoc            The dom document interface being observed
   * @param aPresShell      TEMP: The presentation shell displaying the document
   *                        once events can tell us from what pres shell they originated, 
   *                        this will no longer be necessary and the editor will no longer be
   *                        linked to a single pres shell.
   * @param aRoot           This is the root of the editable section of this document. if it is null then we get root from document body.
   * @param aSelCon         this should be used to get the selection location
   * @param aFlags          A bitmask of flags for specifying the behavior of the editor.
   */
  NS_IMETHOD Init(nsIDOMDocument *aDoc, nsIPresShell *aPresShell, nsIContent *aRoot, nsISelectionController *aSelCon, PRUint32 aFlags)=0;

  /**
   * PostCreate should be called after Init, and is the time that the editor tells
   * its documentStateObservers that the document has been created.
   */
  NS_IMETHOD PostCreate()=0;

  /** return the edit flags for this editor */
  NS_IMETHOD GetFlags(PRUint32 *aFlags)=0;

  /** set the edit flags for this editor.  May be called at any time. */
  NS_IMETHOD SetFlags(PRUint32 aFlags)=0;

  /**
   * return the DOM Document this editor is associated with
   *
   * @param aDoc [OUT] the dom interface being observed, refcounted
   */
  NS_IMETHOD GetDocument(nsIDOMDocument **aDoc)=0;

  /** return the body element 
   *  @param aElement return value for the root of the editable document
  */
  NS_IMETHOD GetRootElement(nsIDOMElement **aElement)=0;

  /**
   * return the presentation shell this editor is associated with
   *
   * @param aPS [OUT] the pres shell, refcounted
   */
  //NS_IMETHOD GetPresShell(nsIPresShell **aPS)=0;

  /**
   * return the selection controller for the current presentation
   *
   * @param aSelCon [OUT] the selection controller, refcounted
   */
  NS_IMETHOD GetSelectionController(nsISelectionController **aSel)=0;


  /** 
   * return the DOM Selection for the presentation shell that has focus
   * (or most recently had focus.)
   * @param aSelection [OUT] the dom interface for the selection
   */
  NS_IMETHOD GetSelection(nsIDOMSelection **aSelection)=0;


  /* ------------ Selected content removal -------------- */

  /** 
   * DeleteSelection removes all nodes in the current selection.
   * @param aDir  if eLTR, delete to the right (for example, the DEL key)
   *              if eRTL, delete to the left (for example, the BACKSPACE key)
   */
  NS_IMETHOD DeleteSelection(EDirection aAction)=0;


  /* ------------ Document info and file methods -------------- */
  
  /** Returns true if the document has no *meaningful* content */
  NS_IMETHOD GetDocumentIsEmpty(PRBool *aDocumentIsEmpty)=0;
  
  /** Returns true if the document is modifed and needs saving */
  NS_IMETHOD GetDocumentModified(PRBool *outDocModified)=0;

  /** Returns the current 'Save' document character set */
  NS_IMETHOD GetDocumentCharacterSet(PRUnichar** characterSet)=0;

  /** Sets the current 'Save' document character set */
  NS_IMETHOD SetDocumentCharacterSet(const PRUnichar* characterSet)=0;

  /** Save document to a file
   *       Note: No UI is used
   *  @param aFileSpec
   *          The file to save to
   *  @param aReplaceExisting
   *          true if replacing an existing file, otherwise false.
   *          If false and aFileSpec exists, SaveFile returns an error.
   *  @param aSaveCopy       
   *          true if we are saving off a copy of the document
   *          without changing the disk file associated with the doc.
   *          This would correspond to a 'Save Copy As' menu command
   *          (currently not in our UI)
   *  @param aFormat
   *          Mime type to save (text/plain or text/html)
   */
  NS_IMETHOD SaveFile(nsFileSpec *aFileSpec, PRBool aReplaceExisting, PRBool aSaveCopy, const nsString& aFormat)=0;

  /* ------------ Transaction methods -------------- */

  /** GetTransactionManagerDo() Get the transaction manager
    *
    * @return aTxnManager the transaction manager that the editor is using
    */
  NS_IMETHOD GetTransactionManager(nsITransactionManager* *aTxnManager)=0;

  /** Do() fires a transaction.  It is provided here so clients can create their own transactions.
    * If a transaction manager is present, it is used.  
    * Otherwise, the transaction is just executed directly.
    *
    * @param aTxn the transaction to execute
    */
  NS_IMETHOD Do(nsITransaction *aTxn)=0;


  /** turn the undo system on or off
    * @param aEnable  if PR_TRUE, the undo system is turned on if it is available
    *                 if PR_FALSE the undo system is turned off if it was previously on
    * @return         if aEnable is PR_TRUE, returns NS_OK if the undo system could be initialized properly
    *                 if aEnable is PR_FALSE, returns NS_OK.
    */
  NS_IMETHOD EnableUndo(PRBool aEnable)=0;

  /** Undo reverses the effects of the last Do operation, if Undo is enabled in the editor.
    * It is provided here so clients need no knowledge of whether the editor has a transaction manager or not.
    * If a transaction manager is present, it is told to undo and the result of
    * that undo is returned.  
    * Otherwise, the Undo request is ignored and an error NS_ERROR_NOT_AVAILABLE is returned.
    *
    */
  NS_IMETHOD Undo(PRUint32 aCount)=0;

  /** returns state information about the undo system.
    * @param aIsEnabled [OUT] PR_TRUE if undo is enabled
    * @param aCanUndo   [OUT] PR_TRUE if at least one transaction is currently ready to be undone.
    */
  NS_IMETHOD CanUndo(PRBool &aIsEnabled, PRBool &aCanUndo)=0;

  /** Redo reverses the effects of the last Undo operation
    * It is provided here so clients need no knowledge of whether the editor has a transaction manager or not.
    * If a transaction manager is present, it is told to redo and the result of the previously undone
    * transaction is reapplied to the document.
    * If no transaction is available for Redo, or if the document has no transaction manager,
    * the Redo request is ignored and an error NS_ERROR_NOT_AVAILABLE is returned.
    *
    */
  NS_IMETHOD Redo(PRUint32 aCount)=0;

  /** returns state information about the redo system.
    * @param aIsEnabled [OUT] PR_TRUE if redo is enabled
    * @param aCanRedo   [OUT] PR_TRUE if at least one transaction is currently ready to be redone.
    */
  NS_IMETHOD CanRedo(PRBool &aIsEnabled, PRBool &aCanRedo)=0;

  /** BeginTransaction is a signal from the caller to the editor that the caller will execute multiple updates
    * to the content tree that should be treated as a single logical operation,
    * in the most efficient way possible.<br>
    * All transactions executed between a call to BeginTransaction and EndTransaction will be undoable as
    * an atomic action.<br>
    * EndTransaction must be called after BeginTransaction.<br>
    * Calls to BeginTransaction can be nested, as long as EndTransaction is called once per BeginUpdate.
    */
  NS_IMETHOD BeginTransaction()=0;

  /** EndTransaction is a signal to the editor that the caller is finished updating the content model.<br>
    * BeginUpdate must be called before EndTransaction is called.<br>
    * Calls to BeginTransaction can be nested, as long as EndTransaction is called once per BeginTransaction.
    */
  NS_IMETHOD EndTransaction()=0;

  NS_IMETHOD BeginPlaceHolderTransaction(nsIAtom *aName)=0;
  NS_IMETHOD EndPlaceHolderTransaction()=0;
  NS_IMETHOD ShouldTxnSetSelection(PRBool *aResult)=0;

  /* ------------ Clipboard methods -------------- */

  /** cut the currently selected text, putting it into the OS clipboard
    * What if no text is selected?
    * What about mixed selections?
    * What are the clipboard formats?
    */
  NS_IMETHOD Cut()=0;

  /** Can we cut? True if the doc is modifiable, and we have a non-
    * collapsed selection.
    */
  NS_IMETHOD CanCut(PRBool &aCanCut)=0;
  
  /** copy the currently selected text, putting it into the OS clipboard
    * What if no text is selected?
    * What about mixed selections?
    * What are the clipboard formats?
    */
  NS_IMETHOD Copy()=0;
  
  /** Can we copy? True if we have a non-collapsed selection.
    */
  NS_IMETHOD CanCopy(PRBool &aCanCopy)=0;
  
  /** paste the text in the OS clipboard at the cursor position, replacing
    * the selected text (if any)
    */
  NS_IMETHOD Paste(PRInt32 aSelectionType)=0;

  /** Can we paste? True if the doc is modifiable, and we have
    * pasteable data in the clipboard.
    */
  NS_IMETHOD CanPaste(PRInt32 aSelectionType, PRBool &aCanPaste)=0;

  /* ------------ Selection methods -------------- */

  /** sets the document selection to the entire contents of the document */
  NS_IMETHOD SelectAll()=0;

  /** sets the document selection to the beginning of the document */
  NS_IMETHOD BeginningOfDocument()=0;

  /** sets the document selection to the end of the document */
  NS_IMETHOD EndOfDocument()=0;


  /* ------------ Node manipulation methods -------------- */

  /**
   * Tests if a node is a BLOCK element according the the HTML 4.0 DTD
   *   This does NOT consider CSS effect on display type
   *
   * @param aNode      the node to test
   */
  NS_IMETHOD NodeIsBlock(nsIDOMNode *aNode, PRBool &aIsBlock)=0;

  /**
   * SetAttribute() sets the attribute of aElement.
   * No checking is done to see if aAttribute is a legal attribute of the node,
   * or if aValue is a legal value of aAttribute.
   *
   * @param aElement    the content element to operate on
   * @param aAttribute  the string representation of the attribute to set
   * @param aValue      the value to set aAttribute to
   */
  NS_IMETHOD SetAttribute(nsIDOMElement * aElement, 
                          const nsString& aAttribute, 
                          const nsString& aValue)=0;

  /**
   * GetAttributeValue() retrieves the attribute's value for aElement.
   *
   * @param aElement      the content element to operate on
   * @param aAttribute    the string representation of the attribute to get
   * @param aResultValue  the value of aAttribute.  only valid if aResultIsSet is PR_TRUE
   * @param aResultIsSet  PR_TRUE if aAttribute is set on the current node, PR_FALSE if it is not.
   */
  NS_IMETHOD GetAttributeValue(nsIDOMElement * aElement, 
                               const nsString& aAttribute, 
                               nsString&       aResultValue, 
                               PRBool&         aResultIsSet)=0;

  /**
   * RemoveAttribute() deletes aAttribute from the attribute list of aElement.
   * If aAttribute is not an attribute of aElement, nothing is done.
   *
   * @param aElement      the content element to operate on
   * @param aAttribute    the string representation of the attribute to get
   */
  NS_IMETHOD RemoveAttribute(nsIDOMElement * aElement, 
                             const nsString& aAttribute)=0;

  /**
   * CloneAttributes() is similar to nsIDOMNode::cloneNode(),
   *   it assures the attribute nodes of the destination are identical with the source node
   *   by copying all existing attributes from the source and deleting those not in the source.
   *   This is used when the destination node (element) already exists
   *
   * The supplied nodes MUST BE ELEMENTS (most callers are working with nodes)
   * @param aDestNode     the destination element to operate on
   * @param aSourceNode   the source element to copy attributes from
   */
  NS_IMETHOD CloneAttributes(nsIDOMNode *aDestNode, nsIDOMNode *aSourceNode)=0;

  /** 
   * CreateNode instantiates a new element of type aTag and inserts it into aParent at aPosition.
   * @param aTag      The type of object to create
   * @param aParent   The node to insert the new object into
   * @param aPosition The place in aParent to insert the new node
   * @param aNewNode  [OUT] The node created.  Caller must release aNewNode.
   */
  NS_IMETHOD CreateNode(const nsString& aTag,
                        nsIDOMNode *    aParent,
                        PRInt32         aPosition,
                        nsIDOMNode **   aNewNode)=0;

  /** 
   * InsertNode inserts aNode into aParent at aPosition.
   * No checking is done to verify the legality of the insertion.  That is the 
   * responsibility of the caller.
   * @param aNode     The DOM Node to insert.
   * @param aParent   The node to insert the new object into
   * @param aPosition The place in aParent to insert the new node
   *                  0=first child, 1=second child, etc.
   *                  any number > number of current children = last child
   */
  NS_IMETHOD InsertNode(nsIDOMNode * aNode,
                        nsIDOMNode * aParent,
                        PRInt32      aPosition)=0;


  /** 
   * SplitNode() creates a new node identical to an existing node, and split the contents between the two nodes
   * @param aExistingRightNode   the node to split.  It will become the new node's next sibling.
   * @param aOffset              the offset of aExistingRightNode's content|children to do the split at
   * @param aNewLeftNode         [OUT] the new node resulting from the split, becomes aExistingRightNode's previous sibling.
   */
  NS_IMETHOD SplitNode(nsIDOMNode  *aExistingRightNode,
                       PRInt32      aOffset,
                       nsIDOMNode **aNewLeftNode)=0;

  /** 
   * JoinNodes() takes 2 nodes and merge their content|children.
   * @param aLeftNode     The left node.  It will be deleted.
   * @param aRightNode    The right node. It will remain after the join.
   * @param aParent       The parent of aExistingRightNode
   *
   *                      There is no requirement that the two nodes be of the same type.
   *                      However, a text node can be merged only with another text node.
   */
  NS_IMETHOD JoinNodes(nsIDOMNode  *aLeftNode,
                       nsIDOMNode  *aRightNode,
                       nsIDOMNode  *aParent)=0;


  /** 
   * DeleteNode removes aChild from aParent.
   * @param aChild    The node to delete
   */
  NS_IMETHOD DeleteNode(nsIDOMNode * aChild)=0;

  /** 
   * InsertFormattingForNode() sets a special dirty attribute on the node.
   * Usually this will be called immediately after creating a new node.
   * @param aNode      The node for which to insert formatting.
   */
  NS_IMETHOD MarkNodeDirty(nsIDOMNode* aNode) = 0;

  /* ------------ Output methods -------------- */

  /**
   * Output methods:
   * aFormatType is a mime type, like text/plain.
   */
  NS_IMETHOD OutputToString(nsAWritableString& aOutputString,
                            const nsAReadableString& aFormatType,
                            PRUint32 aFlags) = 0;
  NS_IMETHOD OutputToStream(nsIOutputStream* aOutputStream,
                            const nsString& aFormatType,
                            const nsString* aCharsetOverride,
                            PRUint32 aFlags) = 0;



  /* ------------ Various listeners methods -------------- */

  /** add an EditorObserver to the editors list of observers. */
  NS_IMETHOD AddEditorObserver(nsIEditorObserver *aObserver)=0;

  /** Remove an EditorObserver from the editor's list of observers. */
  NS_IMETHOD RemoveEditorObserver(nsIEditorObserver *aObserver)=0;

  /** add an EditActionListener to the editors list of listeners. */
  NS_IMETHOD AddEditActionListener(nsIEditActionListener *aListener)=0;

  /** Remove an EditActionListener from the editor's list of listeners. */
  NS_IMETHOD RemoveEditActionListener(nsIEditActionListener *aListener)=0;

  /** Add a DocumentStateListener to the editors list of doc state listeners. */
  NS_IMETHOD AddDocumentStateListener(nsIDocumentStateListener *aListener)=0;

  /** Remove a DocumentStateListener to the editors list of doc state listeners. */
  NS_IMETHOD RemoveDocumentStateListener(nsIDocumentStateListener *aListener)=0;


  /* ------------ Debug methods -------------- */

  /**
   * And a debug method -- show us what the tree looks like right now
   */
  NS_IMETHOD DumpContentTree() = 0;

  /** Dumps a text representation of the content tree to standard out */
  NS_IMETHOD DebugDumpContent() const =0;

  /* Run unit tests. Noop in optimized builds */
  NS_IMETHOD DebugUnitTests(PRInt32 *outNumTests, PRInt32 *outNumTestsFailed)=0;


};


#endif //nsIEditor_h__

