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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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

txOutAttr::txOutAttr(PRInt32 aNsID, nsIAtom* aLocalName,
                         const nsAString& aValue) :
    mName(aNsID, aLocalName),
    mValue(aValue),
    mShorthand(MB_FALSE)
{
}

txXMLOutput::txXMLOutput(txOutputFormat* aFormat, ostream* aOut)
    : mOut(aOut),
      mUseEmptyElementShorthand(MB_TRUE),
      mHaveDocumentElement(MB_FALSE),
      mStartTagOpen(MB_FALSE),
      mAfterEndTag(MB_FALSE),
      mInCDATASection(MB_FALSE),
      mIndentLevel(0)
{
    mOutputFormat.merge(*aFormat);
    mOutputFormat.setFromDefaults();
}

txXMLOutput::~txXMLOutput()
{
}

void txXMLOutput::attribute(const nsAString& aName,
                            const PRInt32 aNsID,
                            const nsAString& aValue)
{
    if (!mStartTagOpen)
        // XXX Signal this? (can't add attributes after element closed)
        return;

    txListIterator iter(&mAttributes);
    nsCOMPtr<nsIAtom> localName = do_GetAtom(XMLUtils::getLocalPart(aName));
    txExpandedName att(aNsID, localName);

    txOutAttr* setAtt = 0;
    while ((setAtt = (txOutAttr*)iter.next())) {
         if (setAtt->mName == att) {
             setAtt->mValue = aValue;
             break;
         }
    }
    if (!setAtt) {
        setAtt = new txOutAttr(aNsID, localName, aValue);
        mAttributes.add(setAtt);
    }
}

void txXMLOutput::characters(const nsAString& aData, PRBool aDOE)
{
    closeStartTag(MB_FALSE);

    if (aDOE) {
        printUTF8Chars(aData);

        return;
    }

    if (mInCDATASection) {
        PRUint32 length = aData.Length();

        *mOut << CDATA_START;

        if (length <= 3) {
            printUTF8Chars(aData);
        }
        else {
            PRUint32 j = 0;
            nsAString::const_iterator iter;
            aData.BeginReading(iter);
            mBuffer[j++] = *(iter++);
            mBuffer[j++] = *(iter++);
            mBuffer[j++] = *(iter++);

            nsAString::const_iterator end;
            aData.EndReading(end);
            while (iter != end) {
                mBuffer[j++] = *(iter++);
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

void txXMLOutput::comment(const nsAString& aData)
{
    closeStartTag(MB_FALSE);

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

void txXMLOutput::endDocument(nsresult aResult)
{
}

void txXMLOutput::endElement(const nsAString& aName,
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
        printUTF8Chars(aName);
        *mOut << R_ANGLE_BRACKET;
    }
    if (mOutputFormat.mIndent == eTrue)
        *mOut << endl;
    mAfterEndTag = MB_TRUE;
    mInCDATASection = mCDATASections.pop() != 0;
}

void txXMLOutput::processingInstruction(const nsAString& aTarget,
                                        const nsAString& aData)
{
    closeStartTag(MB_FALSE);
    if (mOutputFormat.mIndent == eTrue) {
        for (PRUint32 i = 0; i < mIndentLevel; i++)
            *mOut << ' ';
    }
    *mOut << PI_START;
    printUTF8Chars(aTarget);
    *mOut << SPACE;
    printUTF8Chars(aData);
    *mOut << PI_END;
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
    *mOut << DOUBLE_QUOTE << " encoding=\"UTF-8\"" << PI_END << endl;
}

void txXMLOutput::startElement(const nsAString& aName,
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
    printUTF8Chars(aName);
    mStartTagOpen = MB_TRUE;
    if (mOutputFormat.mIndent == eTrue)
        mIndentLevel += DEFAULT_INDENT;

    mCDATASections.push((void*)mInCDATASection);
    mInCDATASection = MB_FALSE;

    nsCOMPtr<nsIAtom> localName = do_GetAtom(aName);
    txExpandedName currentElement(aNsID, localName);
    txListIterator iter(&(mOutputFormat.mCDATASectionElements));
    while (iter.hasNext()) {
        if (currentElement == *(txExpandedName*)iter.next()) {
            mInCDATASection = MB_TRUE;
            break;
        }
    }
}

void txXMLOutput::closeStartTag(MBool aUseEmptyElementShorthand)
{
    mAfterEndTag = aUseEmptyElementShorthand;
    if (mStartTagOpen) {
        txListIterator iter(&mAttributes);
        txOutAttr* att;
        while ((att = (txOutAttr*)iter.next())) {
            *mOut << SPACE;
            const char* attrVal;
            att->mName.mLocalName->GetUTF8String(&attrVal);
            *mOut << attrVal;
            if (!att->mShorthand) {
                *mOut << EQUALS << DOUBLE_QUOTE;
                printWithXMLEntities(att->mValue, MB_TRUE);
                *mOut << DOUBLE_QUOTE;
            }
            delete (txOutAttr*)iter.remove();
        }

        if (aUseEmptyElementShorthand)
            *mOut << FORWARD_SLASH;
        *mOut << R_ANGLE_BRACKET;
        mStartTagOpen = MB_FALSE;
    }
}

void txXMLOutput::printUTF8Char(PRUnichar& ch)
{
    // PRUnichar is 16-bits so we only need to cover up to 0xFFFF

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

void txXMLOutput::printUTF8Chars(const nsAString& aData)
{
    *mOut << NS_ConvertUCS2toUTF8(aData).get();
}

void txXMLOutput::printWithXMLEntities(const nsAString& aData,
                                       MBool aAttribute)
{
    nsAString::const_iterator iter, end;
    aData.EndReading(end);

    for (aData.BeginReading(iter); iter != end; ++iter) {
        PRUnichar currChar = *iter;
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
