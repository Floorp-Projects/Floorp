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

#ifndef nsTemplateMap_h__
#define nsTemplateMap_h__

#include "pldhash.h"

class nsTemplateMap {
protected:
    struct Entry {
        PLDHashEntryHdr mHdr;
        nsIContent*     mContent;
        nsIContent*     mTemplate;
    };

    PLDHashTable mTable;

    void
    Init() { PL_DHashTableInit(&mTable, PL_DHashGetStubOps(), nsnull, sizeof(Entry), PL_DHASH_MIN_SIZE); }

    void
    Finish() { PL_DHashTableFinish(&mTable); }

public:
    nsTemplateMap() { Init(); }

    ~nsTemplateMap() { Finish(); }

    void
    Put(nsIContent* aContent, nsIContent* aTemplate) {
        NS_ASSERTION(PL_DHASH_ENTRY_IS_FREE(PL_DHashTableOperate(&mTable, aContent, PL_DHASH_LOOKUP)),
                     "aContent already in map");

        Entry* entry =
            NS_REINTERPRET_CAST(Entry*, PL_DHashTableOperate(&mTable, aContent, PL_DHASH_ADD));

        if (entry) {
            entry->mContent = aContent;
            entry->mTemplate = aTemplate;
        } }

    void
    Remove(nsIContent* aContent) {
        NS_ASSERTION(PL_DHASH_ENTRY_IS_BUSY(PL_DHashTableOperate(&mTable, aContent, PL_DHASH_LOOKUP)),
                     "aContent not in map");

        PL_DHashTableOperate(&mTable, aContent, PL_DHASH_REMOVE);

        PRInt32 count;

        // If possible, use the special nsIXULContent interface to
        // "peek" at the child count without accidentally creating
        // children as a side effect, since we're about to rip 'em
        // outta the map anyway.
        nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aContent);
        if (xulcontent) {
            xulcontent->PeekChildCount(count);
        }
        else {
            aContent->ChildCount(count);
        }

        for (PRInt32 i = 0; i < count; ++i) {
            nsCOMPtr<nsIContent> child;
            aContent->ChildAt(i, *getter_AddRefs(child));

            Remove(child);
        } }


    void
    GetTemplateFor(nsIContent* aContent, nsIContent** aResult) {
        Entry* entry =
            NS_REINTERPRET_CAST(Entry*, PL_DHashTableOperate(&mTable, aContent, PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_BUSY(&entry->mHdr))
            NS_IF_ADDREF(*aResult = entry->mTemplate);
        else
            *aResult = nsnull; }

    void
    Clear() { Finish(); Init(); }
};

#endif // nsTemplateMap_h__

