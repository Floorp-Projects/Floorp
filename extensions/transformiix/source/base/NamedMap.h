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
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

#ifndef MITREXSL_NAMEDMAP_H
#define MITREXSL_NAMEDMAP_H

#include "String.h"
#include "baseutils.h"
#include "MITREObject.h"

class NamedMap : public MITREObject {


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
     *  Returns the object reference in this Map associated with the given name
     * @return the object reference in this Map associated with the given name
    **/
    MITREObject* get(const String& name);

    /**
     *  Returns the object reference in this Map associated with the given name
     * @return the object reference in this Map associated with the given name
    **/
    MITREObject* get(const char* name);

    /**
     *  Adds the Object reference to the map and associates it with the given name
    **/
    void  put(const String& name, MITREObject* obj);

    /**
     *  Adds the Object reference to the map and associates it with the given name
    **/
    void  put(const char* name, MITREObject* obj);

    /**
     * Removes all elements from the Map table
    **/
    void clear();

    void clear(MBool doObjectDeletion);

    /**
     * Returns true if the specified Node is contained in the set.
     * if the specfied Node is null, then if the NodeSet contains a null
     * value, true will be returned.
     * @param node the element to search the NodeSet for
     * @return true if specified Node is contained in the NodeSet
    **/
    //MBool contains(Node* node);

    /**
     * Compares the specified object with this NamedMap for equality.
     * Returns true if and only if the specified Object is a NamedMap
     * that hashes to the same value as this NamedMap
     * @return true if and only if the specified Object is a NamedMap
     * that hashes to the same value as this NamedMap
    **/
    MBool equals(NamedMap* namedMap);

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
    MITREObject* remove(String& key);

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
        MITREObject* item;
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

    BucketItem* createBucketItem(const String& key, MITREObject* objPtr);

    BucketItem* getBucketItem(const String& key);

    unsigned long hashKey(const String& key);

    /**
     * Helper method for constructors
    **/
    void initialize(int size);

}; //-- NamedMap

#endif
