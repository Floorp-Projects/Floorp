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

#include "List.h"
#include <iostream.h>
  //--------------------------/
 //- Implementation of List -/
//--------------------------/

/**
 * Default constructor for a List;
**/
List::List() {
   firstItem  = 0;
   lastItem   = 0;
   itemCount  = 0;
} //-- List;

/**
 * List destructor, cleans up List Items, but will not delete the Object
 * references
*/
List::~List() {
  ListItem* item = firstItem;
  while (item) {
    ListItem* tItem = item;
    item = item->nextItem;
    delete tItem;
  }
} //-- ~List

void List::insert(int index, void* objPtr) {

    if ( index >= itemCount ) {
        insertBefore(objPtr, 0);
    }
    else {
        //-- add to middle of list
        ListItem* nextItem = firstItem;
        for ( int i = 0; i < index; i++ ) nextItem = nextItem->nextItem;
        insertBefore(objPtr, nextItem);
    }
} //-- insert

void List::add(void* objPtr) {
    insert(itemCount, objPtr);
} //-- add

List::ListItem* List::getFirstItem() {
    return firstItem;
} //-- getFirstItem

List::ListItem* List::getLastItem() {
    return lastItem;
} //-- getLastItem

/**
 * Returns the number of items in this List
**/
Int32 List::getLength() {
   return itemCount;
} //-- getLength


/**
 * Inserts the given Object pointer as the item just after refItem.
 * If refItem is a null pointer the Object will be inserted at the
 * beginning of the List (ie, insert after nothing).
 * This method assumes refItem is a member of this list, and since this
 * is a private method, I feel that's a valid assumption
**/
void List::insertAfter(void* objPtr, ListItem* refItem) {
    //-- if refItem == null insert at front
    if (!refItem) insertBefore(objPtr, firstItem);
    else insertBefore(objPtr, refItem->nextItem);
} //-- insertAfter

/**
 * Inserts the given Object pointer as the item just before refItem.
 * If refItem is a null pointer the Object will be inserted at the
 * end of the List (ie, insert before nothing).
 * This method assumes refItem is a member of this list, and since this
 * is a private method, I feel that's a valid assumption
**/
void List::insertBefore(void* objPtr, ListItem* refItem) {

    ListItem* item = new ListItem;
    item->objPtr = objPtr;
    item->nextItem = 0;
    item->prevItem = 0;

    //-- if refItem == null insert at end
    if (!refItem) {
        //-- add to back of list
        if ( lastItem ) {
            lastItem->nextItem = item;
            item->prevItem = lastItem;
        }
        lastItem = item;
        if ( !firstItem ) firstItem = item;
    }
    else {
        //-- insert before given item
        item->nextItem = refItem;
        item->prevItem = refItem->prevItem;
        refItem->prevItem = item;

        if (refItem == firstItem) firstItem = item;
        if (itemCount == 0) lastItem = item;  // <-should we ever see this?
    }

    // increase the item count
    ++itemCount;
} //-- insertBefore

/**
 * Returns a ListIterator for this List
**/
ListIterator* List::iterator() {
   return new ListIterator(this);
}

void* List::remove(void* objPtr) {
   ListItem* item = firstItem;
   while (item) {
      if (item->objPtr == objPtr) {
         remove(item);
         delete item;
         return objPtr;
      }
      item = item->nextItem;
   }
   // not in list
   return 0;
} //-- remove

List::ListItem* List::remove(ListItem* item) {

    if ( !item ) return item;

    //-- adjust the previous item's next pointer
    if (item->prevItem) {
        item->prevItem->nextItem = item->nextItem;
    }
    //-- adjust the next item's previous pointer
    if ( item->nextItem ) {
        item->nextItem->prevItem = item->prevItem;
    }

    //-- adjust first and last items
    if (item == firstItem) firstItem = item->nextItem;
    if (item == lastItem) lastItem = item->prevItem;

    //-- decrease Item count
    --itemCount;
    return item;
} //-- remove

  //----------------------------------/
 //- Implementation of ListIterator -/
//----------------------------------/


/**
 * Creates a new ListIterator for the given List
 * @param list, the List to create an Iterator for
**/
ListIterator::ListIterator(List* list) {
   this->list   = list;
   currentItem  = 0;
   allowRemove  = MB_FALSE;
   moveForward  = MB_TRUE;
   done         = MB_FALSE;
   count = 0;
} //-- ListIterator

ListIterator::~ListIterator() {
  //-- overrides default destructor to do nothing
} //-- ~ListIterator

/**
 * Adds the Object pointer to the List pointed to by this ListIterator.
 * The Object pointer is inserted as the next item in the List
 * based on the current position within the List
 * @param objPtr the Object pointer to add to the list
**/
void ListIterator::add(void* objPtr) {

   list->insertAfter(objPtr,currentItem);
   allowRemove = MB_FALSE;

} //-- add

/**
 * Returns true if a sucessful call to the next() method can be made
 * @return MB_TRUE if a sucessful call to the next() method can be made,
 * otherwise MB_FALSE
**/
MBool ListIterator::hasNext() {
    MBool hasNext = MB_FALSE;
    if ( done ) return hasNext;
    else if ( currentItem ) {
        if (moveForward) hasNext = (MBool) currentItem->nextItem;
        else hasNext = (MBool)currentItem->prevItem;
    }
    else {
        if (moveForward) hasNext = (MBool) list->firstItem;
        else hasNext = (MBool) list->lastItem;
    }
    return hasNext;
} //-- hasNext

/**
 * Returns true if a sucessful call to the previous() method can be made
 * @return MB_TRUE if a sucessful call to the previous() method can be made,
 * otherwise MB_FALSE
**/
MBool ListIterator::hasPrevious() {
   MBool hasPrevious = MB_FALSE;
   if (currentItem) {
        if (moveForward) hasPrevious = (MBool)(currentItem->prevItem);
        else hasPrevious = (MBool) (currentItem->nextItem);
   }
   return hasPrevious;
} //-- hasPrevious

/**
 * Returns the next Object pointer in the list
**/
void* ListIterator::next() {

    void* obj = 0;
    if ( done ) return obj;

    if (currentItem) {
        if ( moveForward ) currentItem = currentItem->nextItem;
        else currentItem = currentItem->prevItem;
    }
    else {
        if ( moveForward ) currentItem = list->firstItem;
        else currentItem = list->lastItem;
    }

    if ( currentItem ) {
        obj = currentItem->objPtr;
        allowRemove = MB_TRUE;
    }
    else done = MB_TRUE;

    return obj;
} //-- next

/**
 * Returns the previous Object in the list
**/
void* ListIterator::previous() {

    void* obj = 0;

    if (currentItem) {
        if ( moveForward ) currentItem = currentItem->prevItem;
        else currentItem = currentItem->nextItem;
        if ( currentItem ) obj = currentItem->objPtr;
    }
    return obj;
} //-- previous

/**
 * Removes the Object last returned by the next() or previous() methods;
 * @return the removed Object pointer
**/
void* ListIterator::remove() {

    if (!allowRemove) return 0;
    allowRemove = MB_FALSE;

    void* obj = 0;
    if (currentItem) {
        obj = currentItem->objPtr;
        List::ListItem* item = currentItem;
        previous(); //-- make previous item the current item
        list->remove(item);
    }
    return obj;
} //-- remove

/**
 * Resets the current location within the List to the beginning of the List
**/
void ListIterator::reset() {
   done = MB_FALSE;
   currentItem = 0;
} //-- reset

/**
 * sets this iterator to operate in the reverse direction
**/
void ListIterator::reverse() {
    moveForward = (MBool)(!moveForward);
} //-- reverse

