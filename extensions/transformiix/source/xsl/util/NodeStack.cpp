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

#include "NodeStack.h"
#include <iostream.h>
/**
 * @author <a href="kvisco@mitre.org">Keith Visco</a>
 * <BR/>
 * <PRE>
 * Modifications:
 * 19990806: Larry Fitzpatrick
 *   - moved initialization of DEFAULT_SIZE from NodeStack.h to here
 * </PRE>
 *
**/


  //-------------/
 //- Constants -/
//-------------/
const int NodeStack::DEFAULT_SIZE = 25;

  //----------------/
 //- Constructors -/
//----------------/

/**
 * Creates a new NodeStack with the default Size
**/
NodeStack::NodeStack() {
    initialize(DEFAULT_SIZE);
} //-- NodeStack

/**
 * Creates a new NodeStack with the specified size
**/
NodeStack::NodeStack(int size) {
    initialize(size);
} //-- NodeStack

/**
 * Helper method for Constructors
**/
void NodeStack::initialize(int size) {
    elements = new Node*[size];
    elementCount = 0;
    bufferSize = size;
    initialSize = size;
} //-- initialize

/**
 * Destructor for NodeSet
**/
NodeStack::~NodeStack() {
    delete [] elements;
} //-- ~NodeStack

/**
 * Returns the specified Node from the top of this Stack,
 * without removing it from the stack.
 * @param node the Node to add to the top of the Stack
 * @return a pointer to the Node that is the top of this Stack
**/
Node* NodeStack::peek() {
    return get(size()-1);
} //-- peek

/**
 * Removes the specified Node from the top of this Stack.
 * @param node the Node to add to the top of the Stack
 * @return a  pointer to the Node that was the top of this Stack
**/
Node* NodeStack::pop() {
    return remove(size()-1);
} //-- pop

/**
 * Adds the specified Node to the top of this Stack.
 * @param node the Node to add to the top of the Stack
**/
void NodeStack::push(Node* node) {
    if (elementCount == bufferSize) increaseSize();
    elements[elementCount++] = node;
} //-- push


/**
 * Removes all elements from the Stack
**/
void NodeStack::clear() {
    for (int i = 0; i < elementCount; i++) {
        elements[i] = 0;
    }
    elementCount = 0;
} //-- clear

/**
 * Returns true if the specified Node is contained in the Stack.
 * if the specfied Node is null, then if the NodeStack contains a null
 * value, true will be returned.
 * @param node the element to search the NodeStack for
 * @return true if specified Node is contained in the NodeStack
**/
MBool NodeStack::contains(Node* node) {
    cout << "NodeStack#contains [enter]\n";
    MBool retVal = (indexOf(node) >= 0) ? MB_TRUE : MB_FALSE;
    cout << "NodeStack#contains [exit]\n";
    return retVal;

} //-- contains

/**
 * Compares the specified NodeStack with this NodeStack for equality.
 * Returns true if and only if the specified NodeStack is the same
 * size as this NodeSet and all of its associated
 * Nodes are contained within this NodeStack in the same order.
 * @return true if and only if the specified NodeStack is the
 * same size as this NodeStack and all of its associated
 * Nodes are contained within this NodeStack in the same order.
**/
MBool NodeStack::equals(NodeStack* nodeStack) {
    if (!nodeStack) return MB_FALSE;
    if (nodeStack->size() != size()) return MB_FALSE;

    for (int i = 0; i < size(); i++) {
        if (nodeStack->get(i) != get(i)) return MB_FALSE;
    }
    return MB_TRUE;
} //-- equals

/**
 * Returns the Node at the specified position in this NodeStack.
 * @param index the position of the Node to return
**/
Node* NodeStack::get(int index) {
    if ((index < 0) || index >= elementCount) return 0;
    return elements[index];
} //-- get


/**
 * Returns the index of the specified Node,
 * or -1 if the Node is not contained in the NodeStack
 * @param node the Node to get the index for
**/
int NodeStack::indexOf(Node* node) {

    for (int i = 0; i < elementCount; i++) {
        if (node == elements[i]) {
            return i;
        }
    }
    return -1;
} //-- indexOf

/**
 * Returns true if there are no Nodes in the NodeStack.
 * @return true if there are no Nodes in the NodeStack.
**/
MBool NodeStack::isEmpty() {
    return (elementCount == 0) ? MB_TRUE : MB_FALSE;
} //-- isEmpty

/**
 * Removes the Node at the specified index from the NodeStack
 * @param index the position in the NodeStack to remove the Node from
 * @return the Node that was removed from the NodeStack
**/
Node* NodeStack::remove(int index) {

    if ((index < 0) || (index > elementCount)) return 0;
    Node* node = elements[index];
    shiftDown(index+1);
    --elementCount;
    return node;
} //-- remove

/**
 * Removes the the specified Node from the NodeStack
 * @param node the Node to remove from the NodeStack
 * @return true if the Node was removed from the list
**/
MBool NodeStack::remove(Node* node) {
    int index = indexOf(node);

    if (index > -1) {
        remove(index);
    }
    else return MB_FALSE;

    return MB_TRUE;
} //-- remove


/**
 * Returns the number of elements in the NodeStack
 * @return the number of elements in the NodeStack
**/
int NodeStack::size() {
    return elementCount;
} //-- size

  //-------------------/
 //- Private Methods -/
//-------------------/

/**
 * increase the NodeStack capacity by a factor of its initial size
**/
void NodeStack::increaseSize() {

    bufferSize += bufferSize;
    Node** tmpNodes = elements;
    elements = new Node*[bufferSize];
    for (int i=0;i < elementCount; i++) {
        elements[i] = tmpNodes[i];
    }
    delete [] tmpNodes;

} //-- increaseSize

/**
 * Shifts all elements at the specified index to down by 1
**/
void NodeStack::shiftDown(int index) {
    if ((index <= 0) || (index >= elementCount)) return;

    //-- from Java
    //-- System.arraycopy(elements, index, elements, index - 1, elementCount - index);
    for (int i = index; i < elementCount; i++) {
        elements[index-1] = elements[index];
    }

    elements[elementCount-1] = 0;
} //-- shiftDown

/**
 * Shifts all elements at the specified index up by 1
**/
void NodeStack::shiftUp(int index) {
    if (index == elementCount) return;
    if (elementCount == bufferSize) increaseSize();

    //-- from Java
    //-- System.arraycopy(elements, index, elements, index + 1, elementCount - index);
    for (int i = elementCount; i > index; i--) {
        elements[i] = elements[i-1];
    }
} //-- shiftUp
