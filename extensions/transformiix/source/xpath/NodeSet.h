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
 * Larry Fitzpatrick, OpenText, lef@opentext.com
 *   -- moved initialization of DEFAULT_SIZE to NodeSet.cpp
 *
 * $Id: NodeSet.h,v 1.2 2000/04/07 22:57:48 Peter.VanderBeken%pandora.be Exp $
 */

/**
 * NodeSet
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.2 $ $Date: 2000/04/07 22:57:48 $
**/

#ifndef TRANSFRMX_NODESET_H
#define TRANSFRMX_NODESET_H

#include "MITREObject.h"
#include "dom.h"
#include "ExprResult.h"
#include "XMLDOMUtils.h"

class NodeSet : public ExprResult {


public:

      //----------------/
     //- Constructors -/
    //----------------/

    /**
     * Creates a new NodeSet with the default Size
    **/
    NodeSet();

    /**
     * Creates a new NodeSet with the specified Size
    **/
    NodeSet(int size);

    /**
     * Creates a new NodeSet using the given NodeSet
    **/
    NodeSet(const NodeSet& source);

    /**
     * Destructor for NodeSet, will not delete Node References
    **/
    virtual ~NodeSet();

    /**
     * Adds the specified Node to this NodeSet if it is not already
     * contained within in this NodeSet.
     * @param node the Node to add to the NodeSet
     * @return true if the Node is added to the NodeSet
    **/
    MBool add(Node* node);

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
     * Copies the elements of this NodeSet, into the destination NodeSet
    **/
    void copyInto(NodeSet& dest) const;

    /**
     * Compares the specified object with this NodeSet for equality.
     * Returns true if and only if the specified Object is a NodeSet
     * that is the same size as this NodeSet and all of its associated
     * Nodes are contained within this NodeSet.
     * @return true if and only if the specified Object is a NodeSet
     * that is the same size as this NodeSet and all of its associated
     * Nodes are contained within this NodeSet.
    **/
    MBool equals(NodeSet* nodeSet);

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
    int size() const;

    /**
     * Creates a String representation of this NodeSet
     * @param str the destination string to append the String representation to.
    **/
    void toString(String& str);

      //------------------------------------/
     //- Virtual Methods from: ExprResult -/
    //------------------------------------/

    /**
     * Returns the type of ExprResult represented
     * @return the type of ExprResult represented
    **/
    virtual short getResultType();

    /**
     * Converts this ExprResult to a Boolean (MBool) value
     * @return the Boolean value
    **/
    virtual MBool booleanValue();

    /**
     * Converts this ExprResult to a Number (double) value
     * @return the Number value
    **/
    virtual double numberValue();

    /**
     * Creates a String representation of this ExprResult
     * @param str the destination string to append the String representation to.
    **/
    virtual void stringValue(DOMString& str);

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
