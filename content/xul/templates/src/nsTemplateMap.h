/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTemplateMap_h__
#define nsTemplateMap_h__

#include "pldhash.h"
#include "nsXULElement.h"

class nsTemplateMap {
protected:
    struct Entry {
        PLDHashEntryHdr mHdr;
        nsIContent*     mContent;
        nsIContent*     mTemplate;
    };

    PLDHashTable mTable;

    void
    Init() { PL_DHashTableInit(&mTable, PL_DHashGetStubOps(), nullptr, sizeof(Entry), PL_DHASH_MIN_SIZE); }

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
            reinterpret_cast<Entry*>(PL_DHashTableOperate(&mTable, aContent, PL_DHASH_ADD));

        if (entry) {
            entry->mContent = aContent;
            entry->mTemplate = aTemplate;
        }
    }

    void
    Remove(nsIContent* aContent) {
        PL_DHashTableOperate(&mTable, aContent, PL_DHASH_REMOVE);

        for (nsIContent* child = aContent->GetFirstChild();
             child;
             child = child->GetNextSibling()) {
            Remove(child);
        }
    }


    void
    GetTemplateFor(nsIContent* aContent, nsIContent** aResult) {
        Entry* entry =
            reinterpret_cast<Entry*>(PL_DHashTableOperate(&mTable, aContent, PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_BUSY(&entry->mHdr))
            NS_IF_ADDREF(*aResult = entry->mTemplate);
        else
            *aResult = nullptr;
    }

    void
    Clear() { Finish(); Init(); }
};

#endif // nsTemplateMap_h__

