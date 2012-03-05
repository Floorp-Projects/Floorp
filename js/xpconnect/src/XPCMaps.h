/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
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

/* Private maps (hashtables). */

#ifndef xpcmaps_h___
#define xpcmaps_h___

// Maps...

// Note that most of the declarations for hash table entries begin with
// a pointer to something or another. This makes them look enough like
// the JSDHashEntryStub struct that the default OPs (JS_DHashGetStubOps())
// just do the right thing for most of our needs.

// no virtuals in the maps - all the common stuff inlined
// templates could be used to good effect here.

/*************************/

class JSObject2WrappedJSMap
{
public:
    struct Entry : public JSDHashEntryHdr
    {
        JSObject*       key;
        nsXPCWrappedJS* value;
    };

    static JSObject2WrappedJSMap* newMap(int size);

    inline nsXPCWrappedJS* Find(JSObject* Obj)
    {
        NS_PRECONDITION(Obj,"bad param");
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, Obj, JS_DHASH_LOOKUP);
        if (JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline nsXPCWrappedJS* Add(nsXPCWrappedJS* wrapper)
    {
        NS_PRECONDITION(wrapper,"bad param");
        JSObject* obj = wrapper->GetJSObjectPreserveColor();
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, obj, JS_DHASH_ADD);
        if (!entry)
            return nsnull;
        if (entry->key)
            return entry->value;
        entry->key = obj;
        entry->value = wrapper;
        return wrapper;
    }

    inline void Remove(nsXPCWrappedJS* wrapper)
    {
        NS_PRECONDITION(wrapper,"bad param");
        JS_DHashTableOperate(mTable, wrapper->GetJSObjectPreserveColor(),
                             JS_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    size_t SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf);

    ~JSObject2WrappedJSMap();
private:
    JSObject2WrappedJSMap();    // no implementation
    JSObject2WrappedJSMap(int size);

    static size_t SizeOfEntryExcludingThis(JSDHashEntryHdr *hdr, JSMallocSizeOfFun mallocSizeOf, void *);

private:
    JSDHashTable *mTable;
};

/*************************/

class Native2WrappedNativeMap
{
public:
    struct Entry : public JSDHashEntryHdr
    {
        nsISupports*      key;
        XPCWrappedNative* value;
    };

    static Native2WrappedNativeMap* newMap(int size);

    inline XPCWrappedNative* Find(nsISupports* Obj)
    {
        NS_PRECONDITION(Obj,"bad param");
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, Obj, JS_DHASH_LOOKUP);
        if (JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline XPCWrappedNative* Add(XPCWrappedNative* wrapper)
    {
        NS_PRECONDITION(wrapper,"bad param");
        nsISupports* obj = wrapper->GetIdentityObject();
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, obj, JS_DHASH_ADD);
        if (!entry)
            return nsnull;
        if (entry->key)
            return entry->value;
        entry->key = obj;
        entry->value = wrapper;
        return wrapper;
    }

    inline void Remove(XPCWrappedNative* wrapper)
    {
        NS_PRECONDITION(wrapper,"bad param");
#ifdef DEBUG
        XPCWrappedNative* wrapperInMap = Find(wrapper->GetIdentityObject());
        NS_ASSERTION(!wrapperInMap || wrapperInMap == wrapper,
                     "About to remove a different wrapper with the same "
                     "nsISupports identity! This will most likely cause serious "
                     "problems!");
#endif
        JS_DHashTableOperate(mTable, wrapper->GetIdentityObject(), JS_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    size_t SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf);

    ~Native2WrappedNativeMap();
private:
    Native2WrappedNativeMap();    // no implementation
    Native2WrappedNativeMap(int size);

    static size_t SizeOfEntryExcludingThis(JSDHashEntryHdr *hdr, JSMallocSizeOfFun mallocSizeOf, void *);

private:
    JSDHashTable *mTable;
};

/*************************/

class IID2WrappedJSClassMap
{
public:
    struct Entry : public JSDHashEntryHdr
    {
        const nsIID*         key;
        nsXPCWrappedJSClass* value;

        static struct JSDHashTableOps sOps;
    };

    static IID2WrappedJSClassMap* newMap(int size);

    inline nsXPCWrappedJSClass* Find(REFNSIID iid)
    {
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, &iid, JS_DHASH_LOOKUP);
        if (JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline nsXPCWrappedJSClass* Add(nsXPCWrappedJSClass* clazz)
    {
        NS_PRECONDITION(clazz,"bad param");
        const nsIID* iid = &clazz->GetIID();
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, iid, JS_DHASH_ADD);
        if (!entry)
            return nsnull;
        if (entry->key)
            return entry->value;
        entry->key = iid;
        entry->value = clazz;
        return clazz;
    }

    inline void Remove(nsXPCWrappedJSClass* clazz)
    {
        NS_PRECONDITION(clazz,"bad param");
        JS_DHashTableOperate(mTable, &clazz->GetIID(), JS_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~IID2WrappedJSClassMap();
private:
    IID2WrappedJSClassMap();    // no implementation
    IID2WrappedJSClassMap(int size);
private:
    JSDHashTable *mTable;
};

/*************************/

class IID2NativeInterfaceMap
{
public:
    struct Entry : public JSDHashEntryHdr
    {
        const nsIID*        key;
        XPCNativeInterface* value;

        static struct JSDHashTableOps sOps;
    };

    static IID2NativeInterfaceMap* newMap(int size);

    inline XPCNativeInterface* Find(REFNSIID iid)
    {
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, &iid, JS_DHASH_LOOKUP);
        if (JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline XPCNativeInterface* Add(XPCNativeInterface* iface)
    {
        NS_PRECONDITION(iface,"bad param");
        const nsIID* iid = iface->GetIID();
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, iid, JS_DHASH_ADD);
        if (!entry)
            return nsnull;
        if (entry->key)
            return entry->value;
        entry->key = iid;
        entry->value = iface;
        return iface;
    }

    inline void Remove(XPCNativeInterface* iface)
    {
        NS_PRECONDITION(iface,"bad param");
        JS_DHashTableOperate(mTable, iface->GetIID(), JS_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    size_t SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf);

    ~IID2NativeInterfaceMap();
private:
    IID2NativeInterfaceMap();    // no implementation
    IID2NativeInterfaceMap(int size);

    static size_t SizeOfEntryExcludingThis(JSDHashEntryHdr *hdr, JSMallocSizeOfFun mallocSizeOf, void *);

private:
    JSDHashTable *mTable;
};

/*************************/

class ClassInfo2NativeSetMap
{
public:
    struct Entry : public JSDHashEntryHdr
    {
        nsIClassInfo* key;
        XPCNativeSet* value;
    };

    static ClassInfo2NativeSetMap* newMap(int size);

    inline XPCNativeSet* Find(nsIClassInfo* info)
    {
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, info, JS_DHASH_LOOKUP);
        if (JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline XPCNativeSet* Add(nsIClassInfo* info, XPCNativeSet* set)
    {
        NS_PRECONDITION(info,"bad param");
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, info, JS_DHASH_ADD);
        if (!entry)
            return nsnull;
        if (entry->key)
            return entry->value;
        entry->key = info;
        entry->value = set;
        return set;
    }

    inline void Remove(nsIClassInfo* info)
    {
        NS_PRECONDITION(info,"bad param");
        JS_DHashTableOperate(mTable, info, JS_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    // ClassInfo2NativeSetMap holds pointers to *some* XPCNativeSets.
    // So we don't want to count those XPCNativeSets, because they are better
    // counted elsewhere (i.e. in XPCJSRuntime::mNativeSetMap, which holds
    // pointers to *all* XPCNativeSets).  Hence the "Shallow".
    size_t ShallowSizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf);

    ~ClassInfo2NativeSetMap();
private:
    ClassInfo2NativeSetMap();    // no implementation
    ClassInfo2NativeSetMap(int size);
private:
    JSDHashTable *mTable;
};

/*************************/

class ClassInfo2WrappedNativeProtoMap
{
public:
    struct Entry : public JSDHashEntryHdr
    {
        nsIClassInfo*          key;
        XPCWrappedNativeProto* value;
    };

    static ClassInfo2WrappedNativeProtoMap* newMap(int size);

    inline XPCWrappedNativeProto* Find(nsIClassInfo* info)
    {
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, info, JS_DHASH_LOOKUP);
        if (JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline XPCWrappedNativeProto* Add(nsIClassInfo* info, XPCWrappedNativeProto* proto)
    {
        NS_PRECONDITION(info,"bad param");
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, info, JS_DHASH_ADD);
        if (!entry)
            return nsnull;
        if (entry->key)
            return entry->value;
        entry->key = info;
        entry->value = proto;
        return proto;
    }

    inline void Remove(nsIClassInfo* info)
    {
        NS_PRECONDITION(info,"bad param");
        JS_DHashTableOperate(mTable, info, JS_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    size_t SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf);

    ~ClassInfo2WrappedNativeProtoMap();
private:
    ClassInfo2WrappedNativeProtoMap();    // no implementation
    ClassInfo2WrappedNativeProtoMap(int size);

    static size_t SizeOfEntryExcludingThis(JSDHashEntryHdr *hdr, JSMallocSizeOfFun mallocSizeOf, void *);

private:
    JSDHashTable *mTable;
};

/*************************/

class NativeSetMap
{
public:
    struct Entry : public JSDHashEntryHdr
    {
        XPCNativeSet* key_value;

        static JSBool
        Match(JSDHashTable *table,
              const JSDHashEntryHdr *entry,
              const void *key);

        static struct JSDHashTableOps sOps;
    };

    static NativeSetMap* newMap(int size);

    inline XPCNativeSet* Find(XPCNativeSetKey* key)
    {
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, key, JS_DHASH_LOOKUP);
        if (JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->key_value;
    }

    inline XPCNativeSet* Add(const XPCNativeSetKey* key, XPCNativeSet* set)
    {
        NS_PRECONDITION(key,"bad param");
        NS_PRECONDITION(set,"bad param");
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, key, JS_DHASH_ADD);
        if (!entry)
            return nsnull;
        if (entry->key_value)
            return entry->key_value;
        entry->key_value = set;
        return set;
    }

    inline XPCNativeSet* Add(XPCNativeSet* set)
    {
        XPCNativeSetKey key(set, nsnull, 0);
        return Add(&key, set);
    }

    inline void Remove(XPCNativeSet* set)
    {
        NS_PRECONDITION(set,"bad param");

        XPCNativeSetKey key(set, nsnull, 0);
        JS_DHashTableOperate(mTable, &key, JS_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    size_t SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf);

    ~NativeSetMap();
private:
    NativeSetMap();    // no implementation
    NativeSetMap(int size);

    static size_t SizeOfEntryExcludingThis(JSDHashEntryHdr *hdr, JSMallocSizeOfFun mallocSizeOf, void *);

private:
    JSDHashTable *mTable;
};

/***************************************************************************/

class IID2ThisTranslatorMap
{
public:
    struct Entry : public JSDHashEntryHdr
    {
        nsIID                         key;
        nsIXPCFunctionThisTranslator* value;

        static JSBool
        Match(JSDHashTable *table,
              const JSDHashEntryHdr *entry,
              const void *key);

        static void
        Clear(JSDHashTable *table, JSDHashEntryHdr *entry);

        static struct JSDHashTableOps sOps;
    };

    static IID2ThisTranslatorMap* newMap(int size);

    inline nsIXPCFunctionThisTranslator* Find(REFNSIID iid)
    {
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, &iid, JS_DHASH_LOOKUP);
        if (JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline nsIXPCFunctionThisTranslator* Add(REFNSIID iid,
                                             nsIXPCFunctionThisTranslator* obj)
    {

        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, &iid, JS_DHASH_ADD);
        if (!entry)
            return nsnull;
        NS_IF_ADDREF(obj);
        NS_IF_RELEASE(entry->value);
        entry->value = obj;
        entry->key = iid;
        return obj;
    }

    inline void Remove(REFNSIID iid)
    {
        JS_DHashTableOperate(mTable, &iid, JS_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~IID2ThisTranslatorMap();
private:
    IID2ThisTranslatorMap();    // no implementation
    IID2ThisTranslatorMap(int size);
private:
    JSDHashTable *mTable;
};

/***************************************************************************/

class XPCNativeScriptableSharedMap
{
public:
    struct Entry : public JSDHashEntryHdr
    {
        XPCNativeScriptableShared* key;

        static JSDHashNumber
        Hash(JSDHashTable *table, const void *key);

        static JSBool
        Match(JSDHashTable *table,
              const JSDHashEntryHdr *entry,
              const void *key);

        static struct JSDHashTableOps sOps;
    };

    static XPCNativeScriptableSharedMap* newMap(int size);

    JSBool GetNewOrUsed(uint32_t flags, char* name, PRUint32 interfacesBitmap,
                        XPCNativeScriptableInfo* si);

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~XPCNativeScriptableSharedMap();
private:
    XPCNativeScriptableSharedMap();    // no implementation
    XPCNativeScriptableSharedMap(int size);
private:
    JSDHashTable *mTable;
};

/***************************************************************************/

class XPCWrappedNativeProtoMap
{
public:
    static XPCWrappedNativeProtoMap* newMap(int size);

    inline XPCWrappedNativeProto* Add(XPCWrappedNativeProto* proto)
    {
        NS_PRECONDITION(proto,"bad param");
        JSDHashEntryStub* entry = (JSDHashEntryStub*)
            JS_DHashTableOperate(mTable, proto, JS_DHASH_ADD);
        if (!entry)
            return nsnull;
        if (entry->key)
            return (XPCWrappedNativeProto*) entry->key;
        entry->key = proto;
        return proto;
    }

    inline void Remove(XPCWrappedNativeProto* proto)
    {
        NS_PRECONDITION(proto,"bad param");
        JS_DHashTableOperate(mTable, proto, JS_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~XPCWrappedNativeProtoMap();
private:
    XPCWrappedNativeProtoMap();    // no implementation
    XPCWrappedNativeProtoMap(int size);
private:
    JSDHashTable *mTable;
};

class XPCNativeWrapperMap
{
public:
    static XPCNativeWrapperMap* newMap(int size);

    inline JSObject* Add(JSObject* nw)
    {
        NS_PRECONDITION(nw,"bad param");
        JSDHashEntryStub* entry = (JSDHashEntryStub*)
            JS_DHashTableOperate(mTable, nw, JS_DHASH_ADD);
        if (!entry)
            return nsnull;
        if (entry->key)
            return (JSObject*) entry->key;
        entry->key = nw;
        return nw;
    }

    inline void Remove(JSObject* nw)
    {
        NS_PRECONDITION(nw,"bad param");
        JS_DHashTableOperate(mTable, nw, JS_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~XPCNativeWrapperMap();
private:
    XPCNativeWrapperMap();    // no implementation
    XPCNativeWrapperMap(int size);
private:
    JSDHashTable *mTable;
};

class WrappedNative2WrapperMap
{
    static struct JSDHashTableOps sOps;

    static void ClearLink(JSDHashTable* table, JSDHashEntryHdr* entry);
    static void MoveLink(JSDHashTable* table, const JSDHashEntryHdr* from,
                         JSDHashEntryHdr* to);

public:
    struct Link : public PRCList
    {
        JSObject *obj;
    };

    struct Entry : public JSDHashEntryHdr
    {
        // Note: key must be the flat JSObject for a wrapped native.
        JSObject*         key;
        Link              value;
    };

    static WrappedNative2WrapperMap* newMap(int size);

    inline JSObject* Find(JSObject* wrapper)
    {
        NS_PRECONDITION(wrapper, "bad param");
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, wrapper, JS_DHASH_LOOKUP);
        if (JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value.obj;
    }

    // Note: If the entry already exists, then this will overwrite the
    // existing entry, returning the old value.
    JSObject* Add(WrappedNative2WrapperMap* head,
                  JSObject* wrappedObject,
                  JSObject* wrapper);

    // Function to find a link.
    Link* FindLink(JSObject* wrappedObject)
    {
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, wrappedObject, JS_DHASH_LOOKUP);
        if (JS_DHASH_ENTRY_IS_BUSY(entry))
            return &entry->value;
        return nsnull;
    }

    // "Internal" function to add an empty link without doing unnecessary
    // work.
    bool AddLink(JSObject* wrappedObject, Link* oldLink);

    inline void Remove(JSObject* wrapper)
    {
        NS_PRECONDITION(wrapper,"bad param");
        JS_DHashTableOperate(mTable, wrapper, JS_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~WrappedNative2WrapperMap();

private:
    WrappedNative2WrapperMap();    // no implementation
    WrappedNative2WrapperMap(int size);

private:
    JSDHashTable *mTable;
};

class JSObject2JSObjectMap
{
    static struct JSDHashTableOps sOps;

public:
    struct Entry : public JSDHashEntryHdr
    {
        JSObject* key;
        JSObject* value;
    };

    static JSObject2JSObjectMap* newMap(int size)
    {
        JSObject2JSObjectMap* map = new JSObject2JSObjectMap(size);
        if (map && map->mTable)
            return map;
        delete map;
        return nsnull;
    }

    inline JSObject* Find(JSObject* key)
    {
        NS_PRECONDITION(key, "bad param");
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, key, JS_DHASH_LOOKUP);
        if (JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    // Note: If the entry already exists, return the old value.
    inline JSObject* Add(JSObject *key, JSObject *value)
    {
        NS_PRECONDITION(key,"bad param");
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, key, JS_DHASH_ADD);
        if (!entry)
            return nsnull;
        if (entry->key)
            return entry->value;
        entry->key = key;
        entry->value = value;
        return value;
    }

    inline void Remove(JSObject* key)
    {
        NS_PRECONDITION(key,"bad param");
        JS_DHashTableOperate(mTable, key, JS_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}

    inline uint32_t Enumerate(JSDHashEnumerator f, void *arg)
    {
        return JS_DHashTableEnumerate(mTable, f, arg);
    }

    ~JSObject2JSObjectMap()
    {
        if (mTable)
            JS_DHashTableDestroy(mTable);
    }

private:
    JSObject2JSObjectMap(int size)
    {
        mTable = JS_NewDHashTable(&sOps, nsnull, sizeof(Entry), size);
    }

    JSObject2JSObjectMap(); // no implementation

private:
    JSDHashTable *mTable;
};

#endif /* xpcmaps_h___ */
