/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chris Waterson <waterson@netscape.com>
 */

#include "nsContentSupportMap.h"
#include "nsIXULContent.h"

PLDHashTableOps nsContentSupportMap::gOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashGetKeyStub,
    PL_DHashVoidPtrKeyStub,
    PL_DHashMatchEntryStub,
    PL_DHashMoveEntryStub,
    ClearEntry,
    PL_DHashFinalizeStub
};

void PR_CALLBACK
nsContentSupportMap::ClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aHdr)
{
    PL_DHashClearEntryStub(aTable, aHdr);
}

void
nsContentSupportMap::Init()
{
    PL_DHashTableInit(&mMap, &gOps, nsnull, sizeof(Entry), PL_DHASH_MIN_SIZE);
}

void
nsContentSupportMap::Finish()
{
    PL_DHashTableFinish(&mMap);
}

nsresult
nsContentSupportMap::Remove(nsIContent* aElement)
{
    PL_DHashTableOperate(&mMap, aElement, PL_DHASH_REMOVE);

    PRInt32 count;

    // If possible, use the special nsIXULContent interface to "peek"
    // at the child count without accidentally creating children as a
    // side effect, since we're about to rip 'em outta the map anyway.
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
    if (xulcontent) {
        xulcontent->PeekChildCount(count);
    }
    else {
        aElement->ChildCount(count);
    }

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> child;
        aElement->ChildAt(i, *getter_AddRefs(child));

        Remove(child);
    }

    return NS_OK;
}


