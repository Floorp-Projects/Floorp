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

// Tom Kneeland (01/21/2000)
//
//  Implementation of a simple Hash Table.  All objects are stored as 
//  MITREObject pointers and hashed using the provided value.  Collisions
//  are solved using a linked list for each hash bin.
//
// Modification History:
// Who  When        What

#include "HashTable.h"
#include <iostream.h>

HashTable::HashTable()
{
    for (Int32 binIndex=0; binIndex < HASHTABLE_SIZE; binIndex++)
        table[binIndex] = NULL;
}

//
//Destroy the hash table.  Loop through all the bins, deleting all the objects
//and list items.
//
HashTable::~HashTable()
{
    Int32 binIndex = 0;
    HashItem* currentItem = NULL;
    HashItem* nextItem = NULL;

    for (binIndex=0;binIndex<HASHTABLE_SIZE;binIndex++)
    {
        currentItem = table[binIndex];
        while (currentItem != NULL)
        {
            nextItem = currentItem->nextItem;
            delete currentItem->objPtr;
            delete currentItem;
            currentItem = nextItem;
        }
    }
}

//
//Compute the bin from the Hash value, and store the object in the table
//
#ifdef MOZ_XSL
void HashTable::add(MITREObject* obj, void* hashValue)
#else
void HashTable::add(MITREObject* obj, Int32 hashValue)
#endif
{
    Int32 bin = (Int32)hashValue % HASHTABLE_SIZE;
    HashItem* newHashItem = new HashItem;
    HashItem* existingItem = NULL;

    //Create a new Hash Object to hold the MITREObject being inserted
    newHashItem->objPtr = obj;
    newHashItem->hashValue = hashValue;
    newHashItem->prevItem = NULL;
    newHashItem->nextItem = NULL;

    //Addition is simple if there is nothing in the bin
    if (table[bin] == NULL)
        table[bin] = newHashItem;
    else
    {
        //The bin has at least one item in it already, so check the list to
        //make sure the item doesn't already exit.  If it does, replace it,
        //else just prepend a new item.
        if ((existingItem = retrieveHashItem(hashValue)) != NULL)
        {
            delete existingItem->objPtr;
            existingItem->objPtr = obj;
        }
        else
        {
            newHashItem->nextItem = table[bin];
            table[bin]->prevItem = newHashItem;
            table[bin] = newHashItem;
        }
    }
}

//
//Locate the object specified by the hashValue
//
#ifdef MOZ_XSL
MITREObject* HashTable::retrieve(void* hashValue)
#else
MITREObject* HashTable::retrieve(Int32 hashValue)
#endif
{
    HashItem* searchValue = retrieveHashItem(hashValue);

    if (searchValue)
        return searchValue->objPtr;
    else
        return NULL;
}

//
//Locate the object specified by the hashValue, remove it from the table
//and return the orriginal object to the caller
//
#ifdef MOZ_XSL
MITREObject* HashTable::remove(void* hashValue)
#else
MITREObject* HashTable::remove(Int32 hashValue)
#endif
{
    Int32 bin = (Int32)hashValue % HASHTABLE_SIZE;
    MITREObject* returnObj = NULL;
    HashItem* searchItem = table[bin];

    while (1)
    {
        if (searchItem == NULL)
            return NULL;

        if (searchItem->hashValue == hashValue)
        {
            returnObj = searchItem->objPtr;

            if (searchItem->prevItem == NULL)
                table[bin] = searchItem->nextItem;
            else
                searchItem->prevItem->nextItem = searchItem->nextItem;

            if (searchItem->nextItem != NULL)
                searchItem->nextItem->prevItem = searchItem->prevItem;

            delete searchItem;
            return returnObj;
        }
        
        searchItem = searchItem->nextItem;
    }
}

#ifdef MOZ_XSL
HashTable::HashItem* HashTable::retrieveHashItem(void* hashValue)
#else
HashTable::HashItem* HashTable::retrieveHashItem(Int32 hashValue)
#endif
{
    Int32 bin = (Int32)hashValue % HASHTABLE_SIZE;
    HashItem* searchItem = NULL;

    //Goto the calculated bin, and begin a linear search for the specified
    //value.
    searchItem = table[bin];
    while (1)
    {      
        if (searchItem == NULL)
            return NULL;

        if (searchItem->hashValue == hashValue)
            return searchItem;
        
        searchItem = searchItem->nextItem;
    }
}
