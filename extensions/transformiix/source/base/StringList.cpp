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
 * Bob Miller, kbob@oblix.com
 *    -- plugged core leak.
 *
 */

/**
 * StringList
**/

#ifdef TX_EXE
#include <iostream.h>
#endif
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
    if (sItem) {
        sItem->strptr = strptr;
        sItem->nextItem = 0;
        sItem->prevItem = lastItem;
    }
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
PRInt32 StringList::getLength() {
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
    if (sItem) {
        sItem->strptr = strptr;
        sItem->prevItem = refItem;
        sItem->nextItem = refItem->nextItem;
        refItem->nextItem = sItem;

        // increase the item count
        ++itemCount;
    }
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
    if (sItem) {
        sItem->strptr = strptr;
        sItem->nextItem = refItem;
        sItem->prevItem = refItem->prevItem;
        refItem->prevItem = sItem;
    }

    if (refItem == firstItem) firstItem = sItem;
    if (itemCount == 0) lastItem = sItem;

    // increase the item count
    ++itemCount;
} //-- insertBefore

/**
 * Returns a StringListIterator for this StringList, this iterator
 * will need to be deleted by the caller.
**/
StringListIterator* StringList::iterator() {
    return new StringListIterator(this);
} //-- iterator

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
        return (currentItem->nextItem != 0);
    }
    return (stringList->firstItem != 0);
} //-- hasNext

/**
 * Returns true if a successful call to the previous() method can be made
**/
MBool StringListIterator::hasPrevious() {
    if (currentItem) {
        return (currentItem->prevItem != 0);
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
        if (currentItem)
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
