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
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */

/**
 * A Named Map for TxObjects
**/

#ifndef TRANSFRMX_NAMEDMAP_H
#define TRANSFRMX_NAMEDMAP_H

#include "baseutils.h"
#include "TxObject.h"
#include "StringList.h"

class NamedMap : public TxObject {


public:

      //----------------/
     //- Constructors -/
    //----------------/

    /**
     * Creates a new NodeStack with the default Size
    **/
    NamedMap();

    /**
     * Creates a new NodeStack with the specified number of buckets
    **/
    NamedMap(int size);

    /**
     * Destructor for a NamedMap table, will not delete references unless
     * The setObjectDeletion flag has been set to MB_TRUE
    **/
    virtual ~NamedMap();


    /**
     * Returns a list of all the keys of this NamedMap.
     *
     * You will need to delete this List when you are done with it.
    **/
    StringList* keys();

    /**
     *  Returns the object reference in this Map associated with the given name
     * @return the object reference in this Map associated with the given name
    **/
    TxObject* get(const String& name);

    /**
     *  Returns the object reference in this Map associated with the given name
     * @return the object reference in this Map associated with the given name
    **/
    TxObject* get(const char* name);

    /**
     *  Adds the Object reference to the map and associates it with the given name
    **/
    void  put(const String& name, TxObject* obj);

    /**
     *  Adds the Object reference to the map and associates it with the given name
    **/
    void  put(const char* name, TxObject* obj);

    /**
     * Removes all elements from the Map table
    **/
    void clear();

    void clear(MBool doObjectDeletion);

    /**
     * Returns true if there are no Nodes in the NodeSet.
     * @return true if there are no Nodes in the NodeSet.
    **/
    MBool isEmpty();

    /**
     * Removes the Node at the specified index from the NodeSet
     * @param index the position in the NodeSet to remove the Node from
     * @return the Node that was removed from the list
    **/
    TxObject* remove(const String& key);

    /**
     * Sets the object deletion flag. If set to true, objects in
     * the NamedMap will be deleted upon calling the clear() method, or
     * upon destruction. By default this is false.
    **/
    void  setObjectDeletion(MBool deleteObjects);

    /**
     * Returns the number of key-element pairs in the NamedMap
     * @return the number of key-element in the NamedMap
    **/
    int size();

    void dumpMap();




      //-------------------/
     //- Private Members -/
    //-------------------/


private:

    struct BucketItem {
        String key;
        TxObject* item;
        BucketItem* next;
        BucketItem* prev;
    };

    static const int DEFAULT_SIZE;

    // map table
    BucketItem** elements;

    PRInt32 numberOfBuckets;
    PRInt32 numberOfElements;
    MBool doObjectDeletion;

      //-------------------/
     //- Private Methods -/
    //-------------------/

    BucketItem* createBucketItem(const String& key, TxObject* objPtr);

    BucketItem* getBucketItem(const String& key);

    unsigned long hashKey(const String& key);

    /**
     * Helper method for constructors
    **/
    void initialize(int size);

}; //-- NamedMap

#endif

