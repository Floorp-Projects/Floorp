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
 * NodeStack
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
* <BR/>
 * <PRE>
 * Modifications:
 * 19990806: Larry Fitzpatrick
 *   - moved initialization of DEFAULT_SIZE to NodeStack.cpp
 * </PRE>
**/
#include "dom.h"
#include "baseutils.h"

#ifndef NODESTACK_H
#define NODESTACK_H

class NodeStack {


public:

      //----------------/
     //- Constructors -/
    //----------------/

    /**
     * Creates a new NodeStack with the default Size
    **/
    NodeStack();

    /**
     * Creates a new NodeStack with the specified Size
    **/
    NodeStack(int size);

    /**
     * Destructor for NodeStack, will not delete Node References
    **/
    ~NodeStack();

    /**
     * Returns the specified Node from the top of this Stack,
     * without removing it from the stack.
     * @param node the Node to add to the top of the Stack
     * @return a pointer to the Node that is the top of this Stack
    **/
    Node* peek();
    void push(Node* node);
    Node* pop();

    /**
     * Adds the specified Node to the NodeSet at the specified index,
     * as long as the Node is not already contained within the set
     * @param node the Node to add to the NodeSet
     * @return true if the Node is added to the NodeSet
     * @exception IndexOutOfBoundsException
    **/
    MBool add(int index, Node* node);

    /**
     * Removes all elements from the list
    **/
    void clear();

    /**
     * Returns true if the specified Node is contained in the set.
     * if the specfied Node is null, then if the NodeSet contains a null
     * value, true will be returned.
     * @param node the element to search the NodeSet for
     * @return true if specified Node is contained in the NodeSet
    **/
    MBool contains(Node* node);

    /**
     * Compares the specified object with this NodeSet for equality.
     * Returns true if and only if the specified Object is a NodeSet
     * that is the same size as this NodeSet and all of its associated
     * Nodes are contained within this NodeSet.
     * @return true if and only if the specified Object is a NodeSet
     * that is the same size as this NodeSet and all of its associated
     * Nodes are contained within this NodeSet.
    **/
    MBool equals(NodeStack* nodeStack);

    /**
     * Returns the Node at the specified position in this NodeSet.
     * @param index the position of the Node to return
     * @exception IndexOutOfBoundsException
    **/
    Node* get(int index);


    /**
     * Returns the index of the specified Node,
     * or -1 if the Node is not contained in the NodeSet
     * @param node the Node to get the index for
    **/
    int indexOf(Node* node);

    /**
     * Returns true if there are no Nodes in the NodeSet.
     * @return true if there are no Nodes in the NodeSet.
    **/
    MBool isEmpty();

    /**
     * Removes the Node at the specified index from the NodeSet
     * @param index the position in the NodeSet to remove the Node from
     * @return the Node that was removed from the list
    **/
    Node* remove(int index);

    /**
     * Removes the the specified Node from the NodeSet
     * @param node the Node to remove from the NodeSet
     * @return true if the Node was removed from the list
    **/
    MBool remove(Node* node);


    /**
     * Returns the number of elements in the NodeSet
     * @return the number of elements in the NodeSet
    **/
    int size();

private:


      //-------------------/
     //- Private Members -/
    //-------------------/

    static const int DEFAULT_SIZE;

    Node** elements;

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

}; //-- NodeSet

#endif
