/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __inDOMView_h__
#define __inDOMView_h__

#include "inIDOMView.h"
#include "inIDOMUtils.h"

#include "nsITreeView.h"
#include "nsITreeSelection.h"
#include "nsStubMutationObserver.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsStaticAtom.h"

class inDOMViewNode;

class inDOMView : public inIDOMView,
                  public nsITreeView,
                  public nsStubMutationObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INIDOMVIEW
  NS_DECL_NSITREEVIEW

  inDOMView();
  virtual ~inDOMView();

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  static void InitAtoms();

protected:

#define DOMVIEW_ATOM(name_, value_) static nsIAtom* name_;
#include "inDOMViewAtomList.h"
#undef DOMVIEW_ATOM

  static const nsStaticAtom Atoms_info[]; 

  nsCOMPtr<nsITreeBoxObject> mTree;
  nsCOMPtr<nsITreeSelection> mSelection;
  nsCOMPtr<inIDOMUtils> mDOMUtils;

  bool mShowAnonymous;
  bool mShowSubDocuments;
  bool mShowWhitespaceNodes;
  bool mShowAccessibleNodes;
  PRUint32 mWhatToShow;

  nsCOMPtr<nsIDOMNode> mRootNode;
  nsCOMPtr<nsIDOMDocument> mRootDocument;

  nsTArray<inDOMViewNode*> mNodes;

  inDOMViewNode* GetNodeAt(PRInt32 aIndex);
  PRInt32 GetRowCount();
  PRInt32 NodeToRow(inDOMViewNode* aNode);
  bool RowOutOfBounds(PRInt32 aRow, PRInt32 aCount);
  inDOMViewNode* CreateNode(nsIDOMNode* aNode, inDOMViewNode* aParent);
  void AppendNode(inDOMViewNode* aNode);
  void InsertNode(inDOMViewNode* aNode, PRInt32 aIndex);
  void RemoveNode(PRInt32 aIndex);
  void ReplaceNode(inDOMViewNode* aNode, PRInt32 aIndex);
  void InsertNodes(nsTArray<inDOMViewNode*>& aNodes, PRInt32 aIndex);
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

  nsresult GetChildNodesFor(nsIDOMNode* aNode, nsCOMArray<nsIDOMNode>& aResult);
  nsresult AppendKidsToArray(nsIDOMNodeList* aKids, nsCOMArray<nsIDOMNode>& aArray);
  nsresult AppendAttrsToArray(nsIDOMNamedNodeMap* aKids, nsCOMArray<nsIDOMNode>& aArray);
  nsresult GetFirstDescendantOf(inDOMViewNode* aNode, PRInt32 aRow, PRInt32* aResult);
  nsresult GetLastDescendantOf(inDOMViewNode* aNode, PRInt32 aRow, PRInt32* aResult);
  nsresult GetRealPreviousSibling(nsIDOMNode* aNode, nsIDOMNode* aRealParent, nsIDOMNode** aSibling);
};

// {FB5C1775-1BBD-4b9c-ABB0-AE7ACD29E87E}
#define IN_DOMVIEW_CID \
{ 0xfb5c1775, 0x1bbd, 0x4b9c, { 0xab, 0xb0, 0xae, 0x7a, 0xcd, 0x29, 0xe8, 0x7e } }

#endif // __inDOMView_h__


