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

#ifndef nsContentSupportMap_h__
#define nsContentSupportMap_h__

#include "pldhash.h"
#include "nsFixedSizeAllocator.h"
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
    nsContentSupportMap() { Init(); }
    ~nsContentSupportMap() { Finish(); }

    nsresult Put(nsIContent* aElement, nsTemplateMatch* aMatch) {
        PLDHashEntryHdr* hdr = PL_DHashTableOperate(&mMap, aElement, PL_DHASH_ADD);
        if (!hdr)
            return NS_ERROR_OUT_OF_MEMORY;

        Entry* entry = NS_REINTERPRET_CAST(Entry*, hdr);
        NS_ASSERTION(entry->mMatch == nsnull, "over-writing entry");
        entry->mContent = aElement;
        entry->mMatch   = aMatch;
        return NS_OK; }

    PRBool Get(nsIContent* aElement, nsTemplateMatch** aMatch) {
        PLDHashEntryHdr* hdr = PL_DHashTableOperate(&mMap, aElement, PL_DHASH_LOOKUP);
        if (PL_DHASH_ENTRY_IS_FREE(hdr))
            return PR_FALSE;

        Entry* entry = NS_REINTERPRET_CAST(Entry*, hdr);
        *aMatch = entry->mMatch;
        return PR_TRUE; }

    nsresult Remove(nsIContent* aElement);

    void Clear() { Finish(); Init(); }

protected:
    PLDHashTable mMap;

    void Init();
    void Finish();

    struct Entry {
        PLDHashEntryHdr  mHdr;
        nsIContent*      mContent;
        nsTemplateMatch* mMatch;
    };

    static PLDHashTableOps gOps;

    static void PR_CALLBACK
    ClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aHdr);
};

#endif
