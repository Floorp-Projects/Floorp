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
 * StringList
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

#include <iostream.h>
#include "StringList.h"

/**
 * Creates an empty list
**/
StringList::StringList() {
   firstItem  = 0;
   lastItem   = 0;
   itemCount  = 0;
} //-- StringList;

/**
 * StringList Destructor, Cleans up pointers and will delete the String
 * references, make sure you make copies of any needed Strings
*/
StringList::~StringList() {
  StringListItem* item = firstItem;
  while (item) {
    StringListItem* tItem = item;
    item = item->nextItem;
    delete tItem->strptr;
    delete tItem;
  }
} //-- ~StringList

void StringList::add(String* strptr) {
   StringListItem* sItem = new StringListItem;
   sItem->strptr = strptr;
   sItem->nextItem = 0;
   sItem->prevItem = lastItem;
   if (lastItem) lastItem->nextItem = sItem;
   lastItem = sItem;
   if (!firstItem) firstItem = sItem;

   // increase the item count
   ++itemCount;
} //-- add

MBool StringList::contains(String& search) {
    StringListItem* sItem = firstItem;
    while ( sItem ) {
        if ( search.isEqual(*sItem->strptr)) return MB_TRUE;
        sItem = sItem->nextItem;
    }
    return MB_FALSE;
} //-- contains

/**
 * Returns the number of Strings in this List
**/
Int32 StringList::getLength() {
   return itemCount;
} //-- getLength


/**
 * Inserts the given String pointer as the item just after refItem.
 * If refItem is a null pointer the String will inserted at the
 * beginning of the List (ie, insert after nothing).
 * This method assumes refItem is a member of this list, and since this
 * is a private method, I feel that's a valid assumption
**/
void StringList::insertAfter(String* strptr, StringListItem* refItem) {

   //-- if refItem == null insert at end
   if (!refItem) {
      if (firstItem) insertBefore(strptr, firstItem);
      else add(strptr);
      return;
   }

   //-- if inserting at end of list
   if (refItem == lastItem) {
       add(strptr);
       return;
   }

   //-- insert into middle of list
   StringListItem* sItem = new StringListItem;
   sItem->strptr = strptr;
   sItem->prevItem = refItem;
   sItem->nextItem = refItem->nextItem;
   refItem->nextItem = sItem;

   // increase the item count
   ++itemCount;
} //-- insertAfter

/**
 * Inserts the given String pointer as the item just before refItem.
 * If refItem is a null pointer the String will inserted at the
 * end of the List (ie, insert before nothing).
 * This method assumes refItem is a member of this list, and since this
 * is a private method, I feel that's a valid assumption
**/
void StringList::insertBefore(String* strptr, StringListItem* refItem) {

   //-- if refItem == null insert at end
   if (!refItem) {
      add(strptr);
      return;
   }

   StringListItem* sItem = new StringListItem;
   sItem->strptr = strptr;
   sItem->nextItem = refItem;
   sItem->prevItem = refItem->prevItem;
   refItem->prevItem = sItem;

   if (refItem == firstItem) firstItem = sItem;
   if (itemCount == 0) lastItem = sItem;

   // increase the item count
   ++itemCount;
} //-- insertBefore

/**
 * Returns a StringListIterator for this StringList
**/
StringListIterator& StringList::iterator() {
   return *(new StringListIterator(this));
}

String* StringList::remove(String* strptr) {
   StringListItem* sItem = firstItem;
   while (sItem) {
      if (sItem->strptr == strptr) {
         remove(sItem);
         delete sItem;
         return strptr;
      }
      sItem = sItem->nextItem;
   }
   // not in list
   return 0;
} //-- remove

StringList::StringListItem* StringList::remove(StringList::StringListItem* sItem) {
   if (sItem->prevItem) {
       sItem->prevItem->nextItem = sItem->nextItem;
   }
   if (sItem == firstItem) firstItem = sItem->nextItem;
   if (sItem == lastItem) lastItem = sItem->prevItem;
   //-- decrease Item count
   --itemCount;
   return sItem;
} //-- remove

/**
 * Removes all Strings equal to the given String from the list
 * All removed strings will be destroyed
**/
void StringList::remove(String& search) {
   StringListItem* sItem = firstItem;
   while (sItem) {
        if (sItem->strptr->isEqual(search)) {
            delete sItem->strptr;
            StringListItem* temp = remove(sItem);
            sItem = sItem->nextItem;
            delete temp;
        }
        else sItem = sItem->nextItem;
   }
} //-- remove

  //----------------------------------------/
 //- Implementation of StringListIterator -/
//----------------------------------------/


/**
 * Creates a new StringListIterator for the given StringList
**/
StringListIterator::StringListIterator(StringList* list) {
   stringList = list;
   currentItem = 0;
   allowRemove = MB_FALSE;
} //-- StringListIterator

StringListIterator::~StringListIterator() {
  //-- overrides default destructor to do nothing
} //-- ~StringListIterator

/**
 * Adds the String pointer to the StringList of this StringListIterator.
 * The String pointer is inserted as the next item in the StringList
 * based on the current position within the StringList
**/
void StringListIterator::add(String* strptr) {

   stringList->insertAfter(strptr,currentItem);
   allowRemove = MB_FALSE;

} //-- add

/**
 * Returns true if a sucessful call to the next() method can be made
**/
MBool StringListIterator::hasNext() {
   if (currentItem) {
      return (MBool)(currentItem->nextItem);
   }
   return (MBool)(stringList->firstItem);
} //-- hasNext

/**
 * Returns true if a successful call to the previous() method can be made
**/
MBool StringListIterator::hasPrevious() {
   if (currentItem) {
      return (MBool)(currentItem->prevItem);
   }
   return MB_FALSE;
} //-- hasPrevious

/**
 * Returns the next String in the list
**/
String* StringListIterator::next() {

   if (currentItem) {
      if (currentItem->nextItem) {
         currentItem = currentItem->nextItem;
         allowRemove = MB_TRUE;
         return currentItem->strptr;
      }
   }
   else {
      currentItem = stringList->firstItem;
      allowRemove = MB_TRUE;
      return currentItem->strptr;
   }
   return 0;
} //-- next

/**
 * Returns the previous String in the list
**/
String* StringListIterator::previous() {

   if (currentItem) {
      if (currentItem->prevItem) {
         currentItem = currentItem->prevItem;
         allowRemove = MB_TRUE;
         return currentItem->strptr;
      }
   }

   return 0;
}
//-- prev

/**
 * Removes the String last return by the next() or previous();
 * The removed String* is returned
**/
String* StringListIterator::remove() {

   if (allowRemove == MB_FALSE) return 0;

   allowRemove = MB_FALSE;

   StringList::StringListItem* sItem = 0;
   if (currentItem) {
      // Make previous Item the current Item or null
      sItem = currentItem;
      if (stringList->firstItem == sItem) currentItem = 0;
      stringList->remove(sItem);
      return sItem->strptr;
   }
   return 0;
} //-- remove

/**
 * Resets the current location within the StringList to the beginning
**/
void StringListIterator::reset() {
   currentItem = 0;
} //-- reset
