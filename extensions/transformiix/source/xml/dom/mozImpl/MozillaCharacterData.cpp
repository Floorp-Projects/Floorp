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

/* Implementation of the wrapper class to convert the Mozilla
   nsIDOMCharacterData interface into a TransforMIIX CharacterData interface.
*/

#include "mozilladom.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aCharData the nsIDOMCharacterData you want to wrap
 * @param aOwner the document that owns this object
 */
CharacterData::CharacterData(nsIDOMCharacterData* aCharData, Document* aOwner) :
        Node(aCharData, aOwner)
{
}

/**
 * Destructor
 */
CharacterData::~CharacterData()
{
}

/**
 * Call nsIDOMCharacterData::GetData to retrieve the character data.
 *
 * @return the character data
 */
const String& CharacterData::getData()
{
    NSI_FROM_TX(CharacterData)

    nodeValue.clear();
    if (nsCharacterData)
        nsCharacterData->GetData(nodeValue.getNSString());
    return nodeValue;
}

/**
 * Call nsIDOMCharacterData::SetData to set the character data.
 *
 * @param aData the character data
 */
void CharacterData::setData(const String& aData)
{
    NSI_FROM_TX(CharacterData)

    if (nsCharacterData)
        nsCharacterData->SetData(aData.getConstNSString());
}

/**
 * Call nsIDOMCharacterData::GetLength length of the character data.
 *
 * @return the length of the character data
 */
PRInt32 CharacterData::getLength() const
{
    NSI_FROM_TX(CharacterData)
    PRUint32 length = 0;

    if (nsCharacterData)
        nsCharacterData->GetLength(&length);
    return length;
}

/**
 * Call nsIDOMCharacterData::SubstringData to get a substring of the character
 * data.
 *
 * @param aOffset the offset of the substring
 * @param aCount the length of the substring
 * @param aDest the string in which you want the substring
 *
 * @return the length of the character data
 */
String& CharacterData::substringData(PRInt32 aOffset, PRInt32 aCount,
                    String& aDest)
{
    NSI_FROM_TX(CharacterData)

    aDest.clear();
    if (nsCharacterData)
        nsCharacterData->SubstringData(aOffset, aCount, aDest.getNSString());
    return aDest;
}

/**
 * Call nsIDOMCharacterData::AppendData to append data to the character data.
 *
 * @param aSource the string that you want to append to the character data
 */
void CharacterData::appendData(const String& aSource)
{
    NSI_FROM_TX(CharacterData)

    if (nsCharacterData)
        nsCharacterData->AppendData(aSource.getConstNSString());
}

/**
 * Call nsIDOMCharacterData::InsertData to insert data into the character data.
 *
 * @param aOffset the offset at which you want to insert the string
 * @param aSource the string that you want to insert into the character data
 */
void CharacterData::insertData(PRInt32 aOffset, const String& aSource)
{
    NSI_FROM_TX(CharacterData)

    if (nsCharacterData)
        nsCharacterData->InsertData(aOffset, aSource.getConstNSString());
}

/**
 * Call nsIDOMCharacterData::DeleteData to delete data from the character data.
 *
 * @param aOffset the offset at which you want to delete data
 * @param aCount the number of chars you want to delete
 */
void CharacterData::deleteData(PRInt32 aOffset, PRInt32 aCount)
{
    NSI_FROM_TX(CharacterData)

    if (nsCharacterData)
        nsCharacterData->DeleteData(aOffset, aCount);
}

/**
 * Call nsIDOMCharacterData::ReplaceData to append data to the character data.
 *
 * @param aOffset the offset at which you want to replace the data
 * @param aCount the number of chars you want to replace
 * @param aSource the data that you want to replace it with
 */
void CharacterData::replaceData(PRInt32 aOffset, PRInt32 aCount,
        const String& aSource)
{
    NSI_FROM_TX(CharacterData)

    if (nsCharacterData)
        nsCharacterData->ReplaceData(aOffset, aCount, aSource.getConstNSString());
}
