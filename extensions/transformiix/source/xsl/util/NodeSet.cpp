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
 *    -- moved initialization of DEFAULT_SIZE from NodeSet.h to here
 *
 * Olivier Gerardin, ogerardin@vo.lu
 *    -- fixed numberValue()
 *
 * $Id: NodeSet.cpp,v 1.3 2000/03/19 08:01:58 kvisco%ziplink.net Exp $
 */

#include "NodeSet.h"
#include <iostream.h>
/**
 * NodeSet <BR />
 * This class was ported from XSL:P. <BR />
 * @author <A HREF="mailto:kvisco@ziplink.net">Keith Visco</A>
 * @version $Revision: 1.3 $ $Date: 2000/03/19 08:01:58 $
**/


  //-------------/
 //- Constants -/
//-------------/
const int NodeSet::DEFAULT_SIZE = 25;


  //----------------/
 //- Constructors -/
//----------------/

/**
 * Creates a new NodeSet with the default Size
**/
NodeSet::NodeSet() {
    initialize(DEFAULT_SIZE);
} //-- NodeSet

/**
 * Creates a new NodeSet with the default Size
**/
NodeSet::NodeSet(int size) {
    initialize(size);
} //-- NodeSet

/**
 * Creates a new NodeSet, copying the Node references from the source
 * NodeSet
**/
NodeSet::NodeSet(const NodeSet& source) {
    initialize(source.size());
    source.copyInto(*this);
} //--NodeSet

/**
 * Helper method for Constructors
**/
void NodeSet::initialize(int size) {
    elements = new Node*[size];
    for ( int i = 0; i < size; i++ ) elements[i] = 0;
    elementCount = 0;
    bufferSize = size;
    initialSize = size;
} //-- initialize

/**
 * Destructor for NodeSet
**/
NodeSet::~NodeSet() {
    delete [] elements;
} //-- ~NodeSet

/**
 * Adds the specified Node to this NodeSet if it is not already
 * contained within in this NodeSet.
 * @param node the Node to add to the NodeSet
 * @return true if the Node is added to the NodeSet
**/
MBool NodeSet::add(Node* node) {
    if (!contains(node)) {
        if (elementCount == bufferSize) increaseSize();
        elements[elementCount++] = node;
        return MB_TRUE;
    }
    return MB_FALSE;
} //-- add

/**
 * Adds the specified Node to the NodeSet at the specified index,
 * as long as the Node is not already contained within the set
 * @param node the Node to add to the NodeSet
 * @return true if the Node is added to the NodeSet. If the index is
 * out of bounds the Node will not be added to the set and false will be returned.
**/
MBool NodeSet::add(int index, Node* node)
{
    if ((index < 0) || (index > elementCount)) return MB_FALSE;

    if (contains(node)) return MB_FALSE;

    // make sure we have room to add the object
    if (elementCount == bufferSize) increaseSize();

    if (index == elementCount) {
        elements[elementCount++] = node;
    }
    else {
        shiftUp(index);
        elements[index] = node;
        ++elementCount;
    }
    return MB_TRUE;
} //-- add

/**
 * Removes all elements from the list
**/
void NodeSet::clear() {
    for (int i = 0; i < elementCount; i++) {
        elements[i] = 0;
    }
    elementCount = 0;
} //-- clear

/**
 * Returns true if the specified Node is contained in the set.
 * if the specfied Node is null, then if the NodeSet contains a null
 * value, true will be returned.
 * @param node the element to search the NodeSet for
 * @return true if specified Node is contained in the NodeSet
**/
MBool NodeSet::contains(Node* node) {
    return (MBool)(indexOf(node) >= 0);
} //-- contains

/**
 * Copies the elements of this NodeSet, into the destination NodeSet
**/
void NodeSet::copyInto(NodeSet& dest) const {
    for ( int i = 0; i < elementCount; i++ ) dest.add(elements[i]);
} //-- copyInto

/**
 * Compares the specified object with this NodeSet for equality.
 * Returns true if and only if the specified Object is a NodeSet
 * that is the same size as this NodeSet and all of its associated
 * Nodes are contained within this NodeSet.
 * @return true if and only if the specified Object is a NodeSet
 * that is the same size as this NodeSet and all of its associated
 * Nodes are contained within this NodeSet.
**/
MBool NodeSet::equals(NodeSet* nodeSet) {
    if (!nodeSet) return MB_FALSE;
    if (nodeSet->size() != size()) return MB_FALSE;

    for (int i = 0; i < size(); i++) {
        if (!nodeSet->contains(get(i))) return MB_FALSE;
    }
    return MB_TRUE;
} //-- equals

/**
 * Returns the Node at the specified position in this NodeSet.
 * @param index the position of the Node to return
**/
Node* NodeSet::get(int index) {
    if ((index < 0) || index >= elementCount) return 0;
    return elements[index];
} //-- get


/**
 * Returns the index of the specified Node,
 * or -1 if the Node is not contained in the NodeSet
 * @param node the Node to get the index for
**/
int NodeSet::indexOf(Node* node) {
    for (int i = 0; i < elementCount; i++)
        if (node == elements[i]) return i;
    return -1;
} //-- indexOf

/**
 * Returns true if there are no Nodes in the NodeSet.
 * @return true if there are no Nodes in the NodeSet.
**/
MBool NodeSet::isEmpty() {
    return (elementCount == 0) ? MB_TRUE : MB_FALSE;
} //-- isEmpty

/**
 * Removes the Node at the specified index from the NodeSet
 * @param index the position in the NodeSet to remove the Node from
 * @return the Node that was removed from the list
**/
Node* NodeSet::remove(int index) {

    if ((index < 0) || (index >= elementCount)) return 0;

    Node* node = elements[index];
    shiftDown(index+1);
    --elementCount;
    return node;
} //-- remove

/**
 * Removes the the specified Node from the NodeSet
 * @param node the Node to remove from the NodeSet
 * @return true if the Node was removed from the list
**/
MBool NodeSet::remove(Node* node) {
    int index = indexOf(node);

    if (index > -1) {
        remove(index);
    }
    else return MB_FALSE;

    return MB_TRUE;
} //-- remove


/**
 * Returns the number of elements in the NodeSet
 * @return the number of elements in the NodeSet
**/
int NodeSet::size() const{
    return elementCount;
} //-- size

/**
 * Creates a String representation of this NodeSet
 * @param str the destination string to append the String representation to.
**/
void NodeSet::toString(String& str) {
    str.append("#NodeSet");
} //-- toString

  //-------------------/
 //- Private Methods -/
//-------------------/

/**
 * increase the NodeSet capacity by a factor of its initial size
**/
void NodeSet::increaseSize() {

    bufferSize += bufferSize;
    Node** tmpNodes = elements;
    elements = new Node*[bufferSize];
    int i=0;
    for (i=0;i < elementCount; i++) elements[i] = tmpNodes[i];
    for (;i<bufferSize;i++)elements[i] = 0;
    delete [] tmpNodes;

} //-- increaseSize

/**
 * Shifts all elements at the specified index to down by 1
**/
void NodeSet::shiftDown(int index) {
    if ((index <= 0) || (index > elementCount)) return;

    //-- from Java
    //-- System.arraycopy(elements, index, elements, index - 1, elementCount - index);
    for (int i = index; i < elementCount; i++) {
        elements[i-1] = elements[i];
    }

    elements[elementCount-1] = 0;
} //-- shiftDown

/**
 * Shifts all elements at the specified index up by 1
**/
void NodeSet::shiftUp(int index) {
    if (index == elementCount) return;
    if (elementCount == bufferSize) increaseSize();

    //-- from Java
    //-- System.arraycopy(elements, index, elements, index + 1, elementCount - index);
    for (int i = elementCount; i > index; i--) {
        elements[i] = elements[i-1];
    }
} //-- shiftUp

    //------------------------------------/
  //- Virtual Methods from: ExprResult -/
//------------------------------------/

/**
 * Returns the type of ExprResult represented
 * @return the type of ExprResult represented
**/
short NodeSet::getResultType()  {
    return ExprResult::NODESET;
} //-- getResultType

/**
 * Converts this ExprResult to a Boolean (MBool) value
 * @return the Boolean value
**/
MBool NodeSet::booleanValue() {
    return (MBool) (size() > 0);
} //- booleanValue

/**
 * Converts this ExprResult to a Number (double) value
 * @return the Number value
**/
double NodeSet::numberValue() {
  // OG+
  // As per the XPath spec, the number value of a node-set is the number value
  // of its string value.
  String str;
  stringValue(str);
  Double dbl(str);
  return dbl.doubleValue();
  // OG-
} //-- numberValue

/**
 * Creates a String representation of this ExprResult
 * @param str the destination string to append the String representation to.
**/
void NodeSet::stringValue(String& str) {
    if ( size()>0) {
        XMLDOMUtils::getNodeValue(get(0), &str);
    }
} //-- stringValue

