/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsConflictSet.h"
#include "nsTemplateRule.h"

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

        MatchCluster* cluster;

        if (hep && *hep) {
            cluster = NS_REINTERPRET_CAST(MatchCluster*, (*hep)->value);
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
            entry->mHashEntry.value = cluster = &entry->mCluster;
        }

        nsTemplateMatchRefSet& set = cluster->mMatches;
        if (! set.Contains(aMatch))
            set.Add(aMatch);
    }


    // Add the match to a table indexed by supporting MemoryElement
    {
        MemoryElementSet::ConstIterator last = aMatch->mInstantiation.mSupport.Last();
        for (MemoryElementSet::ConstIterator element = aMatch->mInstantiation.mSupport.First(); element != last; ++element) {
            PLHashNumber hash = element->Hash();
            PLHashEntry** hep = PL_HashTableRawLookup(mSupport, hash, element.operator->());

            nsTemplateMatchRefSet* set;

            if (hep && *hep) {
                set = NS_STATIC_CAST(nsTemplateMatchRefSet*, (*hep)->value);
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

            if (! set->Contains(aMatch)) {
                set->Add(aMatch);
                aMatch->AddRef();
            }
        }
    }

    // Add the match to a table indexed by bound MemoryElement
    nsResourceSet::ConstIterator last = aMatch->mBindingDependencies.Last();
    for (nsResourceSet::ConstIterator dep = aMatch->mBindingDependencies.First(); dep != last; ++dep)
        AddBindingDependency(aMatch, *dep);

    return NS_OK;
}


nsConflictSet::MatchCluster*
nsConflictSet::GetMatchesForClusterKey(const nsClusterKey& aKey)
{
    // Retrieve all the matches in a cluster
    return NS_STATIC_CAST(MatchCluster*, PL_HashTableLookup(mClusters, &aKey));
}


nsTemplateMatch*
nsConflictSet::GetMatchWithHighestPriority(const MatchCluster* aMatchCluster) const
{
    // Find the rule with the "highest priority"; i.e., the rule with
    // the lowest value for GetPriority().
    //
    // XXX if people start writing more than a few matches, we should
    // rewrite this to maintain the matches in sorted order, and then
    // just pluck the match off the top of the list.
    nsTemplateMatch* result = nsnull;
    PRInt32 max = PRInt32(PR_BIT(31) - 1);

    const nsTemplateMatchRefSet& set = aMatchCluster->mMatches;
    nsTemplateMatchRefSet::ConstIterator last = set.Last();

    for (nsTemplateMatchRefSet::ConstIterator match = set.First(); match != last; ++match) {
        PRInt32 priority = match->mRule->GetPriority();
        if (priority < max) {
            result = NS_CONST_CAST(nsTemplateMatch*, match.operator->());
            max = priority;
        }
    }

    return result;
}

const nsTemplateMatchRefSet*
nsConflictSet::GetMatchesWithBindingDependency(nsIRDFResource* aResource)
{
    // Retrieve all the matches whose bindings depend on the specified resource
    return NS_STATIC_CAST(nsTemplateMatchRefSet*, PL_HashTableLookup(mBindingDependencies, aResource));
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
    nsTemplateMatchRefSet* set =
        NS_STATIC_CAST(nsTemplateMatchRefSet*, (*hep)->value);

    // We'll iterate through these matches, only paying attention to
    // matches that strictly contain the MemoryElement we're about to
    // remove.
    nsTemplateMatchRefSet::ConstIterator last = set->Last();
    for (nsTemplateMatchRefSet::ConstIterator match = set->First(); match != last; ++match) {
        // Note the retraction, so we can compute new matches, later.
        aRetractedMatches.Add(match.operator->());

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

    nsTemplateMatchRefSet* set;

    if (hep && *hep) {
        set = NS_STATIC_CAST(nsTemplateMatchRefSet*, (*hep)->value);
    }
    else {
        PLHashEntry* he = PL_HashTableRawAdd(mBindingDependencies, hep, hash, aResource, nsnull);

        BindingEntry* entry = NS_REINTERPRET_CAST(BindingEntry*, he);
        if (! entry)
            return NS_ERROR_OUT_OF_MEMORY;

        // Fixup the value.
        entry->mHashEntry.value = set = &entry->mMatchSet;
        
    }

    if (! set->Contains(aMatch))
        set->Add(aMatch);

    return NS_OK;
}

nsresult
nsConflictSet::RemoveBindingDependency(nsTemplateMatch* aMatch, nsIRDFResource* aResource)
{
    // Remove a match's dependency on a source resource
    PLHashNumber hash = HashBindingElement(aResource);
    PLHashEntry** hep = PL_HashTableRawLookup(mBindingDependencies, hash, aResource);

    if (hep && *hep) {
        nsTemplateMatchRefSet* set = NS_STATIC_CAST(nsTemplateMatchRefSet*, (*hep)->value);

        set->Remove(aMatch);

        if (set->Empty())
            PL_HashTableRawRemove(mBindingDependencies, hep, *hep);
    }

    return NS_OK;
}

nsresult
nsConflictSet::ComputeNewMatches(nsTemplateMatchSet& aNewMatches,
                                 nsTemplateMatchSet& aRetractedMatches)
{
    // Given a set of just-retracted matches, compute the set of new
    // matches that have been revealed, updating the key-to-match map
    // as we go.
    nsTemplateMatchSet::ConstIterator last = aRetractedMatches.Last();
    for (nsTemplateMatchSet::ConstIterator retraction = aRetractedMatches.First();
         retraction != last;
         ++retraction) {
        nsClusterKey key(retraction->mInstantiation, retraction->mRule);
        PLHashEntry** hep = PL_HashTableRawLookup(mClusters, key.Hash(), &key);

        // XXXwaterson I'd managed to convince myself that this was really
        // okay, but now I can't remember why.
        //NS_ASSERTION(hep && *hep, "mClusters corrupted");
        if (!hep || !*hep)
            continue;

        MatchCluster* cluster = NS_REINTERPRET_CAST(MatchCluster*, (*hep)->value);
        nsTemplateMatchRefSet& set = cluster->mMatches;

        nsTemplateMatchRefSet::ConstIterator last = set.Last();
        for (nsTemplateMatchRefSet::ConstIterator match = set.First(); match != last; ++match) {
            if (match->mRule == retraction->mRule) {
                set.Remove(match.operator->()); // N.B., iterator no longer valid!

                // See if we've revealed another rule that's applicable
                nsTemplateMatch* newmatch =
                    GetMatchWithHighestPriority(cluster);

                if (newmatch)
                    aNewMatches.Add(newmatch);

                break;
            }
        }

        if (set.Empty())
            PL_HashTableRawRemove(mClusters, hep, *hep);
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

//----------------------------------------------------------------------
//

void
nsConflictSet::SupportEntry::Destroy(nsFixedSizeAllocator& aPool, SupportEntry* aEntry)
{
    // We need to Release() the matches here, because this is where
    // we've got access to the pool from which they were
    // allocated. Since SupportEntry's dtor is protected, nobody else
    // can be creating SupportEntry objects (e.g., on the stack).
    nsTemplateMatchRefSet::ConstIterator last = aEntry->mMatchSet.Last();
    for (nsTemplateMatchRefSet::ConstIterator iter = aEntry->mMatchSet.First();
         iter != last;
         ++iter)
        iter->Release(aPool);

    aEntry->~SupportEntry();
    aPool.Free(aEntry, sizeof(*aEntry));
}
