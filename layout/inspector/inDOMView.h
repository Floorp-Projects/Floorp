/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsCOMPtr.h"

class inDOMViewNode;
class nsIDOMMozNamedAttrMap;

class inDOMView : public inIDOMView,
                  public nsITreeView,
                  public nsStubMutationObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INIDOMVIEW
  NS_DECL_NSITREEVIEW

  inDOMView();

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

protected:
  virtual ~inDOMView();

  nsCOMPtr<nsITreeBoxObject> mTree;
  nsCOMPtr<nsITreeSelection> mSelection;
  nsCOMPtr<inIDOMUtils> mDOMUtils;

  bool mShowAnonymous;
  bool mShowSubDocuments;
  bool mShowWhitespaceNodes;
  bool mShowAccessibleNodes;
  uint32_t mWhatToShow;

  nsCOMPtr<nsIDOMNode> mRootNode;
  nsCOMPtr<nsIDOMDocument> mRootDocument;

  nsTArray<inDOMViewNode*> mNodes;

  inDOMViewNode* GetNodeAt(int32_t aIndex);
  int32_t GetRowCount();
  int32_t NodeToRow(inDOMViewNode* aNode);
  bool RowOutOfBounds(int32_t aRow, int32_t aCount);
  inDOMViewNode* CreateNode(nsIDOMNode* aNode, inDOMViewNode* aParent);
  void AppendNode(inDOMViewNode* aNode);
  void InsertNode(inDOMViewNode* aNode, int32_t aIndex);
  void RemoveNode(int32_t aIndex);
  void ReplaceNode(inDOMViewNode* aNode, int32_t aIndex);
  void InsertNodes(nsTArray<inDOMViewNode*>& aNodes, int32_t aIndex);
  void RemoveNodes(int32_t aIndex, int32_t aCount);
  void RemoveAllNodes();
  void ExpandNode(int32_t aRow);
  void CollapseNode(int32_t aRow);

  nsresult RowToNode(int32_t aRow, inDOMViewNode** aNode);
  nsresult NodeToRow(nsIDOMNode* aNode, int32_t* aRow);

  void InsertLinkAfter(inDOMViewNode* aNode, inDOMViewNode* aInsertAfter);
  void InsertLinkBefore(inDOMViewNode* aNode, inDOMViewNode* aInsertBefore);
  void RemoveLink(inDOMViewNode* aNode);
  void ReplaceLink(inDOMViewNode* aNewNode, inDOMViewNode* aOldNode);

  nsresult GetChildNodesFor(nsIDOMNode* aNode, nsCOMArray<nsIDOMNode>& aResult);
  nsresult AppendKidsToArray(nsINodeList* aKids, nsCOMArray<nsIDOMNode>& aArray);
  nsresult AppendAttrsToArray(nsIDOMMozNamedAttrMap* aKids, nsCOMArray<nsIDOMNode>& aArray);
  nsresult GetFirstDescendantOf(inDOMViewNode* aNode, int32_t aRow, int32_t* aResult);
  nsresult GetLastDescendantOf(inDOMViewNode* aNode, int32_t aRow, int32_t* aResult);
  nsresult GetRealPreviousSibling(nsIDOMNode* aNode, nsIDOMNode* aRealParent, nsIDOMNode** aSibling);
};

// {FB5C1775-1BBD-4b9c-ABB0-AE7ACD29E87E}
#define IN_DOMVIEW_CID \
{ 0xfb5c1775, 0x1bbd, 0x4b9c, { 0xab, 0xb0, 0xae, 0x7a, 0xcd, 0x29, 0xe8, 0x7e } }

#endif // __inDOMView_h__


