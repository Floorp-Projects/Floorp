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

/* Implementation of the wrapper class to convert the Mozilla nsIDOMText
   interface into a TransforMIIX Text interface.
*/

#include "mozilladom.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aText the nsIDOMCharacterData you want to wrap
 * @param aOwner the document that owns this object
 */
Text::Text(nsIDOMText* aText, Document* aOwner) : CharacterData(aText, aOwner)
{
    nsText = aText;
}

/**
 * Destructor
 */
Text::~Text()
{
}

/**
 * Wrap a different Mozilla object with this wrapper.
 *
 * @param aText the nsIDOMText you want to wrap
 */
void Text::setNSObj(nsIDOMText* aText)
{
    CharacterData::setNSObj(aText);
    nsText = aText;
}

/**
 * Call nsIDOMText::SplitText to split the text.
 *
 * @param aOffset the offset at which you want to split
 *
 * @return the resulting Text object
 */
Text* Text::splitText(Int32 aOffset)
{
    nsCOMPtr<nsIDOMText> split;

    if (NS_SUCCEEDED(nsText->SplitText(aOffset, getter_AddRefs(split))))
        return (Text*)ownerDocument->createWrapper(split);
    else
        return NULL;
}
