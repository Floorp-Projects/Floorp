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
    struct Entry : public PLDHashEntryHdr {
        nsIContent*     mContent;
        nsIContent*     mTemplate;
    };

    PLDHashTable mTable;

public:
    nsTemplateMap() : mTable(PL_DHashGetStubOps(), sizeof(Entry)) { }

    ~nsTemplateMap() { }

    void
    Put(nsIContent* aContent, nsIContent* aTemplate) {
        NS_ASSERTION(!mTable.Search(aContent), "aContent already in map");

        auto entry = static_cast<Entry*>(mTable.Add(aContent, fallible));

        if (entry) {
            entry->mContent = aContent;
            entry->mTemplate = aTemplate;
        }
    }

    void
    Remove(nsIContent* aContent) {
        PL_DHashTableRemove(&mTable, aContent);

        for (nsIContent* child = aContent->GetFirstChild();
             child;
             child = child->GetNextSibling()) {
            Remove(child);
        }
    }


    void
    GetTemplateFor(nsIContent* aContent, nsIContent** aResult) {
        auto entry = static_cast<Entry*>(mTable.Search(aContent));
        if (entry)
            NS_IF_ADDREF(*aResult = entry->mTemplate);
        else
            *aResult = nullptr;
    }

    void
    Clear() { mTable.Clear(); }
};

#endif // nsTemplateMap_h__

