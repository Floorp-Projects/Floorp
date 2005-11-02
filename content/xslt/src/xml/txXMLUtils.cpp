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
 * Lidong, lidong520@263.net
 *    -- unicode bug fix
 *
 */

/*
 * XML utility classes
 */

#include "XMLUtils.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "txAtoms.h"
#include "txStringUtils.h"
#include "txNamespaceMap.h"
#include "txXPathTreeWalker.h"

nsresult
txExpandedName::init(const nsAString& aQName, txNamespaceMap* aResolver,
                     MBool aUseDefault)
{
    const nsAFlatString& qName = PromiseFlatString(aQName);
    const PRUnichar* colon;
    PRBool valid = XMLUtils::isValidQName(qName, &colon);
    if (!valid) {
        return NS_ERROR_FAILURE;
    }

    if (colon) {
        nsCOMPtr<nsIAtom> prefix = do_GetAtom(Substring(qName.get(), colon));
        PRInt32 namespaceID = aResolver->lookupNamespace(prefix);
        if (namespaceID == kNameSpaceID_Unknown)
            return NS_ERROR_FAILURE;
        mNamespaceID = namespaceID;

        const PRUnichar *end;
        qName.EndReading(end);
        mLocalName = do_GetAtom(Substring(colon + 1, end));
    }
    else {
        mNamespaceID = aUseDefault ? aResolver->lookupNamespace(nsnull) :
                                     kNameSpaceID_None;
        mLocalName = do_GetAtom(aQName);
    }
    return NS_OK;
}

  //------------------------------/
 //- Implementation of XMLUtils -/
//------------------------------/

nsresult
XMLUtils::splitXMLName(const nsAString& aName, nsIAtom** aPrefix,
                       nsIAtom** aLocalName)
{
    const nsAFlatString& qName = PromiseFlatString(aName);
    const PRUnichar* colon;
    PRBool valid = XMLUtils::isValidQName(qName, &colon);
    if (!valid) {
        return NS_ERROR_FAILURE;
    }

    if (colon) {
        const PRUnichar *end;
        qName.EndReading(end);

        *aPrefix = NS_NewAtom(Substring(qName.get(), colon));
        *aLocalName = NS_NewAtom(Substring(colon + 1, end));
    }
    else {
        *aPrefix = nsnull;
        *aLocalName = NS_NewAtom(aName);
    }

    return NS_OK;
}

const nsDependentSubstring XMLUtils::getLocalPart(const nsAString& src)
{
    // Anything after ':' is the local part of the name
    PRInt32 idx = src.FindChar(':');
    if (idx == kNotFound) {
        return Substring(src, 0, src.Length());
    }

    NS_ASSERTION(idx > 0, "This QName looks invalid.");
    return Substring(src, idx + 1, src.Length() - (idx + 1));
}

/**
 * Returns true if the given string has only whitespace characters
 */
PRBool XMLUtils::isWhitespace(const nsAFlatString& aText)
{
    nsAFlatString::const_char_iterator start, end;
    aText.BeginReading(start);
    aText.EndReading(end);
    for ( ; start != end; ++start) {
        if (!isWhitespace(*start)) {
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}

/**
 * Normalizes the value of a XML processing instruction
**/
void XMLUtils::normalizePIValue(nsAString& piValue)
{
    nsAutoString origValue(piValue);
    PRUint32 origLength = origValue.Length();
    PRUint32 conversionLoop = 0;
    PRUnichar prevCh = 0;
    piValue.Truncate();

    while (conversionLoop < origLength) {
        PRUnichar ch = origValue.CharAt(conversionLoop);
        switch (ch) {
            case '>':
            {
                if (prevCh == '?') {
                    piValue.Append(PRUnichar(' '));
                }
                break;
            }
            default:
            {
                break;
            }
        }
        piValue.Append(ch);
        prevCh = ch;
        ++conversionLoop;
    }
}

//static
MBool XMLUtils::getXMLSpacePreserve(const txXPathNode& aNode)
{
    nsAutoString value;
    txXPathTreeWalker walker(aNode);
    do {
        if (walker.getAttr(txXMLAtoms::space, kNameSpaceID_XML, value)) {
            if (TX_StringEqualsAtom(value, txXMLAtoms::preserve)) {
                return PR_TRUE;
            }
            if (TX_StringEqualsAtom(value, txXMLAtoms::_default)) {
                return PR_FALSE;
            }
        }
    } while (walker.moveToParent());

    return PR_FALSE;
}
