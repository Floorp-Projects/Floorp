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

PLHashAllocOps nsContentSupportMap::gAllocOps = {
    AllocTable, FreeTable, AllocEntry, FreeEntry };

void
nsContentSupportMap::Init()
{
    static const size_t kBucketSizes[] = { sizeof(Entry) };
    static const PRInt32 kNumBuckets = sizeof(kBucketSizes) / sizeof(size_t);

    // Per news://news.mozilla.org/39BEC105.5090206%40netscape.com
    static const PRInt32 kInitialSize = 256;

    mPool.Init("nsContentSupportMap", kBucketSizes, kNumBuckets, kInitialSize);

    static const PRInt32 kInitialEntries = 8; // XXX arbitrary

    mMap = PL_NewHashTable(kInitialEntries,
                           HashPointer,
                           PL_CompareValues,
                           PL_CompareValues,
                           &gAllocOps,
                           &mPool);
}

void
nsContentSupportMap::Finish()
{
    PL_HashTableDestroy(mMap);
}

nsresult
nsContentSupportMap::Put(nsIContent* aElement, nsTemplateMatch* aMatch)
{
    PLHashEntry* he = PL_HashTableAdd(mMap, aElement, nsnull);
    if (! he)
        return NS_ERROR_OUT_OF_MEMORY;

    // "Fix up" the entry's value to refer to the mMatch that's built
    // in to the Entry object.
    Entry* entry = NS_REINTERPRET_CAST(Entry*, he);
    entry->mHashEntry.value = &entry->mMatch;
    entry->mMatch = aMatch;
    aMatch->AddRef();

    return NS_OK;
}


nsresult
nsContentSupportMap::Remove(nsIContent* aElement)
{
    PL_HashTableRemove(mMap, aElement);

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


PRBool
nsContentSupportMap::Get(nsIContent* aElement, nsTemplateMatch** aMatch)
{
    nsTemplateMatch** match = NS_STATIC_CAST(nsTemplateMatch**, PL_HashTableLookup(mMap, aElement));
    if (! match)
        return PR_FALSE;

    *aMatch = *match;
    return PR_TRUE;
}
