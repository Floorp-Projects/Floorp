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

/* Implementation of the wrapper class to convert the Mozilla nsIDOMAttr
   interface into a TransforMIIX Attr interface.
*/

#include "mozilladom.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aAttr the nsIDOMAttr you want to wrap
 * @param aOwner the document that owns this object
 */
Attr::Attr(nsIDOMAttr* aAttr, Document* aOwner) : Node(aAttr, aOwner)
{
    nsAttr = aAttr;
}

/**
 * Destructor
 */
Attr::~Attr()
{
}

/**
 * Return the wrapped nsIDOMAttr.
 */
nsIDOMAttr* Attr::getNSAttr()
{
    return nsAttr;
}

/**
 * Wrap a different Mozilla object with this wrapper.
 *
 * @param aAttr the nsIDOMAttr you want to wrap
 */
void Attr::setNSObj(nsIDOMAttr* aAttr)
{
    Node::setNSObj(aAttr);
    nsAttr = aAttr;
}

/**
 * Call nsIDOMAttr::GetName to retrieve the name of this attribute.
 *
 * @return the attribute's name
 */
const String& Attr::getName()
{
    nodeName.clear();
    nsAttr->GetName(nodeName.getNSString());
    return nodeName;
}

/**
 * Call nsIDOMAttr::GetSpecified to retrieve the specified flag for this
 * attribute.
 *
 * @return the value of the specified flag
 */
MBool Attr::getSpecified() const
{
    MBool specified;

    nsAttr->GetSpecified(&specified);
    return specified;
}

/**
 * Call nsIDOMAttr::GetValue to retrieve the value of this attribute.
 *
 * @return the attribute's value
 */
const String& Attr::getValue()
{
    nodeValue.clear();
    nsAttr->GetValue(nodeValue.getNSString());
    return nodeValue;
}

/**
 * Call nsIDOMAttr::SetValue to set the value of this attribute.
 *
 * @return the attribute's value
 */
void Attr::setValue(const String& aNewValue)
{
    nsAttr->SetValue(aNewValue.getConstNSString());
}
