/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_LIST_H
#define TRANSFRMX_LIST_H

#include "txCore.h"

class txListIterator;

/**
 * Represents an ordered list of Object pointers. Modeled after a Java 2 List.
**/
class txList : public txObject {

friend class txListIterator;

public:

    /**
    * Creates an empty txList
    **/
    txList();

    /**
     * txList destructor, object references will not be deleted.
    **/
    ~txList();

    /**
     * Returns the number of items in this txList
    **/
    int32_t getLength();

    /**
     * Returns true if there are no items in this txList
     */
    inline bool isEmpty()
    {
        return itemCount == 0;
    }

    /**
     * Adds the given Object to the list
    **/
    nsresult add(void* objPtr);

    /*
     * Removes all the objects from the list
     */
    void clear();

protected:

    struct ListItem {
        ListItem* nextItem;
        ListItem* prevItem;
        void* objPtr;
    };

    /**
     * Removes the given ListItem pointer from the list
    **/
    ListItem* remove(ListItem* sItem);

private:
      txList(const txList& aOther); // not implemented

      ListItem* firstItem;
      ListItem* lastItem;
      int32_t itemCount;

      nsresult insertAfter(void* objPtr, ListItem* sItem);
      nsresult insertBefore(void* objPtr, ListItem* sItem);
};



/**
 * An Iterator for the txList Class
**/
class txListIterator {

public:
    /**
     * Creates a new txListIterator for the given txList
     * @param list, the txList to create an Iterator for
    **/
    txListIterator(txList* list);

    /**
     * Adds the Object pointer to the txList pointed to by this txListIterator.
     * The Object pointer is inserted as the next item in the txList
     * based on the current position within the txList
     * @param objPtr the Object pointer to add to the list
    **/
    nsresult addAfter(void* objPtr);

    /**
     * Adds the Object pointer to the txList pointed to by this txListIterator.
     * The Object pointer is inserted as the previous item in the txList
     * based on the current position within the txList
     * @param objPtr the Object pointer to add to the list
    **/
    nsresult addBefore(void* objPtr);

    /**
     * Returns true if a successful call to the next() method can be made
     * @return true if a successful call to the next() method can be made,
     * otherwise false
    **/
    bool  hasNext();

    /**
     * Returns the next Object pointer from the list
    **/
    void* next();

    /**
     * Returns the previous Object pointer from the list
    **/
    void* previous();
    
    /**
     * Returns the current Object
    **/
    void* current();
    
    /**
     * Removes the Object last returned by the next() or previous() methods;
     * @return the removed Object pointer
    **/
    void* remove();

    /**
     * Resets the current location within the txList to the beginning of the txList
    **/
    void reset();

    /**
     * Resets the current location within the txList to the end of the txList
    **/
    void resetToEnd();

private:

   //-- points to the current list item
   txList::ListItem* currentItem;

   //-- points to the list to iterator over
   txList* list;

   //-- we've moved off the end of the list
   bool atEndOfList;
};

typedef txList List;

#endif
