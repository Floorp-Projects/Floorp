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
**/

#include "List.h"
#include "baseutils.h"


#ifndef MITRE_STACK_H
#define MITRE_STACK_H

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
