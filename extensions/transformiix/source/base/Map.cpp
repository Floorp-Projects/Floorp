/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is XSL:P XSLT processor.
 *
 * The Initial Developer of the Original Code is Keith Visco.
 *
 * Portions created by Keith Visco (C) 1999-2000 Keith Visco.
 * All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */

/*
 * A Hashtable for TxObjects
 */


#include "Map.h"

  //-------------/
 //- Constants -/
//-------------/

const int Map::DEFAULT_SIZE = 13;

  //----------------/
 //- Constructors -/
//----------------/

/**
 * Creates a new Map with the default Size
**/
Map::Map() {
    initialize(DEFAULT_SIZE);
} //-- Map

/**
 * Creates a new Map with the specified number of buckets
**/
Map::Map(int size) {
    initialize(size);
} //-- Map

/**
 * Helper method for Constructors
**/
void Map::initialize(PRInt32 size) {

    //-- by default the Map will not delete it's
    //-- object references
    mOwnership = eOwnsNone;

    //-- create a new array of bucket pointers
    elements = new BucketItem*[size];

    //-- initialize all elements to 0;
    for ( PRInt32 i = 0; i < size; i++ ) elements[i] = 0;

    numberOfBuckets = size;
    numberOfElements = 0;
} //-- initialize

/**
 * Destructor for Map
**/
Map::~Map() {
    clear();
    delete [] elements;
} //-- ~Map



/**
 * Removes all elements from the Map. Deletes objects according
 * to the mOwnership attribute
**/
void Map::clear() {

    for (int i = 0; i < numberOfBuckets; i++) {

        BucketItem* bktItem = elements[i];
        while (bktItem) {
            BucketItem* tItem = bktItem;
            bktItem = bktItem->next;
            if (mOwnership & eOwnsItems)
                delete tItem->item;
            if (mOwnership & eOwnsKeys)
                delete tItem->key;
            //--delete tItem;
            delete tItem;
        }
    }
    numberOfElements = 0;
} //-- clear

/**
 * Returns the object reference in this Map associated with the given key
 * @return the object reference in this Map associated with the given key
**/
TxObject* Map::get(TxObject* key) {
    BucketItem* item = getBucketItem(key);
    if ( item ) return item->item;
    return 0;
} //-- get

/**
 * Returns true if there are no objects in this map.
 * @return true if there are no objects in this map.
**/
MBool Map::isEmpty() {
    return (numberOfElements == 0) ? MB_TRUE : MB_FALSE;
} //-- isEmpty


/**
 * Returns a List of all the keys in this Map.
 * Please delete this List when you are done with it
**/
List* Map::keys() {
    List* list = new List();
    for (int i = 0; i < numberOfBuckets; i++) {
        BucketItem* item = elements[i];
        while (item) {
            list->add(item->key);
            item = item->next;
        }
    }
    return list;
} //-- keys

/**
 * Adds the TxObject reference to the map and associates it with the given
 * key
**/
void  Map::put(TxObject* key, TxObject* obj) {

    if ((!key) || (!obj)) return;

    //-- compute hash for key
    PRInt32 hashCode = key->hashCode();

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
            if ( bktItem->key->equals(key) ) {
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
 * Removes the the specified TxObject from the Map
 * @param key the TxObject which is used to calculate the hashCode of
 * the TxObject to remove from the Map
 * @return the TxObject removed from the Map
**/
TxObject* Map::remove(TxObject* key) {

    if (!key) return 0;

    // compute hash for key
    PRInt32 hashCode = key->hashCode();

    int idx = hashCode % numberOfBuckets;

    BucketItem* bktItem = elements[idx];

    while ( bktItem ) {
        if ( bktItem->key->equals(key) ) break;
        bktItem = bktItem->next;
    }

    if ( bktItem ) {
        if (bktItem == elements[idx]) elements[idx] = bktItem->next;
        else bktItem->prev->next = bktItem->next;
        numberOfElements--;
        TxObject* txObject = bktItem->item;
        bktItem->item = 0;
        delete bktItem;
        return txObject;
    }
    return 0;

} //-- remove

/**
 * Sets the ownership flag.
**/
void  Map::setOwnership(txMapOwnership aOwnership) {
    mOwnership = aOwnership;
} //-- setOwnership

/**
 * Returns the number of key-object pairs in the Map
 * @return the number of key-object pairs in the Map
**/
int Map::size() {
    return numberOfElements;
} //-- size

  //-------------------/
 //- Private Methods -/
//-------------------/

Map::BucketItem* Map::createBucketItem(TxObject* key, TxObject* obj)
{
    BucketItem* bktItem = new BucketItem;
    bktItem->next = 0;
    bktItem->prev = 0;
    bktItem->key  = key;
    bktItem->item = obj;
    return bktItem;
} //-- createBucketItem

Map::BucketItem* Map::getBucketItem(TxObject* key) {

    // compute hash for key
    PRInt32 hashCode = key->hashCode();

    int idx = hashCode % numberOfBuckets;

    BucketItem* bktItem = elements[idx];

    while ( bktItem ) {
        if ( bktItem->key->equals(key) ) return bktItem;
        bktItem = bktItem->next;
    }

    return bktItem;

} //-- getBucketItem
