/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
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

/**
 * The definitions of objects that make up a history query result set. This file
 * should only be included by nsNavHistory.h, include that if you want these
 * classes.
 */

#ifndef nsNavHistoryResult_h_
#define nsNavHistoryResult_h_

class nsNavHistory;
class nsNavHistoryResult;
class nsIDateTimeFormat;
class nsNavHistoryQueryOptions;

// Declare methods for implementing nsINavBookmarkObserver
// and nsINavHistoryObserver (some methods, such as BeginUpdateBatch overlap)
#define NS_DECL_BOOKMARK_HISTORY_OBSERVER                               \
  NS_DECL_NSINAVBOOKMARKOBSERVER                                        \
  NS_IMETHOD OnVisit(nsIURI* aURI, PRInt64 aVisitID, PRTime aTime,      \
                     PRInt64 aSessionID, PRInt64 aReferringID,          \
                     PRUint32 aTransitionType);                         \
  NS_IMETHOD OnTitleChanged(nsIURI* aURI, const nsAString& aPageTitle,  \
                            const nsAString& aUserTitle,                \
                            PRBool aIsUserTitleChanged);                \
  NS_IMETHOD OnDeleteURI(nsIURI *aURI);                                 \
  NS_IMETHOD OnClearHistory();                                          \
  NS_IMETHOD OnPageChanged(nsIURI *aURI, PRUint32 aWhat,                \
                           const nsAString &aValue);

// nsNavHistoryResultNode
//
//    This is the base class for every node in a result set. The result itself
//    is a node (nsNavHistoryResult inherits from this), as well as every
//    leaf and branch on the tree.

#define NS_NAVHISTORYRESULTNODE_IID \
  {0x54b61d38, 0x57c1, 0x11da, {0x95, 0xb8, 0x00, 0x13, 0x21, 0xc9, 0xf6, 0x9e}}

class nsNavHistoryResultNode : public nsINavHistoryResultNode
{
public:
  nsNavHistoryResultNode();

#ifdef MOZILLA_1_8_BRANCH
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_NAVHISTORYRESULTNODE_IID)
#else
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYRESULTNODE_IID)
#endif

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYRESULTNODE

  // History/bookmark notifications.  Note that we don't actually inherit
  // these interfaces since multiple-inheritance breaks nsCOMArray.
  NS_DECL_BOOKMARK_HISTORY_OBSERVER

  // Generate the children for this node.
  virtual nsresult BuildChildren(PRBool *aBuilt) {
    *aBuilt = PR_FALSE;
    return NS_OK;;
  }

  // Rebuild the node's data.  This only applies to nodes which have
  // a URL or a folder ID, and does _not_ rebuild the node's children.
  virtual nsresult Rebuild();

  // Non-XPCOM member accessors
  PRInt32 Type() const { return mType; }
  const nsCString& URL() const { return mUrl; }
  virtual PRInt64 FolderId() const { return 0; }
  PRInt32 VisibleIndex() const { return mVisibleIndex; }
  void SetVisibleIndex(PRInt32 aIndex) { mVisibleIndex = aIndex; }
  PRInt32 IndentLevel() const { return mIndentLevel; }
  void SetIndentLevel(PRInt32 aLevel) { mIndentLevel = aLevel; }
  nsNavHistoryResultNode* Parent() { return mParent; }
  void SetParent(nsNavHistoryResultNode *aParent) { mParent = aParent; }

  nsNavHistoryResultNode* ChildAt(PRInt32 aIndex) { return mChildren[aIndex]; }

protected:
  virtual ~nsNavHistoryResultNode() {}

  // Find the top-level NavHistoryResult for this node
  nsNavHistoryResult* GetResult();

  // parent of this element, NULL if no parent. Filled in by FilledAllResults
  // in the result set.
  nsNavHistoryResultNode* mParent;

  // these are all the basic row info, filled in by nsNavHistory::RowToResult
  PRInt64 mID; // keep this for comparing dups, not exposed in interface
               // might be 0, indicating some kind of parent node
  PRInt32 mType;
  nsCString mUrl;
  nsString mTitle;
  PRInt32 mAccessCount;
  PRTime mTime;
  nsString mHost;
  nsCString mFaviconURL;
  PRInt64 mSessionID;

  // Filled in by the result type generator in nsNavHistory
  nsCOMArray<nsNavHistoryResultNode> mChildren;

  // filled in by FillledAllResults in the result set.
  PRInt32 mIndentLevel;

  // index of this element into the mVisibleElements array in the result set
  PRInt32 mVisibleIndex;

  // when there are children, this stores the open state in the tree
  // this is set to the default in the constructor
  PRBool mExpanded;

  friend class nsNavHistory;
  friend class nsNavHistoryResult;
  friend class nsNavBookmarks;
};

// nsNavHistoryQeuryNode
//
//    nsNavHistoryQueryNode is a special type of ResultNode that executes a
//    query when asked to build its children.

class nsNavHistoryQueryNode : public nsNavHistoryResultNode
{
public:
  nsNavHistoryQueryNode()
    : mQueries(nsnull), mQueryCount(0), mBuiltChildren(PR_FALSE) {}

  // nsINavHistoryResultNode methods
  NS_IMETHOD GetFolderId(PRInt64 *aId)
  { *aId = nsNavHistoryQueryNode::FolderId(); return NS_OK; }
  NS_IMETHOD GetFolderType(nsAString& aFolderType);
  NS_IMETHOD GetQueries(PRUint32 *aQueryCount,
                        nsINavHistoryQuery ***aQueries);
  NS_IMETHOD GetQueryOptions(nsINavHistoryQueryOptions **aOptions);
  NS_IMETHOD GetChildrenReadOnly(PRBool *aResult);

  NS_DECL_BOOKMARK_HISTORY_OBSERVER

  // nsNavHistoryResultNode methods
  virtual nsresult BuildChildren(PRBool *aBuilt);
  virtual PRInt64 FolderId() const;
  virtual nsresult Rebuild();

protected:
  virtual ~nsNavHistoryQueryNode();
  nsresult ParseQueries();
  nsresult CreateNode(nsIURI *aURI, nsNavHistoryResultNode **aNode);
  nsresult UpdateQuery();
  PRBool HasFilteredChildren() const;

  nsINavHistoryQuery **mQueries;
  PRUint32 mQueryCount;
  nsCOMPtr<nsNavHistoryQueryOptions> mOptions;
  PRBool mBuiltChildren;
  nsString mFolderType;

  friend class nsNavBookmarks;
};


// nsNavHistoryResult
//
//    nsNavHistory creates this object and fills in mChildren (by getting
//    it through GetTopLevel()). Then FilledAllResults() is called to finish
//    object initialization.
//
//    This object implements nsITreeView so you can just set it to a tree
//    view and it will work. This object also observes the necessary history
//    and bookmark events to keep itself up-to-date.

class nsNavHistoryResult : public nsNavHistoryQueryNode,
                           public nsSupportsWeakReference,
                           public nsINavHistoryResult,
                           public nsITreeView,
                           public nsINavBookmarkObserver,
                           public nsINavHistoryObserver
{
public:
  nsNavHistoryResult(nsNavHistory* aHistoryService,
                     nsIStringBundle* aHistoryBundle,
                     nsINavHistoryQuery** aQueries,
                     PRUint32 aQueryCount,
                     nsNavHistoryQueryOptions *aOptions);

  // Two-stage init, MUST BE CALLED BEFORE ANYTHING ELSE
  nsresult Init();

  nsCOMArray<nsNavHistoryResultNode>* GetTopLevel() { return &mChildren; }
  void ApplyTreeState(const nsDataHashtable<nsStringHashKey, int>& aExpanded);
  void FilledAllResults();

  nsresult BuildChildrenFor(nsNavHistoryResultNode *aNode);

  // These methods are typically called by child nodes from one of the
  // history or bookmark observer notifications.

  // Notify the result that the entire contents of the tree have changed.
  void Invalidate();

  // Notify the result that a row has been added at index aIndex relative
  // to aParent.
  void RowAdded(nsNavHistoryResultNode* aParent, PRInt32 aIndex);

  // Notify the result that the row with visible index aVisibleIndex has been
  // removed from the tree.
  void RowRemoved(PRInt32 aVisibleIndex);

  // Notify the result that the contents of the row at visible index
  // aVisibleIndex has been modified.
  void RowChanged(PRInt32 aVisibleIndex);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSINAVHISTORYRESULT
  NS_DECL_NSITREEVIEW
  NS_FORWARD_NSINAVBOOKMARKOBSERVER(nsNavHistoryQueryNode::)
  NS_IMETHOD OnVisit(nsIURI* aURI, PRInt64 aVisitID, PRTime aTime,
                     PRInt64 aSessionID, PRInt64 aReferringID,
                     PRUint32 aTransitionType)
  { return nsNavHistoryQueryNode::OnVisit(aURI, aVisitID, aTime, aSessionID,
                                          aReferringID, aTransitionType); }
  NS_IMETHOD OnDeleteURI(nsIURI *aURI)
  { return nsNavHistoryQueryNode::OnDeleteURI(aURI); }
  NS_IMETHOD OnClearHistory()
  { return nsNavHistoryQueryNode::OnClearHistory(); }
  NS_IMETHOD OnTitleChanged(nsIURI* aURI, const nsAString& aPageTitle,
                            const nsAString& aUserTitle,
                            PRBool aIsUserTitleChanged)
  { return nsNavHistoryQueryNode::OnTitleChanged(aURI, aPageTitle, aUserTitle,
                                                 aIsUserTitleChanged); }
  NS_IMETHOD OnPageChanged(nsIURI *aURI, PRUint32 aWhat,
                           const nsAString &aValue)
  { return nsNavHistoryQueryNode::OnPageChanged(aURI, aWhat, aValue); }

  NS_FORWARD_NSINAVHISTORYRESULTNODE(nsNavHistoryQueryNode::)

private:
  ~nsNavHistoryResult();

  nsCOMPtr<nsIStringBundle> mBundle;
  nsCOMPtr<nsITreeBoxObject> mTree; // may be null if no tree has bound itself
  nsCOMPtr<nsITreeSelection> mSelection; // may be null

  nsRefPtr<nsNavHistory> mHistoryService;

  PRBool mCollapseDuplicates;

  nsMaybeWeakPtrArray<nsINavHistoryResultViewObserver> mObservers;

  // for locale-specific date formatting and string sorting
  nsCOMPtr<nsILocale> mLocale;
  nsCOMPtr<nsICollation> mCollation;
  nsCOMPtr<nsIDateTimeFormat> mDateFormatter;

  // this is the flattened version of the hierarchy containing everything
  nsVoidArray mAllElements;
  nsNavHistoryResultNode* AllElementAt(PRInt32 index)
  {
    return (nsNavHistoryResultNode*)mAllElements[index];
  }

  nsVoidArray mVisibleElements;
  nsNavHistoryResultNode* VisibleElementAt(PRInt32 index)
  {
    return (nsNavHistoryResultNode*)mVisibleElements[index];
  }

  // This value indicates whether we should try to compute session boundaries.
  // It is cached so we don't have to compute it every time we want to get a
  // row style.
  PRBool mShowSessions;
  void ComputeShowSessions();

  void FillTreeStats(nsNavHistoryResultNode* aResult, PRInt32 aLevel);
  void InitializeVisibleList();
  void RebuildList();
  void RebuildAllListRecurse(const nsCOMArray<nsNavHistoryResultNode>& aSource);
  void BuildVisibleSection(const nsCOMArray<nsNavHistoryResultNode>& aSources,
                           nsVoidArray* aVisible);
  void InsertVisibleSection(const nsVoidArray& aAddition, PRInt32 aInsertHere);
  PRInt32 DeleteVisibleChildrenOf(PRInt32 aIndex);

  void RecursiveSortArray(nsCOMArray<nsNavHistoryResultNode>& aSources,
                          PRUint32 aSortingMode);
  void SetTreeSortingIndicator();

  void RecursiveApplyTreeState(
      nsCOMArray<nsNavHistoryResultNode>& aList,
      const nsDataHashtable<nsStringHashKey, int>& aExpanded);
  void RecursiveExpandCollapse(nsCOMArray<nsNavHistoryResultNode>& aList,
                               PRBool aExpand);

  enum ColumnType { Column_Unknown = -1, Column_Title, Column_URL, Column_Date, Column_VisitCount };
  ColumnType GetColumnType(nsITreeColumn* col);
  ColumnType SortTypeToColumnType(PRUint32 aSortType,
                                  PRBool* aDescending = nsnull);

  PR_STATIC_CALLBACK(int) SortComparison_TitleLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_TitleGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_DateLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_DateGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_URLLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_URLGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_VisitCountLess(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);
  PR_STATIC_CALLBACK(int) SortComparison_VisitCountGreater(
      nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure);

  nsresult FormatFriendlyTime(PRTime aTime, nsAString& aResult);
};

#endif // nsNavHistoryResult_h_
