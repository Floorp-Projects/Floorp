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

#include "nsConflictSet.h"

#ifdef PR_LOGGING
PRLogModule* nsConflictSet::gLog;
#endif

// Allocation operations for the cluster table
PLHashAllocOps nsConflictSet::gClusterAllocOps = {
    AllocClusterTable, FreeClusterTable, AllocClusterEntry, FreeClusterEntry };

// Allocation operations for the support table
PLHashAllocOps nsConflictSet::gSupportAllocOps = {
    AllocSupportTable, FreeSupportTable, AllocSupportEntry, FreeSupportEntry };

// Allocation operations for the binding table
PLHashAllocOps nsConflictSet::gBindingAllocOps = {
    AllocBindingTable, FreeBindingTable, AllocBindingEntry, FreeBindingEntry };


nsresult
nsConflictSet::Init()
{
#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsConflictSet");
#endif

    static const size_t kBucketSizes[] = {
        sizeof(ClusterEntry),
        sizeof(SupportEntry),
        sizeof(BindingEntry),
        nsTemplateMatchSet::kEntrySize,
        nsTemplateMatchSet::kIndexSize
    };

    static const PRInt32 kNumBuckets = sizeof(kBucketSizes) / sizeof(size_t);

    static const PRInt32 kNumResourceElements = 64;

    // Per news://news.mozilla.org/39BEC105.5090206%40netscape.com
    static const PRInt32 kInitialSize = 256;

    mPool.Init("nsConflictSet", kBucketSizes, kNumBuckets, kInitialSize);

    mClusters =
        PL_NewHashTable(kNumResourceElements /* XXXwaterson we need a way to give a hint? */,
                        nsClusterKey::HashClusterKey,
                        nsClusterKey::CompareClusterKeys,
                        PL_CompareValues,
                        &gClusterAllocOps,
                        &mPool);

    mSupport =
        PL_NewHashTable(kNumResourceElements, /* XXXwaterson need hint */
                        HashMemoryElement,
                        CompareMemoryElements,
                        PL_CompareValues,
                        &gSupportAllocOps,
                        &mPool);

    mBindingDependencies =
        PL_NewHashTable(kNumResourceElements /* XXX arbitrary */,
                        HashBindingElement,
                        CompareBindingElements,
                        PL_CompareValues,
                        &gBindingAllocOps,
                        &mPool);

    return NS_OK;
}


nsresult
nsConflictSet::Destroy()
{
    PL_HashTableDestroy(mSupport);
    PL_HashTableDestroy(mClusters);
    PL_HashTableDestroy(mBindingDependencies);
    return NS_OK;
}

nsresult
nsConflictSet::Add(nsTemplateMatch* aMatch)
{
    // Add a match to the conflict set. This involves adding it to
    // the cluster table, the support table, and the binding table.

    // add the match to a table indexed by instantiation key
    {
        nsClusterKey key(aMatch->mInstantiation, aMatch->mRule);

        PLHashNumber hash = key.Hash();
        PLHashEntry** hep = PL_HashTableRawLookup(mClusters, hash, &key);

        nsTemplateMatchSet* set;

        if (hep && *hep) {
            set = NS_STATIC_CAST(nsTemplateMatchSet*, (*hep)->value);
        }
        else {
            PLHashEntry* he = PL_HashTableRawAdd(mClusters, hep, hash, &key, nsnull);
            if (! he)
                return NS_ERROR_OUT_OF_MEMORY;

            ClusterEntry* entry = NS_REINTERPRET_CAST(ClusterEntry*, he);

            // Fixup the key in the hashentry to point to the value
            // that the specially-allocated entry contains (rather
            // than the value on the stack). Do the same for its
            // value.
            entry->mHashEntry.key   = &entry->mKey;
            entry->mHashEntry.value = &entry->mMatchSet;

            set = &entry->mMatchSet;
        }

        if (! set->Contains(*aMatch)) {
            set->Add(mPool, aMatch);
        }
    }


    // Add the match to a table indexed by supporting MemoryElement
    {
        MemoryElementSet::ConstIterator last = aMatch->mInstantiation.mSupport.Last();
        for (MemoryElementSet::ConstIterator element = aMatch->mInstantiation.mSupport.First(); element != last; ++element) {
            PLHashNumber hash = element->Hash();
            PLHashEntry** hep = PL_HashTableRawLookup(mSupport, hash, element.operator->());

            nsTemplateMatchSet* set;

            if (hep && *hep) {
                set = NS_STATIC_CAST(nsTemplateMatchSet*, (*hep)->value);
            }
            else {
                PLHashEntry* he = PL_HashTableRawAdd(mSupport, hep, hash, element.operator->(), nsnull);

                SupportEntry* entry = NS_REINTERPRET_CAST(SupportEntry*, he);
                if (! entry)
                    return NS_ERROR_OUT_OF_MEMORY;

                // Fixup the key and value.
                entry->mHashEntry.key   = entry->mElement;
                entry->mHashEntry.value = &entry->mMatchSet;

                set = &entry->mMatchSet;
            }

            if (! set->Contains(*aMatch)) {
                set->Add(mPool, aMatch);
            }
        }
    }

    // Add the match to a table indexed by bound MemoryElement
    nsResourceSet::ConstIterator last = aMatch->mBindingDependencies.Last();
    for (nsResourceSet::ConstIterator dep = aMatch->mBindingDependencies.First(); dep != last; ++dep)
        AddBindingDependency(aMatch, *dep);

    return NS_OK;
}


void
nsConflictSet::GetMatchesForClusterKey(const nsClusterKey& aKey, const nsTemplateMatchSet** aMatchSet)
{
    // Retrieve all the matches in a cluster
    *aMatchSet = NS_STATIC_CAST(nsTemplateMatchSet*, PL_HashTableLookup(mClusters, &aKey));
}


void
nsConflictSet::GetMatchesWithBindingDependency(nsIRDFResource* aResource, const nsTemplateMatchSet** aMatchSet)
{
    // Retrieve all the matches whose bindings depend on the specified resource
    *aMatchSet = NS_STATIC_CAST(nsTemplateMatchSet*, PL_HashTableLookup(mBindingDependencies, aResource));
}


void
nsConflictSet::Remove(const MemoryElement& aMemoryElement,
                    nsTemplateMatchSet& aNewMatches,
                    nsTemplateMatchSet& aRetractedMatches)
{
    // Use the memory-element-to-match map to figure out what matches
    // will be affected.
    PLHashEntry** hep = PL_HashTableRawLookup(mSupport, aMemoryElement.Hash(), &aMemoryElement);

    if (!hep || !*hep)
        return;

    // 'set' gets the set of all matches containing the first binding.
    nsTemplateMatchSet* set = NS_STATIC_CAST(nsTemplateMatchSet*, (*hep)->value);

    // We'll iterate through these matches, only paying attention to
    // matches that strictly contain the MemoryElement we're about to
    // remove.
    for (nsTemplateMatchSet::Iterator match = set->First(); match != set->Last(); ++match) {
        // Note the retraction, so we can compute new matches, later.
        aRetractedMatches.Add(mPool, match.operator->());

        // Keep the bindings table in sync, as well. Since this match
        // is getting nuked, we need to nuke its bindings as well.
        nsResourceSet::ConstIterator last = match->mBindingDependencies.Last();
        for (nsResourceSet::ConstIterator dep = match->mBindingDependencies.First(); dep != last; ++dep)
            RemoveBindingDependency(match.operator->(), *dep);
    }

    // Unhash it
    PL_HashTableRawRemove(mSupport, hep, *hep);

    // Update the key-to-match map, and see if any new rules have been
    // fired as a result of the retraction.
    ComputeNewMatches(aNewMatches, aRetractedMatches);
}

nsresult
nsConflictSet::AddBindingDependency(nsTemplateMatch* aMatch, nsIRDFResource* aResource)
{
    // Note a match's dependency on a source resource
    PLHashNumber hash = HashBindingElement(aResource);
    PLHashEntry** hep = PL_HashTableRawLookup(mBindingDependencies, hash, aResource);

    nsTemplateMatchSet* set;

    if (hep && *hep) {
        set = NS_STATIC_CAST(nsTemplateMatchSet*, (*hep)->value);
    }
    else {
        PLHashEntry* he = PL_HashTableRawAdd(mBindingDependencies, hep, hash, aResource, nsnull);

        BindingEntry* entry = NS_REINTERPRET_CAST(BindingEntry*, he);
        if (! entry)
            return NS_ERROR_OUT_OF_MEMORY;

        // Fixup the value.
        entry->mHashEntry.value = set = &entry->mMatchSet;
        
    }

    if (! set->Contains(*aMatch)) {
        set->Add(mPool, aMatch);
    }

    return NS_OK;
}

nsresult
nsConflictSet::RemoveBindingDependency(nsTemplateMatch* aMatch, nsIRDFResource* aResource)
{
    // Remove a match's dependency on a source resource
    PLHashNumber hash = HashBindingElement(aResource);
    PLHashEntry** hep = PL_HashTableRawLookup(mBindingDependencies, hash, aResource);

    if (hep && *hep) {
        nsTemplateMatchSet* set = NS_STATIC_CAST(nsTemplateMatchSet*, (*hep)->value);

        set->Remove(aMatch);

        if (set->Empty()) {
            PL_HashTableRawRemove(mBindingDependencies, hep, *hep);
        }
    }

    return NS_OK;
}

nsresult
nsConflictSet::ComputeNewMatches(nsTemplateMatchSet& aNewMatches, nsTemplateMatchSet& aRetractedMatches)
{
    // Given a set of just-retracted matches, compute the set of new
    // matches that have been revealed, updating the key-to-match map
    // as we go.
    nsTemplateMatchSet::ConstIterator last = aRetractedMatches.Last();
    for (nsTemplateMatchSet::ConstIterator retraction = aRetractedMatches.First(); retraction != last; ++retraction) {
        nsClusterKey key(retraction->mInstantiation, retraction->mRule);
        PLHashEntry** hep = PL_HashTableRawLookup(mClusters, key.Hash(), &key);

        // XXXwaterson I'd managed to convince myself that this was really
        // okay, but now I can't remember why.
        //NS_ASSERTION(hep && *hep, "mClusters corrupted");
        if (!hep || !*hep)
            continue;

        nsTemplateMatchSet* set = NS_STATIC_CAST(nsTemplateMatchSet*, (*hep)->value);

        for (nsTemplateMatchSet::Iterator match = set->First(); match != set->Last(); ++match) {
            if (match->mRule == retraction->mRule) {
                set->Erase(match--);

                // See if we've revealed another rule that's applicable
                nsTemplateMatch* newmatch =
                    set->FindMatchWithHighestPriority();

                if (newmatch)
                    aNewMatches.Add(mPool, newmatch);

                break;
            }
        }

        if (set->Empty()) {
            PL_HashTableRawRemove(mClusters, hep, *hep);
        }
    }

    return NS_OK;
}


void
nsConflictSet::Clear()
{
    Destroy();
    Init();
}

PLHashNumber PR_CALLBACK
nsConflictSet::HashMemoryElement(const void* aMemoryElement)
{
    const MemoryElement* element =
        NS_STATIC_CAST(const MemoryElement*, aMemoryElement);

    return element->Hash();
}

PRIntn PR_CALLBACK
nsConflictSet::CompareMemoryElements(const void* aLeft, const void* aRight)
{
    const MemoryElement* left =
        NS_STATIC_CAST(const MemoryElement*, aLeft);

    const MemoryElement* right =
        NS_STATIC_CAST(const MemoryElement*, aRight);

    return *left == *right;
}
