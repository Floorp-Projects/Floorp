/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

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

class JSContext2XPCContextMap
{
public:
    struct Entry : public JSDHashEntryHdr
    {
        JSContext*  key;
        XPCContext* value;
    };

    static JSContext2XPCContextMap* newMap(int size);

    inline XPCContext* Find(JSContext* cx)
    {
        NS_PRECONDITION(cx,"bad param");
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, cx, JS_DHASH_LOOKUP);
        if(JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline XPCContext* Add(XPCContext* xpcc)
    {
        NS_PRECONDITION(xpcc,"bad param");
        JSContext* cx = xpcc->GetJSContext();
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, cx, JS_DHASH_ADD);
        if(!entry)
            return nsnull;
        if(entry->key)
            return entry->value;
        entry->key = cx;
        entry->value = xpcc;
        return xpcc;
    }

    inline void Remove(XPCContext* xpcc)
    {
        NS_PRECONDITION(xpcc,"bad param");
        JS_DHashTableOperate(mTable, xpcc->GetJSContext(), JS_DHASH_REMOVE);
    }

    inline uint32 Count() {return mTable->entryCount;}
    inline uint32 Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~JSContext2XPCContextMap();
private:
    JSContext2XPCContextMap();    // no implementation
    JSContext2XPCContextMap(int size);
private:
    JSDHashTable *mTable;
};

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
        if(JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline nsXPCWrappedJS* Add(nsXPCWrappedJS* wrapper)
    {
        NS_PRECONDITION(wrapper,"bad param");
        JSObject* obj = wrapper->GetJSObject();
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, obj, JS_DHASH_ADD);
        if(!entry)
            return nsnull;
        if(entry->key)
            return entry->value;
        entry->key = obj;
        entry->value = wrapper;
        return wrapper;
    }

    inline void Remove(nsXPCWrappedJS* wrapper)
    {
        NS_PRECONDITION(wrapper,"bad param");
        JS_DHashTableOperate(mTable, wrapper->GetJSObject(), JS_DHASH_REMOVE);
    }

    inline uint32 Count() {return mTable->entryCount;}
    inline uint32 Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~JSObject2WrappedJSMap();
private:
    JSObject2WrappedJSMap();    // no implementation
    JSObject2WrappedJSMap(int size);
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
        if(JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline XPCWrappedNative* Add(XPCWrappedNative* wrapper)
    {
        NS_PRECONDITION(wrapper,"bad param");
        nsISupports* obj = wrapper->GetIdentityObject();
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, obj, JS_DHASH_ADD);
        if(!entry)
            return nsnull;
        if(entry->key)
            return entry->value;
        entry->key = obj;
        entry->value = wrapper;
        return wrapper;
    }

    inline void Remove(XPCWrappedNative* wrapper)
    {
        NS_PRECONDITION(wrapper,"bad param");
        JS_DHashTableOperate(mTable, wrapper->GetIdentityObject(), JS_DHASH_REMOVE);
    }

    inline uint32 Count() {return mTable->entryCount;}
    inline uint32 Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~Native2WrappedNativeMap();
private:
    Native2WrappedNativeMap();    // no implementation
    Native2WrappedNativeMap(int size);
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
        if(JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline nsXPCWrappedJSClass* Add(nsXPCWrappedJSClass* clazz)
    {
        NS_PRECONDITION(clazz,"bad param");
        const nsIID* iid = &clazz->GetIID();
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, iid, JS_DHASH_ADD);
        if(!entry)
            return nsnull;
        if(entry->key)
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

    inline uint32 Count() {return mTable->entryCount;}
    inline uint32 Enumerate(JSDHashEnumerator f, void *arg)
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
        if(JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline XPCNativeInterface* Add(XPCNativeInterface* iface)
    {
        NS_PRECONDITION(iface,"bad param");
        const nsIID* iid = iface->GetIID();
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, iid, JS_DHASH_ADD);
        if(!entry)
            return nsnull;
        if(entry->key)
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

    inline uint32 Count() {return mTable->entryCount;}
    inline uint32 Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~IID2NativeInterfaceMap();
private:
    IID2NativeInterfaceMap();    // no implementation
    IID2NativeInterfaceMap(int size);
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
        if(JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline XPCNativeSet* Add(nsIClassInfo* info, XPCNativeSet* set)
    {
        NS_PRECONDITION(info,"bad param");
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, info, JS_DHASH_ADD);
        if(!entry)
            return nsnull;
        if(entry->key)
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

    inline uint32 Count() {return mTable->entryCount;}
    inline uint32 Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

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
        if(JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline XPCWrappedNativeProto* Add(nsIClassInfo* info, XPCWrappedNativeProto* proto)
    {
        NS_PRECONDITION(info,"bad param");
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, info, JS_DHASH_ADD);
        if(!entry)
            return nsnull;
        if(entry->key)
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

    inline uint32 Count() {return mTable->entryCount;}
    inline uint32 Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~ClassInfo2WrappedNativeProtoMap();
private:
    ClassInfo2WrappedNativeProtoMap();    // no implementation
    ClassInfo2WrappedNativeProtoMap(int size);
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

        static JSBool JS_DLL_CALLBACK
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
        if(JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->key_value;
    }

    inline XPCNativeSet* Add(const XPCNativeSetKey* key, XPCNativeSet* set)
    {
        NS_PRECONDITION(key,"bad param");
        NS_PRECONDITION(set,"bad param");
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, key, JS_DHASH_ADD);
        if(!entry)
            return nsnull;
        if(entry->key_value)
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

    inline uint32 Count() {return mTable->entryCount;}
    inline uint32 Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~NativeSetMap();
private:
    NativeSetMap();    // no implementation
    NativeSetMap(int size);
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

        static const void* JS_DLL_CALLBACK
        GetKey(JSDHashTable *table, JSDHashEntryHdr *entry);

        static JSBool JS_DLL_CALLBACK
        Match(JSDHashTable *table,
              const JSDHashEntryHdr *entry,
              const void *key);

        static void JS_DLL_CALLBACK
        Clear(JSDHashTable *table, JSDHashEntryHdr *entry);

        static struct JSDHashTableOps sOps;
    };

    static IID2ThisTranslatorMap* newMap(int size);

    inline nsIXPCFunctionThisTranslator* Find(REFNSIID iid)
    {
        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, &iid, JS_DHASH_LOOKUP);
        if(JS_DHASH_ENTRY_IS_FREE(entry))
            return nsnull;
        return entry->value;
    }

    inline nsIXPCFunctionThisTranslator* Add(REFNSIID iid,
                                             nsIXPCFunctionThisTranslator* obj)
    {

        Entry* entry = (Entry*)
            JS_DHashTableOperate(mTable, &iid, JS_DHASH_ADD);
        if(!entry)
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

    inline uint32 Count() {return mTable->entryCount;}
    inline uint32 Enumerate(JSDHashEnumerator f, void *arg)
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

        static JSDHashNumber JS_DLL_CALLBACK
        Hash(JSDHashTable *table, const void *key);

        static JSBool JS_DLL_CALLBACK
        Match(JSDHashTable *table,
              const JSDHashEntryHdr *entry,
              const void *key);

        static struct JSDHashTableOps sOps;
    };

    static XPCNativeScriptableSharedMap* newMap(int size);

    JSBool GetNewOrUsed(JSUint32 flags, char* name,
                        XPCNativeScriptableInfo* si);

    inline uint32 Count() {return mTable->entryCount;}
    inline uint32 Enumerate(JSDHashEnumerator f, void *arg)
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
        if(!entry)
            return nsnull;
        if(entry->key)
            return (XPCWrappedNativeProto*) entry->key;
        entry->key = proto;
        return proto;
    }

    inline void Remove(XPCWrappedNativeProto* proto)
    {
        NS_PRECONDITION(proto,"bad param");
        JS_DHashTableOperate(mTable, proto, JS_DHASH_REMOVE);
    }

    inline uint32 Count() {return mTable->entryCount;}
    inline uint32 Enumerate(JSDHashEnumerator f, void *arg)
        {return JS_DHashTableEnumerate(mTable, f, arg);}

    ~XPCWrappedNativeProtoMap();
private:
    XPCWrappedNativeProtoMap();    // no implementation
    XPCWrappedNativeProtoMap(int size);
private:
    JSDHashTable *mTable;
};

#endif /* xpcmaps_h___ */
