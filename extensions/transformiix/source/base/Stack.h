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
 */

/**
 * Stack
**/

#ifndef MITRE_STACK_H
#define MITRE_STACK_H

#include "List.h"
#include "baseutils.h"

typedef ListIterator StackIterator;

class Stack : private List {

public:

      //----------------/
     //- Constructors -/
    //----------------/

    /**
     * Creates a new Stack
    **/
    Stack();


    /**
     * Destructor for Stack, will not delete Object references
    **/
    virtual ~Stack();

    StackIterator* iterator();

    /**
     * Returns the specified Object from the top of this Stack,
     * without removing it from the stack.
     * @return a pointer to the object that is the top of this Stack
    **/
    void* peek();

    /**
     * Adds the specified Object to the top of this Stack.
     * @param obj a pointer to the object that is to be added to the
     * top of this Stack
    **/
    void push(void* obj);

    /**
     * Removes and returns the specified Object from the top of this Stack.
     * @return a pointer to the object that was the top of this Stack
    **/
    void* pop();

    /**
     * Returns true if there are no objects in the Stack.
     * @return true if there are no objects in the Stack.
    **/
    MBool empty();

    /**
     * Returns the number of elements in the Stack
     * @return the number of elements in the Stack
    **/
    int size();

private:

}; //-- Stack


#endif
