/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __inDOMView_h__
#define __inDOMView_h__

#include "inIDOMView.h"

#include "nsITreeView.h"
#include "nsITreeSelection.h"
#include "nsIDocumentObserver.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsVoidArray.h"

class inDOMViewNode;

class inDOMView : public inIDOMView,
                  public nsITreeView,
                  public nsIDocumentObserver
{
public:
	 NS_DECL_ISUPPORTS
	 NS_DECL_INIDOMVIEW
	 NS_DECL_NSITREEVIEW
	
	 inDOMView();
	 virtual ~inDOMView();

  // nsIDocumentObserver
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              PRInt32      aNameSpaceID,
                              nsIAtom*     aAttribute,
                              PRInt32      aModType, 
                              nsChangeHint aHint);
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer);
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer);
  // Not implemented
  NS_IMETHOD ContentChanged(nsIDocument *aDocument,
			                      nsIContent* aContent,
                            nsISupports* aSubContent) { return NS_OK; }
  NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2,
                                  PRInt32 aStateMask) { return NS_OK; }
  NS_IMETHOD BeginUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell) { return NS_OK; }
  NS_IMETHOD EndReflow(nsIDocument *aDocument, nsIPresShell* aShell) { return NS_OK; } 
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument, nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument, nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                            nsIStyleSheet* aStyleSheet,
                                            PRBool aDisabled) { return NS_OK; }
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              nsChangeHint aHint) { return NS_OK; }
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument) { return NS_OK; };

protected:
  static nsIAtom* kAnonymousAtom;
  static nsIAtom* kElementNodeAtom;
  static nsIAtom* kAttributeNodeAtom;
  static nsIAtom* kTextNodeAtom;
  static nsIAtom* kCDataSectionNodeAtom;
  static nsIAtom* kEntityReferenceNodeAtom;
  static nsIAtom* kEntityNodeAtom;
  static nsIAtom* kProcessingInstructionNodeAtom;
  static nsIAtom* kCommentNodeAtom;
  static nsIAtom* kDocumentNodeAtom;
  static nsIAtom* kDocumentTypeNodeAtom;
  static nsIAtom* kDocumentFragmentNodeAtom;
  static nsIAtom* kNotationNodeAtom;

  nsCOMPtr<nsITreeBoxObject> mTree;
  nsCOMPtr<nsITreeSelection> mSelection;

  PRPackedBool mShowAnonymous;
  PRPackedBool mShowSubDocuments;
  PRPackedBool mShowWhitespaceNodes;
  PRUint32 mFilters;

  nsCOMPtr<nsIDOMNode> mRootNode;
  nsCOMPtr<nsIDOMDocument> mRootDocument;

  nsVoidArray mNodes;

  inDOMViewNode* GetNodeAt(PRInt32 aIndex);
  PRInt32 GetRowCount();
  PRBool RowOutOfBounds(PRInt32 aRow, PRInt32 aCount);
  inDOMViewNode* CreateNode(nsIDOMNode* aNode, inDOMViewNode* aParent);
  void AppendNode(inDOMViewNode* aNode);
  void InsertNode(inDOMViewNode* aNode, PRInt32 aIndex);
  void RemoveNode(PRInt32 aIndex);
  void ReplaceNode(inDOMViewNode* aNode, PRInt32 aIndex);
  void InsertNodes(nsVoidArray& aNodes, PRInt32 aIndex);
  void RemoveNodes(PRInt32 aIndex, PRInt32 aCount);
  void RemoveAllNodes();
  void ExpandNode(PRInt32 aRow);
  void CollapseNode(PRInt32 aRow);

  nsresult RowToNode(PRInt32 aRow, inDOMViewNode** aNode);
  nsresult NodeToRow(nsIDOMNode* aNode, PRInt32* aRow);

  void InsertLinkAfter(inDOMViewNode* aNode, inDOMViewNode* aInsertAfter);
  void InsertLinkBefore(inDOMViewNode* aNode, inDOMViewNode* aInsertBefore);
  void RemoveLink(inDOMViewNode* aNode);
  void ReplaceLink(inDOMViewNode* aNewNode, inDOMViewNode* aOldNode);

  inline PRUint16 GetNodeTypeKey(PRUint16 aType)
  {
    NS_ASSERTION(aType < 16, "You doofus");
    return 1 << aType;
  };

  nsresult GetChildNodesFor(nsIDOMNode* aNode, nsISupportsArray **aResult);
  nsresult AppendKidsToArray(nsIDOMNodeList* aKids, nsISupportsArray* aArray);
  nsresult AppendAttrsToArray(nsIDOMNamedNodeMap* aKids, nsISupportsArray* aArray);
  nsresult GetFirstDescendantOf(inDOMViewNode* aNode, PRInt32 aRow, PRInt32* aResult);
  nsresult GetLastDescendantOf(inDOMViewNode* aNode, PRInt32 aRow, PRInt32* aResult);
  nsresult GetRealParent(nsIDOMNode* aNode, nsIDOMNode** aParent);
  nsresult GetRealPreviousSibling(nsIDOMNode* aNode, nsIDOMNode* aRealParent, nsIDOMNode** aSibling);
};

#endif // __inDOMView_h__


