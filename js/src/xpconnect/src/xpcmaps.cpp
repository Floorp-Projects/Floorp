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

#include "xpcprivate.h"

/***************************************************************************/
// static shared...

JS_STATIC_DLL_CALLBACK(JSHashNumber)
hash_root(const void *key)
{
    return ((JSHashNumber) key) >> 2; /* help lame MSVC1.5 on Win16 */
}

// XXX this is just the hacked String hash function, should do better...
JS_STATIC_DLL_CALLBACK(JSHashNumber)
hash_IID(const void *key)
{
    JSHashNumber h;
    const PRUint8 *s;
    int i;

    h = 0;
    for (s = (const PRUint8 *)key, i = 0; i < 16; s++, i++)
        h = (h >> 28) ^ (h << 4) ^ *s;
    return h;
}

JS_STATIC_DLL_CALLBACK(intN)
compare_IIDs(const void *v1, const void *v2)
{
    return ((const nsID*)v1)->Equals(*((const nsID*)v2));
}

/***************************************************************************/
// implement JSContext2XPCContextMap...

// static
JSContext2XPCContextMap*
JSContext2XPCContextMap::newMap(int size)
{
    JSContext2XPCContextMap* map = new JSContext2XPCContextMap(size);
    if(map && map->mHashtable)
        return map;
    delete map;
    return NULL;
}


JSContext2XPCContextMap::JSContext2XPCContextMap(int size)
{
    mHashtable = JS_NewHashTable(size, hash_root,
                                 JS_CompareValues, JS_CompareValues,
                                 NULL, NULL);
}

JSContext2XPCContextMap::~JSContext2XPCContextMap()
{
    if(mHashtable)
        JS_HashTableDestroy(mHashtable);
}

/***************************************************************************/
// implement JSObject2WrappedJSMap...

// static
JSObject2WrappedJSMap*
JSObject2WrappedJSMap::newMap(int size)
{
    JSObject2WrappedJSMap* map = new JSObject2WrappedJSMap(size);
    if(map && map->mHashtable)
        return map;
    delete map;
    return NULL;
}


JSObject2WrappedJSMap::JSObject2WrappedJSMap(int size)
{
    mHashtable = JS_NewHashTable(size, hash_root,
                                 JS_CompareValues, JS_CompareValues,
                                 NULL, NULL);
}

JSObject2WrappedJSMap::~JSObject2WrappedJSMap()
{
    if(mHashtable)
        JS_HashTableDestroy(mHashtable);
}

/***************************************************************************/
// implement Native2WrappedNativeMap...

// static
Native2WrappedNativeMap*
Native2WrappedNativeMap::newMap(int size)
{
    Native2WrappedNativeMap* map = new Native2WrappedNativeMap(size);
    if(map && map->mHashtable)
        return map;
    delete map;
    return NULL;
}

Native2WrappedNativeMap::Native2WrappedNativeMap(int size)
{
    mHashtable = JS_NewHashTable(size, hash_root,
                                 JS_CompareValues, JS_CompareValues,
                                 NULL, NULL);
}

Native2WrappedNativeMap::~Native2WrappedNativeMap()
{
    if(mHashtable)
        JS_HashTableDestroy(mHashtable);
}

/***************************************************************************/
// implement IID2WrappedJSClassMap...

// static
IID2WrappedJSClassMap*
IID2WrappedJSClassMap::newMap(int size)
{
    IID2WrappedJSClassMap* map = new IID2WrappedJSClassMap(size);
    if(map && map->mHashtable)
        return map;
    delete map;
    return NULL;
}

IID2WrappedJSClassMap::IID2WrappedJSClassMap(int size)
{
    mHashtable = JS_NewHashTable(size, hash_IID,
                                 compare_IIDs, JS_CompareValues,
                                 NULL, NULL);
}

IID2WrappedJSClassMap::~IID2WrappedJSClassMap()
{
    if(mHashtable)
        JS_HashTableDestroy(mHashtable);
}

/***************************************************************************/
// implement IID2WrappedJSClassMap...

// static
IID2WrappedNativeClassMap*
IID2WrappedNativeClassMap::newMap(int size)
{
    IID2WrappedNativeClassMap* map = new IID2WrappedNativeClassMap(size);
    if(map && map->mHashtable)
        return map;
    delete map;
    return NULL;
}

IID2WrappedNativeClassMap::IID2WrappedNativeClassMap(int size)
{
    mHashtable = JS_NewHashTable(size, hash_IID,
                                 compare_IIDs, JS_CompareValues,
                                 NULL, NULL);
}

IID2WrappedNativeClassMap::~IID2WrappedNativeClassMap()
{
    if(mHashtable)
        JS_HashTableDestroy(mHashtable);
}
