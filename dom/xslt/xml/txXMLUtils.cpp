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
#include "nsContentUtils.h"

//------------------------------/
//- Implementation of XMLUtils -/
//------------------------------/

// static
nsresult
XMLUtils::splitExpatName(const char16_t *aExpatName, nsIAtom **aPrefix,
                         nsIAtom **aLocalName, int32_t* aNameSpaceID)
{
    /**
     *  Expat can send the following:
     *    localName
     *    namespaceURI<separator>localName
     *    namespaceURI<separator>localName<separator>prefix
     */

    const char16_t *uriEnd = nullptr;
    const char16_t *nameEnd = nullptr;
    const char16_t *pos;
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

    const char16_t *nameStart;
    if (uriEnd) {
        *aNameSpaceID =
            txNamespaceManager::getNamespaceID(nsDependentSubstring(aExpatName,
                                                                    uriEnd));
        if (*aNameSpaceID == kNameSpaceID_Unknown) {
            return NS_ERROR_FAILURE;
        }

        nameStart = (uriEnd + 1);
        if (nameEnd)  {
            const char16_t *prefixStart = nameEnd + 1;
            *aPrefix = NS_Atomize(Substring(prefixStart, pos)).take();
            if (!*aPrefix) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }
        else {
            nameEnd = pos;
            *aPrefix = nullptr;
        }
    }
    else {
        *aNameSpaceID = kNameSpaceID_None;
        nameStart = aExpatName;
        nameEnd = pos;
        *aPrefix = nullptr;
    }

    *aLocalName = NS_Atomize(Substring(nameStart, nameEnd)).take();

    return *aLocalName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
XMLUtils::splitQName(const nsAString& aName, nsIAtom** aPrefix,
                     nsIAtom** aLocalName)
{
    const nsAFlatString& qName = PromiseFlatString(aName);
    const char16_t* colon;
    bool valid = XMLUtils::isValidQName(qName, &colon);
    if (!valid) {
        return NS_ERROR_FAILURE;
    }

    if (colon) {
        const char16_t *end;
        qName.EndReading(end);

        *aPrefix = NS_Atomize(Substring(qName.get(), colon)).take();
        *aLocalName = NS_Atomize(Substring(colon + 1, end)).take();
    }
    else {
        *aPrefix = nullptr;
        *aLocalName = NS_Atomize(aName).take();
    }

    return NS_OK;
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
    uint32_t origLength = origValue.Length();
    uint32_t conversionLoop = 0;
    char16_t prevCh = 0;
    piValue.Truncate();

    while (conversionLoop < origLength) {
        char16_t ch = origValue.CharAt(conversionLoop);
        switch (ch) {
            case '>':
            {
                if (prevCh == '?') {
                    piValue.Append(char16_t(' '));
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
bool XMLUtils::isValidQName(const nsAFlatString& aQName,
                            const char16_t** aColon)
{
  return NS_SUCCEEDED(nsContentUtils::CheckQName(aQName, true, aColon));
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
