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
// Tom Kneeland (02/01/2000)
//
//  Wrapper class to convert the Mozilla nsIDOMText interface into a TransforMIIX
//  Text interface
//
// Modification History:
// Who  When      What
//

#include "mozilladom.h"

//
//Construct a text object with the specified document owner and data
//
Text::Text(nsIDOMText* text, Document* owner) : CharacterData(text, owner)
{
  nsText = text;
}

//
//Destructor.  Do nothing
//
Text::~Text()
{
}

//
//Wrap a different nsIDOMText object with this wrapper
//
void Text::setNSObj(nsIDOMText* text)
{
  CharacterData::setNSObj(text);
  nsText = text;
}

//
//Request the Mozilla object to split, and wrap the result in the appropriate
//wrapper object.
//
Text* Text::splitText(Int32 offset)
{
  nsIDOMText* split = NULL;

  if (nsText->SplitText(offset, &split) == NS_OK)
    return ownerDocument->createTextNode(split);
  else
    return NULL;
}
