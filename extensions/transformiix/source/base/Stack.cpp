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
 * Larry Fitzpatrick, lef@opentext.com
 *    -- 19990806 
 *       - In method ::peek() changed ListItem::ListItem to List::ListItem
 *
 */

/**
 * Stack
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
    return (StackIterator*)List::iterator();
} //-- iterator

/**
 * Returns the specified Object from the top of this Stack,
 * without removing it from the stack.
 * @return a pointer to the object that is the top of this Stack
**/
void* Stack::peek() {
    void* obj = 0;
    List::ListItem* item = getFirstItem();
    if ( item ) obj = item->objPtr;
    return obj;
} //-- peek

/**
 * Adds the specified Object to the top of this Stack.
 * @param obj a pointer to the object that is to be added to the
 * top of this Stack
**/
void Stack::push(void* obj) {
    insert(0,obj);
} //-- push

/**
 * Removes and returns the specified Object from the top of this Stack.
 * @return a pointer to the object that was the top of this Stack
**/
void* Stack::pop() {
    void* obj = 0;
    ListItem* item = getFirstItem();
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

