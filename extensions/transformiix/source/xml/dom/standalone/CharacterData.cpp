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
// Tom Kneeland (3/29/99)
//
//  Implementation of the Document Object Model Level 1 Core
//    Implementation of the CharacterData class
//
// Modification History:
// Who  When      What
// TK   03/29/99  Created
//

#include "dom.h"

//
//Protected constructor.  Just pass parameters onto NodeDefinition.
//
CharacterData::CharacterData(NodeType type, const String& name,
                             const String& value, Document* owner) :
               NodeDefinition(type, name, value, owner)
{
}

//
//Return a constant reference to the data stored by this object.
//
const String& CharacterData::getData() const
{
  return nodeValue;
}

//
//Set the data stored by this object to the string represented by "source".
//
void CharacterData::setData(const String& source)
{
  nodeValue = source;
}

//
//Returns the length of the data object.
//
Int32 CharacterData::getLength() const
{
  return nodeValue.length();
}

//
//Retreive the substring starting at offset anc ending count number of
//characters away.
//    NOTE:  An empty string will be returned in the event of an error.
//
String& CharacterData::substringData(Int32 offset, Int32 count, String& dest)
{
  if ((offset >= 0) && (offset < nodeValue.length()) && (count > 0))
    return nodeValue.subString(offset, offset+count, dest);
  else
    {
    dest.clear();
    return dest;
    }
}

void CharacterData::appendData(const String& arg)
{
  nodeValue.append(arg);
}

void CharacterData::insertData(Int32 offset, const String& arg)
{
  if ((offset >= 0) && (offset < nodeValue.length()))
    nodeValue.insert(offset, arg);
}

void CharacterData::deleteData(Int32 offset, Int32 count)
{
  if ((offset >= 0) && (offset < nodeValue.length()) && (count > 0))
    nodeValue.deleteChars(offset, count);
}

void CharacterData::replaceData(Int32 offset, Int32 count, const String& arg)
{
  String tempString;

  if ((offset >= 0) && (offset < nodeValue.length()) && (count > 0))
    {
      if (count < arg.length())
        {
        tempString = arg.subString(0, count, tempString);
        nodeValue.replace(offset, tempString);
        }
      else
        nodeValue.replace(offset, arg);
    }
}
