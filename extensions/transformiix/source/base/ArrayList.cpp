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

#include "ArrayList.h"

/*
 Implementation of ArrayList
*/


  //-------------/
 //- Constants -/
//-------------/
const int ArrayList::DEFAULT_SIZE = 17;


  //----------------/
 //- Constructors -/
//----------------/

/**
 * Creates a new ArrayList with the default Size
**/
ArrayList::ArrayList() {
    initialize(DEFAULT_SIZE);
} //-- ArrayList

/**
 * Creates a new ArrayList with the given Size
 * @param size the size the list should be initialized to
**/
ArrayList::ArrayList(int size) {
    initialize(size);
} //-- ArrayList


/**
 * Helper method for Constructors
**/
void ArrayList::initialize(int size) {
    elements = new TxObject*[size];
    for ( int i = 0; i < size; i++ ) elements[i] = 0;
    elementCount = 0;
    bufferSize = size;
    initialSize = size;
} //-- initialize

/**
 * Destructor for ArrayList, does not delete elements by default
**/
ArrayList::~ArrayList() {
    delete [] elements;
} //-- ~ArrayList

/**
 * Adds the specified TxObject to this ArrayList
 * @param object the TxObject to add to the ArrayList
**/
void ArrayList::add(TxObject* object) {
    if (!object) return;
    if (elementCount == bufferSize) increaseSize();
    elements[elementCount++] = object;
} //-- add

/**
 * Adds the given TxObject to this ArrayList at the specified index.
 * @param object the TxObject to add
 * @return true if the object has been properly added at the correct index.
**/
MBool ArrayList::add(int index, TxObject* object) {

    if ((index < 0) || (index > elementCount)) return MB_FALSE;

    // make sure we have room to add the object
    if (elementCount == bufferSize) increaseSize();

    if (index == elementCount) {
        elements[elementCount++] = object;
    }
    else {
        shiftUp(index);
        elements[index] = object;
        ++elementCount;
    }
    return MB_TRUE;
} //-- add

/**
 * Removes all elements from the list
**/
void ArrayList::clear() {
    for (int i = 0; i < elementCount; i++) {
        elements[i] = 0;
    }
    elementCount = 0;
} //-- clear

/**
 * Removes all elements from the list
 * @param deleteObjects allows specifying whether or not to delete the TxObjects
 * that are currently in the list.
 *
 * Note: If object deletion is enabled this method will check for duplicate references
 * in the list to prevent possible seg faults and will therefore run slower than an algorithm
 * that doesn't check for duplicates.
**/
void ArrayList::clear(MBool deleteObjects) {


    if (deleteObjects) {
        for (int i = 0; i < elementCount; i++) {
            if (elements[i]) {
                TxObject* tmp = elements[i];
                elements[i] = 0;
                //-- check for duplicates to avoid attempting to free memory that
                //-- has already been freed
                int idx = i+1;
                for ( ; idx < elementCount; idx++) {
                    if (elements[idx] == tmp) elements[idx] = 0;
                }
                delete tmp;
            }
        }
        elementCount = 0;
    }
    else clear();

} //-- clear(MBool);

/**
 * Returns true if the specified TxObject is contained in the list.
 * @param object the TxObject to search for
 * @return true if specified object is contained in the list
**/
MBool ArrayList::contains(TxObject* object) {
    return (MBool)(indexOf(object) >= 0);
} //-- contains

/**
 * Copies the elements of this ArrayList, into the destination ArrayList
**/
void ArrayList::copyInto(ArrayList& dest) const {
    for ( int i = 0; i < elementCount; i++ ) dest.add(elements[i]);
} //-- copyInto

/**
 * Returns the TxObject at the specified position in this ArrayList.
 * @param index the position of the object to return, if the index
 * is out-of-bounds, 0 will be returned.
**/
TxObject* ArrayList::get(int index) {
    if ((index < 0) || index >= elementCount) return 0;
    return elements[index];
} //-- get


/**
 * Returns the index of the specified object,
 * or -1 if the object is not contained in the ArrayList
 * @param object the TxObject to get the index of
**/
int ArrayList::indexOf(TxObject* object) {
    for (int i = 0; i < elementCount; i++)
        if (object == elements[i]) return i;
    return -1;
} //-- indexOf

/**
 * Removes the TxObject at the specified index
 * @param index the position of the TxObject to remove
 * @return the TxObject that was removed from the list
**/
TxObject* ArrayList::remove(int index) {

    if ((index < 0) || (index >= elementCount)) return 0;

    TxObject* object = elements[index];
    shiftDown(index+1);
    --elementCount;
    return object;
} //-- remove

/**
 * Removes the the specified TxObject from the list
 * @param object the TxObject to remove from the list
 * @return true if the object was removed from the list
**/
MBool ArrayList::remove(TxObject* object) {

    int index = indexOf(object);

    if (index > -1) {
        remove(index);
    }
    else return MB_FALSE;

    return MB_TRUE;
} //-- remove


/**
 * Returns the number of elements in the ArrayList
 * @return the number of elements in the ArrayList
**/
int ArrayList::size() const{
    return elementCount;
} //-- size

  //-------------------/
 //- Private Methods -/
//-------------------/

/**
 * increase the capacity by a factor of its initial size
**/
void ArrayList::increaseSize() {

    if (initialSize == 0) bufferSize += DEFAULT_SIZE;
    else bufferSize += initialSize;

    TxObject** tmp = elements;
    elements = new TxObject*[bufferSize];
    int i=0;
    for (;i < elementCount; i++) elements[i] = tmp[i];
    for (;i<bufferSize;i++)elements[i] = 0;
    delete [] tmp;

} //-- increaseSize

/**
 * Shifts all elements at the specified index to down by 1
**/
void ArrayList::shiftDown(int index) {
    if ((index <= 0) || (index > elementCount)) return;

    for (int i = index; i < elementCount; i++) {
        elements[i-1] = elements[i];
    }

    elements[elementCount-1] = 0;
} //-- shiftDown

/**
 * Shifts all elements at the specified index up by 1
**/
void ArrayList::shiftUp(int index) {
    if (index == elementCount) return;
    if (elementCount == bufferSize) increaseSize();

    //-- from Java
    //-- System.arraycopy(elements, index, elements, index + 1, elementCount - index);
    for (int i = elementCount; i > index; i--) {
        elements[i] = elements[i-1];
    }
} //-- shiftUp


