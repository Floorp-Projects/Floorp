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
 * Jonas Sicking, sicking@bigfoot.com
 *    -- Cleanup/bugfix/features in Iterator
 *       Added tx prefix to classnames
 *
 */

#ifndef TRANSFRMX_LIST_H
#define TRANSFRMX_LIST_H

#include "baseutils.h"
#include "TxObject.h"

/**
 * Represents an ordered list of Object pointers. Modeled after a Java 2 List.
**/
class txList : public TxObject {

friend class txListIterator;

public:

    /**
    * Creates an empty txList
    **/
    txList();

    /**
     * txList destructor, object references will not be deleted.
    **/
    virtual ~txList();

    /**
     * Returns the object located at the given index. This may
     * be slow or fast depending on the implementation.
     * @return the object located at the given index
    **/
    void* get(int index);

    /**
     * Returns the number of items in this txList
    **/
    PRInt32 getLength();

    /**
     * Returns a txListIterator for this txList
    **/
    txListIterator* iterator();

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
      PRInt32 itemCount;

      void insertAfter(void* objPtr, ListItem* sItem);
      void insertBefore(void* objPtr, ListItem* sItem);

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
     * Destructor, destroys a given instance of a txListIterator
    **/
    virtual ~txListIterator();

    /**
     * Adds the Object pointer to the txList pointed to by this txListIterator.
     * The Object pointer is inserted as the next item in the txList
     * based on the current position within the txList
     * @param objPtr the Object pointer to add to the list
    **/

    virtual void addAfter(void* objPtr);

    /**
     * Adds the Object pointer to the txList pointed to by this txListIterator.
     * The Object pointer is inserted as the previous item in the txList
     * based on the current position within the txList
     * @param objPtr the Object pointer to add to the list
    **/

    virtual void addBefore(void* objPtr);

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
     * Returns the current Object
    **/
    virtual void* current();
    
    /**
     * Moves the specified number of steps
    **/
    virtual void* advance(int i);

    /**
     * Removes the Object last returned by the next() or previous() methods;
     * @return the removed Object pointer
    **/
    virtual void* remove();

    /**
     * Resets the current location within the txList to the beginning of the txList
    **/
    virtual void reset();

    /**
     * Resets the current location within the txList to the end of the txList
    **/
    virtual void resetToEnd();

private:

   //-- points to the current list item
   txList::ListItem* currentItem;

   //-- points to the list to iterator over
   txList* list;

   //-- we've moved off the end of the list
   MBool atEndOfList;
};

typedef txList List;
typedef txListIterator ListIterator;

#endif
