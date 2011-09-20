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
 *   Lidong <lidong520@263.net>
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
        if (walker.getAttr(nsGkAtoms::space, kNameSpaceID_XML, value)) {
            if (TX_StringEqualsAtom(value, nsGkAtoms::preserve)) {
                return PR_TRUE;
            }
            if (TX_StringEqualsAtom(value, nsGkAtoms::_default)) {
                return PR_FALSE;
            }
        }
    } while (walker.moveToParent());

    return PR_FALSE;
}
