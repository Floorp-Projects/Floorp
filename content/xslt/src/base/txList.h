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
 * $Id: txList.h,v 1.3 2005/11/02 07:33:44 Peter.VanderBeken%pandora.be Exp $
 */

#include "baseutils.h"

#ifndef MITRE_LIST_H
#define MITRE_LIST_H

/**
 * Represents an ordered list of Object pointers. Modeled after a Java 2 List.
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.3 $ $Date: 2005/11/02 07:33:44 $
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
   ListItem* currentItem;

   //-- points to the list to iterator over
   List* list;

   //-- determins if we can remove the current item from the list
   MBool allowRemove;

   MBool done;

   MBool moveForward;
};

#endif
