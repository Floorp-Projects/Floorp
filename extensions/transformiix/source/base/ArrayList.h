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
 * The Initial Developer of the Original Code is Keith Visco
 * Portions created by Keith Visco are
 * Copyright (C) 1999, 2000 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */

/**
 * ArrayList is a simple array which will grow automatically, similar to
 * the Vector class in that other more popular object oriented programming language.
**/

#ifndef TRANSFRMX_ARRAYLIST_H
#define TRANSFRMX_ARRAYLIST_H

#include "TxObject.h"
#include "baseutils.h"

class ArrayList {


public:

      //----------------/
     //- Constructors -/
    //----------------/

    /**
     * Creates a new ArrayList with the default Size
    **/
    ArrayList();

    /**
     * Creates a new ArrayList with the specified Size
    **/
    ArrayList(int size);

    /**
     * Destructor for ArrayList, will not delete TxObject References
     * by default
    **/
    virtual ~ArrayList();

    /**
     * Adds the specified TxObject to this ArrayList
     * @param object the TxObject to add to the ArrayList
    **/
    void add(TxObject* object);

    /**
     * Adds the given TxObject to this ArrayList at the specified index.
     * @param object the TxObject to add
     * @return true if the object has been properly added at the correct index.
    **/
    MBool add(int index, TxObject* object);

    /**
     * Removes all elements from the list
    **/
    void clear();

    /**
     * Removes all elements from the list
     * @param deleteObjects allows specifying whether or not to delete the TxObjects
     * that are currently in the list
    **/
    void clear(MBool deleteObjects);

    /**
     * Returns true if the specified TxObject is contained in the list.
     * @param object the TxObject to search for
     * @return true if specified object is contained in the list
    **/
    MBool contains(TxObject* object);

    /**
     * Copies the elements of this ArrayList, into the destination ArrayList
    **/
    void copyInto(ArrayList& dest) const;


    /**
     * Returns the TxObject at the specified position in this ArrayList.
     * @param index the position of the object to return, if the index
     * is out-of-bounds, 0 will be returned.
    **/
    TxObject* get(int index);


    /**
     * Returns the index of the specified object,
     * or -1 if the object is not contained in the ArrayList
     * @param object the TxObject to get the index of
    **/
    int indexOf(TxObject* object);

    /**
     * Removes the TxObject at the specified index
     * @param index the position of the TxObject to remove
     * @return the TxObject that was removed from the list
    **/
    TxObject* remove(int index);

    /**
     * Removes the the specified TxObject from the list
     * @param object the TxObject to remove from the list
     * @return true if the object was removed from the list
    **/
    MBool remove(TxObject* object);


    /**
     * Returns the number of elements in the list
     * @return the number of elements in the list
    **/
    int size() const;

private:


      //-------------------/
     //- Private Members -/
    //-------------------/

    static const int DEFAULT_SIZE;

    TxObject** elements;

    int initialSize;
    int bufferSize;

    /**
     * The next available location in the elements array
    **/
    int elementCount;

      //-------------------/
     //- Private Methods -/
    //-------------------/

    /**
     * Helper method for constructors
    **/
    void initialize(int size);

    /**
     * increase the NodeSet capacity by a factor of its initial size
    **/
    void increaseSize();

    /**
     * Shifts all elements at the specified index to down by 1
    **/
    void shiftDown(int index);

    /**
     * Shifts all elements at the specified index up by 1
    **/
    void shiftUp(int index);

}; //-- ArrayList

#endif
