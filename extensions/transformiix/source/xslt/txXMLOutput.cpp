/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "txXMLOutput.h"

const int txXMLOutput::DEFAULT_INDENT = 2;

txAttribute::txAttribute(PRInt32 aNsID, txAtom* aLocalName, const String& aValue) :
    mName(aNsID, aLocalName),
    mValue(aValue),
    mShorthand(MB_FALSE)
{
}

txXMLOutput::txXMLOutput() : mUseEmptyElementShorthand(MB_TRUE),
                             mHaveDocumentElement(MB_FALSE),
                             mStartTagOpen(MB_FALSE),
                             mAfterEndTag(MB_FALSE),
                             mInCDATASection(MB_FALSE),
                             mIndentLevel(0)
{
}

txXMLOutput::~txXMLOutput()
{
}

void txXMLOutput::attribute(const String& aName,
                            const PRInt32 aNsID,
                            const String& aValue)
{
    if (!mStartTagOpen)
        // XXX Signal this? (can't add attributes after element closed)
        return;

    txListIterator iter(&mAttributes);
    String localPart;
    XMLUtils::getLocalPart(aName, localPart);
    txAtom* localName = TX_GET_ATOM(localPart);
    txExpandedName att(aNsID, localName);

    txAttribute* setAtt = 0;
    while ((setAtt = (txAttribute*)iter.next())) {
         if (setAtt->mName == att) {
             setAtt->mValue = aValue;
             break;
         }
    }
    if (!setAtt) {
        setAtt = new txAttribute(aNsID, localName, aValue);
        mAttributes.add(setAtt);
    }
    TX_IF_RELEASE_ATOM(localName);
}

void txXMLOutput::characters(const String& aData)
{
    closeStartTag(MB_FALSE);

    if (mInCDATASection) {
        PRInt32 i = 0;
        PRInt32 j = 0;
        PRInt32 length = aData.length();

        *mOut << CDATA_START;

        if (length <= 3) {
            printUTF8Chars(aData);
        }
        else {
            mBuffer[j++] = aData.charAt(i++);
            mBuffer[j++] = aData.charAt(i++);
            mBuffer[j++] = aData.charAt(i++);

            while (i < length) {
                mBuffer[j++] = aData.charAt(i++);
                if (mBuffer[(j - 1) % 4] == ']' &&
                    mBuffer[j % 4] == ']' &&
                    mBuffer[(j + 1) % 4] == '>') {
                    *mOut << CDATA_END;
                    *mOut << CDATA_START;
                }
                j = j % 4;
                printUTF8Char(mBuffer[j]);
            }

            j = ++j % 4;
            printUTF8Char(mBuffer[j]);
            j = ++j % 4;
            printUTF8Char(mBuffer[j]);
            j = ++j % 4;
            printUTF8Char(mBuffer[j]);
        }

        *mOut << CDATA_END;
    }
    else {
        printWithXMLEntities(aData);
    }
}

void txXMLOutput::charactersNoOutputEscaping(const String& aData)
{
    closeStartTag(MB_FALSE);

    printUTF8Chars(aData);
}

void txXMLOutput::comment(const String& aData)
{
    closeStartTag(MB_FALSE);

    if (&aData == &NULL_STRING)
        return;

    if (mOutputFormat.mIndent == eTrue) {
        for (PRUint32 i = 0; i < mIndentLevel; i++)
            *mOut << ' ';
    }
    *mOut << COMMENT_START;
    printUTF8Chars(aData);
    *mOut << COMMENT_END;
    if (mOutputFormat.mIndent == eTrue)
        *mOut << endl;
}

void txXMLOutput::endDocument()
{
}

void txXMLOutput::endElement(const String& aName,
                             const PRInt32 aNsID)
{
    MBool newLine = (mOutputFormat.mIndent == eTrue) && mAfterEndTag;
    MBool writeEndTag = !(mStartTagOpen && mUseEmptyElementShorthand);
    closeStartTag(mUseEmptyElementShorthand);
    if (newLine)
        *mOut << endl;
    if (mOutputFormat.mIndent == eTrue)
        mIndentLevel -= DEFAULT_INDENT;
    if (writeEndTag) {
        if (newLine) {
            for (PRUint32 i = 0; i < mIndentLevel; i++)
                *mOut << ' ';
        }
        *mOut << L_ANGLE_BRACKET << FORWARD_SLASH;
        *mOut << aName;
        *mOut << R_ANGLE_BRACKET;
    }
    if (mOutputFormat.mIndent == eTrue)
        *mOut << endl;
    mAfterEndTag = MB_TRUE;
    mInCDATASection = (MBool)mCDATASections.pop();
}

void txXMLOutput::processingInstruction(const String& aTarget,
                                        const String& aData)
{
    closeStartTag(MB_FALSE);
    if (mOutputFormat.mIndent == eTrue) {
        for (PRUint32 i = 0; i < mIndentLevel; i++)
            *mOut << ' ';
    }
    *mOut << PI_START << aTarget << SPACE << aData << PI_END;
    if (mOutputFormat.mIndent == eTrue)
        *mOut << endl;
}

void txXMLOutput::startDocument()
{
    if (mOutputFormat.mMethod == eMethodNotSet) {
        // XXX We should "cache" content until we have a 
        //     document element
    }
    *mOut << PI_START << XML_DECL << DOUBLE_QUOTE;
    *mOut << XML_VERSION;
    *mOut << DOUBLE_QUOTE << PI_END << endl;
}

void txXMLOutput::startElement(const String& aName,
                               const PRInt32 aNsID)
{
    if (!mHaveDocumentElement) {
        // XXX Output doc type and "cached" content
        mHaveDocumentElement = MB_TRUE;
    }

    MBool newLine = mStartTagOpen || mAfterEndTag;
    closeStartTag(MB_FALSE);

    if (mOutputFormat.mIndent == eTrue) {
        if (newLine) {
            *mOut << endl;
            for (PRUint32 i = 0; i < mIndentLevel; i++)
                *mOut << ' ';
        }
    }
    *mOut << L_ANGLE_BRACKET;
    *mOut << aName;
    mStartTagOpen = MB_TRUE;
    if (mOutputFormat.mIndent == eTrue)
        mIndentLevel += DEFAULT_INDENT;

    mCDATASections.push((void*)mInCDATASection);
    mInCDATASection = MB_FALSE;

    txAtom* localName = TX_GET_ATOM(aName);
    txExpandedName currentElement(aNsID, localName);
    TX_IF_RELEASE_ATOM(localName);
    txListIterator iter(&(mOutputFormat.mCDATASectionElements));
    while (iter.hasNext()) {
        if (currentElement == *(txExpandedName*)iter.next()) {
            mInCDATASection = MB_TRUE;
            break;
        }
    }
}

void txXMLOutput::getOutputStream(ostream** aOutputStream)
{
    if (aOutputStream)
        *aOutputStream = mOut;
}

void txXMLOutput::setOutputStream(ostream* aOutputStream)
{
    mOut = aOutputStream;
}

void txXMLOutput::setOutputFormat(txOutputFormat* aOutputFormat)
{
    mOutputFormat.reset();
    mOutputFormat.merge(*aOutputFormat);
    mOutputFormat.setFromDefaults();
}

void txXMLOutput::closeStartTag(MBool aUseEmptyElementShorthand)
{
    mAfterEndTag = aUseEmptyElementShorthand;
    if (mStartTagOpen) {
        txListIterator iter(&mAttributes);
        txAttribute* att;
        while ((att = (txAttribute*)iter.next())) {
            *mOut << SPACE;
            *mOut << *(att->mName.mLocalName);
            if (!att->mShorthand) {
                *mOut << EQUALS << DOUBLE_QUOTE;
                printWithXMLEntities(att->mValue, MB_TRUE);
                *mOut << DOUBLE_QUOTE;
            }
            delete (txAttribute*)iter.remove();
        }

        if (aUseEmptyElementShorthand)
            *mOut << FORWARD_SLASH;
        *mOut << R_ANGLE_BRACKET;
        mStartTagOpen = MB_FALSE;
    }
}

void txXMLOutput::printUTF8Char(DOM_CHAR& ch)
{
    // DOM_CHAR is 16-bits so we only need to cover up to 0xFFFF

    // 0x0000-0x007F
    if (ch < 128) {
        *mOut << (char)ch;
    }
    // 0x0080-0x07FF
    else if (ch < 2048) {
        *mOut << (char) (192+(ch/64));        // 0xC0 + x/64
        *mOut << (char) (128+(ch%64));        // 0x80 + x%64
    }
    // 0x800-0xFFFF
    else {
        *mOut << (char) (224+(ch/4096));      // 0xE0 + x/64^2
        *mOut << (char) (128+((ch/64)%64));   // 0x80 + (x/64)%64
        *mOut << (char) (128+(ch%64));        // 0x80 + x%64
    }
}

void txXMLOutput::printUTF8Chars(const String& aData)
{
    DOM_CHAR currChar;
    PRInt32 i = 0;

    while (i < aData.length()) {
        currChar = aData.charAt(i++);
        printUTF8Char(currChar);
    }
}

void txXMLOutput::printWithXMLEntities(const String& aData,
                                       MBool aAttribute)
{
    DOM_CHAR currChar;
    PRInt32 i;

    if (&aData == &NULL_STRING)
        return;

    for (i = 0; i < aData.length(); i++) {
        currChar = aData.charAt(i);
        switch (currChar) {
            case AMPERSAND:
                *mOut << AMP_ENTITY;
                break;
            case APOSTROPHE:
                if (aAttribute)
                    *mOut << APOS_ENTITY;
                else
                    printUTF8Char(currChar);
                break;
            case GT:
                *mOut << GT_ENTITY;
                break;
            case LT:
                *mOut << LT_ENTITY;
                break;
            case QUOTE:
                if (aAttribute)
                    *mOut << QUOT_ENTITY;
                else
                    printUTF8Char(currChar);
                break;
            default:
                printUTF8Char(currChar);
                break;
        }
    }
    *mOut << flush;
}
