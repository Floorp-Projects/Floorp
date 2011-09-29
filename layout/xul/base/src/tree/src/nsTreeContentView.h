/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Initial Developer of the Original Code is Jan Varga.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com>
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

#ifndef nsTreeContentView_h__
#define nsTreeContentView_h__

#include "nsFixedSizeAllocator.h"
#include "nsTArray.h"
#include "nsIDocument.h"
#include "nsStubDocumentObserver.h"
#include "nsITreeBoxObject.h"
#include "nsITreeColumns.h"
#include "nsITreeView.h"
#include "nsITreeContentView.h"
#include "nsITreeSelection.h"

class Row;

nsresult NS_NewTreeContentView(nsITreeView** aResult);

class nsTreeContentView : public nsINativeTreeView,
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
    NS_IMETHOD EnsureNative() { return NS_OK; }

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
    void Serialize(nsIContent* aContent, PRInt32 aParentIndex, PRInt32* aIndex,
                   nsTArray<Row*>& aRows);

    void SerializeItem(nsIContent* aContent, PRInt32 aParentIndex,
                       PRInt32* aIndex, nsTArray<Row*>& aRows);

    void SerializeSeparator(nsIContent* aContent, PRInt32 aParentIndex,
                            PRInt32* aIndex, nsTArray<Row*>& aRows);

    void GetIndexInSubtree(nsIContent* aContainer, nsIContent* aContent, PRInt32* aResult);
    
    // Helper methods which we use to manage our plain array of rows.
    PRInt32 EnsureSubtree(PRInt32 aIndex);

    PRInt32 RemoveSubtree(PRInt32 aIndex);

    PRInt32 InsertRow(PRInt32 aParentIndex, PRInt32 aIndex, nsIContent* aContent);

    void InsertRowFor(nsIContent* aParent, nsIContent* aChild);

    PRInt32 RemoveRow(PRInt32 aIndex);

    void ClearRows();
    
    void OpenContainer(PRInt32 aIndex);

    void CloseContainer(PRInt32 aIndex);

    PRInt32 FindContent(nsIContent* aContent);

    void UpdateSubtreeSizes(PRInt32 aIndex, PRInt32 aCount);

    void UpdateParentIndexes(PRInt32 aIndex, PRInt32 aSkip, PRInt32 aCount);

    // Content helpers.
    nsIContent* GetCell(nsIContent* aContainer, nsITreeColumn* aCol);

  private:
    nsCOMPtr<nsITreeBoxObject>          mBoxObject;
    nsCOMPtr<nsITreeSelection>          mSelection;
    nsCOMPtr<nsIContent>                mRoot;
    nsCOMPtr<nsIContent>                mBody;
    nsIDocument*                        mDocument;      // WEAK
    nsFixedSizeAllocator                mAllocator;
    nsTArray<Row*>                      mRows;
};

#endif // nsTreeContentView_h__
