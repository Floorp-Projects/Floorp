/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * XML utility classes
 */

#include "txXMLUtils.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsGkAtoms.h"
#include "txStringUtils.h"
#include "txNamespaceMap.h"
#include "txXPathTreeWalker.h"

nsresult
txExpandedName::init(const nsAString& aQName, txNamespaceMap* aResolver,
                     bool aUseDefault)
{
    const nsAFlatString& qName = PromiseFlatString(aQName);
    const PRUnichar* colon;
    bool valid = XMLUtils::isValidQName(qName, &colon);
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

// static
nsresult
XMLUtils::splitExpatName(const PRUnichar *aExpatName, nsIAtom **aPrefix,
                         nsIAtom **aLocalName, PRInt32* aNameSpaceID)
{
    /**
     *  Expat can send the following:
     *    localName
     *    namespaceURI<separator>localName
     *    namespaceURI<separator>localName<separator>prefix
     */

    const PRUnichar *uriEnd = nsnull;
    const PRUnichar *nameEnd = nsnull;
    const PRUnichar *pos;
    for (pos = aExpatName; *pos; ++pos) {
        if (*pos == kExpatSeparatorChar) {
            if (uriEnd) {
                nameEnd = pos;
            }
            else {
                uriEnd = pos;
            }
        }
    }

    const PRUnichar *nameStart;
    if (uriEnd) {
        *aNameSpaceID =
            txNamespaceManager::getNamespaceID(nsDependentSubstring(aExpatName,
                                                                    uriEnd));
        if (*aNameSpaceID == kNameSpaceID_Unknown) {
            return NS_ERROR_FAILURE;
        }

        nameStart = (uriEnd + 1);
        if (nameEnd)  {
            const PRUnichar *prefixStart = nameEnd + 1;
            *aPrefix = NS_NewAtom(Substring(prefixStart, pos));
            if (!*aPrefix) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }
        else {
            nameEnd = pos;
            *aPrefix = nsnull;
        }
    }
    else {
        *aNameSpaceID = kNameSpaceID_None;
        nameStart = aExpatName;
        nameEnd = pos;
        *aPrefix = nsnull;
    }

    *aLocalName = NS_NewAtom(Substring(nameStart, nameEnd));

    return *aLocalName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
XMLUtils::splitQName(const nsAString& aName, nsIAtom** aPrefix,
                     nsIAtom** aLocalName)
{
    const nsAFlatString& qName = PromiseFlatString(aName);
    const PRUnichar* colon;
    bool valid = XMLUtils::isValidQName(qName, &colon);
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
bool XMLUtils::isWhitespace(const nsAFlatString& aText)
{
    nsAFlatString::const_char_iterator start, end;
    aText.BeginReading(start);
    aText.EndReading(end);
    for ( ; start != end; ++start) {
        if (!isWhitespace(*start)) {
            return false;
        }
    }
    return true;
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
bool XMLUtils::getXMLSpacePreserve(const txXPathNode& aNode)
{
    nsAutoString value;
    txXPathTreeWalker walker(aNode);
    do {
        if (walker.getAttr(nsGkAtoms::space, kNameSpaceID_XML, value)) {
            if (TX_StringEqualsAtom(value, nsGkAtoms::preserve)) {
                return true;
            }
            if (TX_StringEqualsAtom(value, nsGkAtoms::_default)) {
                return false;
            }
        }
    } while (walker.moveToParent());

    return false;
}
