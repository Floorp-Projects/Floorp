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

#include "plhash.h"
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

    nsresult Put(nsIContent* aElement, nsTemplateMatch* aMatch);
    PRBool Get(nsIContent* aElement, nsTemplateMatch** aMatch);
    nsresult Remove(nsIContent* aElement);
    void Clear() { Finish(); Init(); }

protected:
    PLHashTable* mMap;
    nsFixedSizeAllocator mPool;

    void Init();
    void Finish();

    struct Entry {
        PLHashEntry mHashEntry;
        nsTemplateMatch* mMatch;
    };

    static PLHashAllocOps gAllocOps;

    static void* PR_CALLBACK
    AllocTable(void* aPool, PRSize aSize) {
        return new char[aSize]; };

    static void PR_CALLBACK
    FreeTable(void* aPool, void* aItem) {
        delete[] NS_STATIC_CAST(char*, aItem); }

    static PLHashEntry* PR_CALLBACK
    AllocEntry(void* aPool, const void* aKey) {
        nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);

        Entry* entry = NS_STATIC_CAST(Entry*, pool->Alloc(sizeof(Entry)));
        if (! entry)
            return nsnull;

        return NS_REINTERPRET_CAST(PLHashEntry*, entry); }

    static void PR_CALLBACK
    FreeEntry(void* aPool, PLHashEntry* aEntry, PRUintn aFlag) {
        if (aFlag == HT_FREE_ENTRY) {
            Entry* entry = NS_REINTERPRET_CAST(Entry*, aEntry);

            if (entry->mMatch)
                entry->mMatch->Release();

            nsFixedSizeAllocator::Free(entry, sizeof(Entry));
        } }

    static PLHashNumber PR_CALLBACK
    HashPointer(const void* aKey) {
        return PLHashNumber(aKey) >> 3; }
};

#endif
