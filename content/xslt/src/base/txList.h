/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Keith Visco <kvisco@ziplink.net> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    PRInt32 getLength();

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
      PRInt32 itemCount;

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
