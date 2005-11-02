/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */

/**
 * An XML Utility class
**/

#ifndef MITRE_XMLUTILS_H
#define MITRE_XMLUTILS_H

#include "baseutils.h"
#include "txAtom.h"
#include "dom.h"
#include "txError.h"

class String;

class txExpandedName {
public:
    txExpandedName() : mNamespaceID(kNameSpaceID_None),
                       mLocalName(0)
    {
    }

    txExpandedName(PRInt32 aNsID,
                   txAtom* aLocalName) : mNamespaceID(aNsID),
                                         mLocalName(aLocalName)
    {
        TX_IF_ADDREF_ATOM(mLocalName);
    }

    txExpandedName(const txExpandedName& aOther) :
        mNamespaceID(aOther.mNamespaceID),
        mLocalName(aOther.mLocalName)
    {
        TX_IF_ADDREF_ATOM(mLocalName);
    }

    ~txExpandedName()
    {
        TX_IF_RELEASE_ATOM(mLocalName);
    }
    
    nsresult init(const String& aQName, Node* aResolver, MBool aUseDefault);

    txExpandedName& operator = (const txExpandedName& rhs)
    {
        mNamespaceID = rhs.mNamespaceID;
        TX_IF_RELEASE_ATOM(mLocalName);
        mLocalName = rhs.mLocalName;
        TX_IF_ADDREF_ATOM(mLocalName);
        return *this;
    }

    MBool operator == (const txExpandedName& rhs) const
    {
        return ((mLocalName == rhs.mLocalName) &&
                (mNamespaceID == rhs.mNamespaceID));
    }

    MBool operator != (const txExpandedName& rhs) const
    {
        return ((mLocalName != rhs.mLocalName) ||
                (mNamespaceID != rhs.mNamespaceID));
    }

    PRInt32 mNamespaceID;
    txAtom* mLocalName;
};

class XMLUtils {

public:

    static void getPrefix(const String& src, String& dest);
    static void getLocalPart(const String& src, String& dest);


    /**
     * Returns true if the given String is a valid XML QName
     */
    static MBool isValidQName(const String& aName);

    /*
     * Returns true if the given character is whitespace.
     */
    static MBool isWhitespace(const UNICODE_CHAR& aChar)
    {
        return (aChar <= ' ' &&
                (aChar == ' ' || aChar == '\r' ||
                 aChar == '\n'|| aChar == '\t'));
    }

    /**
     * Returns true if the given string has only whitespace characters
    **/
    static MBool isWhitespace(const String& aText);

    /**
     * Normalizes the value of a XML processingInstruction
    **/
    static void normalizePIValue(String& attValue);

    /*
     * Returns true if the given character represents a numeric letter (digit).
     */
    static MBool isDigit(UNICODE_CHAR ch);

    /*
     * Returns true if the given character represents an Alpha letter
     */
    static MBool isLetter(UNICODE_CHAR ch);

    /*
     * Returns true if the given character is an allowable NCName character
     */
    static MBool isNCNameChar(UNICODE_CHAR ch);
};

#endif

