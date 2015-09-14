/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentSupportMap_h__
#define nsContentSupportMap_h__

#include "pldhash.h"
#include "nsTemplateMatch.h"

/**
 * The nsContentSupportMap maintains a mapping from a "resource element"
 * in the content tree to the nsTemplateMatch that was used to instantiate it. This
 * is necessary to allow the XUL content to be built lazily. Specifically,
 * when building "resumes" on a partially-built content element, the builder
 * will walk upwards in the content tree to find the first element with an
 * 'id' attribute. This element is assumed to be the "resource element",
 * and allows the content builder to access the nsTemplateMatch (variable assignments
 * and rule information).
 */
class nsContentSupportMap {
public:
    nsContentSupportMap() : mMap(PL_DHashGetStubOps(), sizeof(Entry)) { }
    ~nsContentSupportMap() { }

    nsresult Put(nsIContent* aElement, nsTemplateMatch* aMatch) {
        PLDHashEntryHdr* hdr = mMap.Add(aElement, mozilla::fallible);
        if (!hdr)
            return NS_ERROR_OUT_OF_MEMORY;

        Entry* entry = static_cast<Entry*>(hdr);
        NS_ASSERTION(entry->mMatch == nullptr, "over-writing entry");
        entry->mContent = aElement;
        entry->mMatch   = aMatch;
        return NS_OK;
    }

    bool Get(nsIContent* aElement, nsTemplateMatch** aMatch) {
        PLDHashEntryHdr* hdr = mMap.Search(aElement);
        if (!hdr)
            return false;

        Entry* entry = static_cast<Entry*>(hdr);
        *aMatch = entry->mMatch;
        return true;
    }

    void Remove(nsIContent* aElement);

    void Clear() { mMap.Clear(); }

protected:
    PLDHashTable mMap;

    struct Entry : public PLDHashEntryHdr {
        nsIContent*      mContent;
        nsTemplateMatch* mMatch;
    };
};

#endif
