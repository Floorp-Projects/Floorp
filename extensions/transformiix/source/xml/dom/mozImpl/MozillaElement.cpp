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
}

/**
 * Destructor
 */
Element::~Element()
{
}

/**
 * Call nsIDOMElement::GetTagName to retrieve the tag name for this element.
 *
 * @return the element's tag name
 */
const String& Element::getTagName()
{
    NSI_FROM_TX(Element)

    nodeName.clear();
    if (nsElement)
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
    NSI_FROM_TX(Element)

    if (nsElement)
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
    NSI_FROM_TX(Element)

    if (nsElement)
        nsElement->SetAttributeNS(aNamespaceURI.getConstNSString(),
                                  aName.getConstNSString(),
                                  aValue.getConstNSString());
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
    NSI_FROM_TX(Element)

    if (nsElement) {
        nsCOMPtr<nsIDOMAttr> attr;
        Attr* attrWrapper = NULL;

        // First, get the nsIDOMAttr object from the nsIDOMElement object
        nsElement->GetAttributeNode(aName.getConstNSString(), getter_AddRefs(attr));

        // Second, remove the attribute wrapper object from the hash table if it is
        // there.  It might not be if the attribute was created using
        // Element::setAttribute. If it was removed, then delete it.
        attrWrapper = (Attr*)ownerDocument->removeWrapper(attr);
        if (attrWrapper)
            delete attrWrapper;

        // Lastly, have the Mozilla object remove the attribute
        nsElement->RemoveAttribute(aName.getConstNSString());
    }
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
    NSI_FROM_TX_NULL_CHECK(Element)
    nsCOMPtr<nsIDOMAttr> attr;

    if (NS_SUCCEEDED(nsElement->GetAttributeNode(aName.getConstNSString(),
                                                 getter_AddRefs(attr))) && attr)
        return (Attr*)ownerDocument->createWrapper(attr);
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
    NSI_FROM_TX_NULL_CHECK(Element)
    nsCOMPtr<nsIDOMAttr> newAttr(do_QueryInterface(GET_NSOBJ(aNewAttr)));
    nsCOMPtr<nsIDOMAttr> returnAttr;

    if (NS_SUCCEEDED(nsElement->SetAttributeNode(newAttr,
            getter_AddRefs(returnAttr))))
        return (Attr*)ownerDocument->createWrapper(returnAttr);
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
    NSI_FROM_TX_NULL_CHECK(Element)
    nsCOMPtr<nsIDOMAttr> oldAttr(do_QueryInterface(GET_NSOBJ(aOldAttr)));
    nsCOMPtr<nsIDOMAttr> removedAttr;
    Attr* attrWrapper = NULL;

    if (NS_SUCCEEDED(nsElement->RemoveAttributeNode(oldAttr,
            getter_AddRefs(removedAttr))))
    {
        attrWrapper = (Attr*)ownerDocument->removeWrapper(aOldAttr);
        if (!attrWrapper)
            attrWrapper =  new Attr(removedAttr, ownerDocument);
        return attrWrapper;
    }
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
    NSI_FROM_TX_NULL_CHECK(Element)
    nsCOMPtr<nsIDOMNodeList> list;

    if (NS_SUCCEEDED(nsElement->GetElementsByTagName(aName.getConstNSString(),
            getter_AddRefs(list))))
        return ownerDocument->createNodeList(list);
    return NULL;
}

/**
 * Simply call nsIDOMElement::Normalize().
 */
void Element::normalize()
{
    NSI_FROM_TX(Element)

    if (nsElement)
        nsElement->Normalize(); 
}
