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
 * $Id: Map.h,v 1.1 2000/04/12 10:49:40 kvisco%ziplink.net Exp $
 */

/*
 * A Hashtable for TxObjects
 * @version $Revision: 1.1 $ $Date: 2000/04/12 10:49:40 $
 */

#ifndef TRANSFRMX_MAP_H
#define TRANSFRMX_MAP_H

#include "baseutils.h"
#include "TxObject.h"
#include "List.h"

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
     * <BR />
     * You will need to delete this List when you are done with it.
    **/
    List* keys();

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
     * Removes all elements from the Map table
    **/
    void clear();

    void clear(MBool doObjectDeletion);

    /**
     * Returns true if there are no elements in this Map
     * @return true if there are no elements in this Map.
    **/
    MBool isEmpty();

    /**
     * Removes the object associated with the given key from this Map
     * @param key the TxObject used to compute the hashCode for the object to remove
     * from the Map
     * @return the removed object
    **/
    TxObject* remove(TxObject* key);

    /**
     * Sets the object deletion flag. If set to true, objects in
     * the Map will be deleted upon calling the clear() method, or
     * upon destruction. By default this is false.
    **/
    void  setObjectDeletion(MBool deleteObjects);

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

    Int32 numberOfBuckets;
    Int32 numberOfElements;
    MBool doObjectDeletion;

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
