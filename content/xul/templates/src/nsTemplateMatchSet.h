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

#ifndef nsTemplateMatchSet_h__
#define nsTemplateMatchSet_h__

#include "nsRuleNetwork.h"
#include "nsFixedSizeAllocator.h"
#include "nsTemplateMatch.h"
#include "pldhash.h"

class nsTemplateMatchSet {
public:
    class ConstIterator;
    friend class ConstIterator; // so it can see Element

protected:
    class Element {
    public:
        Element(nsTemplateMatch* aMatch)
            : mMatch(aMatch), mNext(nsnull) {}

        nsTemplateMatch* mMatch;
        Element*         mNext;
    };

    nsFixedSizeAllocator& mPool;
    Element* mHead;

public:
    nsTemplateMatchSet(nsFixedSizeAllocator& aPool)
        : mPool(aPool), mHead(nsnull) { MOZ_COUNT_CTOR(nsTemplateMatchSet); }

    ~nsTemplateMatchSet();

    class ConstIterator {
    protected:
        friend class nsTemplateMatchSet;

        Element* mCurrent;
        
        ConstIterator(Element* aElement)
            : mCurrent(aElement) {}

    public:
        ConstIterator() : mCurrent(nsnull) {}

        ConstIterator(const ConstIterator& aIterator)
            : mCurrent(aIterator.mCurrent) {}

        ConstIterator&
        operator=(const ConstIterator& aIterator) {
            mCurrent = aIterator.mCurrent;
            return *this; }

        ConstIterator&
        operator++() {
            mCurrent = mCurrent->mNext;
            return *this; };

        ConstIterator
        operator++(int) {
            ConstIterator tmp(*this);
            mCurrent = mCurrent->mNext;
            return tmp; }

        nsTemplateMatch& operator*() const {
            return *(mCurrent->mMatch); }

        nsTemplateMatch* operator->() const {
            return mCurrent->mMatch; }

        PRBool
        operator==(const ConstIterator& aIterator) const {
            return mCurrent == aIterator.mCurrent; }

        PRBool
        operator!=(const ConstIterator& aIterator) const {
            return !aIterator.operator==(*this); }
    };

    ConstIterator First() const { return ConstIterator(mHead); }

    ConstIterator Last() const { return ConstIterator(nsnull); }

    void
    Add(nsTemplateMatch* aMatch) {
        Element* element = new Element(aMatch);
        if (element) {
            aMatch->AddRef();
            element->mMatch = aMatch;
            element->mNext = mHead;
            mHead = element;
        } }
};

/**
 * A set of references nsTemplateMatch objects.
 */
class nsTemplateMatchRefSet
{
public:
    class ConstIterator;
    friend class ConstIterator;

    nsTemplateMatchRefSet() {
        MOZ_COUNT_CTOR(nsTemplateMatchRefSet);
        Init(); }

    nsTemplateMatchRefSet(const nsTemplateMatchRefSet& aMatchSet) {
        MOZ_COUNT_CTOR(nsTemplateMatchRefSet);
        Init();
        CopyFrom(aMatchSet); }

    nsTemplateMatchRefSet&
    operator=(const nsTemplateMatchRefSet& aMatchSet) {
        Finish();
        Init();
        CopyFrom(aMatchSet);
        return *this; }

    ~nsTemplateMatchRefSet() {
        Finish();
        MOZ_COUNT_DTOR(nsTemplateMatchRefSet); }

protected:
    /**
     * Initialize the set. Must be called before the set is used, or
     * after Finish().
     */
    void Init();

    /**
     * Finish the set, releasing all matches. This must be called to
     * properly release matches in the set. Yeah, yeah, this sucks.
     */
    void Finish();

    /**
     * Copy the set's contents from another match set
     */
    void CopyFrom(const nsTemplateMatchRefSet& aMatchSet);

    /**
     * Helper routine that adds the match to the hashtable
     */
    PRBool AddToTable(nsTemplateMatch* aMatch);

    /**
     * Hashtable entry; holds weak reference to a match object.
     *
     * NOTE: this has to be public for several compilers (see 74725) so
     *       that ConstIterator's union can have access to Entry.
     */
public:
    struct Entry {
        friend class ConstIterator;

        PLDHashEntryHdr  mHdr;
        nsTemplateMatch* mMatch;
    };
protected:

    enum { kMaxInlineMatches = (sizeof(PLDHashTable) / sizeof(void*)) - 1 };

    struct InlineMatches;
    friend struct InlineMatches;
    union _stor_elements;
    friend union _stor_elements;

    /**
     * If the set is currently
     */
    struct InlineMatches {
        PRUint32         mCount;
        nsTemplateMatch* mEntries[kMaxInlineMatches];
    };

    /**
     * The set is implemented as a dual datastructure. It is initially
     * a simple array that holds storage for kMaxInlineMatches
     * elements. Once that capacity is exceeded, the storage is
     * re-used for a PLDHashTable header. The hashtable allocates its
     * entries from the normal malloc() heap.
     *
     * the InlineMatches structure is implemented such that its mCount
     * variable overlaps with the PLDHashTable's `ops' member (which
     * is a pointer to the hashtable's callback table). On a 32-bit
     * architecture, we're safe assuming that the value for `ops' will
     * be larger than kMaxInlineMatches when treated as an unsigned
     * integer. And we'd have to get pretty unlucky on a 64-bit
     * system for us to get screwed, I think.
     *
     * Instrumentation (#define NSTEMPLATEMATCHSET_METER) shows that
     * almost all of the match sets contain fewer than seven elements.
     */
    union _stor_elements {
        PLDHashTable  mTable;
        InlineMatches mInlineMatches;
    } mStorageElements;

    static PLDHashTableOps gOps;

    static PLDHashNumber PR_CALLBACK
    HashEntry(PLDHashTable* aTable, const void* aKey);

    static PRBool PR_CALLBACK
    MatchEntry(PLDHashTable* aTable, const PLDHashEntryHdr* aHdr, const void* aKey);

public:
    /**
     * An iterator that can be used to enumerate the contents of the
     * set
     */
    class ConstIterator {
    protected:
        friend class nsTemplateMatchRefSet;

        void Next();
        void Prev();

        ConstIterator(const nsTemplateMatchRefSet* aSet, Entry* aTableEntry)
            : mSet(aSet), mTableEntry(aTableEntry) {}

        ConstIterator(const nsTemplateMatchRefSet* aSet, nsTemplateMatch** aInlineEntry)
            : mSet(aSet), mInlineEntry(aInlineEntry) {}

        const nsTemplateMatchRefSet* mSet;
        union {
            Entry*            mTableEntry;
            nsTemplateMatch** mInlineEntry;
        };

        nsTemplateMatch* get() const {
            return mSet->mStorageElements.mInlineMatches.mCount > PRUint32(kMaxInlineMatches)
                ? mTableEntry->mMatch
                : *mInlineEntry; }

    public:
        ConstIterator() : mSet(nsnull), mTableEntry(nsnull) {}

        ConstIterator(const ConstIterator& aConstIterator)
            : mSet(aConstIterator.mSet),
              mTableEntry(aConstIterator.mTableEntry) {}

        ConstIterator& operator=(const ConstIterator& aConstIterator) {
            mSet = aConstIterator.mSet;
            mTableEntry = aConstIterator.mTableEntry;
            return *this; }

        ConstIterator& operator++() {
            Next();
            return *this; }

        ConstIterator operator++(int) {
            ConstIterator result(*this);
            Next();
            return result; }

        ConstIterator& operator--() {
            Prev();
            return *this; }

        ConstIterator operator--(int) {
            ConstIterator result(*this);
            Prev();
            return result; }

        /*const*/ nsTemplateMatch& operator*() const {
            return *get(); }

        /*const*/ nsTemplateMatch* operator->() const {
            return get(); }

        PRBool operator==(const ConstIterator& aConstIterator) const;

        PRBool operator!=(const ConstIterator& aConstIterator) const {
            return ! aConstIterator.operator==(*this); }
    };

    /**
     * Retrieve an iterator that refers to the first element of the
     * set
     */
    ConstIterator First() const;

    /**
     * Retrieve an iterator that refers to ``one past'' the last
     * element of the set
     */
    ConstIterator Last() const;

    /**
     * Return PR_TRUE if the set is empty
     */
    PRBool Empty() const;

    /**
     * Return PR_TRUE if the set contains aMatch
     */
    PRBool Contains(const nsTemplateMatch* aMatch) const;

    /**
     * Add a match to the set. The set does *not* assume ownership of
     * the match object: it only holds a weak reference. Duplicate
     * matches are not added.
     *
     * @return PR_TRUE if the match was added to the set; PR_FALSE if it
     *   already existed (or could not be added for some other reason)
     */
    PRBool Add(const nsTemplateMatch* aMatch);

    /**
     * Remove a match from the set.
     * @return PR_TRUE if the match was removed from the set; PR_FALSE
     *   if the match was not present in the set.
     */
    PRBool Remove(const nsTemplateMatch* aMatch);
};

#endif // nsTemplateMatchSet_h__
