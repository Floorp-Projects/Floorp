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

#include "baseutils.h"

#ifndef MITRE_LIST_H
#define MITRE_LIST_H

/**
 * Represents an ordered list of Object pointers. Modeled after a Java 2 List.
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
class List {

friend class ListIterator;

public:

    /**
    * Creates an empty List
    **/
    List();

    /**
     * List destructor, object references will not be deleted.
    **/
    virtual ~List();

    /**
     * Returns the number of items in this List
    **/
    Int32 getLength();

    /**
     * Returns a ListIterator for this List
    **/
    ListIterator* iterator();

    /**
     * Adds the given Object to the specified position in the list
    **/
    void insert(int index, void* objPtr);

    /**
     * Adds the given Object to the list
    **/
    void add(void* objPtr);

    /**
     * Removes the given Object pointer from the list
    **/
    void* remove(void* objPtr);


protected:

    struct ListItem {
        ListItem* nextItem;
        ListItem* prevItem;
        void* objPtr;
    };

    ListItem* getFirstItem();
    ListItem* getLastItem();

    /**
     * Removes the given ListItem pointer from the list
    **/
    ListItem* remove(ListItem* sItem);

private:

      ListItem* firstItem;
      ListItem* lastItem;
      Int32 itemCount;

      void insertAfter(void* objPtr, ListItem* sItem);
      void insertBefore(void* objPtr, ListItem* sItem);

};



/**
 * An Iterator for the List Class
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
class ListIterator {

public:


    /**
     * Creates a new ListIterator for the given List
     * @param list, the List to create an Iterator for
    **/
    ListIterator(List* list);

    /**
     * Destructor, destroys a given instance of a ListIterator
    **/
    virtual ~ListIterator();

    /**
     * Adds the Object pointer to the List pointed to by this ListIterator.
     * The Object pointer is inserted as the next item in the List
     * based on the current position within the List
     * @param objPtr the Object pointer to add to the list
    **/

    virtual void add(void* objPtr);

    /**
     * Returns true if a sucessful call to the next() method can be made
     * @return MB_TRUE if a sucessful call to the next() method can be made,
     * otherwise MB_FALSE
    **/
    virtual MBool  hasNext();

    /**
     * Returns true if a sucessful call to the previous() method can be made
     * @return MB_TRUE if a sucessful call to the previous() method can be made,
     * otherwise MB_FALSE
    **/
   virtual MBool  hasPrevious();

    /**
     * Returns the next Object pointer from the list
    **/
    virtual void* next();

    /**
     * Returns the previous Object pointer from the list
    **/
    virtual void* previous();

    /**
     * Removes the Object last returned by the next() or previous() methods;
     * @return the removed Object pointer
    **/
    virtual void* remove();

    /**
     * Resets the current location within the List to the beginning of the List
    **/
    virtual void reset();

    /**
     * sets this iterator to operate in the reverse direction
    **/
    void reverse();

private:

   int count;
   //-- points to the current list item
   List::ListItem* currentItem;

   //-- points to the list to iterator over
   List* list;

   //-- determins if we can remove the current item from the list
   MBool allowRemove;

   MBool done;

   MBool moveForward;
};

#endif
