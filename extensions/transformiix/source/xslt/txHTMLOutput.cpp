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

#include "txHTMLOutput.h"
#include "txAtoms.h"
#include "txOutputFormat.h"
#include "XMLUtils.h"

txHTMLOutput::txHTMLOutput()
{
    mUseEmptyElementShorthand = MB_FALSE;

    mHTMLEmptyTags.setOwnership(Map::eOwnsNone);
    mHTMLEmptyTags.put(txHTMLAtoms::area, txHTMLAtoms::area);
    mHTMLEmptyTags.put(txHTMLAtoms::base, txHTMLAtoms::base);
    mHTMLEmptyTags.put(txHTMLAtoms::basefont, txHTMLAtoms::basefont);
    mHTMLEmptyTags.put(txHTMLAtoms::br, txHTMLAtoms::br);
    mHTMLEmptyTags.put(txHTMLAtoms::col, txHTMLAtoms::col);
    mHTMLEmptyTags.put(txHTMLAtoms::frame, txHTMLAtoms::frame);
    mHTMLEmptyTags.put(txHTMLAtoms::hr, txHTMLAtoms::hr);    
    mHTMLEmptyTags.put(txHTMLAtoms::img, txHTMLAtoms::img);
    mHTMLEmptyTags.put(txHTMLAtoms::input, txHTMLAtoms::input);
    mHTMLEmptyTags.put(txHTMLAtoms::isindex, txHTMLAtoms::isindex);
    mHTMLEmptyTags.put(txHTMLAtoms::link, txHTMLAtoms::link);
    mHTMLEmptyTags.put(txHTMLAtoms::meta, txHTMLAtoms::meta);
    mHTMLEmptyTags.put(txHTMLAtoms::param, txHTMLAtoms::param);

    mHTMLEmptyAttributes.setOwnership(Map::eOwnsItems);

    // checked
    txList* elementList = new List;
    if (!elementList)
        return;
    elementList->add(txHTMLAtoms::input);
    mHTMLEmptyAttributes.put(txHTMLAtoms::checked, elementList);

    // compact
    elementList = new List;
    if (!elementList)
        return;
    elementList->add(txHTMLAtoms::dir);
    elementList->add(txHTMLAtoms::dl);
    elementList->add(txHTMLAtoms::menu);
    elementList->add(txHTMLAtoms::ol);
    elementList->add(txHTMLAtoms::ul);
    mHTMLEmptyAttributes.put(txHTMLAtoms::compact, elementList);

    // declare
    elementList = new List;
    if (!elementList)
        return;
    elementList->add(txHTMLAtoms::object);
    mHTMLEmptyAttributes.put(txHTMLAtoms::declare, elementList);

    // defer
    elementList = new List;
    if (!elementList)
        return;
    elementList->add(txHTMLAtoms::script);
    mHTMLEmptyAttributes.put(txHTMLAtoms::defer, elementList);

    // disabled
    elementList = new List;
    if (!elementList)
        return;
    elementList->add(txHTMLAtoms::button);
    elementList->add(txHTMLAtoms::input);
    elementList->add(txHTMLAtoms::optgroup);
    elementList->add(txHTMLAtoms::option);
    elementList->add(txHTMLAtoms::select);
    elementList->add(txHTMLAtoms::textarea);
    mHTMLEmptyAttributes.put(txHTMLAtoms::disabled, elementList);

    // ismap
    elementList = new List;
    if (!elementList)
        return;
    elementList->add(txHTMLAtoms::img);
    elementList->add(txHTMLAtoms::input);
    mHTMLEmptyAttributes.put(txHTMLAtoms::ismap, elementList);

    // multiple
    elementList = new List;
    if (!elementList)
        return;
    elementList->add(txHTMLAtoms::select);
    mHTMLEmptyAttributes.put(txHTMLAtoms::multiple, elementList);

    // noresize
    elementList = new List;
    if (!elementList)
        return;
    elementList->add(txHTMLAtoms::frame);
    mHTMLEmptyAttributes.put(txHTMLAtoms::noresize, elementList);

    // noshade
    elementList = new List;
    if (!elementList)
        return;
    elementList->add(txHTMLAtoms::hr);
    mHTMLEmptyAttributes.put(txHTMLAtoms::noshade, elementList);

    // nowrap
    elementList = new List;
    if (!elementList)
        return;
    elementList->add(txHTMLAtoms::td);
    elementList->add(txHTMLAtoms::th);
    mHTMLEmptyAttributes.put(txHTMLAtoms::nowrap, elementList);

    // readonly
    elementList = new List;
    if (!elementList)
        return;
    elementList->add(txHTMLAtoms::input);
    elementList->add(txHTMLAtoms::textarea);
    mHTMLEmptyAttributes.put(txHTMLAtoms::readonly, elementList);

    // selected
    elementList = new List;
    if (!elementList)
        return;
    elementList->add(txHTMLAtoms::option);
    mHTMLEmptyAttributes.put(txHTMLAtoms::selected, elementList);
}

txHTMLOutput::~txHTMLOutput()
{
}

void txHTMLOutput::attribute(const String& aName,
                             const PRInt32 aNsID,
                             const String& aValue)
{
    if (!mStartTagOpen)
        // XXX Signal this? (can't add attributes after element closed)
        return;

    MBool shortHand = MB_FALSE;
    if (aNsID == kNameSpaceID_None) {
        String localPart;
        XMLUtils::getLocalPart(aName, localPart);
        shortHand = isShorthandAttribute(localPart);
        if (shortHand && localPart.isEqualIgnoreCase(aValue)) {
            txListIterator iter(&mAttributes);
            txAttribute* setAtt = 0;
            txAtom* localName = TX_GET_ATOM(localPart);
            txExpandedName att(aNsID, localName);
            while ((setAtt = (txAttribute*)iter.next())) {
                 if (setAtt->mName == att) {
                     setAtt->mShorthand = MB_TRUE;
                     break;
                 }
            }
            if (!setAtt) {
                setAtt = new txAttribute(aNsID, localName, "");
                setAtt->mShorthand = MB_TRUE;
                mAttributes.add(setAtt);
            }
            TX_IF_RELEASE_ATOM(localName);
        }
    }
    if (!shortHand)
        txXMLOutput::attribute(aName, aNsID, aValue);
}

void txHTMLOutput::characters(const String& aData)
{
    // Special-case script and style
    txExpandedName* currentElement = (txExpandedName*)mCurrentElements.peek();
    if (currentElement &&
        (currentElement->mNamespaceID == kNameSpaceID_None) &&
        ((currentElement->mLocalName == txHTMLAtoms::script) ||
         (currentElement->mLocalName == txHTMLAtoms::style))) {
        closeStartTag(MB_FALSE);
        printUTF8Chars(aData);
    }
    else {
        txXMLOutput::characters(aData);
    }
}

void txHTMLOutput::endElement(const String& aName,
                              const PRInt32 aNsID)
{
    if ((aNsID == kNameSpaceID_None) && isShorthandElement(aName) &&
        mStartTagOpen) {
        MBool newLine = (mOutputFormat.mIndent == eTrue) &&
                        mAfterEndTag;
        closeStartTag(MB_FALSE);
        if (newLine)
            *mOut << endl;
        if (mOutputFormat.mIndent == eTrue)
            mIndentLevel -= DEFAULT_INDENT;
        mAfterEndTag = MB_TRUE;
    }
    else {
        txXMLOutput::endElement(aName, aNsID);
    }
    delete (txExpandedName*)mCurrentElements.pop();
}

void txHTMLOutput::processingInstruction(const String& aTarget, const String& aData)
{
    closeStartTag(MB_FALSE);
    if (mOutputFormat.mIndent == eTrue) {
        for (PRUint32 i = 0; i < mIndentLevel; i++)
            *mOut << ' ';
    }
    *mOut << PI_START << aTarget << SPACE << aData << R_ANGLE_BRACKET;
    if (mOutputFormat.mIndent == eTrue)
        *mOut << endl;
}

void txHTMLOutput::startDocument()
{
    // XXX Should be using mOutputFormat.getVersion
    *mOut << DOCTYPE_START << "html " << PUBLIC;
    *mOut << " \"-//W3C//DTD HTML 4.0 Transitional//EN\"";
    *mOut << " \"http://www.w3.org/TR/REC-html40/loose.dtd\"";
    *mOut << DOCTYPE_END << endl;
}

void txHTMLOutput::startElement(const String& aName,
                                const PRInt32 aNsID)
{
    txXMLOutput::startElement(aName, aNsID);

    txAtom* localAtom;
    if (aNsID == kNameSpaceID_None) {
        String localName(aName);
        localName.toLowerCase();
        localAtom = TX_GET_ATOM(localName);
    }
    else {
        localAtom = TX_GET_ATOM(aName);
    }
    NS_ASSERTION(localAtom, "Can't get atom");
    txExpandedName* currentElement = new txExpandedName(aNsID, localAtom);
    TX_IF_RELEASE_ATOM(localAtom);
    NS_ASSERTION(currentElement, "Can't create currentElement");
    if (currentElement)
        mCurrentElements.push(currentElement);
}

void txHTMLOutput::closeStartTag(MBool aUseEmptyElementShorthand)
{
    txExpandedName* currentElement = (txExpandedName*)mCurrentElements.peek();
    if (mStartTagOpen && currentElement &&
        (currentElement->mNamespaceID == kNameSpaceID_None) &&
        (currentElement->mLocalName == txHTMLAtoms::head)) {
        txXMLOutput::closeStartTag(MB_FALSE);
        if (mOutputFormat.mIndent == eTrue) {
            *mOut << endl;
            for (PRUint32 i = 0; i < mIndentLevel; i++)
                *mOut << ' ';
        }
        *mOut << LT << "meta http-equiv=" << QUOTE << "Content-Type" << QUOTE;
        *mOut << " content=" << QUOTE << mOutputFormat.mMediaType << ";";
        *mOut << " charset=" << mOutputFormat.mEncoding << QUOTE << GT;
    }
    else {
        txXMLOutput::closeStartTag(aUseEmptyElementShorthand);
    }
}

MBool txHTMLOutput::isShorthandElement(const String& aName)
{
    String localName;
    XMLUtils::getLocalPart(aName, localName);
    localName.toLowerCase();
    txAtom* localAtom = TX_GET_ATOM(localName);
    if (localAtom && mHTMLEmptyTags.get(localAtom))
        return MB_TRUE;
    return MB_FALSE;
}

MBool txHTMLOutput::isShorthandAttribute(const String& aLocalName)
{
    String localName(aLocalName);
    localName.toLowerCase();
    txAtom* localAtom = TX_GET_ATOM(localName);
    txList* elements = (txList*)mHTMLEmptyAttributes.get(localAtom);
    if (localAtom && (elements)) {
        txExpandedName* currentElement = (txExpandedName*)mCurrentElements.peek();
        txListIterator iter(elements);
        while (iter.hasNext()) {
             if ((txAtom*)iter.next() == currentElement->mLocalName)
                 return MB_TRUE;
        }
    }
    return MB_FALSE;
}
