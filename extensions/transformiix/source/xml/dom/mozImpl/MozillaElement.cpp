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
 * Contributor(s): Tom Kneeland
 *                 Peter Van der Beken <peter.vanderbeken@pandora.be>
 *
 */

/* Implementation of the wrapper class to convert the Mozilla nsIDOMElement
   interface into a TransforMIIX Element interface.
*/

#include "mozilladom.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aElement the nsIDOMElement you want to wrap
 * @param aOwner the document that owns this object
 */
Element::Element(nsIDOMElement* aElement, Document* aOwner) :
        Node(aElement, aOwner)
{
    nsElement = aElement;
}

/**
 * Destructor
 */
Element::~Element()
{
}

/**
 * Wrap a different Mozilla object with this wrapper.
 *
 * @param aElement the nsIDOMElement you want to wrap
 */
void Element::setNSObj(nsIDOMElement* aElement)
{
    Node::setNSObj(aElement);
    nsElement = aElement;
}

/**
 * Call nsIDOMElement::GetTagName to retrieve the tag name for this element.
 *
 * @return the element's tag name
 */
const String& Element::getTagName()
{
    nodeName.clear();
    nsElement->GetTagName(nodeName.getNSString());
    return nodeName;
}

/**
 * Get the attribute node for the specified name and return it's value or the
 * null string if the attribute node doesn't exist.
 *
 * @param aName the name of the attribute whose value you want
 *
 * @return the attribute node's value or null string
 */
const String& Element::getAttribute(const String& aName)
{
  Node* tempNode = getAttributeNode(aName);

  if (tempNode)
    return tempNode->getNodeValue();
  else
    return NULL_STRING;
}

/**
 * Call nsIDOMElement::SetAttribute to set an attribute to the specified value.
 *
 * @param aName the name of the attribute you want to set
 * @param aValue the value you want to set the attribute to
 */
void Element::setAttribute(const String& aName, const String& aValue)
{
    nsElement->SetAttribute(aName.getConstNSString(), aValue.getConstNSString());
}

/**
 * Call nsIDOMElement::SetAttributeNS to set an attribute to the specified
 * value.
 *
 * @param aNamespaceURI the URI of the namespace of the attribute
 * @param aName the name of the attribute you want to set
 * @param aValue the value you want to set the attribute to
 */
void Element::setAttributeNS(const String& aNamespaceURI, const String& aName,
        const String& aValue)
{
    nsElement->SetAttributeNS(aNamespaceURI.getConstNSString(),
            aName.getConstNSString(), aValue.getConstNSString());
}

/**
 * Call nsIDOMElement::RemoveAttribute to remove an attribute. We need to make
 * this call a bit more complicated than usual because we want to make sure
 * we remove the attribute wrapper from the document's wrapper hash table.
 *
 * @param aName the name of the attribute you want to remove
 */
void Element::removeAttribute(const String& aName)
{
    nsCOMPtr<nsIDOMAttr> attr;
    Attr* attrWrapper = NULL;

    // First, get the nsIDOMAttr object from the nsIDOMElement object
    nsElement->GetAttributeNode(aName.getConstNSString(), getter_AddRefs(attr));

    // Second, remove the attribute wrapper object from the hash table if it is
    // there.  It might not be if the attribute was created using
    // Element::setAttribute. If it was removed, then delete it.
    attrWrapper = (Attr*)ownerDocument->removeWrapper(attr.get());
    if (attrWrapper)
        delete attrWrapper;

    // Lastly, have the Mozilla object remove the attribute
    nsElement->RemoveAttribute(aName.getConstNSString());
}

/**
 * Call nsIDOMElement::GetAttributeNode to get an nsIDOMAttr. If successful,
 * refer to the owner document for an attribute wrapper.
 *
 * @param aName the name of the attribute node you want to get
 *
 * @return the attribute node's value or null string
 */
Attr* Element::getAttributeNode(const String& aName)
{
    nsCOMPtr<nsIDOMAttr> attr;

    if (NS_SUCCEEDED(nsElement->GetAttributeNode(aName.getConstNSString(),
            getter_AddRefs(attr))))
        return (Attr*)ownerDocument->createWrapper(attr);
    else
        return NULL;
}

/**
 * Call nsIDOMElement::SetAttributeNode passing it the nsIDOMAttr object wrapped
 * by the aNewAttr parameter.
 *
 * @param aNewAttr the wrapper of the new attribute node
 *
 * @return the attribute node's value or null string
 */
Attr* Element::setAttributeNode(Attr* aNewAttr)
{
    nsCOMPtr<nsIDOMAttr> returnAttr;

    if (NS_SUCCEEDED(nsElement->SetAttributeNode(aNewAttr->getNSAttr(),
            getter_AddRefs(returnAttr))))
        return (Attr*)ownerDocument->createWrapper(returnAttr);
    else
        return NULL;
}

/**
 * Call nsIDOMElement::RemoveAttributeNode, then refer to the owner document to
 * remove it from its hash table.  The caller is then responsible for destroying
 * the wrapper.
 *
 * @param aOldAttr the wrapper of attribute node you want to remove
 *
 * @return the attribute node's value or null string
 */
Attr* Element::removeAttributeNode(Attr* aOldAttr)
{
    nsCOMPtr<nsIDOMAttr> removedAttr;
    Attr* attrWrapper = NULL;

    if (NS_SUCCEEDED(nsElement->RemoveAttributeNode(aOldAttr->getNSAttr(),
            getter_AddRefs(removedAttr))))
    {
        attrWrapper = (Attr*)ownerDocument->removeWrapper(removedAttr.get());
        if (!attrWrapper)
            attrWrapper =  new Attr(removedAttr, ownerDocument);
        return attrWrapper;
    }
    else
        return NULL;
}

/**
 * Call nsIDOMElement::GetElementsByTagName. If successful, refer to the owner
 * document for a Nodelist wrapper.
 *
 * @param aName the name of the elements you want to get
 *
 * @return the nodelist containing the returned elements
 */
NodeList* Element::getElementsByTagName(const String& aName)
{
    nsCOMPtr<nsIDOMNodeList> list;

    if (NS_SUCCEEDED(nsElement->GetElementsByTagName(aName.getConstNSString(),
            getter_AddRefs(list))))
        return ownerDocument->createNodeList(list);
    else
        return NULL;
}

/**
 * Simply call nsIDOMElement::Normalize().
 */
void Element::normalize()
{
    nsElement->Normalize(); 
}
