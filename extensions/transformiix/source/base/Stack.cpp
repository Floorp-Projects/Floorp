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
 * Stack
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
 * <BR/>
 * <PRE>
 * Modifications:
 * 19990806: Larry Fitzpatrick
 *  - in method #peek():
 *    - Changed ListItem::ListItem to List::ListItem
 * </PRE>
**/

#include "Stack.h"

  //-------------/
 //- Stack.cpp -/
//-------------/

/**
 * Creates a new Stack
**/
Stack::Stack() : List() {
} //-- Stack


/**
 * Destructor for Stack, will not delete Object references
**/
Stack::~Stack() {
    //-- the base destructor for List will do all clean up
}

/**
 * Returns an iterator that will iterator over the Stack, from the topmost
 * element to the bottom element.
 * You will need to delete this Iterator when you are done
**/
StackIterator* Stack::iterator() {
    StackIterator* iter = (StackIterator*)List::iterator();
    iter->reverse();
    return iter;
} //-- iterator

/**
 * Returns the specified Object from the top of this Stack,
 * without removing it from the stack.
 * @return a pointer to the object that is the top of this Stack
**/
void* Stack::peek() {
    void* obj = 0;
    List::ListItem* item = getLastItem();
    if ( item ) obj = item->objPtr;
    return obj;
} //-- peek

/**
 * Adds the specified Object to the top of this Stack.
 * @param obj a pointer to the object that is to be added to the
 * top of this Stack
**/
void Stack::push(void* obj) {
    add(obj);
} //-- push

/**
 * Removes and returns the specified Object from the top of this Stack.
 * @return a pointer to the object that was the top of this Stack
**/
void* Stack::pop() {
    void* obj = 0;
    ListItem* item = getLastItem();
    if ( item ) obj = item->objPtr;
    item = remove(item);
    item->objPtr = 0;
    delete item;
    return obj;
} //-- pop

/**
 * Returns true if there are no objects in the Stack.
 * @return true if there are no objects in the Stack.
**/
MBool Stack::empty() {
    return (MBool) (getLength() == 0);
} //-- empty

/**
 * Returns the number of elements in the Stack
 * @return the number of elements in the Stack
**/
int Stack::size() {
    return getLength();
} //-- size

