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

#ifndef nsConflictSet_h__
#define nsConflictSet_h__

#include "nscore.h"
#include "plhash.h"
#include "nsTemplateMatch.h"
#include "nsTemplateMatchSet.h"
#include "nsClusterKey.h"
#include "nsIRDFResource.h"

/**
 * Maintains the set of active matches, and the stuff that the matches
 * depend on. An entity-relationship diagram of the situation might
 * look something like this:
 *
 *                         nsIRDFResource objects
 *                         (mBindingDependencies)
 *                                  |
 *                                  | [1]
 *                                  | 
 *                      [2]         ^         [3]
 *   Match ``clusters'' ---< nsTemplateMatch >--- MemoryElement objects
 *       (mClusters)            objects                 (mSupport)
 *
 * [1] Each match object may ``depend'' on one or more RDF resources;
 *     that is, if an assertion about a resource changes, will the
 *     match's bindings need to be updated? By maintaing a map from
 *     nsIRDFResource objects to match objects, we can quickly
 *     determine which matches will be affected when the datasource
 *     notifies us of new assertions about a resource.
 *
 *     The AddBindingDependency() and RemoveBindingDependency()
 *     methods are used to add and remove dependencies from an
 *     nsIRDFResource object to a match. The
 *     GetMatchesWithBindingDependency() method is used to retrieve
 *     the matches that depend upon a particular nsIRDFResource
 *     object.
 *
 * [2] Each match is a ``grouped' by into a cluster for conflict
 *     resolution. If the template has more than one rule, then it's
 *     possible that more than one of the rules may match. If that's
 *     the case, we need to choose amongst the matched rules to select
 *     which one gets ``activated''. By maintaining a map from cluster
 *     ``key'' (really just a <container, member> tuple) to the set of
 *     matches that are active for that key, we can quickly select
 *     amongst the matches to determine which one should be activated.
 *
 *     When a match is first added to the conflict set using the Add()
 *     method, it is automatically placed in the appropriate
 *     group. The GetMatchesForClusterKey() method is used to retrieve
 *     all the active matches in a match cluster; the
 *     GetMatchWithHighestPriority() method is used to select among
 *     these to determine which match should be activated.
 *
 * [3] Each match is ``supported'' by one or more MemoryElement
 *     objects. These objects represent the values that were bound to
 *     variables in the match's rule's conditions. If one of these
 *     MemoryElements is removed, then the match is no longer
 *     valid. We maintain a mapping from memory elements to the
 *     matches that they support so that we can quickly determine
 *     which matches will be affected by the removal of a memory
 *     element object.
 *
 *     When a match is first added to the conflict set using the Add()
 *     method, the memory element dependencies are automatically
 *     computed. The Remove() method is used to notify the conflict
 *     set that a memory element has been removed; the method computes
 *     the matches that have been ``retracted'' because the memory
 *     element is gone, as well as any new matches that may have
 *     ``fired'' because they were blocked by the presence of the
 *     memory element.
 */
class nsConflictSet
{
public:
    /**
     * A ``cluster'' of matches, all with the same nsClusterKey (that
     * is, matches with the same <container, member> tuple).
     */
    class MatchCluster {
    public:
        MatchCluster() : mLastMatch(nsnull) {}

        nsTemplateMatchRefSet mMatches;
        nsTemplateMatch*      mLastMatch;
    };

    nsConflictSet()
        : mClusters(nsnull),
          mSupport(nsnull),
          mBindingDependencies(nsnull) {
        MOZ_COUNT_CTOR(nsConflictSet);
        Init(); }

    ~nsConflictSet() {
        MOZ_COUNT_DTOR(nsConflictSet);
        Destroy(); }

    /**
     * Add a match to the conflict set.
     * @param aMatch the match to add to the conflict set
     * @return NS_OK if no errors occurred
     */
    nsresult
    Add(nsTemplateMatch* aMatch);

    /**
     * Given a cluster key, which is a container-member pair, return the
     * set of matches that are currently "active" for that cluster. (The
     * caller can the select among the active rules to determine which
     * should actually be applied.)
     * @param aKey the cluster key to search for
     * @param aMatchSet the set of matches that are currently active
     *   for the key.
     */
    MatchCluster*
    GetMatchesForClusterKey(const nsClusterKey& aKey);

    /**
     * Retrieve the match from the set with the ``highest priority''
     * (that is, the lowest value returned by
     * nsTemplateMatch::GetPriority()).
     */
    nsTemplateMatch*
    GetMatchWithHighestPriority(const MatchCluster* aMatchCluster) const;

    /**
     * Given a "source" in the RDF graph, return the set of matches
     * that currently depend on the source in some way.
     * @param aSource an RDF resource that is a "source" in the graph.
     * @param aMatchSet the set of matches that depend on aSource.
     */
    const nsTemplateMatchRefSet*
    GetMatchesWithBindingDependency(nsIRDFResource* aSource);

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
    void
    Remove(const MemoryElement& aMemoryElement,
           nsTemplateMatchSet& aNewMatches,
           nsTemplateMatchSet& aRetractedMatches);

    /**
     * Add a binding dependency for aMatch on aResource.
     */
    nsresult
    AddBindingDependency(nsTemplateMatch* aMatch, nsIRDFResource* aResource);

    /**
     * Remove the binding dependency on aResource from aMatch.
     */
    nsresult
    RemoveBindingDependency(nsTemplateMatch* aMatch, nsIRDFResource* aResource);

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

    nsresult
    ComputeNewMatches(nsTemplateMatchSet& aNewMatches, 
                      nsTemplateMatchSet& aRetractedMatches);

    /**
     * "Clusters" of matched rules for the same <content, member>
     * pair. This table makes it O(1) to lookup all of the matches
     * that are active for a cluster, so determining which is active
     * is efficient.
     */
    PLHashTable* mClusters;

    class ClusterEntry {
    private:
        // Hide so that only Create() and Destroy() can be used to
        // allocate and deallocate from the heap
        static void* operator new(size_t) { return 0; }
        static void operator delete(void*, size_t) {}

    public:
        ClusterEntry() { MOZ_COUNT_CTOR(nsConflictSet::ClusterEntry); }
        ~ClusterEntry() { MOZ_COUNT_DTOR(nsConflictSet::ClusterEntry); }

        static ClusterEntry*
        Create(nsFixedSizeAllocator& aPool) {
            void* place = aPool.Alloc(sizeof(ClusterEntry));
            return place ? ::new (place) ClusterEntry() : nsnull; }

        static void
        Destroy(nsFixedSizeAllocator& aPool, ClusterEntry* aEntry) {
            aEntry->~ClusterEntry();
            aPool.Free(aEntry, sizeof(*aEntry)); }

        PLHashEntry  mHashEntry;
        nsClusterKey mKey;
        MatchCluster mCluster;
    };

    static PLHashAllocOps gClusterAllocOps;

    static void* PR_CALLBACK AllocClusterTable(void* aPool, PRSize aSize) {
        return new char[aSize]; }

    static void PR_CALLBACK FreeClusterTable(void* aPool, void* aItem) {
        delete[] NS_STATIC_CAST(char*, aItem); }

    static PLHashEntry* PR_CALLBACK AllocClusterEntry(void* aPool, const void* aKey) {
        nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);

        ClusterEntry* entry = ClusterEntry::Create(*pool);
        if (! entry)
            return nsnull;

        entry->mKey = *NS_STATIC_CAST(const nsClusterKey*, aKey);
        return NS_REINTERPRET_CAST(PLHashEntry*, entry); }

    static void PR_CALLBACK FreeClusterEntry(void* aPool, PLHashEntry* aHashEntry, PRUintn aFlag) {
        nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);
        if (aFlag == HT_FREE_ENTRY)
            ClusterEntry::Destroy(*pool, NS_REINTERPRET_CAST(ClusterEntry*, aHashEntry)); }

    /**
     * Maps a MemoryElement to the nsTemplateMatch objects that it
     * supports. This map allows us to efficiently remove matches from
     * the conflict set when a MemoryElement is removed. Conceptually,
     * a support element is what ``owns'' the match.
     */
    PLHashTable* mSupport;

    class SupportEntry {
    private:
        // Hide so that only Create() and Destroy() can be used to
        // allocate and deallocate from the heap
        static void* operator new(size_t) { return 0; }
        static void operator delete(void*, size_t) {}

    protected:
        SupportEntry() : mElement(nsnull)
            { MOZ_COUNT_CTOR(nsConflictSet::SupportEntry); }

        ~SupportEntry() {
            delete mElement;
            MOZ_COUNT_DTOR(nsConflictSet::SupportEntry); }

    public:
        static SupportEntry*
        Create(nsFixedSizeAllocator& aPool) {
            void* place = aPool.Alloc(sizeof(SupportEntry));
            return place ? ::new (place) SupportEntry() : nsnull; }

        static void
        Destroy(nsFixedSizeAllocator& aPool, SupportEntry* aEntry);

        PLHashEntry           mHashEntry;
        MemoryElement*        mElement;
        nsTemplateMatchRefSet mMatchSet;
    };

    static PLHashAllocOps gSupportAllocOps;

    static void* PR_CALLBACK AllocSupportTable(void* aPool, PRSize aSize) {
        return new char[aSize]; }

    static void PR_CALLBACK FreeSupportTable(void* aPool, void* aItem) {
        delete[] NS_STATIC_CAST(char*, aItem); }

    static PLHashEntry* PR_CALLBACK AllocSupportEntry(void* aPool, const void* aKey) {
        nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);

        SupportEntry* entry = SupportEntry::Create(*pool);
        if (! entry)
            return nsnull;

        const MemoryElement* element = NS_STATIC_CAST(const MemoryElement*, aKey);
        entry->mElement = element->Clone(aPool);

        return NS_REINTERPRET_CAST(PLHashEntry*, entry); }

    static void PR_CALLBACK FreeSupportEntry(void* aPool, PLHashEntry* aHashEntry, PRUintn aFlag) {
        nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);
        if (aFlag == HT_FREE_ENTRY)
            SupportEntry::Destroy(*pool, NS_REINTERPRET_CAST(SupportEntry*, aHashEntry)); }

    static PLHashNumber PR_CALLBACK HashMemoryElement(const void* aBinding);
    static PRIntn PR_CALLBACK CompareMemoryElements(const void* aLeft, const void* aRight);


    /**
     * Maps a MemoryElement to the nsTemplateMatch objects whose bindings it
     * participates in. This makes it possible to efficiently update a
     * match when a binding changes.
     */
    PLHashTable* mBindingDependencies;

    class BindingEntry {
    protected:
        // Hide so that only Create() and Destroy() can be used to
        // allocate and deallocate from the heap
        static void* operator new(size_t) { return 0; }
        static void operator delete(void*, size_t) {}

    public:
        BindingEntry()
            { MOZ_COUNT_CTOR(nsConflictSet::BindingEntry); }

        ~BindingEntry()
            { MOZ_COUNT_DTOR(nsConflictSet::BindingEntry); }

        static BindingEntry*
        Create(nsFixedSizeAllocator& aPool) {
            void* place = aPool.Alloc(sizeof(BindingEntry));
            return place ? ::new (place) BindingEntry() : nsnull; }

        static void
        Destroy(nsFixedSizeAllocator& aPool, BindingEntry* aEntry) {
            aEntry->~BindingEntry();
            aPool.Free(aEntry, sizeof(*aEntry)); }

        PLHashEntry           mHashEntry;
        nsTemplateMatchRefSet mMatchSet;
    };

    static PLHashAllocOps gBindingAllocOps;

    static void* PR_CALLBACK AllocBindingTable(void* aPool, PRSize aSize) {
        return new char[aSize]; }

    static void PR_CALLBACK FreeBindingTable(void* aPool, void* aItem) {
        delete[] NS_STATIC_CAST(char*, aItem); }

    static PLHashEntry* PR_CALLBACK AllocBindingEntry(void* aPool, const void* aKey) {
        nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);

        BindingEntry* entry = BindingEntry::Create(*pool);
        if (! entry)
            return nsnull;

        nsIRDFResource* key = NS_STATIC_CAST(nsIRDFResource*, NS_CONST_CAST(void*, aKey));
        NS_ADDREF(key);

        return NS_REINTERPRET_CAST(PLHashEntry*, entry); }

    static void PR_CALLBACK FreeBindingEntry(void* aPool, PLHashEntry* aHashEntry, PRUintn aFlag) {
        if (aFlag == HT_FREE_ENTRY) {
            nsIRDFResource* key = NS_STATIC_CAST(nsIRDFResource*, NS_CONST_CAST(void*, aHashEntry->key));
            NS_RELEASE(key);
            nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);
            BindingEntry::Destroy(*pool, NS_REINTERPRET_CAST(BindingEntry*, aHashEntry));
        } }

    static PLHashNumber PR_CALLBACK HashBindingElement(const void* aSupport) {
        return PLHashNumber(NS_PTR_TO_INT32(aSupport)) >> 3; }
        
    static PRIntn PR_CALLBACK CompareBindingElements(const void* aLeft, const void* aRight) {
        return aLeft == aRight; }

    // The pool from whence all our slop will be allocated
    nsFixedSizeAllocator mPool;
};

#endif // nsConflictSet_h__
