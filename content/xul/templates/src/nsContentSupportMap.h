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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
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
        if (!mMap.ops)
            return NS_ERROR_NOT_INITIALIZED;

        PLDHashEntryHdr* hdr = PL_DHashTableOperate(&mMap, aElement, PL_DHASH_ADD);
        if (!hdr)
            return NS_ERROR_OUT_OF_MEMORY;

        Entry* entry = NS_REINTERPRET_CAST(Entry*, hdr);
        NS_ASSERTION(entry->mMatch == nsnull, "over-writing entry");
        entry->mContent = aElement;
        entry->mMatch   = aMatch;
        return NS_OK; }

    PRBool Get(nsIContent* aElement, nsTemplateMatch** aMatch) {
        if (!mMap.ops)
            return PR_FALSE;

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
};

#endif
