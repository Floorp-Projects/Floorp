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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com> (original author)
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

#include "nsNavBookmarks.h"
#include "nsNavHistory.h"
#include "mozStorageHelper.h"
#include "nsIServiceManager.h"

const PRInt32 nsNavBookmarks::kFindBookmarksIndex_ItemChild = 0;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_FolderChild = 1;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Parent = 2;
const PRInt32 nsNavBookmarks::kFindBookmarksIndex_Position = 3;

const PRInt32 nsNavBookmarks::kGetFolderInfoIndex_FolderID = 0;
const PRInt32 nsNavBookmarks::kGetFolderInfoIndex_Title = 1;

// These columns sit to the right of the kGetInfoIndex_* columns.
const PRInt32 nsNavBookmarks::kGetChildrenIndex_Position = 6;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_ItemChild = 7;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_FolderChild = 8;
const PRInt32 nsNavBookmarks::kGetChildrenIndex_FolderTitle = 9;

nsNavBookmarks* nsNavBookmarks::sInstance = nsnull;

nsNavBookmarks::nsNavBookmarks()
{
  NS_ASSERTION(!sInstance, "Multiple nsNavBookmarks instances!");
  sInstance = this;
}

nsNavBookmarks::~nsNavBookmarks()
{
  NS_ASSERTION(sInstance == this, "Expected sInstance == this");
  sInstance = nsnull;
}

NS_IMPL_ISUPPORTS2(nsNavBookmarks,
                   nsINavBookmarksService, nsINavHistoryObserver)

nsresult
nsNavBookmarks::Init()
{
  nsresult rv;

  nsNavHistory *history = History();
  NS_ENSURE_TRUE(history, NS_ERROR_UNEXPECTED);
  history->AddObserver(this); // allows us to notify on title changes
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRBool exists = PR_FALSE;
  dbConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks_assoc"), &exists);
  if (!exists) {
    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_bookmarks_assoc (item_child INTEGER, folder_child INTEGER, parent INTEGER, position INTEGER)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  dbConn->TableExists(NS_LITERAL_CSTRING("moz_bookmarks_containers"), &exists);
  if (!exists) {
    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_bookmarks_containers (id INTEGER PRIMARY KEY, name LONGVARCHAR)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mRoot = 0;
  {
    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT folder_child FROM moz_bookmarks_assoc WHERE parent IS NULL"),
                                 getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = statement->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);
    if (results) {
      mRoot = statement->AsInt64(0);
    }
  }

  if (!mRoot) {
    // Create the root container
    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks_containers (name) VALUES (NULL)"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dbConn->GetLastInsertRowID(&mRoot);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString buffer;
    buffer.AssignLiteral("INSERT INTO moz_bookmarks_assoc (folder_child) VALUES(");
    buffer.AppendInt(mRoot);
    buffer.AppendLiteral(")");

    rv = dbConn->ExecuteSimpleSQL(buffer);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT a.* FROM moz_bookmarks_assoc a, moz_history h WHERE h.url = ?1 AND a.item_child = h.id"),
                               getter_AddRefs(mDBFindURIBookmarks));
  NS_ENSURE_SUCCESS(rv, rv);

  // This gigantic statement constructs a result where the first columns exactly match
  // those returned by mDBGetVisitPageInfo, and additionally contains columns for
  // position, item_child, and folder_child from moz_bookmarks_assoc, and name from
  // moz_bookmarks_containers.  The end result is a list of all folder and item children
  // for a given folder id, sorted by position.
  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT h.id, h.url, h.title, h.visit_count, MAX(fullv.visit_date), h.rev_host, a.position, a.item_child, a.folder_child, null FROM moz_bookmarks_assoc a JOIN moz_history h ON a.item_child = h.id LEFT JOIN moz_historyvisit v ON h.id = v.page_id LEFT JOIN moz_historyvisit fullv ON h.id = fullv.page_id WHERE a.parent = ?1 GROUP BY h.id UNION ALL SELECT null, null, null, null, null, null, a.position, a.item_child, a.folder_child, c.name FROM moz_bookmarks_assoc a JOIN moz_bookmarks_containers c ON c.id = a.folder_child WHERE a.parent = ?1 ORDER BY a.position"),
                               getter_AddRefs(mDBGetChildren));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING("SELECT COUNT(*) FROM moz_bookmarks_assoc WHERE parent = ?1"),
                               getter_AddRefs(mDBFolderCount));
  NS_ENSURE_SUCCESS(rv, rv);

  return transaction.Commit();
}

nsresult
nsNavBookmarks::AdjustIndices(PRInt64 aFolder,
                              PRInt32 aStartIndex, PRInt32 aEndIndex,
                              PRInt32 aDelta)
{
  nsCAutoString buffer;
  buffer.AssignLiteral("UPDATE moz_bookmarks_assoc SET position = position + ");
  buffer.AppendInt(aDelta);
  buffer.AppendLiteral(" WHERE parent = ");
  buffer.AppendInt(aFolder);

  if (aStartIndex != 0) {
    buffer.AppendLiteral(" AND position >= ");
    buffer.AppendInt(aStartIndex);
  }
  if (aEndIndex != -1) {
    buffer.AppendLiteral(" AND position <= ");
    buffer.AppendInt(aEndIndex);
  }

  // TODO notify observers about renumbering?

  return DBConn()->ExecuteSimpleSQL(buffer);
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarksRoot(PRInt64 *aRoot)
{
  *aRoot = mRoot;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::InsertItem(PRInt64 aFolder, nsIURI *aItem, PRInt32 aIndex)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt64 childID;
  nsresult rv = History()->GetUrlIdFor(aItem, &childID, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 index = (aIndex == -1) ? FolderCount(aFolder) : aIndex;

  rv = AdjustIndices(aFolder, index, -1, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString buffer;
  buffer.AssignLiteral("INSERT INTO moz_bookmarks_assoc (item_child, parent, position) VALUES (");
  buffer.AppendInt(childID);
  buffer.AppendLiteral(", ");
  buffer.AppendInt(aFolder);
  buffer.AppendLiteral(", ");
  buffer.AppendInt(index);
  buffer.AppendLiteral(")");

  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnItemAdded(aItem, aFolder, index);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::RemoveItem(PRInt64 aFolder, nsIURI *aItem)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt64 childID;
  nsresult rv = History()->GetUrlIdFor(aItem, &childID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (childID == 0) {
    return NS_OK; // the item isn't in history at all
  }

  PRInt32 childIndex;
  nsCAutoString buffer;
  {
    buffer.AssignLiteral("SELECT position FROM moz_bookmarks_assoc WHERE parent = ");
    buffer.AppendInt(aFolder);
    buffer.AppendLiteral(" AND item_child = ");
    buffer.AppendInt(childID);

    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(buffer, getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = statement->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);

    // We _should_ always have a result here, but maybe we don't if the table
    // has become corrupted.  Just silently skip adjusting indices.
    childIndex = results ? statement->AsInt32(0) : -1;
  }

  buffer.AssignLiteral("DELETE FROM moz_bookmarks_assoc WHERE parent = ");
  buffer.AppendInt(aFolder);
  buffer.AppendLiteral(" AND item_child = ");
  buffer.AppendInt(childID);

  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  if (childIndex != -1) {
    rv = AdjustIndices(aFolder, childIndex + 1, -1, -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnItemRemoved(aItem, aFolder, childIndex);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::CreateFolder(PRInt64 aParent, const nsAString &aName,
                             PRInt32 aIndex, PRInt64 *aNewFolder)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  PRInt32 index = (aIndex == -1) ? FolderCount(aParent) : aIndex;

  nsresult rv = AdjustIndices(aParent, index, -1, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("INSERT INTO moz_bookmarks_containers (name) VALUES (") +
                                NS_ConvertUTF16toUTF8(aName) +
                                NS_LITERAL_CSTRING(")"));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 child;
  rv = dbConn->GetLastInsertRowID(&child);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString buffer;
  buffer.AssignLiteral("INSERT INTO moz_bookmarks_assoc (folder_child, parent, position) VALUES (");
  buffer.AppendInt(child);
  buffer.AppendLiteral(", ");
  buffer.AppendInt(aParent);
  buffer.AppendLiteral(", ");
  buffer.AppendInt(index);
  buffer.AppendLiteral(")");
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnFolderAdded(child, aParent, index);
  }

  *aNewFolder = child;
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::RemoveFolder(PRInt64 aFolder)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCAutoString buffer;
  buffer.AssignLiteral("SELECT parent, position FROM moz_bookmarks_assoc WHERE folder_child = ");
  buffer.AppendInt(aFolder);

  nsresult rv;
  PRInt64 parent;
  PRInt32 index;
  {
    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(buffer, getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = statement->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!results) {
      return NS_ERROR_INVALID_ARG; // folder is not in the hierarchy
    }

    parent = statement->AsInt64(0);
    index = statement->AsInt32(1);
  }

  buffer.AssignLiteral("DELETE FROM moz_bookmarks_containers WHERE id = ");
  buffer.AppendInt(aFolder);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  buffer.AssignLiteral("DELETE FROM moz_bookmarks_assoc WHERE folder_child = ");
  buffer.AppendInt(aFolder);
  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AdjustIndices(parent, index + 1, -1, -1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnFolderRemoved(aFolder, parent, index);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::MoveFolder(PRInt64 aFolder, PRInt64 aNewParent, PRInt32 aIndex)
{
  mozIStorageConnection *dbConn = DBConn();
  mozStorageTransaction transaction(dbConn, PR_FALSE);

  nsCAutoString buffer;
  buffer.AssignLiteral("SELECT parent, position FROM moz_bookmarks_assoc WHERE folder_child = ");
  buffer.AppendInt(aFolder);

  nsresult rv;
  PRInt64 parent;
  PRInt32 index;

  {
    nsCOMPtr<mozIStorageStatement> statement;
    rv = dbConn->CreateStatement(buffer, getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool results;
    rv = statement->ExecuteStep(&results);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!results) {
      return NS_ERROR_INVALID_ARG; // folder is not in the hierarchy
    }

    parent = statement->AsInt64(0);
    index = statement->AsInt32(1);
  }

  if (aNewParent == parent && aIndex == index) {
    // Nothing to do!
    return NS_OK;
  }

  if (parent == aNewParent) {
    // We can optimize the updates if moving within the same container
    if (index > aIndex) {
      rv = AdjustIndices(parent, aIndex, index - 1, 1);
    } else {
      rv = AdjustIndices(parent, index + 1, aIndex, -1);
    }
  } else {
    rv = AdjustIndices(parent, index + 1, -1, -1);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AdjustIndices(aNewParent, aIndex, -1, 1);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  buffer.AssignLiteral("UPDATE moz_bookmarks_assoc SET parent = ");
  buffer.AppendInt(aNewParent);
  buffer.AppendLiteral(", position = ");
  buffer.AppendInt(aIndex);
  buffer.AppendLiteral(" WHERE id = ");
  buffer.AppendInt(aFolder);

  rv = dbConn->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnFolderMoved(aFolder, aNewParent, aIndex);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::SetItemTitle(nsIURI *aURI, const nsAString &aTitle)
{
  return History()->SetPageTitle(aURI, aTitle);
}


NS_IMETHODIMP
nsNavBookmarks::GetItemTitle(nsIURI *aURI, nsAString &aTitle)
{
  mozIStorageStatement *statement = DBGetURLPageInfo();
  nsresult rv = BindStatementURI(statement, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageStatementScoper scope(statement);

  PRBool results;
  rv = statement->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results) {
    return NS_ERROR_INVALID_ARG;
  }

  return statement->GetString(nsNavHistory::kGetInfoIndex_Title, aTitle);
}

NS_IMETHODIMP
nsNavBookmarks::SetFolderTitle(PRInt64 aFolder, const nsAString &aTitle)
{
  nsCAutoString buffer;
  buffer.AssignLiteral("UPDATE moz_bookmarks_container SET title = ");
  AppendUTF16toUTF8(aTitle, buffer);
  buffer.AppendLiteral(" WHERE id = ");
  buffer.AppendInt(aFolder);

  nsresult rv = DBConn()->ExecuteSimpleSQL(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnFolderChanged(aFolder, NS_LITERAL_CSTRING("title"));
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetFolderTitle(PRInt64 aFolder, nsAString &aTitle)
{
  mozStorageStatementScoper scope(mDBGetFolderInfo);
  nsresult rv = mDBGetFolderInfo->BindInt64Parameter(0, aFolder);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = mDBGetFolderInfo->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results) {
    return NS_ERROR_INVALID_ARG;
  }

  return mDBGetFolderInfo->GetString(kGetFolderInfoIndex_Title, aTitle);
}

NS_IMETHODIMP
nsNavFolderResultNode::GetChildCount(PRInt32 *aChildCount)
{
  if (!mQueriedChildren) {
    nsresult rv = nsNavBookmarks::sInstance->FillFolderChildren(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return nsNavHistoryResultNode::GetChildCount(aChildCount);
}

NS_IMETHODIMP
nsNavFolderResultNode::GetChild(PRInt32 aIndex,
                                nsINavHistoryResultNode **aChild)
{
  if (!mQueriedChildren) {
    nsresult rv = nsNavBookmarks::sInstance->FillFolderChildren(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return nsNavHistoryResultNode::GetChild(aIndex, aChild);
}

nsresult
nsNavBookmarks::ResultNodeForFolder(PRInt64 aID,
                                    const nsString &aTitle,
                                    nsNavHistoryResultNode **aNode)
{
  nsNavFolderResultNode *node = new nsNavFolderResultNode();
  NS_ENSURE_TRUE(node, NS_ERROR_OUT_OF_MEMORY);

  node->mID = aID;
  node->mType = nsINavHistoryResultNode::RESULT_TYPE_FOLDER;
  node->mTitle = aTitle;
  node->mAccessCount = 0;
  node->mTime = 0;

  NS_ADDREF(*aNode = node);
  return NS_OK;
}


NS_IMETHODIMP
nsNavBookmarks::GetBookmarks(nsINavHistoryResult **aResult)
{
  *aResult = nsnull;

  nsNavHistory *history = History();
  nsRefPtr<nsNavHistoryResult> result(History()->NewHistoryResult());
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = result->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  // Fill the result with a single result node for the bookmarks root.
  nsCOMPtr<nsNavHistoryResultNode> topNode;
  rv = ResultNodeForFolder(mRoot, EmptyString(), getter_AddRefs(topNode));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(result->GetTopLevel()->AppendObject(topNode), 
                 NS_ERROR_OUT_OF_MEMORY);

  result->FilledAllResults();

  NS_STATIC_CAST(nsRefPtr<nsINavHistoryResult>, result).swap(*aResult);
  return NS_OK;
}

nsresult
nsNavBookmarks::FillFolderChildren(nsNavFolderResultNode *aNode)
{
  NS_ASSERTION(!aNode->mQueriedChildren, "children already queried");
  NS_ASSERTION(aNode->mChildren.Count() == 0, "already have child nodes");

  mozStorageStatementScoper scope(mDBGetChildren);
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  nsresult rv = mDBGetChildren->BindInt64Parameter(0, aNode->mID);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMArray<nsNavHistoryResultNode>* children = &aNode->mChildren;

  PRBool results;
  while (NS_SUCCEEDED(mDBGetChildren->ExecuteStep(&results)) && results) {
    nsCOMPtr<nsNavHistoryResultNode> node;
    if (mDBGetChildren->IsNull(kGetChildrenIndex_FolderChild)) {
      rv = History()->RowToResult(mDBGetChildren, PR_FALSE, getter_AddRefs(node));
    } else {
      PRInt64 folder = mDBGetChildren->AsInt64(kGetChildrenIndex_FolderChild);
      nsAutoString title;
      mDBGetChildren->GetString(kGetChildrenIndex_FolderTitle, title);
      rv = ResultNodeForFolder(folder, title, getter_AddRefs(node));
    }

    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(children->AppendObject(node), NS_ERROR_OUT_OF_MEMORY);
  }

  aNode->mQueriedChildren = PR_TRUE;
  return transaction.Commit();
}

PRInt32
nsNavBookmarks::FolderCount(PRInt64 aFolder)
{
  mozStorageStatementScoper scope(mDBFolderCount);

  nsresult rv = mDBFolderCount->BindInt64Parameter(0, aFolder);
  NS_ENSURE_SUCCESS(rv, 0);

  PRBool results;
  rv = mDBFolderCount->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  return mDBFolderCount->AsInt32(0);
}

NS_IMETHODIMP
nsNavBookmarks::IsBookmarked(nsIURI *aURI, PRBool *aBookmarked)
{
  *aBookmarked = PR_FALSE;

  mozStorageStatementScoper scope(mDBFindURIBookmarks);

  nsresult rv = BindStatementURI(mDBFindURIBookmarks, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBFindURIBookmarks->ExecuteStep(aBookmarked);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetBookmarkCategories(nsIURI *aURI, PRUint32 *aCount,
                                      PRInt64 **aCategories)
{
  *aCount = 0;
  *aCategories = nsnull;

  mozStorageStatementScoper scope(mDBFindURIBookmarks);
  mozStorageTransaction transaction(DBConn(), PR_FALSE);

  nsresult rv = BindStatementURI(mDBFindURIBookmarks, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 arraySize = 8;
  PRInt64 *categories = NS_STATIC_CAST(PRInt64*,
                                       nsMemory::Alloc(arraySize * sizeof(PRInt64)));
  NS_ENSURE_TRUE(categories, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 count = 0;
  PRBool more;

  while (NS_SUCCEEDED((rv = mDBFindURIBookmarks->ExecuteStep(&more))) && more) {
    if (count >= arraySize) {
      arraySize <<= 1;
      PRInt64 *res = NS_STATIC_CAST(PRInt64*, nsMemory::Realloc(categories,
                                                 arraySize * sizeof(PRInt64)));
      if (!res) {
        delete categories;
        return NS_ERROR_OUT_OF_MEMORY;
      }
      categories = res;
    }
    categories[count++] =
      mDBFindURIBookmarks->AsInt64(kFindBookmarksIndex_Parent);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  *aCount = count;
  *aCategories = categories;

  return transaction.Commit();
}

NS_IMETHODIMP
nsNavBookmarks::BeginUpdateBatch()
{
  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnBeginUpdateBatch();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::EndUpdateBatch()
{
  for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
    mObservers[i]->OnEndUpdateBatch();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::AddObserver(nsINavBookmarkObserver *aObserver)
{
  NS_ENSURE_TRUE(mObservers.AppendObject(aObserver), NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::RemoveObserver(nsINavBookmarkObserver *aObserver)
{
  mObservers.RemoveObject(aObserver);
  return NS_OK;
}

// nsNavBookmarks::nsINavHistoryObserver

NS_IMETHODIMP
nsNavBookmarks::OnBeginUpdateBatch()
{
  // These aren't passed through to bookmark observers currently.
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnEndUpdateBatch()
{
  // These aren't passed through to bookmark observers currently.
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::GetWantAllDetails(PRBool *aWant)
{
  *aWant = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnAddURI(nsIURI *aURI, PRTime aTime)
{
  // A new URI won't yet be bookmarked, so don't notify.
#ifdef DEBUG
  PRBool bookmarked;
  IsBookmarked(aURI, &bookmarked);
  NS_ASSERTION(!bookmarked, "New URI shouldn't be bookmarked!");
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnDeleteURI(nsIURI *aURI)
{
  // A deleted URI shouldn't be bookmarked.
#ifdef DEBUG
  PRBool bookmarked;
  IsBookmarked(aURI, &bookmarked);
  NS_ASSERTION(!bookmarked, "Deleted URI shouldn't be bookmarked!");
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnClearHistory()
{
  // Nothing being cleared should be bookmarked.
  return NS_OK;
}

NS_IMETHODIMP
nsNavBookmarks::OnPageChanged(nsIURI *aURI, PRUint32 aWhat,
                              const nsAString &aValue)
{
  if (aWhat == ATTRIBUTE_TITLE) {
    for (PRInt32 i = 0; i < mObservers.Count(); ++i) {
      mObservers[i]->OnItemChanged(aURI, NS_LITERAL_CSTRING("title"));
    }
  }

  return NS_OK;
}
