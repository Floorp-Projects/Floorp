/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Private maps (hashtables). */

#ifndef xpcmaps_h___
#define xpcmaps_h___

// Maps...

// no virtuals in the maps - all the common stuff inlined
// templates could be used to good effect here.

class JSContext2XPCContextMap
{
public:
    static JSContext2XPCContextMap* newMap(int size);

    inline XPCContext* Find(JSContext* cx)
    {
        NS_PRECONDITION(cx,"bad param");
        return (XPCContext*) JS_HashTableLookup(mHashtable, cx);
    }

    inline void Add(XPCContext* xpcc)
    {
        NS_PRECONDITION(xpcc,"bad param");
        JS_HashTableAdd(mHashtable, xpcc->GetJSContext(), xpcc);
    }

    inline void Remove(XPCContext* xpcc)
    {
        NS_PRECONDITION(xpcc,"bad param");
        JS_HashTableRemove(mHashtable, xpcc->GetJSContext());
    }

    inline uint32 Count() {return mHashtable->nentries;}
    inline intN Enumerate(JSHashEnumerator f, void *arg)
        {return JS_HashTableEnumerateEntries(mHashtable, f, arg);}

    ~JSContext2XPCContextMap();
private:
    JSContext2XPCContextMap();    // no implementation
    JSContext2XPCContextMap(int size);
private:
    JSHashTable *mHashtable;
};

/*************************/

class JSObject2WrappedJSMap
{
public:
    static JSObject2WrappedJSMap* newMap(int size);

    inline nsXPCWrappedJS* Find(JSObject* Obj)
    {
        NS_PRECONDITION(Obj,"bad param");
        return (nsXPCWrappedJS*) JS_HashTableLookup(mHashtable, Obj);
    }

    inline void Add(nsXPCWrappedJS* Wrapper)
    {
        NS_PRECONDITION(Wrapper,"bad param");
        JS_HashTableAdd(mHashtable, Wrapper->GetJSObject(), Wrapper);
    }

    inline void Remove(nsXPCWrappedJS* Wrapper)
    {
        NS_PRECONDITION(Wrapper,"bad param");
        JS_HashTableRemove(mHashtable, Wrapper->GetJSObject());
    }

    inline uint32 Count() {return mHashtable->nentries;}
    inline intN Enumerate(JSHashEnumerator f, void *arg)
        {return JS_HashTableEnumerateEntries(mHashtable, f, arg);}

    ~JSObject2WrappedJSMap();
private:
    JSObject2WrappedJSMap();    // no implementation
    JSObject2WrappedJSMap(int size);
private:
    JSHashTable *mHashtable;
};

/*************************/

class Native2WrappedNativeMap
{
public:
    static Native2WrappedNativeMap* newMap(int size);

    inline nsXPCWrappedNative* Find(nsISupports* Obj)
    {
        NS_PRECONDITION(Obj,"bad param");
        return (nsXPCWrappedNative*) JS_HashTableLookup(mHashtable, Obj);
    }

    inline void Add(nsXPCWrappedNative* Wrapper)
    {
        NS_PRECONDITION(Wrapper,"bad param");
        JS_HashTableAdd(mHashtable, Wrapper->GetNative(), Wrapper);
    }

    inline void Remove(nsXPCWrappedNative* Wrapper)
    {
        NS_PRECONDITION(Wrapper,"bad param");
        JS_HashTableRemove(mHashtable, Wrapper->GetNative());
    }

    inline uint32 Count() {return mHashtable->nentries;}
    inline intN Enumerate(JSHashEnumerator f, void *arg)
        {return JS_HashTableEnumerateEntries(mHashtable, f, arg);}

    ~Native2WrappedNativeMap();
private:
    Native2WrappedNativeMap();    // no implementation
    Native2WrappedNativeMap(int size);
private:
    JSHashTable *mHashtable;
};

/*************************/

class IID2WrappedJSClassMap
{
public:
    static IID2WrappedJSClassMap* newMap(int size);

    inline nsXPCWrappedJSClass* Find(REFNSIID iid)
    {
        return (nsXPCWrappedJSClass*) JS_HashTableLookup(mHashtable, &iid);
    }

    inline void Add(nsXPCWrappedJSClass* Class)
    {
        NS_PRECONDITION(Class,"bad param");
        JS_HashTableAdd(mHashtable, &Class->GetIID(), Class);
    }

    inline void Remove(nsXPCWrappedJSClass* Class)
    {
        NS_PRECONDITION(Class,"bad param");
        JS_HashTableRemove(mHashtable, &Class->GetIID());
    }

    inline uint32 Count() {return mHashtable->nentries;}
    inline intN Enumerate(JSHashEnumerator f, void *arg)
        {return JS_HashTableEnumerateEntries(mHashtable, f, arg);}

    ~IID2WrappedJSClassMap();
private:
    IID2WrappedJSClassMap();    // no implementation
    IID2WrappedJSClassMap(int size);
private:
    JSHashTable *mHashtable;
};

/*************************/

class IID2WrappedNativeClassMap
{
public:
    static IID2WrappedNativeClassMap* newMap(int size);

    inline nsXPCWrappedNativeClass* Find(REFNSIID iid)
    {
        return (nsXPCWrappedNativeClass*) JS_HashTableLookup(mHashtable, &iid);
    }

    inline void Add(nsXPCWrappedNativeClass* Class)
    {
        NS_PRECONDITION(Class,"bad param");
        JS_HashTableAdd(mHashtable, &Class->GetIID(), Class);
    }

    inline void Remove(nsXPCWrappedNativeClass* Class)
    {
        NS_PRECONDITION(Class,"bad param");
        JS_HashTableRemove(mHashtable, &Class->GetIID());
    }

    inline uint32 Count() {return mHashtable->nentries;}
    inline intN Enumerate(JSHashEnumerator f, void *arg)
        {return JS_HashTableEnumerateEntries(mHashtable, f, arg);}

    ~IID2WrappedNativeClassMap();
private:
    IID2WrappedNativeClassMap();    // no implementation
    IID2WrappedNativeClassMap(int size);
private:
    JSHashTable *mHashtable;
};

#endif /* xpcmaps_h___ */
