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
#include "dom.h"
#include "nsDependentSubstring.h"
#include "txAtom.h"
#include "txError.h"

class txExpandedName {
public:
    txExpandedName() : mNamespaceID(kNameSpaceID_None)
    {
    }

    txExpandedName(PRInt32 aNsID,
                   nsIAtom* aLocalName) : mNamespaceID(aNsID),
                                          mLocalName(aLocalName)
    {
    }

    txExpandedName(const txExpandedName& aOther) :
        mNamespaceID(aOther.mNamespaceID),
        mLocalName(aOther.mLocalName)
    {
    }

    ~txExpandedName()
    {
    }
    
    nsresult init(const nsAString& aQName, Node* aResolver, MBool aUseDefault);

    txExpandedName& operator = (const txExpandedName& rhs)
    {
        mNamespaceID = rhs.mNamespaceID;
        mLocalName = rhs.mLocalName;
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
    nsCOMPtr<nsIAtom> mLocalName;
};

class XMLUtils {

public:

    static void getPrefix(const nsAString& src, nsIAtom** dest);
    static const nsDependentSubstring getLocalPart(const nsAString& src);
    static void getLocalPart(const nsAString& src, nsIAtom** dest);

    /**
     * Returns true if the given string is a valid XML QName
     */
    static MBool isValidQName(const nsAFlatString& aName);

    /*
     * Returns true if the given character is whitespace.
     */
    static MBool isWhitespace(const PRUnichar& aChar)
    {
        return (aChar <= ' ' &&
                (aChar == ' ' || aChar == '\r' ||
                 aChar == '\n'|| aChar == '\t'));
    }

    /**
     * Returns true if the given string has only whitespace characters
     */
    static PRBool isWhitespace(const nsAFlatString& aText);

    /**
     * Returns true if the given node's DOM nodevalue has only whitespace
     * characters
     */
    static PRBool isWhitespace(Node* aNode);

    /**
     * Normalizes the value of a XML processingInstruction
    **/
    static void normalizePIValue(nsAString& attValue);

    /*
     * Returns true if the given character represents a numeric letter (digit).
     */
    static MBool isDigit(PRUnichar ch);

    /*
     * Returns true if the given character represents an Alpha letter
     */
    static MBool isLetter(PRUnichar ch);

    /*
     * Returns true if the given character is an allowable NCName character
     */
    static MBool isNCNameChar(PRUnichar ch);

    /*
     * Walks up the document tree and returns true if the closest xml:space
     * attribute is "preserve"
     */
    static MBool getXMLSpacePreserve(Node* aNode);
};

#endif
