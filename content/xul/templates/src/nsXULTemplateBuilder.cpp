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
 * Original Author(s):
 *   Robert Churchill <rjc@netscape.com>
 *   David Hyatt <hyatt@netscape.com>
 *   Chris Waterson <waterson@netscape.com>
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*

  Builds content from an RDF graph using the XUL <template> tag.

  TO DO

  . fix ContentTagTest's location in the network contruction
  . use arenas
  . extended template syntax

  To turn on logging for this module, set:

    NSPR_LOG_MODULES nsRDFGenericBuilder:5

 */

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDocument.h"
#include "nsIHTMLContent.h"
#include "nsIElementFactory.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContainerUtils.h" 
#include "nsIRDFContentModelBuilder.h"
#include "nsIXULDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsITextContent.h"
#include "nsITimer.h"
#include "nsIURL.h"
#include "nsIXMLContent.h"
#include "nsIXPConnect.h"
#include "nsIXULSortService.h"
#include "nsLayoutCID.h"
#include "nsRDFCID.h"
#include "nsIXULContent.h"
#include "nsIXULContentUtils.h"
#include "nsRDFSort.h"
#include "nsRuleNetwork.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsXULElement.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "prlog.h"
#include "rdf.h"
#include "rdfutil.h"
#include "nsIFormControl.h"
#include "nsIDOMHTMLFormElement.h"

// Return values for EnsureElementHasGenericChild()
#define NS_RDF_ELEMENT_GOT_CREATED NS_RDF_NO_VALUE
#define NS_RDF_ELEMENT_WAS_THERE   NS_OK
static PRLogModuleInfo* gLog;

//----------------------------------------------------------------------

static NS_DEFINE_CID(kHTMLElementFactoryCID,     NS_HTML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,       NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,      NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kTextNodeCID,               NS_TEXTNODE_CID);
static NS_DEFINE_CID(kXMLElementFactoryCID,      NS_XML_ELEMENT_FACTORY_CID);
static NS_DEFINE_CID(kXULContentUtilsCID,        NS_XULCONTENTUTILS_CID);
static NS_DEFINE_CID(kXULSortServiceCID,         NS_XULSORTSERVICE_CID);

//----------------------------------------------------------------------

#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"

#ifdef DEBUG

// XXX the aValue argument is really "const", but since it's probably
// an nsISupports, and QueryInterface(), AddRef() and Release() are
// not "const" methods, we need to let it be mutable.
static nsISupports*
value_to_isupports(const nsIID& aIID, const Value& aValue)
{
    nsresult rv;
    nsISupports* isupports = NS_STATIC_CAST(nsISupports*, NS_CONST_CAST(Value&, aValue));
    if (isupports) {
        nsISupports* dummy;
        rv = isupports->QueryInterface(aIID, (void**) &dummy);
        if (NS_SUCCEEDED(rv)) {
            NS_RELEASE(dummy);
        }
        else {
            NS_ERROR("value does not support expected interface");
        }
    }
    return isupports;
}

#define VALUE_TO_ISUPPORTS(type, v) NS_STATIC_CAST(type*, value_to_isupports(NS_GET_IID(type), (v)))

#else
#define VALUE_TO_ISUPPORTS(type, v) NS_STATIC_CAST(type*, NS_STATIC_CAST(nsISupports*, (v)))
#endif

#define VALUE_TO_IRDFRESOURCE(v) VALUE_TO_ISUPPORTS(nsIRDFResource, (v))
#define VALUE_TO_IRDFNODE(v)     VALUE_TO_ISUPPORTS(nsIRDFNode, (v))
#define VALUE_TO_ICONTENT(v)     VALUE_TO_ISUPPORTS(nsIContent, (v))

//----------------------------------------------------------------------

static PRBool
IsElementContainedBy(nsIContent* aElement, nsIContent* aContainer)
{
    // Make sure that we're actually creating content for the tree
    // content model that we've been assigned to deal with.

    // Walk up the parent chain from us to the root and
    // see what we find.
    if (aElement == aContainer)
        return PR_TRUE;

    // walk up the tree until you find rootAtom
    nsCOMPtr<nsIContent> element(do_QueryInterface(aElement));
    nsCOMPtr<nsIContent> parent;
    element->GetParent(*getter_AddRefs(parent));
    element = parent;
    
    while (element) {
        if (element.get() == aContainer)
            return PR_TRUE;

        element->GetParent(*getter_AddRefs(parent));
        element = parent;
    }
    
    return PR_FALSE;
}

//----------------------------------------------------------------------

class PropertySet
{
public:
    PropertySet()
        : mProperties(nsnull),
          mCount(0),
          mCapacity(0) {
        MOZ_COUNT_CTOR(PropertySet); }
    
    ~PropertySet();

    nsresult Clear();
    nsresult Add(nsIRDFResource* aProperty);

    PRBool Contains(nsIRDFResource* aProperty) const;

protected:
    nsIRDFResource** mProperties;
    PRInt32 mCount;
    PRInt32 mCapacity;

public:
    class ConstIterator {
    protected:
        nsIRDFResource** mCurrent;

    public:
        ConstIterator(const ConstIterator& aConstIterator)
            : mCurrent(aConstIterator.mCurrent) {}

        ConstIterator& operator=(const ConstIterator& aConstIterator) {
            mCurrent = aConstIterator.mCurrent;
            return *this; }

        ConstIterator& operator++() {
            ++mCurrent;
            return *this; }

        ConstIterator operator++(int) {
            ConstIterator result(*this);
            ++mCurrent;
            return result; }

        /*const*/ nsIRDFResource* operator*() const {
            return *mCurrent; }

        /*const*/ nsIRDFResource* operator->() const {
            return *mCurrent; }

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }

    protected:
        ConstIterator(nsIRDFResource** aProperty) : mCurrent(aProperty) {}
        friend class PropertySet;
    };

    ConstIterator First() const { return ConstIterator(mProperties); }
    ConstIterator Last() const { return ConstIterator(mProperties + mCount); }
};


PropertySet::~PropertySet()
{
    MOZ_COUNT_DTOR(PropertySet);
    Clear();
    delete[] mProperties;
}

nsresult
PropertySet::Clear()
{
    while (--mCount >= 0) {
        NS_RELEASE(mProperties[mCount]);
    }
    mCount = 0;
    return NS_OK;
}

nsresult
PropertySet::Add(nsIRDFResource* aProperty)
{
    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    if (Contains(aProperty))
        return NS_OK;

    if (mCount >= mCapacity) {
        PRInt32 capacity = mCapacity + 4;
        nsIRDFResource** properties = new nsIRDFResource*[capacity];
        if (! properties)
            return NS_ERROR_OUT_OF_MEMORY;

        for (PRInt32 i = mCount - 1; i >= 0; --i)
            properties[i] = mProperties[i];

        delete[] mProperties;

        mProperties = properties;
        mCapacity = capacity;
    }

    mProperties[mCount++] = aProperty;
    NS_ADDREF(aProperty);
    return NS_OK;
}

PRBool
PropertySet::Contains(nsIRDFResource* aProperty) const
{
    for (PRInt32 i = mCount - 1; i >= 0; --i) {
        if (mProperties[i] == aProperty)
            return PR_TRUE;
    }

    return PR_FALSE;
}

//----------------------------------------------------------------------
//
// Rule
//

class Rule
{
public:
    Rule(nsIContent* aContent,
         PRInt32 aContainerVariable,
         PRInt32 aMemberVariable,
         PRInt32 aPriority)
        : mContent(aContent),
          mContainerVariable(aContainerVariable),
          mMemberVariable(aMemberVariable),
          mPriority(aPriority) {
        MOZ_COUNT_CTOR(Rule); }

    ~Rule() { MOZ_COUNT_DTOR(Rule); }

    nsresult GetContent(nsIContent** aResult) const;
    PRInt32 GetContainerVariable() const { return mContainerVariable; }
    PRInt32 GetMemberVariable() const { return mMemberVariable; }
    PRInt32 GetPriority() const { return mPriority; }

protected:
    nsCOMPtr<nsIContent> mContent;
    PRInt32 mContainerVariable;
    PRInt32 mMemberVariable;
    PRInt32 mPriority;
};


nsresult
Rule::GetContent(nsIContent** aResult) const
{
    *aResult = mContent.get();
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

//----------------------------------------------------------------------

/**
 * A set of <rule, instantiation> tuples.
 */
class MatchSet
{
public:
    class ConstIterator;
    friend class ConstIterator;

    class Iterator;
    friend class Iterator;

    /**
     * A single <rule, instantiation> tuple.
     */
    class Match {
    public:
        Match() { MOZ_COUNT_CTOR(Match); }

        Match(const Rule* aRule, const Instantiation& aInstantiation)
            : mRule(aRule), mInstantiation(aInstantiation) {
            MOZ_COUNT_CTOR(Match); }

        Match(const Match& aMatch)
            : mRule(aMatch.mRule), mInstantiation(aMatch.mInstantiation) {
            MOZ_COUNT_CTOR(Match); }

        Match& operator=(const Match& aMatch) {
            mRule = aMatch.mRule;
            mInstantiation = aMatch.mInstantiation;
            return *this; }

        ~Match() { MOZ_COUNT_DTOR(Match); }

        PRBool operator==(const Match& aMatch) const {
            return mRule == aMatch.mRule && mInstantiation == aMatch.mInstantiation; }

        PRBool operator!=(const Match& aMatch) const {
            return !(*this == aMatch); }

        const Rule*   mRule;
        Instantiation mInstantiation;
    };

    MatchSet();
    ~MatchSet();

private:
    MatchSet(const MatchSet& aMatchSet); // XXX not to be implemented
    void operator=(const MatchSet& aMatchSet); // XXX not to be implemented

protected:
    struct MatchList {
        Match mMatch;
        MatchList* mNext;
        MatchList* mPrev;
    };

    MatchList mHead;

    // XXXwaterson Lazily create this if we pass a size threshold.
    PLHashTable* mMatches;
    PRInt32 mCount;

    static PLHashNumber HashMatch(const void* aMatch) {
        const Match* match = NS_STATIC_CAST(const Match*, aMatch);
        return Instantiation::Hash(&match->mInstantiation) ^ (PLHashNumber(match->mRule) >> 2); }

    static PRIntn CompareMatches(const void* aLeft, const void* aRight) {
        const Match* left  = NS_STATIC_CAST(const Match*, aLeft);
        const Match* right = NS_STATIC_CAST(const Match*, aRight);
        return *left == *right; }

    enum { kHashTableThreshold = 8 };

public:
    class ConstIterator {
    protected:
        friend class Iterator; // XXXwaterson so broken.
        MatchList* mCurrent;

    public:
        ConstIterator(MatchList* aCurrent) : mCurrent(aCurrent) {}

        ConstIterator(const ConstIterator& aConstIterator)
            : mCurrent(aConstIterator.mCurrent) {}

        ConstIterator& operator=(const ConstIterator& aConstIterator) {
            mCurrent = aConstIterator.mCurrent;
            return *this; }

        ConstIterator& operator++() {
            mCurrent = mCurrent->mNext;
            return *this; }

        ConstIterator operator++(int) {
            ConstIterator result(*this);
            mCurrent = mCurrent->mNext;
            return result; }

        ConstIterator& operator--() {
            mCurrent = mCurrent->mPrev;
            return *this; }

        ConstIterator operator--(int) {
            ConstIterator result(*this);
            mCurrent = mCurrent->mPrev;
            return result; }

        const Match& operator*() const {
            return mCurrent->mMatch; }

        const Match* operator->() const {
            return &mCurrent->mMatch; }

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }
    };

    ConstIterator First() const { return ConstIterator(mHead.mNext); }
    ConstIterator Last() const { return ConstIterator(NS_CONST_CAST(MatchList*, &mHead)); }

    class Iterator : public ConstIterator {
    public:
        Iterator(MatchList* aCurrent) : ConstIterator(aCurrent) {}

        Iterator& operator++() {
            mCurrent = mCurrent->mNext;
            return *this; }

        Iterator operator++(int) {
            Iterator result(*this);
            mCurrent = mCurrent->mNext;
            return result; }

        Iterator& operator--() {
            mCurrent = mCurrent->mPrev;
            return *this; }

        Iterator operator--(int) {
            Iterator result(*this);
            mCurrent = mCurrent->mPrev;
            return result; }

        Match& operator*() const {
            return mCurrent->mMatch; }

        Match* operator->() const {
            return &mCurrent->mMatch; }

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }

        friend class MatchSet;
    };

    Iterator First() { return Iterator(mHead.mNext); }
    Iterator Last() { return Iterator(&mHead); }

    PRBool Empty() const { return First() == Last(); }

    PRBool Contains(const Rule* aRule, const Instantiation& aInstantiation) const;

    nsresult FindMatchWithHighestPriority(const Match** aMatch) const;

    Iterator Insert(Iterator aIterator, const Match& aMatch);

    Iterator Add(const Match& aMatch) {
        return Insert(Last(), aMatch); }

    Iterator Add(const Rule* aRule, const Instantiation& aInstantiation) {
        return Add(Match(aRule, aInstantiation)); }

    void Clear();
    Iterator Erase(Iterator aIterator);
};



MatchSet::MatchSet()
    : mMatches(nsnull), mCount(0)
{
    MOZ_COUNT_CTOR(MatchSet);
    mHead.mNext = mHead.mPrev = &mHead;
}

#ifdef NEED_CPP_UNUSED_IMPLEMENTATIONS
MatchSet::MatchSet(const MatchSet& aMatchSet) {}
void MatchSet::operator=(const MatchSet& aMatchSet) {}
#endif

MatchSet::~MatchSet()
{
    if (mMatches) {
        PL_HashTableDestroy(mMatches);
        mMatches = nsnull;
    }

    Clear();
    MOZ_COUNT_DTOR(MatchSet);
}


MatchSet::Iterator
MatchSet::Insert(Iterator aIterator, const Match& aMatch)
{
    if (++mCount > kHashTableThreshold && !mMatches) {
        // If we've exceeded a high-water mark, then hash everything.
        mMatches = PL_NewHashTable(2 * kHashTableThreshold,
                                   HashMatch,
                                   CompareMatches,
                                   PL_CompareValues,
                                   nsnull /* XXXwaterson use an arena */,
                                   nsnull);

        Iterator last = Last();
        for (Iterator match = First(); match != last; ++match)
            PL_HashTableAdd(mMatches, &*match, &*match);
    }

    MatchList* newelement = new MatchList();
    if (newelement) {
        newelement->mMatch = aMatch;

        aIterator.mCurrent->mPrev->mNext = newelement;

        newelement->mNext = aIterator.mCurrent;
        newelement->mPrev = aIterator.mCurrent->mPrev;

        aIterator.mCurrent->mPrev = newelement;

        if (mMatches)
            PL_HashTableAdd(mMatches, &*aIterator, &*aIterator);
    }
    return aIterator;
}

void
MatchSet::Clear()
{
    Iterator match = First();
    while (match != Last())
        Erase(match++);
}


PRBool
MatchSet::Contains(const Rule* aRule, const Instantiation& aInstantiation) const
{
    Match match(aRule, aInstantiation);
    if (mMatches) {
        return PL_HashTableLookup(mMatches, &match) != nsnull;
    }
    else {
        ConstIterator last = Last();
        for (ConstIterator i = First(); i != last; ++i) {
            if (*i == match)
                return PR_TRUE;
        }
    }

    return PR_FALSE;
}

nsresult
MatchSet::FindMatchWithHighestPriority(const Match** aMatch) const
{
    // Find the rule with the "highest priority"; i.e., the rule with
    // the lowest value for GetPriority().
    const Match* result = nsnull;
    PRInt32 max = ~(1 << 31); // XXXwaterson portable?
    for (ConstIterator match = First(); match != Last(); ++match) {
        PRInt32 priority = match->mRule->GetPriority();
        if (priority < max) {
            result = &*match;
            max = priority;
        }
    }
    *aMatch = result;
    return NS_OK;
}

MatchSet::Iterator
MatchSet::Erase(Iterator aIterator)
{
    Iterator result = aIterator;

    --mCount;

    if (mMatches)
        PL_HashTableRemove(mMatches, &*aIterator);

    ++result;
    aIterator.mCurrent->mNext->mPrev = aIterator.mCurrent->mPrev;
    aIterator.mCurrent->mPrev->mNext = aIterator.mCurrent->mNext;

    delete aIterator.mCurrent;
    return result;
}

//----------------------------------------------------------------------

class Key { // XXXwaterson this needs a better name
public:
    Key() { MOZ_COUNT_CTOR(Key); }

    Key(const Instantiation& aInstantiation, const Rule* aRule);

    Key(const Key& aKey)
        : mContainerVariable(aKey.mContainerVariable),
          mContainerValue(aKey.mContainerValue),
          mMemberVariable(aKey.mMemberVariable),
          mMemberValue(aKey.mMemberValue) {
        MOZ_COUNT_CTOR(Key); }

    ~Key() { MOZ_COUNT_DTOR(Key); }

    Key& operator=(const Key& aKey) {
        mContainerVariable = aKey.mContainerVariable;
        mContainerValue    = aKey.mContainerValue;
        mMemberVariable    = aKey.mMemberVariable;
        mMemberValue       = aKey.mMemberValue;
        return *this; }

    PRBool operator==(const Key& aKey) const {
        return Equals(aKey); }

    PRBool operator!=(const Key& aKey) const {
        return !Equals(aKey); }

    PRInt32     mContainerVariable;
    Value       mContainerValue;
    PRInt32     mMemberVariable;
    Value       mMemberValue;

    PLHashNumber Hash() const {
        PLHashNumber temp1;
        temp1 = mContainerValue.Hash();
        temp1 &= 0xffff;
        temp1 |= PLHashNumber(mContainerVariable) << 16;
        PLHashNumber temp2;
        temp2 = mMemberValue.Hash();
        temp2 &= 0xffff;
        temp2 |= PLHashNumber(mMemberVariable) << 16;
        return temp1 ^ temp2; }

    static PLHashNumber HashKey(const void* aKey);
    static PRIntn CompareKeys(const void* aLeft, const void* aRight);

protected:
    PRBool Equals(const Key& aKey) const {
        return mContainerVariable == aKey.mContainerVariable &&
            mContainerValue == aKey.mContainerValue &&
            mMemberVariable == aKey.mMemberVariable &&
            mMemberValue == aKey.mMemberValue; }
};


Key::Key(const Instantiation& aInstantiation, const Rule* aRule)
{
    PRBool hasbinding;

    mContainerVariable = aRule->GetContainerVariable();
    hasbinding = aInstantiation.mBindings.GetBindingFor(mContainerVariable, &mContainerValue);
    NS_ASSERTION(hasbinding, "no binding for container variable");

    mMemberVariable = aRule->GetMemberVariable();
    hasbinding = aInstantiation.mBindings.GetBindingFor(mMemberVariable, &mMemberValue);
    NS_ASSERTION(hasbinding, "no binding for member variable");

    MOZ_COUNT_CTOR(Key);
}


PLHashNumber
Key::HashKey(const void* aKey)
{
    const Key* key = NS_STATIC_CAST(const Key*, aKey);
    return key->Hash();
}

PRIntn
Key::CompareKeys(const void* aLeft, const void* aRight)
{
    const Key* left  = NS_STATIC_CAST(const Key*, aLeft);
    const Key* right = NS_STATIC_CAST(const Key*, aRight);
    return *left == *right;
}


//----------------------------------------------------------------------

class KeySet {
public:
    class ConstIterator;
    friend class ConstIterator;

protected:
    class Entry {
    public:
        Entry() { MOZ_COUNT_CTOR(KeySet::Entry); }

        Entry(const Key& aKey) : mKey(aKey) {
            MOZ_COUNT_CTOR(KeySet::Entry); }

        ~Entry() { MOZ_COUNT_DTOR(KeySet::Entry); }

        PLHashEntry mHashEntry;
        Key         mKey;
        Entry*      mPrev;
        Entry*      mNext;
    };

    PLHashTable* mTable;
    Entry mHead;

public:
    KeySet();
    ~KeySet();

    class ConstIterator {
    protected:
        Entry* mCurrent;

    public:
        ConstIterator(Entry* aEntry) : mCurrent(aEntry) {}

        ConstIterator(const ConstIterator& aConstIterator)
            : mCurrent(aConstIterator.mCurrent) {}

        ConstIterator& operator=(const ConstIterator& aConstIterator) {
            mCurrent = aConstIterator.mCurrent;
            return *this; }

        ConstIterator& operator++() {
            mCurrent = mCurrent->mNext;
            return *this; }

        ConstIterator operator++(int) {
            ConstIterator result(*this);
            mCurrent = mCurrent->mNext;
            return result; }

        const Key& operator*() const {
            return mCurrent->mKey; }

        const Key* operator->() const {
            return &mCurrent->mKey; }

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }
    };

    ConstIterator First() const { return ConstIterator(mHead.mNext); }
    ConstIterator Last() const { return ConstIterator(NS_CONST_CAST(Entry*, &mHead)); }

    PRBool Contains(const Key& aKey);
    nsresult Add(const Key& aKey);

protected:
    static PLHashAllocOps gAllocOps;

    static void*        PR_CALLBACK AllocTable(void* aPool, PRSize aSize);
    static void         PR_CALLBACK FreeTable(void* aPool, void* aItem);
    static PLHashEntry* PR_CALLBACK AllocEntry(void* aPool, const void* aKey);
    static void         PR_CALLBACK FreeEntry(void* aPool, PLHashEntry* aEntry, PRUintn aFlag);
};

PLHashAllocOps KeySet::gAllocOps = {
    AllocTable,
    FreeTable,
    AllocEntry,
    FreeEntry
};

void* PR_CALLBACK
KeySet::AllocTable(void* aPool, PRSize aSize)
{
    return new char[aSize];
}

void PR_CALLBACK
KeySet::FreeTable(void* aPool, void* aItem)
{
    delete[] NS_STATIC_CAST(char*, aItem);
}

PLHashEntry* PR_CALLBACK
KeySet::AllocEntry(void* aPool, const void* aKey)
{
    const Key* key = NS_STATIC_CAST(const Key*, aKey);
    Entry* entry = new Entry(*key);
    return NS_REINTERPRET_CAST(PLHashEntry*, entry);
}

void PR_CALLBACK
KeySet::FreeEntry(void* aPool, PLHashEntry* aEntry, PRUintn aFlag)
{
    Entry* entry = NS_REINTERPRET_CAST(Entry*, aEntry);
    delete entry;
}

KeySet::KeySet()
    : mTable(nsnull)
{
    mHead.mPrev = mHead.mNext = &mHead;

    mTable = PL_NewHashTable(8, Key::HashKey, Key::CompareKeys, PL_CompareValues,
                             &gAllocOps, nsnull);

    MOZ_COUNT_CTOR(KeySet);
}


KeySet::~KeySet()
{
    PL_HashTableDestroy(mTable);
    MOZ_COUNT_DTOR(KeySet);
}

PRBool
KeySet::Contains(const Key& aKey)
{
    return nsnull != PL_HashTableLookup(mTable, &aKey);
}

nsresult
KeySet::Add(const Key& aKey)
{
    PLHashNumber hash = aKey.Hash();
    PLHashEntry** hep = PL_HashTableRawLookup(mTable, hash, &aKey);

    if (hep && *hep)
        return NS_OK; // already had it.

    PLHashEntry* he = PL_HashTableRawAdd(mTable, hep, hash, &aKey, nsnull);
    if (! he)
        return NS_ERROR_OUT_OF_MEMORY;

    Entry* entry = NS_REINTERPRET_CAST(Entry*, he);

    // XXX yes, I am evil. Fixup the key in the hashentry to point to
    // the value it contains, rather than the one on the stack.
    entry->mHashEntry.key = &entry->mKey;

    // thread
    mHead.mPrev->mNext = entry;
    entry->mPrev = mHead.mPrev;
    entry->mNext = &mHead;
    mHead.mPrev = entry;
    
    return NS_OK;
}


//----------------------------------------------------------------------

/**
 * Maintains the set of active instantiations.
 */
class ConflictSet
{
public:
    ConflictSet();
    ~ConflictSet();

    nsresult Add(const Instantiation& aInstantiation, const Rule* aRule);

    void GetMatches(const Key& aKey, const MatchSet** aMatchSet) const;

    void GetMatches(PRInt32 aVariable,
                    const Value& aValue,
                    const MatchSet** aMatchSet) const;

    void Remove(const MemoryElement& aInstantiation,
                MatchSet& aNewMatches,
                MatchSet& aRetractedMatches);

    void Clear();

protected:
    nsresult Init();
    nsresult Destroy();

    // "Clusters" of matched rules for the same <content, member> pair
    PLHashTable* mMatches;

    class MatchEntry {
    public:
        MatchEntry() { MOZ_COUNT_CTOR(ConflictSet::MatchEntry); }
        ~MatchEntry() { MOZ_COUNT_DTOR(ConflictSet::MatchEntry); }

        struct PLHashEntry mHashEntry;
        Key                mKey;
        MatchSet           mMatchSet;
    };

    static PLHashAllocOps gMatchAllocOps;
    static void*        PR_CALLBACK AllocMatchTable(void* aPool, PRSize aSize);
    static void         PR_CALLBACK FreeMatchTable(void* aPool, void* aItem);
    static PLHashEntry* PR_CALLBACK AllocMatchEntry(void* aPool, const void* aKey);
    static void         PR_CALLBACK FreeMatchEntry(void* aPool, PLHashEntry* aHashEntry, PRUintn aFlag);

    // Maps an Instantiation::Binding to a MatchSet
    PLHashTable* mBindings;

    class BindingEntry {
    public:
        BindingEntry() : mElement(nsnull)
            { MOZ_COUNT_CTOR(ConflictSet::BindingEntry); }

        ~BindingEntry() {
            delete mElement;
            MOZ_COUNT_DTOR(ConflictSet::BindingEntry); }

        struct PLHashEntry mHashEntry;
        MemoryElement*     mElement;
        MatchSet           mMatchSet;
    };

    static PLHashAllocOps gBindingAllocOps;
    static void*        PR_CALLBACK AllocBindingTable(void* aPool, PRSize aSize);
    static void         PR_CALLBACK FreeBindingTable(void* aPool, void* aItem);
    static PLHashEntry* PR_CALLBACK AllocBindingEntry(void* aPool, const void* aKey);
    static void         PR_CALLBACK FreeBindingEntry(void* aPool, PLHashEntry* aHashEntry, PRUintn aFlag);

    static PLHashNumber HashMemoryElement(const void* aBinding);
    static PRIntn CompareMemoryElements(const void* aLeft, const void* aRight);
};

// Allocation operations for the match table
PLHashAllocOps ConflictSet::gMatchAllocOps = {
    AllocMatchTable, FreeMatchTable, AllocMatchEntry, FreeMatchEntry };


void* PR_CALLBACK
ConflictSet::AllocMatchTable(void* aPool, PRSize aSize)
{
    return new char[aSize];
}

void PR_CALLBACK
ConflictSet::FreeMatchTable(void* aPool, void* aItem)
{
    delete[] NS_STATIC_CAST(char*, aItem);
}

PLHashEntry* PR_CALLBACK
ConflictSet::AllocMatchEntry(void* aPool, const void* aKey)
{
    MatchEntry* entry = new MatchEntry();
    if (! entry)
        return nsnull;

    entry->mKey = *NS_STATIC_CAST(const Key*, aKey);
    return NS_REINTERPRET_CAST(PLHashEntry*, entry);
}

void PR_CALLBACK
ConflictSet::FreeMatchEntry(void* aPool, PLHashEntry* aHashEntry, PRUintn aFlag)
{
    MatchEntry* entry = NS_REINTERPRET_CAST(MatchEntry*, aHashEntry);
    delete entry;
}

// Allocation operations for the bindings table
PLHashAllocOps ConflictSet::gBindingAllocOps = {
    AllocBindingTable, FreeBindingTable, AllocBindingEntry, FreeBindingEntry };


void* PR_CALLBACK
ConflictSet::AllocBindingTable(void* aPool, PRSize aSize)
{
    return new char[aSize];
}

void PR_CALLBACK
ConflictSet::FreeBindingTable(void* aPool, void* aItem)
{
    delete[] NS_STATIC_CAST(char*, aItem);
}

PLHashEntry* PR_CALLBACK
ConflictSet::AllocBindingEntry(void* aPool, const void* aKey)
{
    BindingEntry* entry = new BindingEntry();
    if (! entry)
        return nsnull;

    const MemoryElement* element = NS_STATIC_CAST(const MemoryElement*, aKey);
    entry->mElement = element->Clone();

    return NS_REINTERPRET_CAST(PLHashEntry*, entry);
}

void PR_CALLBACK
ConflictSet::FreeBindingEntry(void* aPool, PLHashEntry* aHashEntry, PRUintn aFlag)
{
    BindingEntry* entry = NS_REINTERPRET_CAST(BindingEntry*, aHashEntry);
    delete entry;
}


ConflictSet::ConflictSet()
    : mMatches(nsnull), mBindings(nsnull)
{
    Init();
}

ConflictSet::~ConflictSet()
{
    Destroy();
}


nsresult
ConflictSet::Init()
{
    mMatches = PL_NewHashTable(16 /* XXXwaterson we need a way to give a hint? */,
                               Key::HashKey,
                               Key::CompareKeys,
                               PL_CompareValues,
                               &gMatchAllocOps,
                               nsnull /* XXXwaterson use an arena */);

    mBindings = PL_NewHashTable(16, /* XXXwaterson need hint */
                                HashMemoryElement,
                                CompareMemoryElements,
                                PL_CompareValues,
                                &gBindingAllocOps,
                                nsnull /* XXXwaterson use an arena */);

    return NS_OK;
}


nsresult
ConflictSet::Destroy()
{
    PL_HashTableDestroy(mBindings);
    PL_HashTableDestroy(mMatches);
    return NS_OK;
}

nsresult
ConflictSet::Add(const Instantiation& aInstantiation, const Rule* aRule)
{
    // add the match to a table indexed by instantiation key
    {
        Key key(aInstantiation, aRule);

        PLHashNumber hash = key.Hash();
        PLHashEntry** hep = PL_HashTableRawLookup(mMatches, hash, &key);

        MatchSet* set;

        if (hep && *hep) {
            set = NS_STATIC_CAST(MatchSet*, (*hep)->value);
        }
        else {
            PLHashEntry* he = PL_HashTableRawAdd(mMatches, hep, hash, &key, nsnull);
            if (! he)
                return NS_ERROR_OUT_OF_MEMORY;

            MatchEntry* entry = NS_REINTERPRET_CAST(MatchEntry*, he);

            // Fixup the key in the hashentry to point to the value
            // that the specially-allocated entry contains (rather
            // than the value on the stack). Do the same for its
            // value.
            entry->mHashEntry.key   = &entry->mKey;
            entry->mHashEntry.value = &entry->mMatchSet;

            set = &entry->mMatchSet;
        }

        if (! set->Contains(aRule, aInstantiation)) {
            set->Add(aRule, aInstantiation);
        }
    }


    // Add the match to a table indexed by binding
    {
        MemoryElementSet::ConstIterator last = aInstantiation.mSupport.Last();
        for (MemoryElementSet::ConstIterator element = aInstantiation.mSupport.First(); element != last; ++element) {
            PLHashNumber hash = element->Hash();
            PLHashEntry** hep = PL_HashTableRawLookup(mBindings, hash, &*element);

            MatchSet* set;

            if (hep && *hep) {
                set = NS_STATIC_CAST(MatchSet*, (*hep)->value);
            }
            else {
                PLHashEntry* he = PL_HashTableRawAdd(mBindings, hep, hash, &*element, nsnull);

                BindingEntry* entry = NS_REINTERPRET_CAST(BindingEntry*, he);
                if (! entry)
                    return NS_ERROR_OUT_OF_MEMORY;

                // Fixup the key and value.
                entry->mHashEntry.key   = entry->mElement;
                entry->mHashEntry.value = &entry->mMatchSet;

                set = &entry->mMatchSet;
            }

            if (! set->Contains(aRule, aInstantiation)) {
                set->Add(aRule, aInstantiation);
            }
        }
    }

    return NS_OK;
}


void
ConflictSet::GetMatches(const Key& aKey, const MatchSet** aMatchSet) const
{
    *aMatchSet = NS_STATIC_CAST(MatchSet*, PL_HashTableLookup(mMatches, &aKey));
}


void
ConflictSet::GetMatches(PRInt32 aVariable,
                        const Value& aValue,
                        const MatchSet** aMatchSet) const
{
    Binding binding(aVariable, aValue);
    *aMatchSet = NS_STATIC_CAST(MatchSet*, PL_HashTableLookup(mBindings, &binding));
}


void
ConflictSet::Remove(const MemoryElement& aMemoryElement,
                    MatchSet& aNewMatches,
                    MatchSet& aRetractedMatches)
{
    {
        // First, use the memory-element-to-instantiations map to
        // figure out what instantiations will be affected.
        PLHashEntry** hep = PL_HashTableRawLookup(mBindings, aMemoryElement.Hash(), &aMemoryElement);

        if (!hep || !*hep)
            return;

        // 'set' gets the set of all instantiations containing the first binding.
        MatchSet* set = NS_STATIC_CAST(MatchSet*, (*hep)->value);

        // We'll iterate through these instantiations, only paying
        // attention to instantiations that strictly contain the
        // instantiation we're to remove.
        for (MatchSet::Iterator match = set->First(); match != set->Last(); ++match) {
            // Note the retraction
            aRetractedMatches.Add(*match);
        }

        // We'll eagerly remove it from the table.
        PL_HashTableRawRemove(mBindings, hep, *hep);
    }

    // Now, update the key-to-instantiation map, and see if any new
    // rules have been fired as a result of the retraction.
    MatchSet::ConstIterator lastretraction = aRetractedMatches.Last();
    for (MatchSet::ConstIterator retraction = aRetractedMatches.First(); retraction != lastretraction; ++retraction) {
        Key key(retraction->mInstantiation, retraction->mRule);
        PLHashEntry** hep = PL_HashTableRawLookup(mMatches, key.Hash(), &key);

        // XXXwaterson I'd managed to convince myself that this was really
        // okay, but now I can't remember why.
        //NS_ASSERTION(hep && *hep, "mMatches corrupted");
        if (!hep || !*hep)
            continue;

        MatchSet* set = NS_STATIC_CAST(MatchSet*, (*hep)->value);

        for (MatchSet::Iterator match = set->First(); match != set->Last(); ++match) {
            if (match->mRule == retraction->mRule) {
                set->Erase(match--);

                // See if we've revealed another rule that's applicable
                const MatchSet::Match* newmatch;
                set->FindMatchWithHighestPriority(&newmatch);

                if (newmatch)
                    aNewMatches.Add(*newmatch);

                break;
            }
        }

        if (set->Empty()) {
            PL_HashTableRawRemove(mMatches, hep, *hep);
        }
    }
}


void
ConflictSet::Clear()
{
    Destroy();
    Init();
}

PLHashNumber
ConflictSet::HashMemoryElement(const void* aMemoryElement)
{
    const MemoryElement* element =
        NS_STATIC_CAST(const MemoryElement*, aMemoryElement);

    return element->Hash();
}

PRIntn
ConflictSet::CompareMemoryElements(const void* aLeft, const void* aRight)
{
    const MemoryElement* left =
        NS_STATIC_CAST(const MemoryElement*, aLeft);

    const MemoryElement* right =
        NS_STATIC_CAST(const MemoryElement*, aRight);

    return *left == *right;
}

//----------------------------------------------------------------------

/**
 * A leaf-level node in the rule network. If any instantiations propogate
 * to this node, then we know we've matched a rule.
 */
class InstantiationNode : public ReteNode
{
public:
    InstantiationNode(Rule* aRule, ConflictSet* aConflictSet)
        : mRule(aRule), mConflictSet(aConflictSet) {}

    ~InstantiationNode();
        
    // "downward" propogations
    virtual nsresult Propogate(const InstantiationSet& aInstantiations, void* aClosure);

protected:
    Rule*        mRule;
    ConflictSet* mConflictSet;
};


InstantiationNode::~InstantiationNode()
{
    delete mRule;
}


nsresult
InstantiationNode::Propogate(const InstantiationSet& aInstantiations, void* aClosure)
{
    // If we get here, we've matched the rule associated with this
    // node. Add it to the conflict set, and the set of new <content,
    // member> pairs.
    KeySet* keyset = NS_STATIC_CAST(KeySet*, aClosure);

    InstantiationSet::ConstIterator last = aInstantiations.Last();
    for (InstantiationSet::ConstIterator inst = aInstantiations.First(); inst != last; ++inst) {
        mConflictSet->Add(*inst, mRule);
        keyset->Add(Key(*inst, mRule));
    }
    
    return NS_OK;
}

//----------------------------------------------------------------------

/**
 * An abstract base class for all of the RDF-related tests. This interface
 * allows us to iterate over all of the RDF tests to find the one in the
 * network that is apropos for a newly-added assertion.
 */
class RDFTestNode : public TestNode
{
public:
    RDFTestNode(InnerNode* aParent)
        : TestNode(aParent) {}

    virtual PRBool CanPropogate(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aTarget,
                                Instantiation& aInitialBindings) const = 0;

    virtual void Retract(ConflictSet& aConflictSet,
                         nsIRDFResource* aSource,
                         nsIRDFResource* aProperty,
                         nsIRDFNode* aTarget,
                         MatchSet& aFirings,
                         MatchSet& aRetractions) const = 0;
};


//----------------------------------------------------------------------
//
// RDFGenericBuilderImpl
//

class RDFGenericBuilderImpl : public nsIRDFContentModelBuilder,
                              public nsIRDFObserver
{
public:
    RDFGenericBuilderImpl();
    virtual ~RDFGenericBuilderImpl();

    nsresult Init();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFContentModelBuilder interface
    NS_IMETHOD SetDocument(nsIXULDocument* aDocument);
    NS_IMETHOD SetDataBase(nsIRDFCompositeDataSource* aDataBase);
    NS_IMETHOD GetDataBase(nsIRDFCompositeDataSource** aDataBase);
    NS_IMETHOD CreateRootContent(nsIRDFResource* aResource);
    NS_IMETHOD SetRootContent(nsIContent* aElement);
    NS_IMETHOD CreateContents(nsIContent* aElement);
    NS_IMETHOD OpenContainer(nsIContent* aContainer);
    NS_IMETHOD CloseContainer(nsIContent* aContainer);
    NS_IMETHOD RebuildContainer(nsIContent* aContainer);

    // nsIRDFObserver interface
    NS_IMETHOD OnAssert(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget);

    NS_IMETHOD OnUnassert(nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget);

    NS_IMETHOD OnChange(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aOldTarget,
                        nsIRDFNode* aNewTarget);

    NS_IMETHOD OnMove(nsIRDFResource* aOldSource,
                      nsIRDFResource* aNewSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aTarget);

    // Implementation methods
    nsresult
    InitializeRuleNetwork(InnerNode** aChildNode);

    nsresult
    ComputeContainmentProperties();

    nsresult
    CompileRules();

    nsresult
    CompileRule(nsIContent* aRule, PRInt32 aPriorty, InnerNode* aParentNode);

    PRBool
    IsIgnoreableAttribute(PRInt32 aNameSpaceID, nsIAtom* aAtom);

    nsresult
    SubstituteText(nsIRDFResource* aResource,
                   nsString& aAttributeValue);

    nsresult
    BuildContentFromTemplate(nsIContent *aTemplateNode,
                             nsIContent *aResourceNode,
                             nsIContent *aRealNode,
                             PRBool aIsUnique,
                             nsIRDFResource* aChild,
                             PRBool aNotify,
                             nsIContent** aContainer,
                             PRInt32* aNewIndexInContainer);

    nsresult
    AddPersistentAttributes(nsIContent* aTemplateNode, nsIRDFResource* aResource, nsIContent* aRealNode);

    enum UpdateAction { eSet, eClear };

    nsresult
    SynchronizeAll(nsIRDFResource* aSource,
                   nsIRDFResource* aProperty,
                   nsIRDFNode* aTarget,
                   UpdateAction aAction);

    nsresult
    SynchronizeUsingTemplate(nsIContent *aTemplateNode,
                             nsIContent* aRealNode,
                             UpdateAction aAction,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aValue);

    nsresult
    Propogate(nsIRDFResource* aSource,
              nsIRDFResource* aProperty,
              nsIRDFNode* aTarget,
              KeySet& aNewKeys);

    nsresult
    FireNewlyMatchedRules(const KeySet& aNewKeys);

    nsresult
    Retract(nsIRDFResource* aSource,
            nsIRDFResource* aProperty,
            nsIRDFNode* aTarget);

    nsresult
    RemoveMember(nsIContent* aContainerElement,
                 nsIRDFResource* aMember,
                 PRBool aNotify);

    nsresult
    CreateTemplateAndContainerContents(nsIContent* aElement,
                                       nsIContent** aContainer,
                                       PRInt32* aNewIndexInContainer);

    nsresult
    CreateContainerContents(nsIContent* aElement,
                            nsIRDFResource* aResource,
                            PRBool aNotify,
                            nsIContent** aContainer,
                            PRInt32* aNewIndexInContainer);

    nsresult
    CreateTemplateContents(nsIContent* aElement,
                           const nsString& aTemplateID,
                           nsIContent** aContainer,
                           PRInt32* aNewIndexInContainer);

    nsresult
    EnsureElementHasGenericChild(nsIContent* aParent,
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aTag,
                                 PRBool aNotify,
                                 nsIContent** aResult);

    PRBool
    IsContainmentProperty(nsIContent* aElement, nsIRDFResource* aProperty);

    PRBool
    IsIgnoredProperty(nsIContent* aElement, nsIRDFResource* aProperty);

    PRBool
    IsContainer(nsIContent* aParentElement, nsIRDFResource* aTargetResource);

    PRBool
    IsEmpty(nsIContent* aParentElement, nsIRDFResource* aContainer);

    PRBool
    IsOpen(nsIContent* aElement);

    PRBool
    IsElementInWidget(nsIContent* aElement);
   
    nsresult RemoveGeneratedContent(nsIContent* aElement);

    // XXX. Urg. Hack until layout can batch reflows. See bug 10818.
    PRBool
    IsTreeWidgetItem(nsIContent* aElement);

    static void
    GetElementFactory(PRInt32 aNameSpaceID, nsIElementFactory** aResult);

    nsresult
    AddDatabasePropertyToHTMLElement(nsIContent* aElement, nsIRDFCompositeDataSource* aDataBase);

    nsresult
    GetElementsForResource(nsIRDFResource* aResource, nsISupportsArray* aElements);

    nsresult
    CreateElement(PRInt32 aNameSpaceID,
                  nsIAtom* aTag,
                  nsIContent** aResult);

    nsresult
    SetEmpty(nsIContent *element, PRBool empty);

#ifdef PR_LOGGING
    nsresult
    Log(const char* aOperation,
        nsIRDFResource* aSource,
        nsIRDFResource* aProperty,
        nsIRDFNode* aTarget);

#define LOG(_op, _src, _prop, _targ) \
    Log(_op, _src, _prop, _targ)

#else
#define LOG(_op, _src, _prop, _targ)
#endif

protected:
    nsIXULDocument*            mDocument; // [WEAK]

    // We are an observer of the composite datasource. The cycle is
    // broken by out-of-band SetDataBase(nsnull) call when document is
    // destroyed.
    nsCOMPtr<nsIRDFCompositeDataSource> mDB;
    nsCOMPtr<nsIContent>                mRoot;

    nsCOMPtr<nsIRDFDataSource>		mCache;
    nsCOMPtr<nsITimer>			mTimer;

	nsRDFSortState			sortState;

    // For the rule network
    PropertySet   mContainmentProperties;
    PRBool        mRulesCompiled;
    nsRuleNetwork mRules;
    PRInt32       mContentVar;
    PRInt32       mContainerVar;
    PRInt32       mMemberVar;
    ConflictSet   mConflictSet;
    NodeSet       mRDFTests;

    // pseudo-constants
    static nsrefcnt gRefCnt;
    static nsIRDFService*         gRDFService;
    static nsINameSpaceManager*   gNameSpaceManager;
    static nsIElementFactory*     gHTMLElementFactory;
    static nsIElementFactory*  gXMLElementFactory;
    static nsIXULContentUtils*    gXULUtils;

    static nsIAtom* kContainerAtom;
    static nsIAtom* kContainmentAtom;
    static nsIAtom* kEmptyAtom;
    static nsIAtom* kIdAtom;
    static nsIAtom* kIgnoreAtom;
    static nsIAtom* kInstanceOfAtom;
    static nsIAtom* kIsContainerAtom;
    static nsIAtom* kIsEmptyAtom;
    static nsIAtom* kMenuAtom;
    static nsIAtom* kMenuPopupAtom;
    static nsIAtom* kOpenAtom;
    static nsIAtom* kParentAtom;
    static nsIAtom* kPersistAtom;
    static nsIAtom* kPropertyAtom;
    static nsIAtom* kResourceAtom;
    static nsIAtom* kRuleAtom;
    static nsIAtom* kTemplateAtom;
    static nsIAtom* kTextNodeAtom;
    static nsIAtom* kTreeAtom;
    static nsIAtom* kTreeChildrenAtom;
    static nsIAtom* kTreeItemAtom;
    static nsIAtom* kURIAtom;
    static nsIAtom* kValueAtom;
    static nsIAtom* kXULContentsGeneratedAtom;

    static PRInt32  kNameSpaceID_RDF;
    static PRInt32  kNameSpaceID_XUL;

    static nsIRDFResource* kNC_Title;
    static nsIRDFResource* kNC_child;
    static nsIRDFResource* kNC_Column;
    static nsIRDFResource* kNC_Folder;
    static nsIRDFResource* kRDF_child;
    static nsIRDFResource* kRDF_instanceOf;
    static nsIRDFResource* kXUL_element;

    static nsIXULSortService* gXULSortService;

    static nsString    trueStr;
    static nsString    falseStr;


    class ContentIdTestNode : public TestNode
    {
    public:
        ContentIdTestNode(InnerNode* aParent,
                          nsIXULDocument* aDocument,
                          nsIContent* aRoot,
                          PRInt32 aContentVariable,
                          PRInt32 aIdVariable)
            : TestNode(aParent),
              mDocument(aDocument),
              mRoot(aRoot),
              mContentVariable(aContentVariable),
              mIdVariable(aIdVariable) {}

        virtual nsresult FilterInstantiations(InstantiationSet& aInstantiations) const;

        virtual nsresult GetAncestorVariables(VariableSet& aVariables) const;

        class Element : public MemoryElement {
        public:
            Element(nsIContent* aContent)
                : mContent(aContent) {
                MOZ_COUNT_CTOR(ContentIdTestNode::Element); }

            virtual ~Element() { MOZ_COUNT_DTOR(ContentIdTestNode::Element); }

            virtual const char* Type() const {
                return "RDFGenericBuilderImpl::ContentIdTestNode::Element"; }

            virtual PLHashNumber Hash() const {
                return PLHashNumber(mContent.get()) >> 2; }

            virtual PRBool Equals(const MemoryElement& aElement) const {
                if (aElement.Type() == Type()) {
                    const Element& element = NS_STATIC_CAST(const Element&, aElement);
                    return mContent == element.mContent;
                }
                return PR_FALSE; }

        virtual MemoryElement* Clone() const {
            return new Element(mContent); }

        protected:
            nsCOMPtr<nsIContent> mContent;
        };

    protected:
        nsIXULDocument* mDocument; // [WEAK] because we know the document will outlive us
        nsCOMPtr<nsIContent> mRoot;
        PRInt32 mContentVariable;
        PRInt32 mIdVariable;
    };

    friend class ContentIdTestNode;
};

//----------------------------------------------------------------------

nsrefcnt            RDFGenericBuilderImpl::gRefCnt = 0;
nsIXULSortService*  RDFGenericBuilderImpl::gXULSortService = nsnull;

nsIAtom* RDFGenericBuilderImpl::kContainerAtom;
nsIAtom* RDFGenericBuilderImpl::kContainmentAtom;
nsIAtom* RDFGenericBuilderImpl::kEmptyAtom;
nsIAtom* RDFGenericBuilderImpl::kIdAtom;
nsIAtom* RDFGenericBuilderImpl::kIgnoreAtom;
nsIAtom* RDFGenericBuilderImpl::kInstanceOfAtom;
nsIAtom* RDFGenericBuilderImpl::kIsContainerAtom;
nsIAtom* RDFGenericBuilderImpl::kIsEmptyAtom;
nsIAtom* RDFGenericBuilderImpl::kMenuAtom;
nsIAtom* RDFGenericBuilderImpl::kMenuPopupAtom;
nsIAtom* RDFGenericBuilderImpl::kOpenAtom;
nsIAtom* RDFGenericBuilderImpl::kParentAtom;
nsIAtom* RDFGenericBuilderImpl::kPersistAtom;
nsIAtom* RDFGenericBuilderImpl::kPropertyAtom;
nsIAtom* RDFGenericBuilderImpl::kResourceAtom;
nsIAtom* RDFGenericBuilderImpl::kRuleAtom;
nsIAtom* RDFGenericBuilderImpl::kTemplateAtom;
nsIAtom* RDFGenericBuilderImpl::kTextNodeAtom;
nsIAtom* RDFGenericBuilderImpl::kTreeAtom;
nsIAtom* RDFGenericBuilderImpl::kTreeChildrenAtom;
nsIAtom* RDFGenericBuilderImpl::kTreeItemAtom;
nsIAtom* RDFGenericBuilderImpl::kURIAtom;
nsIAtom* RDFGenericBuilderImpl::kValueAtom;
nsIAtom* RDFGenericBuilderImpl::kXULContentsGeneratedAtom;

nsString RDFGenericBuilderImpl::trueStr;
nsString RDFGenericBuilderImpl::falseStr;

PRInt32  RDFGenericBuilderImpl::kNameSpaceID_RDF;
PRInt32  RDFGenericBuilderImpl::kNameSpaceID_XUL;

nsIRDFService*  RDFGenericBuilderImpl::gRDFService;
nsINameSpaceManager* RDFGenericBuilderImpl::gNameSpaceManager;
nsIElementFactory* RDFGenericBuilderImpl::gHTMLElementFactory;
nsIElementFactory* RDFGenericBuilderImpl::gXMLElementFactory;
nsIXULContentUtils* RDFGenericBuilderImpl::gXULUtils;

nsIRDFResource* RDFGenericBuilderImpl::kNC_Title;
nsIRDFResource* RDFGenericBuilderImpl::kNC_child;
nsIRDFResource* RDFGenericBuilderImpl::kNC_Column;
nsIRDFResource* RDFGenericBuilderImpl::kNC_Folder;
nsIRDFResource* RDFGenericBuilderImpl::kRDF_child;
nsIRDFResource* RDFGenericBuilderImpl::kRDF_instanceOf;
nsIRDFResource* RDFGenericBuilderImpl::kXUL_element;

static nsIRDFContainerUtils*  gRDFContainerUtils;

//----------------------------------------------------------------------

class RDFPropertyTestNode : public RDFTestNode
{
public:
    // Both source and target unbound (?source ^property ?target)
    RDFPropertyTestNode(InnerNode* aParent,
                        nsIRDFDataSource* aDataSource,
                        PRInt32 aSourceVariable,
                        nsIRDFResource* aProperty,
                        PRInt32 aTargetVariable)
        : RDFTestNode(aParent),
          mDataSource(aDataSource),
          mSourceVariable(aSourceVariable),
          mSource(nsnull),
          mProperty(aProperty),
          mTargetVariable(aTargetVariable),
          mTarget(nsnull) {}

    // Source bound, target unbound (source ^property ?target)
    RDFPropertyTestNode(InnerNode* aParent,
                        nsIRDFDataSource* aDataSource,
                        nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        PRInt32 aTargetVariable)
        : RDFTestNode(aParent),
          mDataSource(aDataSource),
          mSourceVariable(0),
          mSource(aSource),
          mProperty(aProperty),
          mTargetVariable(aTargetVariable),
          mTarget(nsnull) {}

    // Source unbound, target bound (?source ^property target)
    RDFPropertyTestNode(InnerNode* aParent,
                        nsIRDFDataSource* aDataSource,
                        PRInt32 aSourceVariable,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget)
        : RDFTestNode(aParent),
          mDataSource(aDataSource),
          mSourceVariable(aSourceVariable),
          mSource(nsnull),
          mProperty(aProperty),
          mTargetVariable(0),
          mTarget(aTarget) {}

    virtual nsresult FilterInstantiations(InstantiationSet& aInstantiations) const;

    virtual nsresult GetAncestorVariables(VariableSet& aVariables) const;

    virtual PRBool
    CanPropogate(nsIRDFResource* aSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aTarget,
                 Instantiation& aInitialBindings) const;

    virtual void
    Retract(ConflictSet& aConflictSet,
            nsIRDFResource* aSource,
            nsIRDFResource* aProperty,
            nsIRDFNode* aTarget,
            MatchSet& aFirings,
            MatchSet& aRetractions) const;


    class Element : public MemoryElement {
    public:
        Element(nsIRDFResource* aSource,
                nsIRDFResource* aProperty,
                nsIRDFNode* aTarget)
            : mSource(aSource),
              mProperty(aProperty),
              mTarget(aTarget) {
            MOZ_COUNT_CTOR(RDFPropertyTestNode::Element); }

        virtual ~Element() { MOZ_COUNT_DTOR(RDFPropertyTestNode::Element); }

        virtual const char* Type() const {
            return "RDFPropertyTestNode::Element"; }

        virtual PLHashNumber Hash() const {
            return PLHashNumber(mSource.get()) ^
                (PLHashNumber(mProperty.get()) >> 4) ^
                (PLHashNumber(mTarget.get()) >> 12); }

        virtual PRBool Equals(const MemoryElement& aElement) const {
            if (aElement.Type() == Type()) {
                const Element& element = NS_STATIC_CAST(const Element&, aElement);
                return mSource == element.mSource
                    && mProperty == element.mProperty
                    && mTarget == element.mTarget;
            }
            return PR_FALSE; }

        virtual MemoryElement* Clone() const {
            return new Element(mSource, mProperty, mTarget); }

    protected:
        nsCOMPtr<nsIRDFResource> mSource;
        nsCOMPtr<nsIRDFResource> mProperty;
        nsCOMPtr<nsIRDFNode> mTarget;
    };

protected:
    nsCOMPtr<nsIRDFDataSource> mDataSource;
    PRInt32                  mSourceVariable;
    nsCOMPtr<nsIRDFResource> mSource;
    nsCOMPtr<nsIRDFResource> mProperty;
    PRInt32                  mTargetVariable;
    nsCOMPtr<nsIRDFNode>     mTarget;
};


nsresult
RDFPropertyTestNode::FilterInstantiations(InstantiationSet& aInstantiations) const
{
    nsresult rv;

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        PRBool hasSourceBinding;
        Value sourceValue;

        if (mSource) {
            hasSourceBinding = PR_TRUE;
            sourceValue = mSource;
        }
        else {
            hasSourceBinding = inst->mBindings.GetBindingFor(mSourceVariable, &sourceValue);
        }

        PRBool hasTargetBinding;
        Value targetValue;

        if (mTarget) {
            hasTargetBinding = PR_TRUE;
            targetValue = mTarget;
        }
        else {
            hasTargetBinding = inst->mBindings.GetBindingFor(mTargetVariable, &targetValue);
        }

        if (hasSourceBinding && hasTargetBinding) {
            // it's a consistency check. see if we have a binding that is consistent
            PRBool hasAssertion;
            rv = mDataSource->HasAssertion(VALUE_TO_IRDFRESOURCE(sourceValue),
                                           mProperty,
                                           VALUE_TO_IRDFNODE(targetValue),
                                           PR_TRUE,
                                           &hasAssertion);
            if (NS_FAILED(rv)) return rv;

            if (hasAssertion) {
                // it's consistent.
                inst->AddSupportingElement(new Element(VALUE_TO_IRDFRESOURCE(sourceValue),
                                                       mProperty,
                                                       VALUE_TO_IRDFNODE(targetValue)));
            }
            else {
                // it's inconsistent. remove it.
                aInstantiations.Erase(inst--);
            }
        }
        else if ((hasSourceBinding && ! hasTargetBinding) ||
                 (! hasSourceBinding && hasTargetBinding)) {
            // it's an open ended query on the source or
            // target. figure out what matches and add as a
            // cross-product.
            nsCOMPtr<nsISimpleEnumerator> results;
            if (hasSourceBinding) {
                rv = mDataSource->GetTargets(VALUE_TO_IRDFRESOURCE(sourceValue),
                                             mProperty,
                                             PR_TRUE,
                                             getter_AddRefs(results));
            }
            else {
                rv = mDataSource->GetSources(mProperty,
                                             VALUE_TO_IRDFNODE(targetValue),
                                             PR_TRUE,
                                             getter_AddRefs(results));
                if (NS_FAILED(rv)) return rv;
            }

            while (1) {
                PRBool hasMore;
                rv = results->HasMoreElements(&hasMore);
                if (NS_FAILED(rv)) return rv;

                if (! hasMore)
                    break;

                nsCOMPtr<nsISupports> isupports;
                rv = results->GetNext(getter_AddRefs(isupports));
                if (NS_FAILED(rv)) return rv;

                PRInt32 variable;
                Value value;

                if (hasSourceBinding) {
                    variable = mTargetVariable;

                    nsCOMPtr<nsIRDFNode> target = do_QueryInterface(isupports);
                    NS_ASSERTION(target != nsnull, "target is not an nsIRDFNode");
                    if (! target) continue;

                    value = target.get();
                }
                else {
                    variable = mSourceVariable;

                    nsCOMPtr<nsIRDFResource> source = do_QueryInterface(isupports);
                    NS_ASSERTION(source != nsnull, "source is not an nsIRDFResource");
                    if (! source) continue;

                    value = source.get();
                }

                // Copy the original instantiation, and add it to the
                // instantiation set with the new binding that we've
                // introduced. Ownership will be transferred to the
                Instantiation newinst = *inst;
                newinst.AddBinding(variable, value);
                newinst.AddSupportingElement(new Element(VALUE_TO_IRDFRESOURCE(sourceValue),
                                                         mProperty,
                                                         VALUE_TO_IRDFRESOURCE(targetValue)));
                aInstantiations.Insert(inst, newinst);
            }

            // finally, remove the "under specified" instantiation.
            aInstantiations.Erase(inst--);
        }
        else {
            // Neither source nor target binding!
            NS_ERROR("can't do open ended queries like that!");
            return NS_ERROR_UNEXPECTED;
        }
    }

    return NS_OK;
}


nsresult
RDFPropertyTestNode::GetAncestorVariables(VariableSet& aVariables) const
{
    nsresult rv;

    if (mSourceVariable) {
        rv = aVariables.Add(mSourceVariable);
        if (NS_FAILED(rv)) return rv;
    }

    if (mTargetVariable) {
        rv = aVariables.Add(mTargetVariable);
        if (NS_FAILED(rv)) return rv;
    }

    return TestNode::GetAncestorVariables(aVariables);
}

PRBool
RDFPropertyTestNode::CanPropogate(nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget,
                                  Instantiation& aInitialBindings) const
{
    if ((mProperty.get() != aProperty) ||
        (mSource && mSource.get() != aSource) ||
        (mTarget && mTarget.get() != aTarget)) {
        return PR_FALSE;
    }

    if (mSourceVariable) {
        aInitialBindings.AddBinding(mSourceVariable, Value(aSource));
    }

    if (mTargetVariable) {
        aInitialBindings.AddBinding(mTargetVariable, Value(aTarget));
    }

    return PR_TRUE;
}

void
RDFPropertyTestNode::Retract(ConflictSet& aConflictSet,
                             nsIRDFResource* aSource,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aTarget,
                             MatchSet& aFirings,
                             MatchSet& aRetractions) const
{
    if (aProperty == mProperty.get()) {
        aConflictSet.Remove(Element(aSource, aProperty, aTarget), aFirings, aRetractions);
    }
}


//----------------------------------------------------------------------

class RDFContainerMemberTestNode : public RDFTestNode
{
public:
    RDFContainerMemberTestNode(InnerNode* aParent,
                               nsIRDFDataSource* aDataSource,
                               const PropertySet& aMembershipProperties,
                               PRInt32 aContainerVariable,
                               PRInt32 aMemberVariable)
        : RDFTestNode(aParent),
          mDataSource(aDataSource),
          mMembershipProperties(aMembershipProperties),
          mContainerVariable(aContainerVariable),
          mMemberVariable(aMemberVariable) {}

    virtual nsresult FilterInstantiations(InstantiationSet& aInstantiations) const;

    virtual nsresult GetAncestorVariables(VariableSet& aVariables) const;

    virtual PRBool
    CanPropogate(nsIRDFResource* aSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aTarget,
                 Instantiation& aInitialBindings) const;

    virtual void
    Retract(ConflictSet& aConflictSet,
            nsIRDFResource* aSource,
            nsIRDFResource* aProperty,
            nsIRDFNode* aTarget,
            MatchSet& aFirings,
            MatchSet& aRetractions) const;

    class Element : public MemoryElement {
    public:
        Element(nsIRDFResource* aContainer,
                nsIRDFNode* aMember)
            : mContainer(aContainer),
              mMember(aMember) {
            MOZ_COUNT_CTOR(RDFContainerMemberTestNode::Element); }

        virtual ~Element() { MOZ_COUNT_DTOR(RDFContainerMemberTestNode::Element); }

        virtual const char* Type() const {
            return "RDFContainerMemberTestNode::Element"; }

        virtual PLHashNumber Hash() const {
            return PLHashNumber(mContainer.get()) ^
                (PLHashNumber(mMember.get()) >> 12); }

        virtual PRBool Equals(const MemoryElement& aElement) const {
            if (aElement.Type() == Type()) {
                const Element& element = NS_STATIC_CAST(const Element&, aElement);
                return mContainer == element.mContainer && mMember == element.mMember;
            }
            return PR_FALSE; }

        virtual MemoryElement* Clone() const {
            return new Element(mContainer, mMember); }

    protected:
        nsCOMPtr<nsIRDFResource> mContainer;
        nsCOMPtr<nsIRDFNode> mMember;
    };

protected:
    nsCOMPtr<nsIRDFDataSource> mDataSource;
    const PropertySet& mMembershipProperties;
    PRInt32 mContainerVariable;
    PRInt32 mMemberVariable;
};

nsresult
RDFContainerMemberTestNode::FilterInstantiations(InstantiationSet& aInstantiations) const
{
    // XXX Uh, factor me, please!
    nsresult rv;

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        PRBool hasContainerBinding;
        Value containerValue;
        hasContainerBinding = inst->mBindings.GetBindingFor(mContainerVariable, &containerValue);

        nsCOMPtr<nsIRDFContainer> rdfcontainer;

        if (hasContainerBinding) {
            // If we have a container binding, then see if the
            // container is an RDF container (bag, seq, alt), and if
            // so, wrap it.
            PRBool isRDFContainer;
            rv = gRDFContainerUtils->IsContainer(mDataSource,
                                                 VALUE_TO_IRDFRESOURCE(containerValue),
                                                 &isRDFContainer);
            if (NS_FAILED(rv)) return rv;

            if (isRDFContainer) {
                rv = NS_NewRDFContainer(getter_AddRefs(rdfcontainer));
                if (NS_FAILED(rv)) return rv;

                rv = rdfcontainer->Init(mDataSource, VALUE_TO_IRDFRESOURCE(containerValue));
                if (NS_FAILED(rv)) return rv;
            }
        }

        PRBool hasMemberBinding;
        Value memberValue;
        hasMemberBinding = inst->mBindings.GetBindingFor(mMemberVariable, &memberValue);

        if (hasContainerBinding && hasMemberBinding) {
            // it's a consistency check. see if we have a binding that is consistent
            PRBool isconsistent = PR_FALSE;

            if (rdfcontainer) {
                // RDF containers are easy. Just use the container API.
                PRInt32 index;
                rv = rdfcontainer->IndexOf(VALUE_TO_IRDFRESOURCE(memberValue), &index);
                if (NS_FAILED(rv)) return rv;

                if (index >= 0)
                    isconsistent = PR_TRUE;
            }

            // XXXwaterson oof. if we *are* an RDF container, why do
            // we still need to grovel through all the containment
            // properties if the thing we're looking for wasn't there?

            if (! isconsistent) {
                // Othewise, we'll need to grovel through the
                // membership properties to see if we have an
                // assertion that indicates membership.
                for (PropertySet::ConstIterator property = mMembershipProperties.First();
                     property != mMembershipProperties.Last();
                     ++property) {
                    PRBool hasAssertion;
                    rv = mDataSource->HasAssertion(VALUE_TO_IRDFRESOURCE(containerValue),
                                                   *property,
                                                   VALUE_TO_IRDFNODE(memberValue),
                                                   PR_TRUE,
                                                   &hasAssertion);
                    if (NS_FAILED(rv)) return rv;

                    if (hasAssertion) {
                        // it's consistent. leave it in the set and we'll
                        // run it up to our parent.
                        isconsistent = PR_TRUE;
                        break;
                    }
                }
            }

            if (isconsistent) {
                // Add a memory element to our set-of-support.
                inst->AddSupportingElement(new Element(VALUE_TO_IRDFRESOURCE(containerValue),
                                                       VALUE_TO_IRDFNODE(memberValue)));
            }
            else {
                // it's inconsistent. remove it.
                aInstantiations.Erase(inst--);
            }

            // We're done, go on to the next instantiation
            continue;
        }

        if (hasContainerBinding && rdfcontainer) {
            // We've got a container binding, and the container is
            // bound to an RDF container. Add each member as a new
            // instantiation.
            nsCOMPtr<nsISimpleEnumerator> elements;
            rv = rdfcontainer->GetElements(getter_AddRefs(elements));
            if (NS_FAILED(rv)) return rv;

            while (1) {
                PRBool hasmore;
                rv = elements->HasMoreElements(&hasmore);
                if (NS_FAILED(rv)) return rv;

                if (! hasmore)
                    break;

                nsCOMPtr<nsISupports> isupports;
                rv = elements->GetNext(getter_AddRefs(isupports));
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsIRDFNode> node = do_QueryInterface(isupports);
                if (! node)
                    return NS_ERROR_UNEXPECTED;

                Instantiation newinst = *inst;
                newinst.AddBinding(mMemberVariable, Value(node.get()));
                newinst.AddSupportingElement(new Element(VALUE_TO_IRDFRESOURCE(containerValue),
                                                         node));
                aInstantiations.Insert(inst, newinst);
            }
        }

        if (hasMemberBinding) {
            // Oh, this is so nasty. If we have a member binding, then
            // grovel through each one of our inbound arcs to see if
            // any of them are ordinal properties (like an RDF
            // container might have). If so, walk it backwards to get
            // the container we're in.
            nsCOMPtr<nsISimpleEnumerator> arcsin;
            rv = mDataSource->ArcLabelsIn(VALUE_TO_IRDFNODE(memberValue), getter_AddRefs(arcsin));
            if (NS_FAILED(rv)) return rv;

            while (1) {
                nsCOMPtr<nsIRDFResource> property;

                {
                    PRBool hasmore;
                    rv = arcsin->HasMoreElements(&hasmore);
                    if (NS_FAILED(rv)) return rv;

                    if (! hasmore)
                        break;

                    nsCOMPtr<nsISupports> isupports;
                    rv = arcsin->GetNext(getter_AddRefs(isupports));
                    if (NS_FAILED(rv)) return rv;

                    property = do_QueryInterface(isupports);
                    if (! property)
                        return NS_ERROR_UNEXPECTED;
                }

                // Ordinal properties automagically indicate container
                // membership as far as we're concerned. Note that
                // we're *only* concerned with ordinal properties
                // here: the next block will worry about the other
                // membership properties.
                PRBool isordinal;
                rv = gRDFContainerUtils->IsOrdinalProperty(property, &isordinal);
                if (NS_FAILED(rv)) return rv;

                if (isordinal) {
                    // If we get here, we've found a property that
                    // indicates container membership leading *into* a
                    // member node. Find all the people that point to
                    // it, and call them containers.
                    nsCOMPtr<nsISimpleEnumerator> sources;
                    rv = mDataSource->GetSources(property, VALUE_TO_IRDFNODE(memberValue), PR_TRUE,
                                                 getter_AddRefs(sources));
                    if (NS_FAILED(rv)) return rv;

                    while (1) {
                        PRBool hasmore;
                        rv = sources->HasMoreElements(&hasmore);
                        if (NS_FAILED(rv)) return rv;

                        if (! hasmore)
                            break;

                        nsCOMPtr<nsISupports> isupports;
                        rv = sources->GetNext(getter_AddRefs(isupports));
                        if (NS_FAILED(rv)) return rv;

                        nsCOMPtr<nsIRDFResource> source = do_QueryInterface(isupports);
                        if (! source)
                            return NS_ERROR_UNEXPECTED;

                        // Add a new instantiation
                        Instantiation newinst = *inst;
                        newinst.AddBinding(mContainerVariable, Value(source.get()));
                        newinst.AddSupportingElement(new Element(source, VALUE_TO_IRDFNODE(memberValue)));
                        aInstantiations.Insert(inst, newinst);
                    }
                }
            }
        }

        if ((hasContainerBinding && ! hasMemberBinding) ||
            (! hasContainerBinding && hasMemberBinding)) {
            // it's an open ended query on the container or member. go
            // through our containment properties to see if anything
            // applies.
            for (PropertySet::ConstIterator property = mMembershipProperties.First();
                 property != mMembershipProperties.Last();
                 ++property) {
                nsCOMPtr<nsISimpleEnumerator> results;
                if (hasContainerBinding) {
                    rv = mDataSource->GetTargets(VALUE_TO_IRDFRESOURCE(containerValue), *property, PR_TRUE,
                                                 getter_AddRefs(results));
                }
                else {
                    rv = mDataSource->GetSources(*property, VALUE_TO_IRDFNODE(memberValue), PR_TRUE,
                                                 getter_AddRefs(results));
                }
                if (NS_FAILED(rv)) return rv;

                while (1) {
                    PRBool hasmore;
                    rv = results->HasMoreElements(&hasmore);
                    if (NS_FAILED(rv)) return rv;

                    if (! hasmore)
                        break;

                    nsCOMPtr<nsISupports> isupports;
                    rv = results->GetNext(getter_AddRefs(isupports));
                    if (NS_FAILED(rv)) return rv;

                    PRInt32 variable;
                    Value value;

                    if (hasContainerBinding) {
                        variable = mMemberVariable;

                        nsCOMPtr<nsIRDFNode> member = do_QueryInterface(isupports);
                        NS_ASSERTION(member != nsnull, "member is not an nsIRDFNode");
                        if (! member) continue;

                        value = member.get();
                    }
                    else {
                        variable = mContainerVariable;

                        nsCOMPtr<nsIRDFResource> container = do_QueryInterface(isupports);
                        NS_ASSERTION(container != nsnull, "container is not an nsIRDFResource");
                        if (! container) continue;

                        value = container.get();
                    }

                    // Copy the original instantiation, and add it to the
                    // instantiation set with the new binding that we've
                    // introduced. Ownership will be transferred to the
                    Instantiation newinst = *inst;
                    newinst.AddBinding(variable, value);
                    if (hasContainerBinding) {
                        newinst.AddSupportingElement(new Element(VALUE_TO_IRDFRESOURCE(containerValue),
                                                                 VALUE_TO_IRDFNODE(value)));
                    }
                    else {
                        newinst.AddSupportingElement(new Element(VALUE_TO_IRDFRESOURCE(value),
                                                                 VALUE_TO_IRDFNODE(memberValue)));
                    }
                    aInstantiations.Insert(inst, newinst);
                }
            }
        }

        if (! hasContainerBinding && ! hasMemberBinding) {
            // Neither container nor member binding!
            NS_ERROR("can't do open ended queries like that!");
            return NS_ERROR_UNEXPECTED;
        }

        // finally, remove the "under specified" instantiation.
        aInstantiations.Erase(inst--);
    }

    return NS_OK;
}

nsresult
RDFContainerMemberTestNode::GetAncestorVariables(VariableSet& aVariables) const
{
    nsresult rv;

    rv = aVariables.Add(mContainerVariable);
    if (NS_FAILED(rv)) return rv;

    rv = aVariables.Add(mMemberVariable);
    if (NS_FAILED(rv)) return rv;

    return TestNode::GetAncestorVariables(aVariables);
}


PRBool
RDFContainerMemberTestNode::CanPropogate(nsIRDFResource* aSource,
                                         nsIRDFResource* aProperty,
                                         nsIRDFNode* aTarget,
                                         Instantiation& aInitialBindings) const
{
    nsresult rv;

    PRBool canpropogate = PR_FALSE;

    // We can certainly propogate ordinal properties
    rv = gRDFContainerUtils->IsOrdinalProperty(aProperty, &canpropogate);
    if (NS_FAILED(rv)) return PR_FALSE;

    if (! canpropogate) {
        canpropogate = mMembershipProperties.Contains(aProperty);
    }

    if (canpropogate) {
        aInitialBindings.AddBinding(mContainerVariable, Value(aSource));
        aInitialBindings.AddBinding(mMemberVariable, Value(aTarget));
        return PR_TRUE;
    }

    return PR_FALSE;
}

void
RDFContainerMemberTestNode::Retract(ConflictSet& aConflictSet,
                                    nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget,
                                    MatchSet& aFirings,
                                    MatchSet& aRetractions) const
{
    PRBool canretract = PR_FALSE;

    // We can certainly retract ordinal properties
    nsresult rv;
    rv = gRDFContainerUtils->IsOrdinalProperty(aProperty, &canretract);
    if (NS_FAILED(rv)) return;

    if (! canretract) {
        canretract = mMembershipProperties.Contains(aProperty);
    }

    if (canretract) {
        aConflictSet.Remove(Element(aSource, aTarget), aFirings, aRetractions);
    }
}

//----------------------------------------------------------------------

class RDFContainerInstanceTestNode : public RDFTestNode
{
public:
    enum Test { eFalse, eTrue, eDontCare };

    RDFContainerInstanceTestNode(InnerNode* aParent,
                                 nsIRDFDataSource* aDataSource,
                                 const PropertySet& aMembershipProperties,
                                 PRInt32 aContainerVariable,
                                 Test aContainer,
                                 Test aEmpty)
        : RDFTestNode(aParent),
          mDataSource(aDataSource),
          mMembershipProperties(aMembershipProperties),
          mContainerVariable(aContainerVariable),
          mContainer(aContainer),
          mEmpty(aEmpty) {}

    virtual nsresult FilterInstantiations(InstantiationSet& aInstantiations) const;

    virtual nsresult GetAncestorVariables(VariableSet& aVariables) const;

    virtual PRBool
    CanPropogate(nsIRDFResource* aSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aTarget,
                 Instantiation& aInitialBindings) const;

    virtual void
    Retract(ConflictSet& aConflictSet,
            nsIRDFResource* aSource,
            nsIRDFResource* aProperty,
            nsIRDFNode* aTarget,
            MatchSet& aFirings,
            MatchSet& aRetractions) const;


    class Element : public MemoryElement {
    public:
        Element(nsIRDFResource* aContainer,
                Test aContainerTest,
                Test aEmptyTest)
            : mContainer(aContainer),
              mContainerTest(aContainerTest),
              mEmptyTest(aEmptyTest) {
            MOZ_COUNT_CTOR(RDFContainerInstanceTestNode::Element); }

        virtual ~Element() { MOZ_COUNT_DTOR(RDFContainerInstanceTestNode::Element); }

        virtual const char* Type() const {
            return "RDFContainerInstanceTestNode::Element"; }

        virtual PLHashNumber Hash() const {
            return (PLHashNumber(mContainer.get()) >> 4) ^
                PLHashNumber(mContainerTest) ^
                (PLHashNumber(mEmptyTest) << 4); }

        virtual PRBool Equals(const MemoryElement& aElement) const {
            if (aElement.Type() == Type()) {
                const Element& element = NS_STATIC_CAST(const Element&, aElement);
                return mContainer == element.mContainer
                    && mContainerTest == element.mContainerTest
                    && mEmptyTest == element.mEmptyTest;
            }
            return PR_FALSE; }

        virtual MemoryElement* Clone() const {
            return new Element(mContainer, mContainerTest, mEmptyTest); }

    protected:
        nsCOMPtr<nsIRDFResource> mContainer;
        Test mContainerTest;
        Test mEmptyTest;
    };

protected:
    nsCOMPtr<nsIRDFDataSource> mDataSource;
    const PropertySet& mMembershipProperties;
    PRInt32 mContainerVariable;
    Test mContainer;
    Test mEmpty;
};

nsresult
RDFContainerInstanceTestNode::FilterInstantiations(InstantiationSet& aInstantiations) const
{
    nsresult rv;

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        Value value;
        if (! inst->mBindings.GetBindingFor(mContainerVariable, &value)) {
            NS_ERROR("can't do unbounded container testing");
            return NS_ERROR_UNEXPECTED;
        }

        nsCOMPtr<nsIRDFContainer> rdfcontainer;

        PRBool isRDFContainer;
        rv = gRDFContainerUtils->IsContainer(mDataSource, VALUE_TO_IRDFRESOURCE(value), &isRDFContainer);
        if (NS_FAILED(rv)) return rv;

        // If they've asked us to test for emptiness, do that first
        // because it doesn't require us to create any enumerators.
        if (mEmpty != eDontCare) {
            Test empty;

            if (isRDFContainer) {
                // It's an RDF container. Use the container utilities
                // to deduce what's in it.
                rv = NS_NewRDFContainer(getter_AddRefs(rdfcontainer));
                if (NS_FAILED(rv)) return rv;

                rv = rdfcontainer->Init(mDataSource, VALUE_TO_IRDFRESOURCE(value));
                if (NS_FAILED(rv)) return rv;

                PRInt32 count;
                rv = rdfcontainer->GetCount(&count);
                if (NS_FAILED(rv)) return rv;

                empty = (count == 0) ? eTrue : eFalse;
            }
            else {
                empty = eTrue;

                for (PropertySet::ConstIterator property = mMembershipProperties.First();
                     property != mMembershipProperties.Last();
                     ++property) {
                    nsCOMPtr<nsIRDFNode> target;
                    rv = mDataSource->GetTarget(VALUE_TO_IRDFRESOURCE(value), *property, PR_TRUE, getter_AddRefs(target));
                    if (NS_FAILED(rv)) return rv;

                    if (target != nsnull) {
                        // bingo. we found one.
                        empty = eFalse;
                        break;
                    }
                }
            }

            if (empty == mEmpty) {
                inst->AddSupportingElement(new Element(VALUE_TO_IRDFRESOURCE(value),
                                                       mContainer, mEmpty));
            }
            else {
                aInstantiations.Erase(inst--);
            }
        }
        else if (mContainer != eDontCare) {
            // We didn't care about emptiness, only containerhood.
            Test container;

            if (isRDFContainer) {
                // Wow, this is easy.
                container = eTrue;
            }
            else {
                // Okay, suckage. We need to look at all of the arcs
                // leading out of the thing, and see if any of them
                // are properties that are deemed as denoting
                // containerhood.
                container = eFalse;

                nsCOMPtr<nsISimpleEnumerator> arcsout;
                rv = mDataSource->ArcLabelsOut(VALUE_TO_IRDFRESOURCE(value), getter_AddRefs(arcsout));
                if (NS_FAILED(rv)) return rv;

                while (1) {
                    PRBool hasmore;
                    rv = arcsout->HasMoreElements(&hasmore);
                    if (NS_FAILED(rv)) return rv;

                    if (! hasmore)
                        break;

                    nsCOMPtr<nsISupports> isupports;
                    rv = arcsout->GetNext(getter_AddRefs(isupports));
                    if (NS_FAILED(rv)) return rv;

                    nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);
                    NS_ASSERTION(property != nsnull, "not a property");
                    if (! property)
                        return NS_ERROR_UNEXPECTED;

                    if (mMembershipProperties.Contains(property)) {
                        container = eTrue;
                        break;
                    }
                }
            }

            if (container == mContainer) {
                inst->AddSupportingElement(new Element(VALUE_TO_IRDFRESOURCE(value),
                                                       mContainer, mEmpty));
            }
            else {
                aInstantiations.Erase(inst--);
            }
        }
    }

    return NS_OK;
}

nsresult
RDFContainerInstanceTestNode::GetAncestorVariables(VariableSet& aVariables) const
{
    nsresult rv;

    rv = aVariables.Add(mContainerVariable);
    if (NS_FAILED(rv)) return rv;

    return TestNode::GetAncestorVariables(aVariables);
}

PRBool
RDFContainerInstanceTestNode::CanPropogate(nsIRDFResource* aSource,
                                           nsIRDFResource* aProperty,
                                           nsIRDFNode* aTarget,
                                           Instantiation& aInitialBindings) const
{
    nsresult rv;

    PRBool canpropogate = PR_FALSE;

    // We can certainly propogate ordinal properties
    rv = gRDFContainerUtils->IsOrdinalProperty(aProperty, &canpropogate);
    if (NS_FAILED(rv)) return PR_FALSE;

    if (! canpropogate) {
        canpropogate = mMembershipProperties.Contains(aProperty);
    }

    if (canpropogate) {
        aInitialBindings.AddBinding(mContainerVariable, Value(aSource));
        return PR_TRUE;
    }

    return PR_FALSE;
}

void
RDFContainerInstanceTestNode::Retract(ConflictSet& aConflictSet,
                                      nsIRDFResource* aSource,
                                      nsIRDFResource* aProperty,
                                      nsIRDFNode* aTarget,
                                      MatchSet& aFirings,
                                      MatchSet& aRetractions) const
{
    // XXXwaterson oof. complicated. figure this out.
    if (0) {
        aConflictSet.Remove(Element(aSource, mContainer, mEmpty), aFirings, aRetractions);
    }
}

//----------------------------------------------------------------------

class ContentTagTestNode : public TestNode
{
public:
    ContentTagTestNode(InnerNode* aParent,
                       PRInt32 aContentVariable,
                       nsIAtom* aTag)
        : TestNode(aParent),
          mContentVariable(aContentVariable),
          mTag(aTag) {}

    virtual nsresult FilterInstantiations(InstantiationSet& aInstantiations) const;

    virtual nsresult GetAncestorVariables(VariableSet& aVariables) const;

protected:
    PRInt32 mContentVariable;
    nsCOMPtr<nsIAtom> mTag;
};

nsresult
ContentTagTestNode::FilterInstantiations(InstantiationSet& aInstantiations) const
{
    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        Value value;
        if (! inst->mBindings.GetBindingFor(mContentVariable, &value)) {
            NS_ERROR("cannot handle open-ended tag name query");
            return NS_ERROR_UNEXPECTED;
        }

        nsCOMPtr<nsIAtom> tag;
        rv = VALUE_TO_ICONTENT(value)->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        if (tag != mTag) {
            aInstantiations.Erase(inst--);
        }
    }
    
    return NS_OK;
}

nsresult
ContentTagTestNode::GetAncestorVariables(VariableSet& aVariables) const
{
    return NS_OK;
}


//----------------------------------------------------------------------


nsresult
NS_NewXULTemplateBuilder(nsIRDFContentModelBuilder** aResult)
{
    nsresult rv;
    RDFGenericBuilderImpl* result = new RDFGenericBuilderImpl();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result); // stabilize

    rv = result->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = result->QueryInterface(NS_GET_IID(nsIRDFContentModelBuilder), (void**) aResult);
    }

    NS_RELEASE(result);
    return rv;
}


RDFGenericBuilderImpl::RDFGenericBuilderImpl(void)
    : mDocument(nsnull),
      mDB(nsnull),
      mRoot(nsnull),
      mTimer(nsnull),
      mRulesCompiled(PR_FALSE)
{
    NS_INIT_REFCNT();
}

RDFGenericBuilderImpl::~RDFGenericBuilderImpl(void)
{
    // NS_IF_RELEASE(mDocument) not refcounted

    --gRefCnt;
    if (gRefCnt == 0) {
        NS_IF_RELEASE(kContainerAtom);
        NS_IF_RELEASE(kContainmentAtom);
        NS_IF_RELEASE(kEmptyAtom);
        NS_IF_RELEASE(kIdAtom);
        NS_IF_RELEASE(kIgnoreAtom);
        NS_IF_RELEASE(kInstanceOfAtom);
        NS_IF_RELEASE(kIsContainerAtom);
        NS_IF_RELEASE(kIsEmptyAtom);
        NS_IF_RELEASE(kMenuAtom);
        NS_IF_RELEASE(kMenuPopupAtom);
        NS_IF_RELEASE(kOpenAtom);
        NS_IF_RELEASE(kParentAtom);
        NS_IF_RELEASE(kPersistAtom);
        NS_IF_RELEASE(kPropertyAtom);
        NS_IF_RELEASE(kResourceAtom);
        NS_IF_RELEASE(kRuleAtom);
        NS_IF_RELEASE(kTemplateAtom);
        NS_IF_RELEASE(kTextNodeAtom);
        NS_IF_RELEASE(kTreeAtom);
        NS_IF_RELEASE(kTreeChildrenAtom);
        NS_IF_RELEASE(kTreeItemAtom);
        NS_IF_RELEASE(kURIAtom);
        NS_IF_RELEASE(kValueAtom);
        NS_IF_RELEASE(kXULContentsGeneratedAtom);

        NS_IF_RELEASE(kNC_Title);
        NS_IF_RELEASE(kNC_child);
        NS_IF_RELEASE(kNC_Column);
        NS_IF_RELEASE(kNC_Folder);
        NS_IF_RELEASE(kRDF_child);
        NS_IF_RELEASE(kRDF_instanceOf);
        NS_IF_RELEASE(kXUL_element);

        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }

        if (gRDFContainerUtils) {
            nsServiceManager::ReleaseService(kRDFContainerUtilsCID, gRDFContainerUtils);
            gRDFContainerUtils = nsnull;
        }

        if (gXULSortService) {
            nsServiceManager::ReleaseService(kXULSortServiceCID, gXULSortService);
            gXULSortService = nsnull;
        }

        NS_RELEASE(gNameSpaceManager);
        NS_IF_RELEASE(gHTMLElementFactory);

        if (gXULUtils) {
            nsServiceManager::ReleaseService(kXULContentUtilsCID, gXULUtils);
            gXULUtils = nsnull;
        }
    }
}


nsresult
RDFGenericBuilderImpl::Init()
{
    if (gRefCnt++ == 0) {
        kContainerAtom                  = NS_NewAtom("container");
        kContainmentAtom                = NS_NewAtom("containment");
        kEmptyAtom                      = NS_NewAtom("empty");
        kIdAtom                         = NS_NewAtom("id");
        kIgnoreAtom                     = NS_NewAtom("ignore");
        kInstanceOfAtom                 = NS_NewAtom("instanceOf");
        kIsContainerAtom                = NS_NewAtom("iscontainer");
        kIsEmptyAtom                    = NS_NewAtom("isempty");
        kMenuAtom                       = NS_NewAtom("menu");
        kMenuPopupAtom                  = NS_NewAtom("menupopup");
        kOpenAtom                       = NS_NewAtom("open");
        kParentAtom                     = NS_NewAtom("parent");
        kPersistAtom                    = NS_NewAtom("persist");
        kPropertyAtom                   = NS_NewAtom("property");
        kResourceAtom                   = NS_NewAtom("resource");
        kRuleAtom                       = NS_NewAtom("rule");
        kTemplateAtom                   = NS_NewAtom("template");
        kTextNodeAtom                   = NS_NewAtom("textnode");
        kTreeAtom                       = NS_NewAtom("tree");
        kTreeChildrenAtom               = NS_NewAtom("treechildren");
        kTreeItemAtom                   = NS_NewAtom("treeitem");
        kURIAtom                        = NS_NewAtom("uri");
        kValueAtom                      = NS_NewAtom("value");
        kXULContentsGeneratedAtom       = NS_NewAtom("xulcontentsgenerated");

    trueStr = "true";
    falseStr = "false";

        nsresult rv;

        // Register the XUL and RDF namespaces: these'll just retrieve
        // the IDs if they've already been registered by someone else.
        rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                nsnull,
                                                NS_GET_IID(nsINameSpaceManager),
                                                (void**) &gNameSpaceManager);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create namespace manager");
        if (NS_FAILED(rv)) return rv;

        // XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
        static const char kXULNameSpaceURI[]
            = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

        static const char kRDFNameSpaceURI[]
            = RDF_NAMESPACE_URI;

        rv = gNameSpaceManager->RegisterNameSpace(kXULNameSpaceURI, kNameSpaceID_XUL);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XUL namespace");
        if (NS_FAILED(rv)) return rv;

        rv = gNameSpaceManager->RegisterNameSpace(kRDFNameSpaceURI, kNameSpaceID_RDF);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register RDF namespace");
        if (NS_FAILED(rv)) return rv;


        // Initialize the global shared reference to the service
        // manager and get some shared resource objects.
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          NS_GET_IID(nsIRDFService),
                                          (nsISupports**) &gRDFService);
        if (NS_FAILED(rv)) return rv;

        gRDFService->GetResource(NC_NAMESPACE_URI "Title",   &kNC_Title);
        gRDFService->GetResource(NC_NAMESPACE_URI "child",   &kNC_child);
        gRDFService->GetResource(NC_NAMESPACE_URI "Column",  &kNC_Column);
        gRDFService->GetResource(NC_NAMESPACE_URI "Folder",  &kNC_Folder);
        gRDFService->GetResource(RDF_NAMESPACE_URI "child",  &kRDF_child);
        gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_instanceOf);
        gRDFService->GetResource(XUL_NAMESPACE_URI "element",    &kXUL_element);

        rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
                                          NS_GET_IID(nsIRDFContainerUtils),
                                          (nsISupports**) &gRDFContainerUtils);
        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(kXULSortServiceCID,
                                          NS_GET_IID(nsIXULSortService),
                                          (nsISupports**) &gXULSortService);
        if (NS_FAILED(rv)) return rv;

        rv = nsComponentManager::CreateInstance(kHTMLElementFactoryCID,
                                                nsnull,
                                                NS_GET_IID(nsIElementFactory),
                                                (void**) &gHTMLElementFactory);
        if (NS_FAILED(rv)) return rv;

        rv = nsComponentManager::CreateInstance(kXMLElementFactoryCID,
                                                nsnull,
                                                NS_GET_IID(nsIElementFactory),
                                                (void**) &gXMLElementFactory);
        if (NS_FAILED(rv)) return rv;

        rv = nsServiceManager::GetService(kXULContentUtilsCID,
                                          NS_GET_IID(nsIXULContentUtils),
                                          (nsISupports**) &gXULUtils);
        if (NS_FAILED(rv)) return rv;
    }

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsRDFGenericBuilder");
#endif

    return NS_OK;
}

NS_IMPL_ISUPPORTS2(RDFGenericBuilderImpl, nsIRDFContentModelBuilder, nsIRDFObserver);

//----------------------------------------------------------------------
//
// nsIRDFContentModelBuilder methods
//

NS_IMETHODIMP
RDFGenericBuilderImpl::SetDocument(nsIXULDocument* aDocument)
{
    // note: null now allowed, it indicates document going away

    mDocument = aDocument; // not refcounted
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::SetDataBase(nsIRDFCompositeDataSource* aDataBase)
{
    NS_PRECONDITION(mRoot != nsnull, "not initialized");
    if (! mRoot)
        return NS_ERROR_NOT_INITIALIZED;

    
    if (mDB)
        mDB->RemoveObserver(this);

    mDB = dont_QueryInterface(aDataBase);

    if (mDB) {
        nsresult rv;
        mDB->AddObserver(this);

        // Now set the database on the element, so that script writers can
        // access it.
        nsCOMPtr<nsIDOMXULElement> element( do_QueryInterface(mRoot) );
        if (element) {
            rv = element->SetDatabase(aDataBase);
            if (NS_FAILED(rv)) return rv;
        }
        else {
            // Hmm. This must be an HTML element. Try to set it as a
            // JS property "by hand".
            rv = AddDatabasePropertyToHTMLElement(mRoot, mDB);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::GetDataBase(nsIRDFCompositeDataSource** aDataBase)
{
    NS_PRECONDITION(aDataBase != nsnull, "null ptr");
    if (! aDataBase)
        return NS_ERROR_NULL_POINTER;

    *aDataBase = mDB;
    NS_ADDREF(*aDataBase);
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::CreateRootContent(nsIRDFResource* aResource)
{
    // XXX Remove this method from the interface
    NS_NOTREACHED("whoops");
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::SetRootContent(nsIContent* aElement)
{
    mRoot = dont_QueryInterface(aElement);
    
    if (mCache)
    {
    	// flush (delete) the cache when re-rerooting the generated content
    	mCache = nsnull;
    }
    
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::CreateContents(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    // First, make sure that the element is in the right widget -- ours.
    nsresult rv = NS_OK;
    if (IsElementInWidget(aElement)) {
        rv = CreateTemplateAndContainerContents(aElement, nsnull /* don't care */, nsnull /* don't care */);
    }

    return rv;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::OpenContainer(nsIContent* aElement)
{
    nsresult rv;

    // First, make sure that the element is in the right widget -- ours.
    if (!IsElementInWidget(aElement))
        return NS_OK;

    nsCOMPtr<nsIRDFResource> resource;
    rv = gXULUtils->GetElementRefResource(aElement, getter_AddRefs(resource));

    // If it has no resource, there's nothing that we need to be
    // concerned about here.
    if (NS_FAILED(rv))
        return NS_OK;

    // The element has a resource; that means that it corresponds
    // to something in the graph, so we need to go to the graph to
    // create its contents.
    //
    // Create the container's contents "quietly" (i.e., |aNotify ==
    // PR_FALSE|), and then use the |container| and |newIndex| to
    // notify layout where content got created.
    nsCOMPtr<nsIContent> container;
    PRInt32 newIndex;
    rv = CreateContainerContents(aElement, resource, PR_FALSE, getter_AddRefs(container), &newIndex);
    if (NS_FAILED(rv)) return rv;

    if (container && IsTreeWidgetItem(aElement)) {
        // The tree widget is special, and has to be spanked every
        // time we add content to a container.
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
        if (! doc)
            return NS_ERROR_UNEXPECTED;

        rv = doc->ContentAppended(container, newIndex);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::CloseContainer(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    // First, make sure that the element is in the right widget -- ours.
    if (!IsElementInWidget(aElement))
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return rv;

    if (tag.get() == kTreeItemAtom) {
        // Find the tag that contains the children so that we can
        // remove all of the children. This is a -total- hack, that is
        // necessary for the tree control because...I'm not sure. But
        // it's necessary. Maybe we should fix the tree control to
        // reflow itself when the 'open' attribute changes on a
        // treeitem.
        //
        // OTOH, we could treat this as a (premature?) optimization so
        // that nodes which are not being displayed don't hang around
        // taking up space. Unfortunately, the tree widget currently
        // _relies_ on this behavior and will break if we don't do it
        // :-(.

        // Find the <treechildren> beneath the <treeitem>...
        nsCOMPtr<nsIContent> insertionpoint;
        rv = gXULUtils->FindChildByTag(aElement, kNameSpaceID_XUL, kTreeChildrenAtom, getter_AddRefs(insertionpoint));
        if (NS_FAILED(rv)) return rv;

        if (insertionpoint) {
            // ...and blow away all the generated content.
            PRInt32 count;
            rv = insertionpoint->ChildCount(count);
            if (NS_FAILED(rv)) return rv;

            rv = RemoveGeneratedContent(insertionpoint);
            if (NS_FAILED(rv)) return rv;
        }

        // Force the XUL element to remember that it needs to re-generate
        // its kids next time around.
        nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
        NS_ASSERTION(xulcontent != nsnull, "not an nsIXULContent");
        if (! xulcontent)
            return NS_ERROR_UNEXPECTED;

        rv = xulcontent->SetLazyState(nsIXULContent::eChildrenMustBeRebuilt);
        if (NS_FAILED(rv)) return rv;

        // Clear the contents-generated attribute so that the next time we
        // come back, we'll regenerate the kids we just killed.
        rv = xulcontent->ClearLazyState(nsIXULContent::eContainerContentsBuilt);
        if (NS_FAILED(rv)) return rv;

        // Remove any instantiations involving this element from the
        // conflict set.
        MatchSet firings, retractions;
        mConflictSet.Remove(ContentIdTestNode::Element(aElement), firings, retractions);
    }

    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::RebuildContainer(nsIContent* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    // First, make sure that the element is in the right widget -- ours.
    if (!IsElementInWidget(aElement))
        return NS_OK;

    nsresult rv;

    // Next, see if it's a XUL element whose contents have never even
    // been generated. If so, short-circuit and bail; there's nothing
    // for us to "rebuild" yet. They'll get built correctly the next
    // time somebody asks for them. 
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);

    if (xulcontent) {
        PRBool containerContentsBuilt;
        rv = xulcontent->GetLazyState(nsIXULContent::eContainerContentsBuilt, containerContentsBuilt);
        if (NS_FAILED(rv)) return rv;

        if (! containerContentsBuilt)
            return NS_OK;
    }

    // If we get here, then we've tried to generate content for this
    // element. Remove it.
    rv = RemoveGeneratedContent(aElement);
    if (NS_FAILED(rv)) return rv;

    // Remove any instantiations involving this element from the
    // conflict set.
    MatchSet firings, retractions;
    mConflictSet.Remove(ContentIdTestNode::Element(aElement), firings, retractions);

    // Forces the XUL element to remember that it needs to
    // re-generate its children next time around.
    if (xulcontent) {
        rv = xulcontent->SetLazyState(nsIXULContent::eChildrenMustBeRebuilt);
        if (NS_FAILED(rv)) return rv;

        rv = xulcontent->ClearLazyState(nsIXULContent::eTemplateContentsBuilt);
        if (NS_FAILED(rv)) return rv;

        rv = xulcontent->ClearLazyState(nsIXULContent::eContainerContentsBuilt);
        if (NS_FAILED(rv)) return rv;
    }

    // Now, regenerate both the template- and container-generated
    // contents for the current element...
    nsCOMPtr<nsIContent> container;
    PRInt32 newIndex;
    rv = CreateTemplateAndContainerContents(aElement, getter_AddRefs(container), &newIndex);
    if (NS_FAILED(rv)) return rv;

    if (container) {
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
        if (! doc)
            return NS_ERROR_UNEXPECTED;

        rv = doc->ContentAppended(container, newIndex);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}



//----------------------------------------------------------------------
//
// nsIRDFObserver interface
//

nsresult
RDFGenericBuilderImpl::Propogate(nsIRDFResource* aSource,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aTarget,
                                 KeySet& aNewKeys)
{
    // Find the "dominating" tests that could be used to propogate the
    // assertion we've just received. (Test A "dominates" test B if A
    // is an ancestor of B in the rule network).
    nsresult rv;

    // First, we'll go through and find all of the test nodes that can
    // propogate the assertion.
    NodeSet livenodes;

    {
        NodeSet::Iterator last = mRDFTests.Last();
        for (NodeSet::Iterator i = mRDFTests.First(); i != last; ++i) {
            RDFTestNode* rdftestnode = NS_STATIC_CAST(RDFTestNode*, *i);

            Instantiation seed;
            if (rdftestnode->CanPropogate(aSource, aProperty, aTarget, seed)) {
                livenodes.Add(rdftestnode);
            }
        }
    }

    // Now, we'll go through each, and any that aren't dominated by
    // another live node will be used to propogate the assertion
    // through the rule network
    {
        NodeSet::Iterator last = livenodes.Last();
        for (NodeSet::Iterator i = livenodes.First(); i != last; ++i) {
            RDFTestNode* rdftestnode = NS_STATIC_CAST(RDFTestNode*, *i);

            PRBool isdominated = PR_FALSE;

            for (NodeSet::ConstIterator j = livenodes.First(); j != last; ++j) {
                // we can't be dominated by ourself
                if (j == i)
                    continue;

                if (rdftestnode->HasAncestor(*j)) {
                    isdominated = PR_TRUE;
                    break;
                }
            }

            if (! isdominated) {
                // Bogus, to get the seed instantiation
                Instantiation seed;
                rdftestnode->CanPropogate(aSource, aProperty, aTarget, seed);

                InstantiationSet instantiations;
                instantiations.Append(seed);

                rv = rdftestnode->Constrain(instantiations);
                if (NS_FAILED(rv)) return rv;

                if (! instantiations.Empty()) {
                    rv = rdftestnode->Propogate(instantiations, &aNewKeys);
                    if (NS_FAILED(rv)) return rv;
                }
            }
        }
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::FireNewlyMatchedRules(const KeySet& aNewKeys)
{
    // Iterate through newly added keys to determine which rules fired.
    //
    // XXXwaterson Unfortunately, this could also lead to retractions;
    // e.g., (contaner ?a ^empty false) could become "unmatched". How
    // to track those?
    nsresult rv;

    KeySet::ConstIterator last = aNewKeys.Last();
    for (KeySet::ConstIterator key = aNewKeys.First(); key != last; ++key) {
        const MatchSet* matches;
        mConflictSet.GetMatches(*key, &matches);

        NS_ASSERTION(matches != nsnull, "no matched rules for new key");
        if (! matches)
            continue;

        const MatchSet::Match* bestmatch;
        rv = matches->FindMatchWithHighestPriority(&bestmatch);
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(bestmatch != nsnull, "no matches in match set");
        if (! bestmatch)
            continue;

        // XXXwaterson fix me!
        const MatchSet::Match* lastmatch = nsnull;
        if (bestmatch != lastmatch) {
            Value value;
            nsIContent* content;
            PRBool hasbinding;

            if (lastmatch) do {
                // we had content built by a different rule that's now
                // been superceded. Remove the old content.
                hasbinding = lastmatch->mInstantiation.mBindings.GetBindingFor(mContentVar, &value);
                NS_ASSERTION(hasbinding, "no content binding");
                if (! hasbinding) break;

                content = VALUE_TO_ICONTENT(value);

                nsCOMPtr<nsIContent> parent;
                rv = content->GetParent(*getter_AddRefs(parent));
                if (NS_FAILED(rv)) return rv;

                NS_ASSERTION(parent != nsnull, "no parent");
                if (! parent) break;

                PRInt32 pos;
                rv = parent->IndexOf(content, pos);
                if (NS_FAILED(rv)) return rv;

                NS_ASSERTION(pos >= 0, "parent doesn't think this child has an index");
                if (pos < 0) break;

                rv = parent->RemoveChildAt(pos, PR_TRUE);
                if (NS_FAILED(rv)) return rv;
            } while (0);

            // Get the content node to which we were bound
            hasbinding = bestmatch->mInstantiation.mBindings.GetBindingFor(mContentVar, &value);

            NS_ASSERTION(hasbinding, "no content binding");
            if (! hasbinding)
                continue;

            content = VALUE_TO_ICONTENT(value);

            nsCOMPtr<nsIContent> tmpl;
            bestmatch->mRule->GetContent(getter_AddRefs(tmpl));

            rv = BuildContentFromTemplate(tmpl, content, content, PR_TRUE,
                                          VALUE_TO_IRDFRESOURCE(key->mMemberValue),
                                          PR_TRUE, nsnull, nsnull);

            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::OnAssert(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aTarget)
{
    // Just silently fail, because this can happen "normally" as part
    // of tear-down code. (Bug 9098)
    if (! mDocument)
        return NS_OK;

    nsresult rv;

	if (mCache)
        {
            mCache->Assert(aSource, aProperty, aTarget, PR_TRUE /* XXX should be value passed in */);
        }

    LOG("onassert", aSource, aProperty, aTarget);

    KeySet newkeys;
    rv = Propogate(aSource, aProperty, aTarget, newkeys);
    if (NS_FAILED(rv)) return rv;

    rv = FireNewlyMatchedRules(newkeys);
    if (NS_FAILED(rv)) return rv;

    rv = SynchronizeAll(aSource, aProperty, aTarget, eSet);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::Retract(nsIRDFResource* aSource,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aTarget)
{
    // Retract any currently active rules that will no longer be
    // matched.
    NodeSet::ConstIterator lastnode = mRDFTests.Last();
    for (NodeSet::ConstIterator node = mRDFTests.First(); node != lastnode; ++node) {
        const RDFTestNode* rdftestnode = NS_STATIC_CAST(const RDFTestNode*, *node);

        MatchSet firings;
        MatchSet retractions;
        rdftestnode->Retract(mConflictSet, aSource, aProperty, aTarget, firings, retractions);

        {
            MatchSet::ConstIterator last = retractions.Last();
            for (MatchSet::ConstIterator match = retractions.First(); match != last; ++match) {
                Value member;
                match->mInstantiation.mBindings.GetBindingFor(match->mRule->GetMemberVariable(), &member);

                Value content;
                match->mInstantiation.mBindings.GetBindingFor(mContentVar, &content);

                RemoveMember(VALUE_TO_ICONTENT(content),
                             VALUE_TO_IRDFRESOURCE(member),
                             PR_TRUE);
            }
        }

        // Now fire any newly revealed rules
        {
            MatchSet::ConstIterator last = firings.Last();
            for (MatchSet::ConstIterator match = firings.First(); match != last; ++match) {
                // XXXwaterson yo. write me.
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFGenericBuilderImpl::OnUnassert(nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget)
{
    // Just silently fail, because this can happen "normally" as part
    // of tear-down code. (Bug 9098)
    if (! mDocument)
        return NS_OK;

    nsresult rv;

	if (mCache)
	{
		mCache->Unassert(aSource, aProperty, aTarget);
	}

    LOG("onunassert", aSource, aProperty, aTarget);

    rv = Retract(aSource, aProperty, aTarget);
    if (NS_FAILED(rv)) return rv;

    rv = SynchronizeAll(aSource, aProperty, aTarget, eClear);
    if (NS_FAILED(rv)) return rv;
    
    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::OnChange(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aOldTarget,
                                nsIRDFNode* aNewTarget)
{
    // Just silently fail, because this can happen "normally" as part
    // of tear-down code. (Bug 9098)
    if (! mDocument)
        return NS_OK;

	if (mCache)
	{
		if (aOldTarget)
		{
			// XXX fix this: in-memory DS doesn't like a null oldTarget
			mCache->Change(aSource, aProperty, aOldTarget, aNewTarget);
		}
		else
		{
			// XXX should get tv via observer interface
			mCache->Assert(aSource, aProperty, aNewTarget, PR_TRUE);
		}
	}

    nsresult rv;


    rv = SynchronizeAll(aSource, aProperty, aNewTarget, eSet);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
RDFGenericBuilderImpl::OnMove(nsIRDFResource* aOldSource,
                              nsIRDFResource* aNewSource,
                              nsIRDFResource* aProperty,
                              nsIRDFNode* aTarget)
{
    NS_NOTYETIMPLEMENTED("write me");

	if (mCache)
	{
		mCache->Move(aOldSource, aNewSource, aProperty, aTarget);
	}

    return NS_ERROR_NOT_IMPLEMENTED;
}


//----------------------------------------------------------------------
//
// Implementation methods
//

PRBool
RDFGenericBuilderImpl::IsIgnoreableAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute)
{
    // XXX Note that we patently ignore namespaces. This is because
    // HTML elements lie and tell us that their attributes are
    // _always_ in the HTML namespace. Urgh.

    // never copy the ID attribute
    if (aAttribute == kIdAtom) {
        return PR_TRUE;
    }
    // never copy {}:uri attribute
    else if (aAttribute == kURIAtom) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }
}


nsresult
RDFGenericBuilderImpl::SubstituteText(nsIRDFResource* aResource,
                                      nsString& aAttributeValue)
{
    nsresult rv;

    if (aAttributeValue.Equals("...") || aAttributeValue.Equals("rdf:*")) {
        const char *uri = nsnull;
        aResource->GetValueConst(&uri);
        aAttributeValue = uri;
    }
    else if (aAttributeValue.Find("rdf:") == 0) {
        // found an attribute which wants to bind its value to RDF so
        // look it up in the graph
        aAttributeValue.Cut(0,4);

        nsCOMPtr<nsIRDFResource> property;
        rv = gRDFService->GetUnicodeResource(aAttributeValue.GetUnicode(), getter_AddRefs(property));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFNode> valueNode;
        rv = mDB->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(valueNode));
        if (NS_FAILED(rv)) return rv;

        if ((rv != NS_RDF_NO_VALUE) && (valueNode)) {
            rv = gXULUtils->GetTextForNode(valueNode, aAttributeValue);
            if (NS_FAILED(rv)) return rv;
        }
        else {
            aAttributeValue.Truncate();
        }
    }
    else {
        // Nothing to do!
    }

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::BuildContentFromTemplate(nsIContent *aTemplateNode,
                                                nsIContent *aResourceNode,
                                                nsIContent *aRealNode,
                                                PRBool aIsUnique,
                                                nsIRDFResource* aChild,
                                                PRBool aNotify,
                                                nsIContent** aContainer,
                                                PRInt32* aNewIndexInContainer)
{
    // This is the mother lode. Here is where we grovel through an
    // element in the template, copying children from the template
    // into the "real" content tree, performing substitution as we go
    // by looking stuff up in the RDF graph.
    //
    //   |aTemplateNode| is the element in the "template tree", whose
    //   children we will duplicate and move into the "real" content
    //   tree.
    //
    //   |aResourceNode| is the element in the "real" content tree that
    //   has the "id" attribute set to an RDF resource's URI. This is
    //   not directly used here, but rather passed down to the XUL
    //   sort service to perform container-level sort.
    //
    //   |aRealNode| is the element in the "real" content tree to which
    //   the new elements will be copied.
    //
    //   |aIsUnique| is set to "true" so long as content has been
    //   "unique" (or "above" the resource element) so far in the
    //   template.
    //
    //   |aChild| is the RDF resource at the end of a property link for
    //   which we are building content.
    //
    //   |aNotify| is set to "true" if content should be constructed
    //   "noisily"; that is, whether the document observers should be
    //   notified when new content is added to the content model.
    //
    //   |aContainer| is an out parameter that will be set to the first
    //   container element in the "real" content tree to which content
    //   was appended.
    //
    //   |aNewIndexInContainer| is an out parameter that will be set to
    //   the index in aContainer at which new content is first
    //   constructed.
    //
    // If |aNotify| is "false", then |aContainer| and
    // |aNewIndexInContainer| are used to determine where in the
    // content model new content is constructed. This allows a single
    // notification to be propogated to document observers.
    //

    nsresult rv;

#ifdef PR_LOGGING
    // Dump out the template node's tag, the template ID, and the RDF
    // resource that is being used as the index into the graph.
    if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        nsCOMPtr<nsIAtom> tag;
        rv = aTemplateNode->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

        nsXPIDLCString resourceCStr;
        rv = aChild->GetValue(getter_Copies(resourceCStr));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tagstr;
        tag->ToString(tagstr);

        nsAutoString templatestr;
        aTemplateNode->GetAttribute(kNameSpaceID_None, kIdAtom, templatestr);

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("rdfgeneric[%p] build-content-from-template %s (template='%s') [%s]",
                this,
                (const char*) nsCAutoString(tagstr),
                (const char*) nsCAutoString(templatestr),
                (const char*) resourceCStr));
    }
#endif

    // Iterate through all of the template children, constructing
    // "real" content model nodes for each "template" child.
    PRInt32    count;
    rv = aTemplateNode->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 kid = 0; kid < count; kid++) {
        nsCOMPtr<nsIContent> tmplKid;
        rv = aTemplateNode->ChildAt(kid, *getter_AddRefs(tmplKid));
        if (NS_FAILED(rv)) return rv;

        PRInt32 nameSpaceID;
        rv = tmplKid->GetNameSpaceID(nameSpaceID);
        if (NS_FAILED(rv)) return rv;

        // Check whether this element is the "resource" element. The
        // "resource" element is the element that is cookie-cutter
        // copied once for each different RDF resource specified by
        // |aChild|.
        //
        // Nodes that appear -above- the resource element
        // (that is, are ancestors of the resource element in the
        // content model) are unique across all values of |aChild|,
        // and are created only once.
        //
        // Nodes that appear -below- the resource element (that is,
        // are descnendants of the resource element in the conte
        // model), are cookie-cutter copied for each distinct value of
        // |aChild|.
        //
        // For example, in a <tree> template:
        //
        //   <tree>
        //     <template>
        //       <treechildren> [1]
        //         <treeitem uri="rdf:*"> [2]
        //           <treerow> [3]
        //             <treecell value="rdf:urn:foo" /> [4]
        //             <treecell value="rdf:urn:bar" /> [5]
        //           </treerow>
        //         </treeitem>
        //       </treechildren>
        //     </template>
        //   </tree>
        //
        // The <treeitem> element [2] is the "resource element". This
        // element, and all of its descendants ([3], [4], and [5])
        // will be duplicated for each different |aChild|
        // resource. It's ancestor <treechildren> [1] is unique, and
        // will only be created -once-, no matter how many <treeitem>s
        // are created below it.
        //
        // Note that |isResourceElement| and |isUnique| are mutually
        // exclusive.
        PRBool isResourceElement = PR_FALSE;
        PRBool isUnique = aIsUnique;

        {
            // We identify the resource element by presence of a
            // "uri='rdf:*'" attribute. (We also support the older
            // "uri='...'" syntax.)
            PRUnichar buf[128];
            nsAutoString idValue(CBufDescriptor(buf, PR_TRUE, sizeof(buf) / sizeof(PRUnichar), 0));
            rv = tmplKid->GetAttribute(kNameSpaceID_None,
                                       kURIAtom,
                                       idValue);
            if (NS_FAILED(rv)) return rv;

            if ((rv == NS_CONTENT_ATTR_HAS_VALUE) &&
                (idValue.Equals("...") || idValue.Equals("rdf:*"))) {
                // If we -are- the resource element, then we are no
                // matter unique.
                isResourceElement = PR_TRUE;
                isUnique = PR_FALSE;
            }
        }

        nsCOMPtr<nsIAtom> tag;
        rv = tmplKid->GetTag(*getter_AddRefs(tag));
        if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
            nsAutoString tagname;
            tag->ToString(tagname);
            PR_LOG(gLog, PR_LOG_DEBUG,
                   ("rdfgeneric[%p]     building %s %s %s",
                    this, (const char*) nsCAutoString(tagname),
                    (isResourceElement ? "[resource]" : ""),
                    (isUnique ? "[unique]" : "")));
        }
#endif

        // Set to PR_TRUE if the child we're trying to create now
        // already existed in the content model.
        PRBool realKidAlreadyExisted = PR_FALSE;

        nsCOMPtr<nsIContent> realKid;
        if (isUnique) {
            // The content is "unique"; that is, we haven't descended
            // far enough into the tempalte to hit the "resource"
            // element yet. |EnsureElementHasGenericChild()| will
            // conditionally create the element iff it isn't there
            // already.
            rv = EnsureElementHasGenericChild(aRealNode, nameSpaceID, tag, aNotify, getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_RDF_ELEMENT_WAS_THERE) {
                realKidAlreadyExisted = PR_TRUE;
            }
            else {
                // Mark the element's contents as being generated so
                // that any re-entrant calls don't trigger an infinite
                // recursion.
                nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(realKid);
                if (xulcontent) {
                    rv = xulcontent->SetLazyState(nsIXULContent::eTemplateContentsBuilt);
                    if (NS_FAILED(rv)) return rv;
                }

                // Potentially remember the index of this element as the first
                // element that we've generated. Note that we remember
                // this -before- we recurse!
                if (aContainer && !*aContainer) {
                    *aContainer = aRealNode;
                    NS_ADDREF(*aContainer);

					PRInt32 indx;
                    aRealNode->ChildCount(indx);

					// Since EnsureElementHasGenericChild() added us, make
					// sure to subtract one for our real index.
					*aNewIndexInContainer = indx - 1;
                }
            }

            // Recurse until we get to the resource element. Since
            // -we're- unique, assume that our child will be
            // unique. The check for the "resource" element at the top
            // of the function will trip this to |false| as soon as we
            // encounter it.
            rv = BuildContentFromTemplate(tmplKid, aResourceNode, realKid, PR_TRUE, aChild, aNotify, aContainer, aNewIndexInContainer);
            if (NS_FAILED(rv)) return rv;
        }
        else if (isResourceElement) {
            // It's the "resource" element. Create a new element using
            // the namespace ID and tag from the template element.
            rv = CreateElement(nameSpaceID, tag, getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;

            // Assign the element an 'id' attribute using the URI of
            // the |aChild| resource.
            const char *uri;
            rv = aChild->GetValueConst(&uri);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource URI");
            if (NS_FAILED(rv)) return rv;

            nsAutoString id(uri);
            rv = realKid->SetAttribute(kNameSpaceID_None, kIdAtom, id, PR_FALSE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set id attribute");
            if (NS_FAILED(rv)) return rv;

            if (! aNotify) {
                // XUL document will watch us, and take care of making
                // sure that we get added to or removed from the
                // element map if aNotify is true. If not, we gotta do
                // it ourselves. Yay.
                rv = mDocument->AddElementForID(id, realKid);
                if (NS_FAILED(rv)) return rv;
            }

            // XXX Hackery to ensure that mailnews works. Force the
            // element to hold a reference to the
            // resource. Unfortunately, this'll break for HTML
            // elements.
            {
                nsCOMPtr<nsIXULContent> xulele = do_QueryInterface(realKid);
                if (xulele) {
                    xulele->ForceElementToOwnResource(PR_TRUE);
                }
            }

            // Set up the element's 'container' and 'empty'
            // attributes.
            if (IsContainer(tmplKid, aChild)) {
                rv = realKid->SetAttribute(kNameSpaceID_None, kContainerAtom, trueStr, PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                // test to see if the container has contents
                nsAutoString isEmpty = IsEmpty(tmplKid, aChild) ? trueStr : falseStr;
                rv = realKid->SetAttribute(kNameSpaceID_None, kEmptyAtom, isEmpty, PR_FALSE);
                if (NS_FAILED(rv)) return rv;
            }
        }
        else if ((tag.get() == kTextNodeAtom) && (nameSpaceID == kNameSpaceID_XUL)) {
            // <xul:text value="..."> is replaced by text of the
            // actual value of the 'rdf:resource' attribute for the
            // given node.
            PRUnichar attrbuf[128];
            nsAutoString attrValue(CBufDescriptor(attrbuf, PR_TRUE, sizeof(attrbuf) / sizeof(PRUnichar), 0));
            rv = tmplKid->GetAttribute(kNameSpaceID_None, kValueAtom, attrValue);
            if (NS_FAILED(rv)) return rv;

            if ((rv == NS_CONTENT_ATTR_HAS_VALUE) && (attrValue.Length() > 0)) {
                rv = SubstituteText(aChild, attrValue);
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsITextContent> content;
                rv = nsComponentManager::CreateInstance(kTextNodeCID,
                                                        nsnull,
                                                        NS_GET_IID(nsITextContent),
                                                        getter_AddRefs(content));
                if (NS_FAILED(rv)) return rv;

                rv = content->SetText(attrValue.GetUnicode(), attrValue.Length(), PR_FALSE);
                if (NS_FAILED(rv)) return rv;

                rv = aRealNode->AppendChildTo(nsCOMPtr<nsIContent>( do_QueryInterface(content) ), aNotify);
                if (NS_FAILED(rv)) return rv;

                // XXX Don't bother remembering text nodes as the
                // first element we've generated?
            }
        }
        else {
            // It's just a generic element. Create it!
            rv = CreateElement(nameSpaceID, tag, getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;
        }

        if (realKid && !realKidAlreadyExisted) {
            // Potentially remember the index of this element as the
            // first element that we've generated.
            if (aContainer && !*aContainer) {
                *aContainer = aRealNode;
                NS_ADDREF(*aContainer);

                PRInt32 indx;
                aRealNode->ChildCount(indx);

                // Since we haven't inserted any content yet, our new
                // index in the container will be the current count of
                // elements in the container.
                *aNewIndexInContainer = indx;
            }

            // Mark the node with the ID of the template node used to
            // create this element. This allows us to sync back up
            // with the template to incrementally build content.
            nsAutoString templateID;
            rv = tmplKid->GetAttribute(kNameSpaceID_None, kIdAtom, templateID);
            if (NS_FAILED(rv)) return rv;

            rv = realKid->SetAttribute(kNameSpaceID_None, kTemplateAtom, templateID, PR_FALSE);
            if (NS_FAILED(rv)) return rv;

            // Copy all attributes from the template to the new
            // element.
            PRInt32    numAttribs;
            rv = tmplKid->GetAttributeCount(numAttribs);
            if (NS_FAILED(rv)) return rv;

            for (PRInt32 attr = 0; attr < numAttribs; attr++) {
                PRInt32 attribNameSpaceID;
                nsCOMPtr<nsIAtom> attribName;

                rv = tmplKid->GetAttributeNameAt(attr, attribNameSpaceID, *getter_AddRefs(attribName));
                if (NS_FAILED(rv)) return rv;

                if (! IsIgnoreableAttribute(attribNameSpaceID, attribName)) {
                    // Create a buffer here, because there's a good
                    // chance that an attribute in the template is
                    // going to be an RDF URI, which is usually
                    // longish.
                    PRUnichar attrbuf[128];
                    nsAutoString attribValue(CBufDescriptor(attrbuf, PR_TRUE, sizeof(attrbuf) / sizeof(PRUnichar), 0));
                    rv = tmplKid->GetAttribute(attribNameSpaceID, attribName, attribValue);
                    if (NS_FAILED(rv)) return rv;

                    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                        rv = SubstituteText(aChild, attribValue);
                        if (NS_FAILED(rv)) return rv;

                        rv = realKid->SetAttribute(attribNameSpaceID, attribName, attribValue, PR_FALSE);
                        if (NS_FAILED(rv)) return rv;
                    }
                }
            }

            // Add any persistent attributes
            if (isResourceElement) {
                rv = AddPersistentAttributes(tmplKid, aChild, realKid);
                if (NS_FAILED(rv)) return rv;
            }

            
            if (nameSpaceID == kNameSpaceID_HTML) {
                // If we just built HTML, then we have to recurse "by
                // hand" because HTML won't build itself up
                // lazily. Note that we _don't_ need to notify: we'll
                // add the entire subtree in a single whack.
                //
                // Note that we don't bother passing aContainer and
                // aNewIndexInContainer down: since we're HTML, we
                // -know- that we -must- have just been created.
                rv = BuildContentFromTemplate(tmplKid, aResourceNode, realKid, isUnique,
                                              aChild, PR_FALSE,
                                              nsnull /* don't care */,
                                              nsnull /* don't care */);

                if (NS_FAILED(rv)) return rv;

                if (isResourceElement) {
                    rv = CreateContainerContents(realKid, aChild, PR_FALSE,
                                                 nsnull /* don't care */,
                                                 nsnull /* don't care */);
                    if (NS_FAILED(rv)) return rv;
                }
            }
            else {
                // Otherwise, just mark the XUL element as requiring
                // more work to be done. We'll get around to it when
                // somebody asks for it.
                nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(realKid);
                if (! xulcontent)
                    return NS_ERROR_UNEXPECTED;

                rv = xulcontent->SetLazyState(nsIXULContent::eChildrenMustBeRebuilt);
                if (NS_FAILED(rv)) return rv;
            }

            // We'll _already_ have added the unique elements; but if
            // it's -not- unique, then use the XUL sort service now to
            // append the element to the content model.
            if (! isUnique) {
                rv = NS_ERROR_UNEXPECTED;

                if (gXULSortService && isResourceElement) {
                    rv = gXULSortService->InsertContainerNode(mDB, &sortState,
                                                              mRoot, aResourceNode,
                                                              aRealNode, realKid,
                                                              aNotify);
                }

                if (NS_FAILED(rv)) {
                    rv = aRealNode->AppendChildTo(realKid, aNotify);
                    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to insert element");
                }
            }
        }
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::AddPersistentAttributes(nsIContent* aTemplateNode, nsIRDFResource* aResource, nsIContent* aRealNode)
{
    nsresult rv;

    nsAutoString persist;
    rv = aTemplateNode->GetAttribute(kNameSpaceID_None, kPersistAtom, persist);
    if (NS_FAILED(rv)) return rv;

    if (rv != NS_CONTENT_ATTR_HAS_VALUE)
        return NS_OK;

    nsAutoString attribute;
    while (persist.Length() > 0) {
        attribute.Truncate();

        PRInt32 offset = persist.FindCharInSet(" ,");
        if (offset > 0) {
            persist.Left(attribute, offset);
            persist.Cut(0, offset + 1);
        }
        else {
            attribute = persist;
            persist.Truncate();
        }

        attribute.Trim(" ");

        if (attribute.Length() == 0)
            break;

        PRInt32 nameSpaceID;
        nsCOMPtr<nsIAtom> tag;
        rv = aTemplateNode->ParseAttributeString(attribute, *getter_AddRefs(tag), nameSpaceID);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFResource> property;
        rv = gXULUtils->GetResource(nameSpaceID, tag, getter_AddRefs(property));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFNode> target;
        rv = mDB->GetTarget(aResource, property, PR_TRUE, getter_AddRefs(target));
        if (NS_FAILED(rv)) return rv;

        if (! target)
            continue;

        nsCOMPtr<nsIRDFLiteral> value = do_QueryInterface(target);
        NS_ASSERTION(value != nsnull, "unable to stomach that sort of node");
        if (! value)
            continue;

        const PRUnichar* valueStr;
        rv = value->GetValueConst(&valueStr);
        if (NS_FAILED(rv)) return rv;

        rv = aRealNode->SetAttribute(nameSpaceID, tag, nsAutoString(valueStr), PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::SynchronizeAll(nsIRDFResource* aSource,
                                      nsIRDFResource* aProperty,
                                      nsIRDFNode* aTarget,
                                      UpdateAction aAction)
{
    // Find all the elements in the content model that correspond to
    // aSource: for each, we'll try to build XUL children if
    // appropriate.
    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create new ISupportsArray");
    if (NS_FAILED(rv)) return rv;

    rv = GetElementsForResource(aSource, elements);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to retrieve elements from resource");
    if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = elements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
        nsISupports* isupports = elements->ElementAt(i);
        nsCOMPtr<nsIContent> element( do_QueryInterface(isupports) );
        NS_IF_RELEASE(isupports);

        // XXX somehow figure out if building XUL kids on this
        // particular element makes any sense whatsoever.

        // We'll start by making sure that the element at least has
        // the same parent has the content model builder's root
        if (!IsElementInWidget(element))
            continue;

        nsAutoString templateID;
        rv = element->GetAttribute(kNameSpaceID_None,
                                   kTemplateAtom,
                                   templateID);
        if (NS_FAILED(rv)) return rv;

        if (rv != NS_CONTENT_ATTR_HAS_VALUE)
            continue;

        nsCOMPtr<nsIDOMXULDocument>    xulDoc;
        xulDoc = do_QueryInterface(mDocument);
        if (! xulDoc)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIDOMElement>    domElement;
        rv = xulDoc->GetElementById(templateID, getter_AddRefs(domElement));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find template node");
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIContent> templateNode = do_QueryInterface(domElement);
        if (! templateNode)
            return NS_ERROR_UNEXPECTED;

        // this node was created by a XUL template, so update it accordingly
        rv = SynchronizeUsingTemplate(templateNode, element, aAction, aProperty, aTarget);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::SynchronizeUsingTemplate(nsIContent* aTemplateNode,
                                                nsIContent* aRealElement,
                                                UpdateAction aAction,
                                                nsIRDFResource* aProperty,
                                                nsIRDFNode* aValue)
{
    nsresult rv;

    // check all attributes on the template node; if they reference a resource,
    // update the equivalent attribute on the content node

    PRInt32    numAttribs;
    rv = aTemplateNode->GetAttributeCount(numAttribs);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        for (PRInt32 aLoop=0; aLoop<numAttribs; aLoop++) {
            PRInt32    attribNameSpaceID;
            nsCOMPtr<nsIAtom> attribName;
            rv = aTemplateNode->GetAttributeNameAt(aLoop,
                                                   attribNameSpaceID,
                                                   *getter_AddRefs(attribName));
            if (NS_FAILED(rv)) return rv;

            nsAutoString attribValue;
            rv = aTemplateNode->GetAttribute(attribNameSpaceID,
                                             attribName,
                                             attribValue);
            if (NS_FAILED(rv)) return rv;

            if (rv != NS_CONTENT_ATTR_HAS_VALUE)
                continue;

            if (attribValue.Find("rdf:") != 0)
                continue;

            // found an attribute which wants to bind its value
            // to RDF so look it up in the graph
            attribValue.Cut(0,4);

            nsCOMPtr<nsIRDFResource> property;
            rv = gRDFService->GetUnicodeResource(attribValue.GetUnicode(),
                                                 getter_AddRefs(property));
            if (NS_FAILED(rv)) return rv;

            if (property.get() == aProperty) {
                nsAutoString text("");

                rv = gXULUtils->GetTextForNode(aValue, text);
                if (NS_FAILED(rv)) return rv;

                if ((text.Length() > 0) && (aAction == eSet)) {
                    aRealElement->SetAttribute(attribNameSpaceID,
                                                   attribName,
                                                   text,
                                                   PR_TRUE);
                }
                else {
                    aRealElement->UnsetAttribute(attribNameSpaceID,
                                                     attribName,
                                                     PR_TRUE);
                }
            }
        }
    }

    // See if we've generated kids for this node yet. If we have, then
    // recursively sync up template kids with content kids
    PRBool contentsGenerated = PR_TRUE;
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aRealElement);
    if (xulcontent) {
        rv = xulcontent->GetLazyState(nsIXULContent::eTemplateContentsBuilt,
                                      contentsGenerated);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // HTML content will _always_ have been generated up-front
    }

    if (contentsGenerated) {
        PRInt32 count;
        rv = aTemplateNode->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 loop=0; loop<count; loop++) {
            nsCOMPtr<nsIContent> tmplKid;
            rv = aTemplateNode->ChildAt(loop, *getter_AddRefs(tmplKid));
            if (NS_FAILED(rv)) return rv;

            if (! tmplKid)
                break;

            nsCOMPtr<nsIContent> realKid;
            rv = aRealElement->ChildAt(loop, *getter_AddRefs(realKid));
            if (NS_FAILED(rv)) return rv;

            if (! realKid)
                break;

            rv = SynchronizeUsingTemplate(tmplKid, realKid, aAction, aProperty, aValue);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}



nsresult
RDFGenericBuilderImpl::RemoveMember(nsIContent* aContainerElement,
                                    nsIRDFResource* aMember,
                                    PRBool aNotify)
{
    // This works as follows. It finds all of the elements in the
    // document that correspond to aMember. Any that are contained
    // within aContainerElement are removed from their direct parent.
    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    rv = GetElementsForResource(aMember, elements);
    if (NS_FAILED(rv)) return rv;

    PRUint32 cnt;
    rv = elements->Count(&cnt);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(cnt) - 1; i >= 0; --i) {
        nsISupports* isupports = elements->ElementAt(i);
        nsCOMPtr<nsIContent> child( do_QueryInterface(isupports) );
        NS_IF_RELEASE(isupports);

        if (! gXULUtils->IsContainedBy(child, aContainerElement))
            continue;

        nsCOMPtr<nsIContent> parent;
        rv = child->GetParent(*getter_AddRefs(parent));
        if (NS_FAILED(rv)) return rv;

        PRInt32 pos;
        rv = parent->IndexOf(child, pos);
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(pos >= 0, "parent doesn't think this child has an index");
        if (pos < 0) continue;

        rv = parent->RemoveChildAt(pos, PR_TRUE);
        if (NS_FAILED(rv)) return rv;

        // Set its document to null so that it'll get knocked out of
        // the XUL doc's resource-to-element map.
        rv = child->SetDocument(nsnull, PR_TRUE);
        if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
        if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
            nsCOMPtr<nsIAtom> parentTag;
            rv = parent->GetTag(*getter_AddRefs(parentTag));
            if (NS_FAILED(rv)) return rv;

            nsAutoString parentTagStr;
            rv = parentTag->ToString(parentTagStr);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIAtom> childTag;
            rv = child->GetTag(*getter_AddRefs(childTag));
            if (NS_FAILED(rv)) return rv;

            nsAutoString childTagStr;
            rv = childTag->ToString(childTagStr);
            if (NS_FAILED(rv)) return rv;

            const char* resourceCStr;
            rv = aMember->GetValueConst(&resourceCStr);
            if (NS_FAILED(rv)) return rv;
            
            PR_LOG(gLog, PR_LOG_ALWAYS,
                   ("rdfgeneric[%p] remove-member %s->%s [%s]",
                    this,
                    (const char*) nsCAutoString(parentTagStr),
                    (const char*) nsCAutoString(childTagStr),
                    resourceCStr));
        }
#endif
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::CreateTemplateAndContainerContents(nsIContent* aElement,
                                                          nsIContent** aContainer,
                                                          PRInt32* aNewIndexInContainer)
{
    // Generate both 1) the template content for the current element,
    // and 2) recursive subcontent (if the current element refers to a
    // container resource in the RDF graph).
    nsresult rv;

    if (! mRulesCompiled) {
        rv = CompileRules();
        if (NS_FAILED(rv)) return rv;
    }

    // If we're asked to return the first generated child, then
    // initialize to "none".
    if (aContainer) {
        *aContainer = nsnull;
        *aNewIndexInContainer = -1;
    }

    // Create the current resource's contents from the template, if
    // appropriate
    nsAutoString templateID;
    rv = aElement->GetAttribute(kNameSpaceID_None, kTemplateAtom, templateID);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        rv = CreateTemplateContents(aElement, templateID, aContainer, aNewIndexInContainer);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIRDFResource> resource;
    rv = gXULUtils->GetElementRefResource(aElement, getter_AddRefs(resource));
    if (NS_SUCCEEDED(rv)) {
        // The element has a resource; that means that it corresponds
        // to something in the graph, so we need to go to the graph to
        // create its contents.
        rv = CreateContainerContents(aElement, resource, PR_FALSE, aContainer, aNewIndexInContainer);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::CreateContainerContents(nsIContent* aElement,
                                               nsIRDFResource* aResource,
                                               PRBool aNotify,
                                               nsIContent** aContainer,
                                               PRInt32* aNewIndexInContainer)
{
    // Create the contents of a container by iterating over all of the
    // "containment" arcs out of the element's resource.
    nsresult rv;

    if (aContainer) {
        *aContainer = nsnull;
        *aNewIndexInContainer = -1;
    }

    // The tree widget is special. If the item isn't open, then just
    // "pretend" that there aren't any contents here. We'll create
    // them when OpenContainer() gets called.
    if (IsTreeWidgetItem(aElement) && !IsOpen(aElement))
        return NS_OK;

    // See if the element's templates contents have been generated:
    // this prevents a re-entrant call from triggering another
    // generation.
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
    if (xulcontent) {
        PRBool contentsGenerated;
        rv = xulcontent->GetLazyState(nsIXULContent::eContainerContentsBuilt, contentsGenerated);
        if (NS_FAILED(rv)) return rv;

        if (contentsGenerated)
            return NS_OK;

        // Now mark the element's contents as being generated so that
        // any re-entrant calls don't trigger an infinite recursion.
        rv = xulcontent->SetLazyState(nsIXULContent::eContainerContentsBuilt);
    }
    else {
        // HTML is always needs to be generated.
        //
        // XXX Big ass-umption here -- I am assuming that this will
        // _only_ ever get called (in the case of an HTML element)
        // when the XUL builder is descending thru the graph and
        // stumbles on a template that is rooted at an HTML element.
        // (/me crosses fingers...)
    }

    // Seed the rule network with bindings for the content and
    // container variables
    //
    // XXXwaterson could this code be shared with
    // RDFGenericBuilderImpl::Propogate()?
    Instantiation seed;
    seed.AddBinding(mContentVar, Value(aElement));
    seed.AddBinding(mContainerVar, Value(aResource));

    InstantiationSet instantiations;
    instantiations.Append(seed);

    // Propogate the bindings through the network
    KeySet newkeys;
    rv = mRules.GetRoot()->Propogate(instantiations, &newkeys);
    if (NS_FAILED(rv)) return rv;

    // Iterate through newly added keys to determine which rules fired
    KeySet::ConstIterator last = newkeys.Last();
    for (KeySet::ConstIterator key = newkeys.First(); key != last; ++key) {
        const MatchSet* matches;
        mConflictSet.GetMatches(*key, &matches);

        if (! matches)
            continue;

        const MatchSet::Match* match;
        matches->FindMatchWithHighestPriority(&match);

        // Grab the template node
        nsCOMPtr<nsIContent> tmpl;
        match->mRule->GetContent(getter_AddRefs(tmpl));

        rv = BuildContentFromTemplate(tmpl, aElement, aElement, PR_TRUE,
                                      VALUE_TO_IRDFRESOURCE(key->mMemberValue),
                                      aNotify, aContainer, aNewIndexInContainer);

        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::CreateTemplateContents(nsIContent* aElement,
                                              const nsString& aTemplateID,
                                              nsIContent** aContainer,
                                              PRInt32* aNewIndexInContainer)
{
    // Create the contents of an element using the templates
    nsresult rv;

    // See if the element's templates contents have been generated:
    // this prevents a re-entrant call from triggering another
    // generation.
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(aElement);
    if (! xulcontent)
        return NS_OK; // HTML content is _always_ generated up-front

    PRBool contentsGenerated;
    rv = xulcontent->GetLazyState(nsIXULContent::eTemplateContentsBuilt, contentsGenerated);
    if (NS_FAILED(rv)) return rv;

    if (contentsGenerated)
        return NS_OK;

    // Now mark the element's contents as being generated so that
    // any re-entrant calls don't trigger an infinite recursion.
    rv = xulcontent->SetLazyState(nsIXULContent::eTemplateContentsBuilt);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set template-contents-generated attribute");
    if (NS_FAILED(rv)) return rv;

    // Find the template node that corresponds to the "real" node for
    // which we're trying to generate contents.
    nsCOMPtr<nsIDOMXULDocument> xulDoc;
    xulDoc = do_QueryInterface(mDocument);
    if (! xulDoc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDOMElement> tmplNode;
    rv = xulDoc->GetElementById(aTemplateID, getter_AddRefs(tmplNode));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIContent> tmpl = do_QueryInterface(tmplNode);
    if (! tmpl)
        return NS_ERROR_UNEXPECTED;

    // Crawl up the content model until we find the "resource" element
    // that spawned this template.
    nsCOMPtr<nsIRDFResource> resource;

    nsCOMPtr<nsIContent> element = aElement;
    while (element) {
        rv = gXULUtils->GetElementRefResource(element, getter_AddRefs(resource));
        if (NS_SUCCEEDED(rv)) break;

        nsCOMPtr<nsIContent> parent;
        rv = element->GetParent(*getter_AddRefs(parent));
        if (NS_FAILED(rv)) return rv;

        element = parent;
    }

    rv = BuildContentFromTemplate(tmpl, aElement, aElement, PR_FALSE, resource, PR_FALSE,
                                  aContainer, aNewIndexInContainer);

    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::EnsureElementHasGenericChild(nsIContent* parent,
                                                    PRInt32 nameSpaceID,
                                                    nsIAtom* tag,
                                                    PRBool aNotify,
                                                    nsIContent** result)
{
    nsresult rv;

    rv = gXULUtils->FindChildByTag(parent, nameSpaceID, tag, result);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_RDF_NO_VALUE) {
        // we need to construct a new child element.
        nsCOMPtr<nsIContent> element;

        rv = CreateElement(nameSpaceID, tag, getter_AddRefs(element));
        if (NS_FAILED(rv)) return rv;

        // XXX Note that the notification ensures we won't batch insertions! This could be bad! - Dave
        rv = parent->AppendChildTo(element, aNotify);
        if (NS_FAILED(rv)) return rv;

        *result = element;
        NS_ADDREF(*result);
        return NS_RDF_ELEMENT_GOT_CREATED;
    }
    else {
        return NS_RDF_ELEMENT_WAS_THERE;
    }
}


PRBool
RDFGenericBuilderImpl::IsContainmentProperty(nsIContent* aElement, nsIRDFResource* aProperty)
{
    // XXX is this okay to _always_ treat ordinal properties as tree
    // properties? Probably not...
    nsresult        rv;
    PRBool        isOrdinal;

    rv = gRDFContainerUtils->IsOrdinalProperty(aProperty, &isOrdinal);
    if (NS_FAILED(rv))
        return PR_FALSE;

    if (isOrdinal)
        return PR_TRUE;

    const char        *propertyURI;

    rv = aProperty->GetValueConst(&propertyURI);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get property URI");
    if (NS_FAILED(rv)) return PR_FALSE;

    PRUnichar		buffer[256];
    nsAutoString	containment(CBufDescriptor(buffer, PR_TRUE, sizeof(buffer) / sizeof(PRUnichar), 0));

    // rjc: Optimization: 99% of trees that use "containment='...'" put the
    // attribute on the root of the tree, so check that first
    rv = mRoot->GetAttribute(kNameSpaceID_None, kContainmentAtom, containment);
    if (NS_FAILED(rv)) return rv;
    if (rv == NS_CONTENT_ATTR_HAS_VALUE)
    {
        if (containment.Find(propertyURI) >= 0)
            return PR_TRUE;
        else    return PR_FALSE;
    }

    // If we get here, we didn't find any tree property: so now
    // defaults start to kick in.

#define TREE_PROPERTY_HACK
#if defined(TREE_PROPERTY_HACK)
    if ((aProperty == kNC_child) ||
        (aProperty == kNC_Folder) ||
        (aProperty == kRDF_child)) {
        return PR_TRUE;
    }
#endif // defined(TREE_PROPERTY_HACK)

    return PR_FALSE;
}


PRBool
RDFGenericBuilderImpl::IsIgnoredProperty(nsIContent* aElement, nsIRDFResource* aProperty)
{
    nsresult        rv;
    const char        *propertyURI;

    rv = aProperty->GetValueConst(&propertyURI);
    if (NS_FAILED(rv)) return PR_FALSE;

    PRUnichar		buffer[256];
    nsAutoString	uri(CBufDescriptor(buffer, PR_TRUE, sizeof(buffer) / sizeof(PRUnichar), 0));
    PRInt32        	nameSpaceID;

    // rjc: Optimization: 99% of trees that use "ignore='...'" put the
    // attribute on the root of the tree, so check that first
    rv = mRoot->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return PR_FALSE;
        // Never ever ask an HTML element about non-HTML attributes
        if (nameSpaceID != kNameSpaceID_HTML)
        {
            rv = mRoot->GetAttribute(kNameSpaceID_None, kIgnoreAtom, uri);
            if (NS_FAILED(rv)) return PR_FALSE;
            if (rv == NS_CONTENT_ATTR_HAS_VALUE)
            {
                return uri.Find(propertyURI) >= 0 ? PR_TRUE : PR_FALSE;
            }
        }

    return PR_FALSE;
}

PRBool
RDFGenericBuilderImpl::IsContainer(nsIContent* aElement, nsIRDFResource* aResource)
{
    // Look at all of the arcs extending _out_ of the resource: if any
    // of them are that "containment" property, then we know we'll
    // have children.

    nsCOMPtr<nsISimpleEnumerator> arcs;
    nsresult rv;

    rv = mDB->ArcLabelsOut(aResource, getter_AddRefs(arcs));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get arcs out");
    if (NS_FAILED(rv))
        return PR_FALSE;

    while (1) {
        PRBool hasMore;
        rv = arcs->HasMoreElements(&hasMore);
        NS_ASSERTION(NS_SUCCEEDED(rv), "severe error advancing cursor");
        if (NS_FAILED(rv))
            return PR_FALSE;

        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = arcs->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv))
            return PR_FALSE;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);

        if (! IsContainmentProperty(aElement, property))
            continue;

        return PR_TRUE;
    }

    return PR_FALSE;
}


PRBool
RDFGenericBuilderImpl::IsEmpty(nsIContent* aElement, nsIRDFResource* aContainer)
{
    // Look at all of the arcs extending _out_ of the resource: if any
    // of them are that "containment" property, then we know we'll
    // have children.

    nsCOMPtr<nsISimpleEnumerator> arcs;
    nsresult rv;

    rv = mDB->ArcLabelsOut(aContainer, getter_AddRefs(arcs));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get arcs out");
    if (NS_FAILED(rv))
        return PR_TRUE;

    while (1) {
        PRBool hasMore;
        rv = arcs->HasMoreElements(&hasMore);
        NS_ASSERTION(NS_SUCCEEDED(rv), "severe error advancing cursor");
        if (NS_FAILED(rv))
            return PR_TRUE;

        if (! hasMore)
            break;

        nsCOMPtr<nsISupports> isupports;
        rv = arcs->GetNext(getter_AddRefs(isupports));
        if (NS_FAILED(rv))
            return PR_TRUE;

        nsCOMPtr<nsIRDFResource> property = do_QueryInterface(isupports);

        if (! IsContainmentProperty(aElement, property))
            continue;

        // now that we know its a container, check to see if it's "empty"
        // by testing to see if the property has a target.
        nsCOMPtr<nsIRDFNode> dummy;
        rv = mDB->GetTarget(aContainer, property, PR_TRUE, getter_AddRefs(dummy));
        if (NS_FAILED(rv)) return rv;

        if (dummy)
            return PR_FALSE;
    }

    return PR_TRUE;
}


PRBool
RDFGenericBuilderImpl::IsOpen(nsIContent* aElement)
{
    nsresult rv;

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return PR_FALSE;

    // Treat the 'root' element as always open, -unless- it's a
    // menu/menupopup. We don't need to "fake" these as being open.
    if ((aElement == mRoot.get()) && (tag.get() != kMenuAtom))
      return PR_TRUE;

    nsAutoString value;
    rv = aElement->GetAttribute(kNameSpaceID_None, kOpenAtom, value);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get open attribute");
    if (NS_FAILED(rv)) return PR_FALSE;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        if (value.Equals(trueStr))
            return PR_TRUE;
    }

    
    return PR_FALSE;
}


PRBool
RDFGenericBuilderImpl::IsElementInWidget(nsIContent* aElement)
{
    return IsElementContainedBy(aElement, mRoot);
}

nsresult
RDFGenericBuilderImpl::RemoveGeneratedContent(nsIContent* aElement)
{
    nsresult rv;

    // Although we *could* iterate through all the retractions and
    // pick them out one-by-one, it turns out that this is pretty
    // inefficient. So do the dumb thing and nuke all the content from
    // the back to the front.

    PRInt32 count;
    rv = aElement->ChildCount(count);
    if (NS_FAILED(rv)) return rv;

    while (--count >= 0) {
        nsCOMPtr<nsIContent> child;
        rv = aElement->ChildAt(count, *getter_AddRefs(child));
        if (NS_FAILED(rv)) return rv;

        nsAutoString tmplID;
        rv = child->GetAttribute(kNameSpaceID_None, kTemplateAtom, tmplID);
        if (NS_FAILED(rv)) return rv;

        if (rv != NS_CONTENT_ATTR_HAS_VALUE)
            continue;

        // It's a generated element. Remove it, and set its document
        // to null so that it'll get knocked out of the XUL doc's
        // resource-to-element map.
        rv = aElement->RemoveChildAt(count, PR_TRUE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "error removing child");

        rv = child->SetDocument(nsnull, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


PRBool
RDFGenericBuilderImpl::IsTreeWidgetItem(nsIContent* aElement)
{
    // Determine if this is a <tree> or a <treeitem> tag, in which
    // case, some special logic will kick in to force batched reflows.
    // XXX Should be removed when Bug 10818 is fixed.
    nsresult rv;

    PRInt32 nameSpaceID;
    rv = aElement->GetNameSpaceID(nameSpaceID);
    if (NS_FAILED(rv)) return PR_FALSE;

    nsCOMPtr<nsIAtom> tag;
    rv = aElement->GetTag(*getter_AddRefs(tag));
    if (NS_FAILED(rv)) return PR_FALSE;

    // If we're building content under a <tree> or a <treeitem>,
    // then DO NOT notify layout until we're all done.
    if ((nameSpaceID == kNameSpaceID_XUL) &&
        ((tag.get() == kTreeAtom) || (tag.get() == kTreeItemAtom))) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }

}

nsresult
RDFGenericBuilderImpl::AddDatabasePropertyToHTMLElement(nsIContent* aElement, nsIRDFCompositeDataSource* aDataBase)
{
    // Use XPConnect and the JS APIs to whack aDatabase as the
    // 'database' property onto aElement.
    nsresult rv;

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    NS_ASSERTION(doc != nsnull, "no document");
    if (! doc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIScriptGlobalObject> global;
    doc->GetScriptGlobalObject(getter_AddRefs(global));
    if (! global)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIScriptContext> context;
    global->GetContext(getter_AddRefs(context));
    if (! context)
        return NS_ERROR_UNEXPECTED;

    JSContext* jscontext = NS_STATIC_CAST(JSContext*, context->GetNativeContext());
    NS_ASSERTION(context != nsnull, "no jscontext");
    if (! jscontext)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIScriptObjectOwner> owner = do_QueryInterface(aElement);
    NS_ASSERTION(owner != nsnull, "unable to get script object owner");
    if (! owner)
         return NS_ERROR_UNEXPECTED;

    JSObject* jselement;
    rv = owner->GetScriptObject(context, (void**) &jselement);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get element's script object");
    if (NS_FAILED(rv)) return rv;

    static NS_DEFINE_CID(kXPConnectCID, NS_XPCONNECT_CID);
    NS_WITH_SERVICE(nsIXPConnect, xpc, kXPConnectCID, &rv);

    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
    rv = xpc->WrapNative(jscontext,                       
                         jselement,
                         aDataBase,
                         NS_GET_IID(nsIRDFCompositeDataSource),
                         getter_AddRefs(wrapper));

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to xpconnect-wrap database");
    if (NS_FAILED(rv)) return rv;

    JSObject* jsobj;
    rv = wrapper->GetJSObject(&jsobj);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get jsobj from xpconnect wrapper");
    if (NS_FAILED(rv)) return rv;

    jsval jsdatabase = OBJECT_TO_JSVAL(jsobj);

    PRBool ok;
    ok = JS_SetProperty(jscontext, jselement, "database", &jsdatabase);
    NS_ASSERTION(ok, "unable to set database property");
    if (! ok)
        return NS_ERROR_FAILURE;

    return NS_OK;
}


nsresult
RDFGenericBuilderImpl::GetElementsForResource(nsIRDFResource* aResource, nsISupportsArray* aElements)
{
    nsresult rv;

    const char *uri;
    rv = aResource->GetValueConst(&uri);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource URI");
    if (NS_FAILED(rv)) return rv;

    rv = mDocument->GetElementsForID(nsAutoString(uri), aElements);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to retrieve elements from resource");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::CreateElement(PRInt32 aNameSpaceID,
                                     nsIAtom* aTag,
                                     nsIContent** aResult)
{
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    if (! doc)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;
    nsCOMPtr<nsIContent> result;

    if (aNameSpaceID == kNameSpaceID_XUL) {
        rv = nsXULElement::Create(aNameSpaceID, aTag, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else if (aNameSpaceID == kNameSpaceID_HTML) {
        const PRUnichar *tagName;
        aTag->GetUnicode(&tagName);

        rv = gHTMLElementFactory->CreateInstanceByTag(tagName, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;
    }
    else {
        const PRUnichar *tagName;
        aTag->GetUnicode(&tagName);

        nsCOMPtr<nsIElementFactory> elementFactory;
        GetElementFactory(aNameSpaceID, getter_AddRefs(elementFactory));
        rv = elementFactory->CreateInstanceByTag(tagName, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;

        if (! result)
            return NS_ERROR_UNEXPECTED;
    }

    // 
    nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(result);
    if (formControl) {
        nsCOMPtr<nsIDOMHTMLFormElement> form;
        rv = mDocument->GetForm(getter_AddRefs(form));
        if (NS_SUCCEEDED(rv) && form)
            formControl->SetForm(form);
    }
    
    rv = result->SetDocument(doc, PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set element's document");
    if (NS_FAILED(rv)) return rv;

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::SetEmpty(nsIContent *element, PRBool empty)
{
    nsresult rv;

    nsAutoString newEmptyStr(empty ? trueStr : falseStr);
    nsAutoString emptyStr;

    rv = element->GetAttribute(kNameSpaceID_None, kEmptyAtom, emptyStr);
    if (NS_FAILED(rv)) return rv;

    if ((rv != NS_CONTENT_ATTR_HAS_VALUE) || (! emptyStr.Equals(newEmptyStr))) {
        rv = element->SetAttribute(kNameSpaceID_None, kEmptyAtom, newEmptyStr, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;

}

void 
RDFGenericBuilderImpl::GetElementFactory(PRInt32 aNameSpaceID, nsIElementFactory** aResult)
{
  nsresult rv;
  nsAutoString nameSpace;
  gNameSpaceManager->GetNameSpaceURI(aNameSpaceID, nameSpace);

  nsCAutoString progID = NS_ELEMENT_FACTORY_PROGID_PREFIX;
  progID.Append(nameSpace);

  // Retrieve the appropriate factory.
  NS_WITH_SERVICE(nsIElementFactory, elementFactory, progID, &rv);

  if (!elementFactory)
    elementFactory = gXMLElementFactory; // Nothing found. Use generic XML element.

  *aResult = elementFactory;
  NS_IF_ADDREF(*aResult);
}

#ifdef PR_LOGGING
nsresult
RDFGenericBuilderImpl::Log(const char* aOperation,
                           nsIRDFResource* aSource,
                           nsIRDFResource* aProperty,
                           nsIRDFNode* aTarget)
{
    if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        nsresult rv;

        const char* sourceStr;
        rv = aSource->GetValueConst(&sourceStr);
        if (NS_FAILED(rv)) return rv;

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("rdfgeneric[%p] %8s [%s]--", this, aOperation, sourceStr));

        const char* propertyStr;
        rv = aProperty->GetValueConst(&propertyStr);
        if (NS_FAILED(rv)) return rv;

        nsAutoString targetStr;
        rv = gXULUtils->GetTextForNode(aTarget, targetStr);
        if (NS_FAILED(rv)) return rv;

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("                       --[%s]-->[%s]",
                propertyStr,
                (const char*) nsCAutoString(targetStr)));
    }
    return NS_OK;
}
#endif

//----------------------------------------------------------------------

nsresult
RDFGenericBuilderImpl::ContentIdTestNode::FilterInstantiations(InstantiationSet& aInstantiations) const
{
    nsresult rv;

    nsCOMPtr<nsISupportsArray> elements;
    rv = NS_NewISupportsArray(getter_AddRefs(elements));
    if (NS_FAILED(rv)) return rv;

    InstantiationSet::Iterator last = aInstantiations.Last();
    for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
        Value contentValue;
        PRBool hasContentBinding = inst->mBindings.GetBindingFor(mContentVariable, &contentValue);

        Value idValue;
        PRBool hasIdBinding = inst->mBindings.GetBindingFor(mIdVariable, &idValue);

        if (hasContentBinding && hasIdBinding) {
            // both are bound, consistency check
            nsCOMPtr<nsIRDFResource> resource;
            gXULUtils->GetElementRefResource(VALUE_TO_ICONTENT(contentValue), getter_AddRefs(resource));
            
            if (resource.get() == VALUE_TO_IRDFRESOURCE(idValue)) {
                inst->AddSupportingElement(new Element(VALUE_TO_ICONTENT(contentValue)));
            }
            else {
                aInstantiations.Erase(inst--);
            }
        }
        else if (hasContentBinding) {
            // the content node is bound, get its id
            nsAutoString id;
            rv = VALUE_TO_ICONTENT(contentValue)->GetAttribute(kNameSpaceID_None, kIdAtom, id);
            if (NS_FAILED(rv)) return rv;

            if (id.Length()) {
                nsCOMPtr<nsIRDFResource> resource;
                rv = gRDFService->GetUnicodeResource(id.GetUnicode(), getter_AddRefs(resource));
                if (NS_FAILED(rv)) return rv;

                Instantiation newinst = *inst;
                newinst.AddBinding(mIdVariable, Value(resource.get()));
                newinst.AddSupportingElement(new Element(VALUE_TO_ICONTENT(contentValue)));
                aInstantiations.Insert(inst, newinst);
            }
            else {
                aInstantiations.Erase(inst--);
            }
        }
        else if (hasIdBinding) {
            // the 'id' is bound, find elements in the content tree that match
            const char* uri;
            rv = VALUE_TO_IRDFRESOURCE(idValue)->GetValueConst(&uri);
            if (NS_FAILED(rv)) return rv;

            rv = mDocument->GetElementsForID(nsAutoString(uri), elements);
            if (NS_FAILED(rv)) return rv;

            PRUint32 count;
            rv = elements->Count(&count);
            if (NS_FAILED(rv)) return rv;

            for (PRInt32 j = PRInt32(count) - 1; j >= 0; --j) {
                nsISupports* isupports = elements->ElementAt(j);
                nsCOMPtr<nsIContent> element = do_QueryInterface(isupports);
                NS_IF_RELEASE(isupports);

                if (IsElementContainedBy(element, mRoot)) {
                    Instantiation newinst = *inst;
                    newinst.AddBinding(mContentVariable, Value(element.get()));
                    newinst.AddSupportingElement(new Element(element));
                    aInstantiations.Insert(inst, newinst);
                }
            }

            aInstantiations.Erase(inst--);
        }
    }

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::ContentIdTestNode::GetAncestorVariables(VariableSet& aVariables) const
{
    nsresult rv;

    rv = aVariables.Add(mContentVariable);
    if (NS_FAILED(rv)) return rv;

    rv = aVariables.Add(mIdVariable);
    if (NS_FAILED(rv)) return rv;

    return TestNode::GetAncestorVariables(aVariables);
}

//----------------------------------------------------------------------


nsresult
RDFGenericBuilderImpl::ComputeContainmentProperties()
{
    // The 'containment' attribute on the root node is a
    // whitespace-separated list that tells us which properties we
    // should use to test for containment.
    nsresult rv;

    mContainmentProperties.Clear();

    nsAutoString containment;
    rv = mRoot->GetAttribute(kNameSpaceID_None, kContainmentAtom, containment);
    if (NS_FAILED(rv)) return rv;

    PRUint32 len = containment.Length();
    PRUint32 offset = 0;
    while (offset < len) {
        while (offset < len && nsCRT::IsAsciiSpace(containment[offset]))
            ++offset;

        if (offset >= len)
            break;

        PRUint32 end = offset;
        while (end < len && !nsCRT::IsAsciiSpace(containment[end]))
            ++end;

        nsAutoString propertyStr;
        containment.Mid(propertyStr, offset, end - offset);

        nsCOMPtr<nsIRDFResource> property;
        rv = gRDFService->GetUnicodeResource(propertyStr.GetUnicode(), getter_AddRefs(property));
        if (NS_FAILED(rv)) return rv;

        rv = mContainmentProperties.Add(property);
        if (NS_FAILED(rv)) return rv;

        offset = end;
    }

#if defined(TREE_PROPERTY_HACK)
    if (!len || 1 /*XXX*/) {
        // Some ever-present membership tests.
        mContainmentProperties.Add(kNC_child);
        mContainmentProperties.Add(kNC_Folder);
        mContainmentProperties.Add(kRDF_child);
    }
#endif

    return NS_OK;
}

nsresult
RDFGenericBuilderImpl::InitializeRuleNetwork(InnerNode** aChildNode)
{
    // The rule network will start off looking something like this:
    //
    //   (root)-->(content ^id ?a)-->(?a ^member ?b)
    //

    nsresult rv;

    rv = mRules.Clear();
    if (NS_FAILED(rv)) return rv;

    rv = mRDFTests.Clear();
    if (NS_FAILED(rv)) return rv;

    rv = ComputeContainmentProperties();
    if (NS_FAILED(rv)) return rv;

    // Create (content ?content ^id ?container)
    mRules.CreateVariable(&mContentVar);
    mRules.CreateVariable(&mContainerVar);
    mRules.CreateVariable(&mMemberVar);

    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mDocument);
    if (! xuldoc)
        return NS_ERROR_UNEXPECTED;

    ContentIdTestNode* idnode =
        new ContentIdTestNode(mRules.GetRoot(),
                              xuldoc,
                              mRoot,
                              mContentVar,
                              mContainerVar);

    if (! idnode)
        return NS_ERROR_OUT_OF_MEMORY;

    mRules.GetRoot()->AddChild(idnode);
    mRules.AddNode(idnode);

    // Create (?container ^member ?member)
    RDFContainerMemberTestNode* membernode =
        new RDFContainerMemberTestNode(idnode,
                                       mDB,
                                       mContainmentProperties,
                                       mContainerVar,
                                       mMemberVar);

    if (! membernode)
        return NS_ERROR_OUT_OF_MEMORY;

    idnode->AddChild(membernode);
    mRules.AddNode(membernode);

    mRDFTests.Add(membernode);

    *aChildNode = membernode;
    return NS_OK;
}



nsresult
RDFGenericBuilderImpl::CompileRules()
{
    NS_PRECONDITION(mRoot != nsnull, "not initialized");
    if (! mRoot)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv;

    mRulesCompiled = PR_FALSE; // in case an error occurs

    InnerNode* childnode;
    rv = InitializeRuleNetwork(&childnode);
    if (NS_FAILED(rv)) return rv;

    // First, check and see if the root has a template attribute. This
    // allows a template to be specified "out of line"; e.g.,
    //
    //   <window>
    //     <foo template="MyTemplate">...</foo>
    //     <template id="MyTemplate">...</template>
    //   </window>
    //

    nsAutoString templateID;
    rv = mRoot->GetAttribute(kNameSpaceID_None, kTemplateAtom, templateID);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIContent> tmpl;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        nsCOMPtr<nsIDOMXULDocument>    xulDoc;
        xulDoc = do_QueryInterface(mDocument);
        if (! xulDoc)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIDOMElement>    domElement;
        rv = xulDoc->GetElementById(templateID, getter_AddRefs(domElement));
        if (NS_FAILED(rv)) return rv;

        tmpl = do_QueryInterface(domElement);
    }

    // If root node has no template attribute, then look for a child
    // node which is a template tag
    if (! tmpl) {
        PRInt32    count;
        rv = mRoot->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = 0; i < count; i++) {
            nsCOMPtr<nsIContent> child;
            rv = mRoot->ChildAt(i, *getter_AddRefs(child));
            if (NS_FAILED(rv)) return rv;

            PRInt32 nameSpaceID;
            rv = child->GetNameSpaceID(nameSpaceID);
            if (NS_FAILED(rv)) return rv;

            if (nameSpaceID != kNameSpaceID_XUL)
                continue;

            nsCOMPtr<nsIAtom> tag;
            rv = child->GetTag(*getter_AddRefs(tag));
            if (NS_FAILED(rv)) return rv;

            if (tag.get() != kTemplateAtom)
                continue;

            tmpl = child;
            break;
        }
    }

    // Found a template; check against any (optional) rules...
    if (tmpl) {
        PRInt32    count;
        rv = tmpl->ChildCount(count);
        if (NS_FAILED(rv)) return rv;

        PRInt32 nrules = 0;

        for (PRInt32 i = 0; i < count; i++) {
            nsCOMPtr<nsIContent> rule;
            rv = tmpl->ChildAt(i, *getter_AddRefs(rule));
            if (NS_FAILED(rv)) return rv;

            PRInt32    nameSpaceID;
            rv = rule->GetNameSpaceID(nameSpaceID);
            if (NS_FAILED(rv)) return rv;

            if (nameSpaceID != kNameSpaceID_XUL)
                continue;

            nsCOMPtr<nsIAtom> tag;
            rv = rule->GetTag(*getter_AddRefs(tag));
            if (NS_FAILED(rv)) return rv;

            if (tag.get() == kRuleAtom) {
                ++nrules;

                rv = CompileRule(rule, nrules, childnode);
                if (NS_FAILED(rv)) return rv;
            }
        }

        if (nrules == 0) {
            // if no rules are specified in the template, then the
            // contents of the <template> tag are the one-and-only
            // template.
            rv = CompileRule(tmpl, 1, childnode);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // XXXwaterson post-process the rule network to optimize

    mRulesCompiled = PR_TRUE;
    return NS_OK;
}



nsresult
RDFGenericBuilderImpl::CompileRule(nsIContent* aRule, PRInt32 aPriority, InnerNode* aParentNode)
{
    nsresult rv;

    PRBool hasContainerTest = PR_FALSE;

    PRInt32 count;
    aRule->GetAttributeCount(count);

    // Add constraints for the LHS
    for (PRInt32 i = 0; i < count; ++i) {
        PRInt32 attrNameSpaceID;
        nsCOMPtr<nsIAtom> attr;
        rv = aRule->GetAttributeNameAt(i, attrNameSpaceID, *getter_AddRefs(attr));
        if (NS_FAILED(rv)) return rv;

        // Note: some attributes must be skipped on XUL template rule subtree

        // never compare against rdf:property attribute
        if ((attr.get() == kPropertyAtom) && (attrNameSpaceID == kNameSpaceID_RDF))
            continue;
        // never compare against rdf:instanceOf attribute
        else if ((attr.get() == kInstanceOfAtom) && (attrNameSpaceID == kNameSpaceID_RDF))
            continue;
        // never compare against {}:id attribute
        else if ((attr.get() == kIdAtom) && (attrNameSpaceID == kNameSpaceID_None))
            continue;
        // never compare against {}:xulcontentsgenerated attribute
        else if ((attr.get() == kXULContentsGeneratedAtom) && (attrNameSpaceID == kNameSpaceID_None))
            continue;

        nsAutoString value;
        rv = aRule->GetAttribute(attrNameSpaceID, attr, value);
        if (NS_FAILED(rv)) return rv;

        TestNode* testnode = nsnull;

        if ((attrNameSpaceID == kNameSpaceID_None) && (attr.get() == kParentAtom)) {
            // The "parent" test.
            //
            // XXXwaterson this is wrong: we can't add this below the
            // the previous node, because it'll cause an unconstrained
            // search if we ever came "up" through this path. Need a
            // JoinNode in here somewhere.
            nsCOMPtr<nsIAtom> tag = dont_AddRef(NS_NewAtom(value));

            testnode = new ContentTagTestNode(aParentNode, mContentVar, tag);
            if (! testnode)
                return NS_ERROR_OUT_OF_MEMORY;
        }
        else if (((attrNameSpaceID == kNameSpaceID_None) && (attr.get() == kIsContainerAtom)) ||
                 ((attrNameSpaceID == kNameSpaceID_None) && (attr.get() == kIsEmptyAtom))) {
            // Tests about containerhood and emptiness. These can be
            // globbed together, mostly. Check to see if we've already
            // added a container test: we only need one.
            if (hasContainerTest)
                continue;

            RDFContainerInstanceTestNode::Test iscontainer =
                RDFContainerInstanceTestNode::eDontCare;

            rv = aRule->GetAttribute(kNameSpaceID_None, kIsContainerAtom, value);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                if (value.Equals("true")) {
                    iscontainer = RDFContainerInstanceTestNode::eTrue;
                }
                else if (value.Equals("false")) {
                    iscontainer = RDFContainerInstanceTestNode::eFalse;
                }
            }

            RDFContainerInstanceTestNode::Test isempty =
                RDFContainerInstanceTestNode::eDontCare;

            rv = aRule->GetAttribute(kNameSpaceID_None, kIsEmptyAtom, value);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                if (value.Equals("true")) {
                    isempty = RDFContainerInstanceTestNode::eTrue;
                }
                else if (value.Equals("false")) {
                    isempty = RDFContainerInstanceTestNode::eFalse;
                }
            }

            testnode = new RDFContainerInstanceTestNode(aParentNode,
                                                        mDB,
                                                        mContainmentProperties,
                                                        mMemberVar,
                                                        iscontainer,
                                                        isempty);

            if (! testnode)
                return NS_ERROR_OUT_OF_MEMORY;

            mRDFTests.Add(testnode);
        }
        else {
            // It's a simple RDF test
            nsCOMPtr<nsIRDFResource> property;
            rv = gXULUtils->GetResource(attrNameSpaceID, attr, getter_AddRefs(property));
            if (NS_FAILED(rv)) return rv;

            // XXXwaterson this is so manky
            nsCOMPtr<nsIRDFNode> target;
            if (value.FindChar(':') != -1) { // XXXwaterson WRONG WRONG WRONG!
                nsCOMPtr<nsIRDFResource> resource;
                rv = gRDFService->GetUnicodeResource(value.GetUnicode(), getter_AddRefs(resource));
                if (NS_FAILED(rv)) return rv;

                target = do_QueryInterface(resource);
            }
            else {
                nsCOMPtr<nsIRDFLiteral> literal;
                rv = gRDFService->GetLiteral(value.GetUnicode(), getter_AddRefs(literal));
                if (NS_FAILED(rv)) return rv;

                target = do_QueryInterface(literal);
            }

            testnode = new RDFPropertyTestNode(aParentNode, mDB, mMemberVar, property, target);
            if (! testnode)
                return NS_ERROR_OUT_OF_MEMORY;

            mRDFTests.Add(testnode);
        }

        aParentNode->AddChild(testnode);
        mRules.AddNode(testnode);
        aParentNode = testnode;
    }

    // XXXwaterson Add unbounds for the RHS? The problem with doing
    // this is that it will require variables to have values, which
    // isn't the current semantics of the <rule> tag.

    Rule* rule = new Rule(aRule, mContainerVar, mMemberVar, aPriority);
    if (! rule)
        return NS_ERROR_OUT_OF_MEMORY;

    // The InstantiationNode owns the rule now.
    InstantiationNode* instnode =
        new InstantiationNode(rule, &mConflictSet);

    if (! instnode)
        return NS_ERROR_OUT_OF_MEMORY;

    aParentNode->AddChild(instnode);
    mRules.AddNode(instnode);
    
    return NS_OK;
}
