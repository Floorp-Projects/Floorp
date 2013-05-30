/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeContentView_h__
#define nsTreeContentView_h__

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsStubDocumentObserver.h"
#include "nsITreeBoxObject.h"
#include "nsITreeColumns.h"
#include "nsITreeView.h"
#include "nsITreeContentView.h"
#include "nsITreeSelection.h"
#include "mozilla/Attributes.h"

class nsIDocument;
class Row;

nsresult NS_NewTreeContentView(nsITreeView** aResult);

class nsTreeContentView MOZ_FINAL : public nsINativeTreeView,
                                    public nsITreeContentView,
                                    public nsStubDocumentObserver
{
  public:
    nsTreeContentView(void);

    ~nsTreeContentView(void);

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsTreeContentView,
                                             nsINativeTreeView)

    NS_DECL_NSITREEVIEW
    // nsINativeTreeView: Untrusted code can use us
    NS_IMETHOD EnsureNative() MOZ_OVERRIDE { return NS_OK; }

    NS_DECL_NSITREECONTENTVIEW

    // nsIDocumentObserver
    NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
    NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

    static bool CanTrustTreeSelection(nsISupports* aValue);

  protected:
    // Recursive methods which deal with serializing of nested content.
    void Serialize(nsIContent* aContent, int32_t aParentIndex, int32_t* aIndex,
                   nsTArray<nsAutoPtr<Row> >& aRows);

    void SerializeItem(nsIContent* aContent, int32_t aParentIndex,
                       int32_t* aIndex, nsTArray<nsAutoPtr<Row> >& aRows);

    void SerializeSeparator(nsIContent* aContent, int32_t aParentIndex,
                            int32_t* aIndex, nsTArray<nsAutoPtr<Row> >& aRows);

    void GetIndexInSubtree(nsIContent* aContainer, nsIContent* aContent, int32_t* aResult);
    
    // Helper methods which we use to manage our plain array of rows.
    int32_t EnsureSubtree(int32_t aIndex);

    int32_t RemoveSubtree(int32_t aIndex);

    int32_t InsertRow(int32_t aParentIndex, int32_t aIndex, nsIContent* aContent);

    void InsertRowFor(nsIContent* aParent, nsIContent* aChild);

    int32_t RemoveRow(int32_t aIndex);

    void ClearRows();
    
    void OpenContainer(int32_t aIndex);

    void CloseContainer(int32_t aIndex);

    int32_t FindContent(nsIContent* aContent);

    void UpdateSubtreeSizes(int32_t aIndex, int32_t aCount);

    void UpdateParentIndexes(int32_t aIndex, int32_t aSkip, int32_t aCount);

    // Content helpers.
    nsIContent* GetCell(nsIContent* aContainer, nsITreeColumn* aCol);

  private:
    nsCOMPtr<nsITreeBoxObject>          mBoxObject;
    nsCOMPtr<nsITreeSelection>          mSelection;
    nsCOMPtr<nsIContent>                mRoot;
    nsCOMPtr<nsIContent>                mBody;
    nsIDocument*                        mDocument;      // WEAK
    nsTArray<nsAutoPtr<Row> >           mRows;
};

#endif // nsTreeContentView_h__
