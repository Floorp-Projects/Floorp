/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsDependentSubstring.h"
#include "nsIAtom.h"
#include "txError.h"
#include "txXPathNode.h"

#ifndef TX_EXE
#include "nsIParserService.h"
#endif

class txNamespaceMap;

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
    
    nsresult init(const nsAString& aQName, txNamespaceMap* aResolver,
                  MBool aUseDefault);

    void reset()
    {
        mNamespaceID = kNameSpaceID_None;
        mLocalName = nsnull;
    }

    PRBool isNull()
    {
        return mNamespaceID == kNameSpaceID_None && !mLocalName;
    }

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

#ifdef TX_EXE
extern "C" int MOZ_XMLIsValidQName(const char* ptr, const char* end,
                                   int ns_aware, const char** colon);
extern "C" int MOZ_XMLIsLetter(const char* ptr);
extern "C" int MOZ_XMLIsNCNameChar(const char* ptr);
#else
extern nsIParserService *gTxParserService;
#endif

class XMLUtils {

public:
    static nsresult splitXMLName(const nsAString& aName, nsIAtom** aPrefix,
                                 nsIAtom** aLocalName);
    static const nsDependentSubstring getLocalPart(const nsAString& src);

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
     * Normalizes the value of a XML processingInstruction
    **/
    static void normalizePIValue(nsAString& attValue);

    /**
     * Returns true if the given string is a valid XML QName
     */
    static PRBool isValidQName(const nsAFlatString& aQName,
                               const PRUnichar** aColon)
    {
#ifdef TX_EXE
        const PRUnichar* end;
        aQName.EndReading(end);

        const char *colonPtr;
        PRBool valid = MOZ_XMLIsValidQName(NS_REINTERPRET_CAST(const char*,
                                                               aQName.get()),
                                           NS_REINTERPRET_CAST(const char*,
                                                               end),
                                           PR_TRUE, &colonPtr);
        *aColon = NS_REINTERPRET_CAST(const PRUnichar*, colonPtr);
        return valid;
#else
        return gTxParserService->IsValidQName(aQName, PR_TRUE, aColon);
#endif
    }

    /**
     * Returns true if the given character represents an Alpha letter
     */
    static PRBool isLetter(PRUnichar aChar)
    {
#ifdef TX_EXE
        return MOZ_XMLIsLetter(NS_REINTERPRET_CAST(const char*, &aChar));
#else
        return gTxParserService->IsXMLLetter(aChar);
#endif
    }

    /**
     * Returns true if the given character is an allowable NCName character
     */
    static PRBool isNCNameChar(PRUnichar aChar)
    {
#ifdef TX_EXE
        return MOZ_XMLIsNCNameChar(NS_REINTERPRET_CAST(const char*, &aChar));
#else
        return gTxParserService->IsXMLNCNameChar(aChar);
#endif
    }

    /*
     * Walks up the document tree and returns true if the closest xml:space
     * attribute is "preserve"
     */
    static MBool getXMLSpacePreserve(const txXPathNode& aNode);
};

#endif
