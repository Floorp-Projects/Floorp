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

#ifndef TRANSFRMX_MAP_H
#define TRANSFRMX_MAP_H

#include "baseutils.h"
#include "TxObject.h"

class txList;

class Map : public TxObject {


public:

      //----------------/
     //- Constructors -/
    //----------------/

    /**
     * Creates a new Map with the default Size
    **/
    Map();

    /**
     * Creates a new Map with the specified number of buckets
    **/
    Map(int size);

    /**
     * Destructor for a Map table, will not delete references unless
     * The setObjectDeletion flag has been set to MB_TRUE
    **/
    virtual ~Map();


    /**
     * Returns a list of all the keys of this Map.
     *
     * You will need to delete this List when you are done with it.
    **/
    txList* keys();

    /**
     *  Returns the object reference in this Map associated with the given name
     * @return the object reference in this Map associated with the given name
    **/
    TxObject* get(TxObject* key);

    /**
     * Adds the TxObject reference to the map and associates it with the given 
     * key
    **/
    void  put(TxObject* key, TxObject* obj);

    /**
     * enum used when setting ownership
    **/
    enum txMapOwnership
    {
        eOwnsNone         = 0x00,
        eOwnsItems        = 0x01,
        eOwnsKeys         = 0x02,
        eOwnsKeysAndItems = eOwnsItems | eOwnsKeys
    };

    /**
     * Removes all elements from the Map. Deletes objects according
     * to the mOwnership attribute
    **/
    void clear();

    /**
     * Returns true if there are no elements in this Map
     * @return true if there are no elements in this Map.
    **/
    MBool isEmpty();

    // THIS IS DEPRECATED
    TxObject* remove(TxObject* key);

    // THIS IS DEPRECATED, use setOwnership
    void  setObjectDeletion(MBool deleteObjects)
    {
        setOwnership(deleteObjects ? eOwnsKeysAndItems : eOwnsNone );
    }

    /**
     * Sets the ownership attribute.
    **/
    void setOwnership(txMapOwnership aOwnership);

    /**
     * Returns the number of key-element pairs in the Map
     * @return the number of key-element in the Map
    **/
    int size();

      //-------------------/
     //- Private Members -/
    //-------------------/


private:

    struct BucketItem {
        TxObject*  key;
        TxObject* item;
        BucketItem* next;
        BucketItem* prev;
    };

    static const int DEFAULT_SIZE;

    // map table
    BucketItem** elements;

    PRInt32 numberOfBuckets;
    PRInt32 numberOfElements;

    /**
     * The ownership flag. Used to decide which objects are
     * owned by the map and thus should be deleted when released
    **/
    txMapOwnership mOwnership;

      //-------------------/
     //- Private Methods -/
    //-------------------/

    BucketItem* createBucketItem(TxObject* key, TxObject* objPtr);

    BucketItem* getBucketItem(TxObject* key);

    /**
     * Helper method for constructors
    **/
    void initialize(int size);

}; //-- NamedMap

#endif
