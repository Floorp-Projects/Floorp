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
//  Implementation of the wrapper class to convert the nsIDOMCharacterData into
//  a TransforMIIX CharacterData interface.
//
// Modification History:
// Who  When      What

//

#include "mozilladom.h"

//
//Protected constructor.  Just pass parameters onto Node.
//
CharacterData::CharacterData(nsIDOMCharacterData* charData, Document* owner) : 
  Node(charData, owner)
{
  nsCharacterData = charData;
}

//
//Destructor.  Just do nothing
//
CharacterData::~CharacterData()
{
}

//
//Store a new nsIDOMCharacterData object for wrapping
//
void CharacterData::setNSObj(nsIDOMCharacterData* charData)
{
  Node::setNSObj(charData);
  nsCharacterData = charData;
}

//
//Retrieve the data from the Mozilla object, and wrap it appropriately
//
const String& CharacterData::getData() const
{
  nsString* data = new nsString();

  if (nsCharacterData->GetData(*data) == NS_OK)
    return *(ownerDocument->createDOMString(data));
  else
    {
      //name won't be used, so delete it.
      delete data;
      return NULL_STRING;
    }
}

//
//Set the data stored by this object to the string represented by "source".
//
void CharacterData::setData(const String& source)
{
  nsCharacterData->SetData(source.getConstNSString());
}

//
//Retrieve the length from the Mozilla object and return it to the caller
//
Int32 CharacterData::getLength() const
{
  UInt32 length = 0;

  nsCharacterData->GetLength(&length);

  return length;
}

//
//Refer to the Mozilla Object for its substring, and return the result in 
//the provided Mozilla String wrapper.
//    NOTE:  An empty string will be returned in the event of an error.
//
String& CharacterData::substringData(Int32 offset, Int32 count, 
                    String& dest)
{
  if (nsCharacterData->SubstringData(offset, count, dest.getNSString()) == NS_OK)
    return dest;
  else
    {
    dest.clear();
    return dest;
    }
}

void CharacterData::appendData(const String& arg)
{
  nsCharacterData->AppendData(arg.getConstNSString());
}

void CharacterData::insertData(Int32 offset, const String& arg)
{
  nsCharacterData->InsertData(offset, arg.getConstNSString());
}

void CharacterData::deleteData(Int32 offset, Int32 count)
{
  nsCharacterData->DeleteData(offset, count);
}

void CharacterData::replaceData(Int32 offset, Int32 count, const String& arg)
{
  nsCharacterData->ReplaceData(offset, count, arg.getConstNSString());
}
