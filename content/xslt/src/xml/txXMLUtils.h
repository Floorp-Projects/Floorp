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
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Keith Visco <kvisco@ziplink.net> (Original Author)
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

/**
 * An XML Utility class
**/

#ifndef MITRE_XMLUTILS_H
#define MITRE_XMLUTILS_H

#include "txCore.h"
#include "nsCOMPtr.h"
#include "nsDependentSubstring.h"
#include "nsIAtom.h"
#include "txXPathNode.h"
#include "nsIParserService.h"
#include "nsContentUtils.h"

#define kExpatSeparatorChar 0xFFFF

class nsIAtom;
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

    nsresult init(const nsAString& aQName, txNamespaceMap* aResolver,
                  MBool aUseDefault);

    void reset()
    {
        mNamespaceID = kNameSpaceID_None;
        mLocalName = nsnull;
    }

    bool isNull()
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

class XMLUtils {

public:
    static nsresult splitExpatName(const PRUnichar *aExpatName,
                                   nsIAtom **aPrefix, nsIAtom **aLocalName,
                                   PRInt32* aNameSpaceID);
    static nsresult splitQName(const nsAString& aName, nsIAtom** aPrefix,
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
    static bool isWhitespace(const nsAFlatString& aText);

    /**
     * Normalizes the value of a XML processingInstruction
    **/
    static void normalizePIValue(nsAString& attValue);

    /**
     * Returns true if the given string is a valid XML QName
     */
    static bool isValidQName(const nsAFlatString& aQName,
                               const PRUnichar** aColon)
    {
        nsIParserService* ps = nsContentUtils::GetParserService();
        return ps && NS_SUCCEEDED(ps->CheckQName(aQName, PR_TRUE, aColon));
    }

    /**
     * Returns true if the given character represents an Alpha letter
     */
    static bool isLetter(PRUnichar aChar)
    {
        nsIParserService* ps = nsContentUtils::GetParserService();
        return ps && ps->IsXMLLetter(aChar);
    }

    /**
     * Returns true if the given character is an allowable NCName character
     */
    static bool isNCNameChar(PRUnichar aChar)
    {
        nsIParserService* ps = nsContentUtils::GetParserService();
        return ps && ps->IsXMLNCNameChar(aChar);
    }

    /*
     * Walks up the document tree and returns true if the closest xml:space
     * attribute is "preserve"
     */
    static MBool getXMLSpacePreserve(const txXPathNode& aNode);
};

#endif
