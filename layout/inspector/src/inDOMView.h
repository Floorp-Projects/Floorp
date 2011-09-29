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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


