/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

/**
 * A Named Map for MITREObjects
 * @author <a href="kvisco@mitre.org">Keith Visco</a>
**/

#include "NamedMap.h"

    //-------------/
  //- Constants -/
//-------------/

const int NamedMap::DEFAULT_SIZE = 17;

  //----------------/
 //- Constructors -/
//----------------/

/**
 * Creates a new NamedMap with the default Size
**/
NamedMap::NamedMap() {
    initialize(DEFAULT_SIZE);
} //-- NamedMap

/**
 * Creates a new NamedMap with the specified number of buckets
**/
NamedMap::NamedMap(int size) {
    initialize(size);
} //-- NamedMap

/**
 * Helper method for Constructors
**/
void NamedMap::initialize(Int32 size) {

    //-- by default the NamedMap will not delete it's
    //-- object references
    doObjectDeletion = MB_FALSE;

    //-- create a new array of bucket pointers
    elements = new BucketItem*[size];

    //-- initialize all elements to 0;
    for ( Int32 i = 0; i < size; i++ ) elements[i] = 0;

    numberOfBuckets = size;
    numberOfElements = 0;
} //-- initialize

/**
 * Destructor for NamedMap
**/
NamedMap::~NamedMap() {
    //cout << "~NamedMap() - start"<<endl;
    clear();
    delete [] elements;
    //cout << "~NamedMap() - done"<<endl;
} //-- ~NamedMap



/**
 * Removes all elements from the NamedMap. If the object deletion flag
 * has been set to true (by a call to setObjectDeletion) objects
 * will also be deleted as they are removed from the map
**/
void NamedMap::clear() {
    clear(doObjectDeletion);
} //-- clear

/**
 * Removes all elements from the NamedMap
**/
void NamedMap::clear(MBool deleteObjects) {

    for (int i = 0; i < numberOfBuckets; i++) {

        BucketItem* bktItem = elements[i];
        while (bktItem) {
            BucketItem* tItem = bktItem;
            bktItem = bktItem->next;
            //-- repoint item to 0 to prevent deletion
            if ( ! deleteObjects ) tItem->item = 0;
            else {
                delete tItem->item;
            }
            //--delete tItem;
            delete tItem;
        }
    }
    numberOfElements = 0;
} //-- clear

void NamedMap::dumpMap() {


    cout << "#NamedMap -------- { "<<endl;

    for (int i = 0; i < numberOfBuckets; i++) {

        cout << "[";
        if (i < 10 ) cout << '0';
        cout << i << "]->{";

        BucketItem* item = elements[i];
        MBool hasPrevItem = MB_FALSE;
        while (item) {
            if (hasPrevItem) cout << ", ";
            cout << item->key;
            hasPrevItem = MB_TRUE;
            item = item->next;
        }
        cout << "}"<<endl;
    }
    cout <<"} #NamedMap"<<endl;
} //-- dumpMap

/**
 * Compares the specified object with this NamedMap for equality.
 * Returns true if and only if the specified Object is a NamedMap
 * that hashes to the same value as this NamedMap
 * @return true if and only if the specified Object is a NamedMap
 * that hashes to the same value as this NamedMap
**/
MBool NamedMap::equals(NamedMap* namedMap) {
    //-- currently does nothing
    return MB_FALSE;
} //-- equals

/**
 *  Returns the object reference in this Map associated with the given name
 * @return the object reference in this Map associated with the given name
**/
MITREObject* NamedMap::get(const char* key) {
    String sKey = key;
    return get(sKey);
} //-- get

/**
 *  Returns the object reference in this Map associated with the given name
 * @return the object reference in this Map associated with the given name
**/
MITREObject* NamedMap::get(const String& key) {
    BucketItem* item = getBucketItem(key);
    if ( item ) return item->item;
    return 0;
} //-- get

/**
 * Returns true if there are no Nodes in the NodeStack.
 * @return true if there are no Nodes in the NodeStack.
**/
MBool NamedMap::isEmpty() {
    return (numberOfElements == 0) ? MB_TRUE : MB_FALSE;
} //-- isEmpty

/**
 * Adds the specified Node to the top of this Stack.
 * @param node the Node to add to the top of the Stack
**/
void NamedMap::put(const char* key, MITREObject* obj) {
    String sKey = key;
    put(sKey, obj);
} //-- put

/**
 * Adds the specified Node to the top of this Stack.
 * @param node the Node to add to the top of the Stack
**/
void NamedMap::put(const String& key, MITREObject* obj) {

    //-- compute hash for key
    unsigned long hashCode = hashKey(key);
    //-- calculate index
    int idx = hashCode % numberOfBuckets;

    //-- fetch first item in bucket
    BucketItem* bktItem = elements[idx];

    //-- if bktItem is 0 then there are no items is this Bucket,
    //-- add to front of list
    if ( !bktItem ) {
        elements[idx] = createBucketItem(key, obj);
        ++numberOfElements;
    }
    //-- find current item, or add to end of list
    else {
        BucketItem* prevItem = bktItem;
        //-- advance to next spot
        while ( bktItem ) {
            //-- if current key equals desired key, break
            if ( bktItem->key.isEqual(key) ) {
                break;
            }
            prevItem = bktItem;
            bktItem = bktItem->next;
        }
        //-- if we did not find a bucket Item create a new one
        if ( !bktItem) {
            bktItem = createBucketItem(key, obj);
            prevItem->next = bktItem;
            bktItem->prev = prevItem;
            ++numberOfElements;
        }
        //-- we found bucket item, just set value
        else bktItem->item = obj;
    }
} //-- put
/**
 * Removes the the specified Object from the NamedMap
 * @param key the key of the Object to remove from the NamedMap
 * @return the Object being removed
**/
MITREObject* NamedMap::remove(String& key) {

    BucketItem* bktItem = getBucketItem(key);

    if ( bktItem ) {
        bktItem->prev->next = bktItem->next;
        numberOfElements--;
        return bktItem->item;
    }
    return 0;

} //-- remove

/**
 * Sets the object deletion flag. If set to true, objects in
 * the NamedMap will be deleted upon calling the clear() method, or
 * upon destruction. By default this is false.
**/
void  NamedMap::setObjectDeletion(MBool deleteObjects) {
    doObjectDeletion = deleteObjects;
} //-- setObjectDeletion

/**
 * Returns the number of elements in the NodeStack
 * @return the number of elements in the NodeStack
**/
int NamedMap::size() {
    return numberOfElements;
} //-- size

  //-------------------/
 //- Private Methods -/
//-------------------/

NamedMap::BucketItem* NamedMap::createBucketItem(const String& key, MITREObject* objPtr)
{
    BucketItem* bktItem = new BucketItem;
    bktItem->next = 0;
    bktItem->prev = 0;
    bktItem->key  = key;
    bktItem->item = objPtr;
    return bktItem;
} //-- createBucketItem

NamedMap::BucketItem* NamedMap::getBucketItem(const String& key) {
    // compute hash for key
    long hashCode = hashKey(key);

    int idx = hashCode % numberOfBuckets;

    BucketItem* bktItem = elements[idx];

    while ( bktItem ) {
        if ( bktItem->key.isEqual(key) ) return bktItem;
        bktItem = bktItem->next;
    }

    return bktItem;

} //-- getBucketItem

/**
**/
unsigned long NamedMap::hashKey(const String& key) {

    Int32 len = key.length();
    UNICODE_CHAR* chars = new UNICODE_CHAR[len];
    key.toUnicode(chars);

    unsigned long hashCode = 0;
    for (Int32 i = 0; i < len; i++) {
        hashCode +=  ((Int32)chars[i]) << 3;
    }

    return hashCode;
} //-- hashKey


