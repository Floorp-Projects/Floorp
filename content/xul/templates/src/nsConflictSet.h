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

#ifndef nsConflictSet_h__
#define nsConflictSet_h__

#include "plhash.h"
#include "nsTemplateMatch.h"
#include "nsTemplateMatchSet.h"
#include "nsClusterKey.h"
#include "nsIRDFResource.h"

/**
 * Maintains the set of active matches, and the stuff that the
 * matches depend on.
 */
class nsConflictSet
{
public:
    nsConflictSet()
        : mClusters(nsnull),
          mSupport(nsnull),
          mBindingDependencies(nsnull) { Init(); }

    ~nsConflictSet() { Destroy(); }

    /**
     * Add a match to the conflict set.
     * @param aMatch the match to add to the conflict set
     * @return NS_OK if no errors occurred
     */
    nsresult Add(nsTemplateMatch* aMatch);

    /**
     * Given a cluster key, which is a container-member pair, return the
     * set of matches that are currently "active" for that cluster. (The
     * caller can the select among the active rules to determine which
     * should actually be applied.)
     * @param aKey the cluster key to search for
     * @param aMatchSet the set of matches that are currently active
     *   for the key.
     */
    void GetMatchesForClusterKey(const nsClusterKey& aKey, const nsTemplateMatchSet** aMatchSet);

    /**
     * Given a "source" in the RDF graph, return the set of matches
     * that currently depend on the source in some way.
     * @param aSource an RDF resource that is a "source" in the graph.
     * @param aMatchSet the set of matches that depend on aSource.
     */
    void GetMatchesWithBindingDependency(nsIRDFResource* aSource, const nsTemplateMatchSet** aMatchSet);

    /**
     * Remove a memory element from the conflict set. This may
     * potentially retract matches that depended on the memory
     * element, as well as trigger previously masked matches that are
     * now "revealed".
     * @param aMemoryElement the memory element that is being removed.
     * @param aNewMatches new matches that have been revealed.
     * @param aRetractedMatches matches whose validity depended
     *   on aMemoryElement and have been retracted.
     */
    void Remove(const MemoryElement& aMemoryElement,
                nsTemplateMatchSet& aNewMatches,
                nsTemplateMatchSet& aRetractedMatches);

    nsresult AddBindingDependency(nsTemplateMatch* aMatch, nsIRDFResource* aResource);

    nsresult RemoveBindingDependency(nsTemplateMatch* aMatch, nsIRDFResource* aResource);

    /**
     * Remove all match support information currently stored in the conflict
     * set, and re-initialize the set.
     */
    void Clear();

    /**
     * Get the fixed-size arena allocator used by the conflict set
     * @return the fixed-size arena allocator used by the conflict set
     */
    nsFixedSizeAllocator& GetPool() { return mPool; }

protected:
    nsresult Init();
    nsresult Destroy();

    nsresult ComputeNewMatches(nsTemplateMatchSet& aNewMatches, nsTemplateMatchSet& aRetractedMatches);

    /**
     * "Clusters" of matched rules for the same <content, member>
     * pair. This table makes it O(1) to lookup all of the matches
     * that are active for a cluster, so determining which is active
     * is efficient.
     */
    PLHashTable* mClusters;

    class ClusterEntry {
    public:
        static void* operator new(size_t aSize, nsFixedSizeAllocator& aAllocator) {
            return aAllocator.Alloc(aSize); }

        static void operator delete(void* aPtr, size_t aSize) {
            nsFixedSizeAllocator::Free(aPtr, aSize); }

        ClusterEntry() { MOZ_COUNT_CTOR(nsConflictSet::ClusterEntry); }
        ~ClusterEntry() { MOZ_COUNT_DTOR(nsConflictSet::ClusterEntry); }

        PLHashEntry mHashEntry;
        nsClusterKey  mKey;
        nsTemplateMatchSet    mMatchSet;
    };

    static PLHashAllocOps gClusterAllocOps;

    static void* PR_CALLBACK AllocClusterTable(void* aPool, PRSize aSize) {
        return new char[aSize]; }

    static void PR_CALLBACK FreeClusterTable(void* aPool, void* aItem) {
        delete[] NS_STATIC_CAST(char*, aItem); }

    static PLHashEntry* PR_CALLBACK AllocClusterEntry(void* aPool, const void* aKey) {
        nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);

        ClusterEntry* entry = new (*pool) ClusterEntry();
        if (! entry)
            return nsnull;

        entry->mKey = *NS_STATIC_CAST(const nsClusterKey*, aKey);
        return NS_REINTERPRET_CAST(PLHashEntry*, entry); }

    static void PR_CALLBACK FreeClusterEntry(void* aPool, PLHashEntry* aHashEntry, PRUintn aFlag) {
        if (aFlag == HT_FREE_ENTRY)
            delete NS_REINTERPRET_CAST(ClusterEntry*, aHashEntry); }

    /**
     * Maps a MemoryElement to the nsTemplateMatch objects that it
     * supports. This map allows us to efficiently remove rules from
     * the conflict set when a MemoryElement is removed.
     */
    PLHashTable* mSupport;

    class SupportEntry {
    public:
        static void* operator new(size_t aSize, nsFixedSizeAllocator& aAllocator) {
            return aAllocator.Alloc(aSize); }

        static void operator delete(void* aPtr, size_t aSize) {
            nsFixedSizeAllocator::Free(aPtr, aSize); }

        SupportEntry() : mElement(nsnull)
            { MOZ_COUNT_CTOR(nsConflictSet::SupportEntry); }

        ~SupportEntry() {
            delete mElement;
            MOZ_COUNT_DTOR(nsConflictSet::SupportEntry); }

        PLHashEntry    mHashEntry;
        MemoryElement* mElement;
        nsTemplateMatchSet       mMatchSet;
    };

    static PLHashAllocOps gSupportAllocOps;

    static void* PR_CALLBACK AllocSupportTable(void* aPool, PRSize aSize) {
        return new char[aSize]; }

    static void PR_CALLBACK FreeSupportTable(void* aPool, void* aItem) {
        delete[] NS_STATIC_CAST(char*, aItem); }

    static PLHashEntry* PR_CALLBACK AllocSupportEntry(void* aPool, const void* aKey) {
        nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);

        SupportEntry* entry = new (*pool) SupportEntry();
        if (! entry)
            return nsnull;

        const MemoryElement* element = NS_STATIC_CAST(const MemoryElement*, aKey);
        entry->mElement = element->Clone(aPool);

        return NS_REINTERPRET_CAST(PLHashEntry*, entry); }

    static void PR_CALLBACK FreeSupportEntry(void* aPool, PLHashEntry* aHashEntry, PRUintn aFlag) {
        if (aFlag == HT_FREE_ENTRY)
            delete NS_REINTERPRET_CAST(SupportEntry*, aHashEntry); }

    static PLHashNumber PR_CALLBACK HashMemoryElement(const void* aBinding);
    static PRIntn PR_CALLBACK CompareMemoryElements(const void* aLeft, const void* aRight);


    /**
     * Maps a MemoryElement to the nsTemplateMatch objects whose bindings it
     * participates in. This makes it possible to efficiently update a
     * match when a binding changes.
     */
    PLHashTable* mBindingDependencies;

    class BindingEntry {
    public:
        static void* operator new(size_t aSize, nsFixedSizeAllocator& aAllocator) {
            return aAllocator.Alloc(aSize); }

        static void operator delete(void* aPtr, size_t aSize) {
            nsFixedSizeAllocator::Free(aPtr, aSize); }

        BindingEntry()
            { MOZ_COUNT_CTOR(nsConflictSet::BindingEntry); }

        ~BindingEntry()
            { MOZ_COUNT_DTOR(nsConflictSet::BindingEntry); }

        PLHashEntry mHashEntry;
        nsTemplateMatchSet mMatchSet;
    };

    static PLHashAllocOps gBindingAllocOps;

    static void* PR_CALLBACK AllocBindingTable(void* aPool, PRSize aSize) {
        return new char[aSize]; }

    static void PR_CALLBACK FreeBindingTable(void* aPool, void* aItem) {
        delete[] NS_STATIC_CAST(char*, aItem); }

    static PLHashEntry* PR_CALLBACK AllocBindingEntry(void* aPool, const void* aKey) {
        nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);

        BindingEntry* entry = new (*pool) BindingEntry();
        if (! entry)
            return nsnull;

        nsIRDFResource* key = NS_STATIC_CAST(nsIRDFResource*, NS_CONST_CAST(void*, aKey));
        NS_ADDREF(key);

        return NS_REINTERPRET_CAST(PLHashEntry*, entry); }

    static void PR_CALLBACK FreeBindingEntry(void* aPool, PLHashEntry* aHashEntry, PRUintn aFlag) {
        if (aFlag == HT_FREE_ENTRY) {
            nsIRDFResource* key = NS_STATIC_CAST(nsIRDFResource*, NS_CONST_CAST(void*, aHashEntry->key));
            NS_RELEASE(key);
            delete NS_REINTERPRET_CAST(BindingEntry*, aHashEntry);
        } }

    static PLHashNumber PR_CALLBACK HashBindingElement(const void* aSupport) {
        return PLHashNumber(aSupport) >> 3; }
        
    static PRIntn PR_CALLBACK CompareBindingElements(const void* aLeft, const void* aRight) {
        return aLeft == aRight; }

    // The pool from whence all our slop will be allocated
    nsFixedSizeAllocator mPool;
};

#endif // nsConflictSet_h__
