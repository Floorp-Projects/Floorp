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
 * Lidong, lidong520@263.net
 *    -- unicode bug fix
 *
 */

/*
 * XML utility classes
 */

#include "XMLUtils.h"
#include "txAtoms.h"

nsresult txExpandedName::init(const String& aQName,
                              Node* aResolver,
                              MBool aUseDefault)
{
    NS_ASSERTION(aResolver, "missing resolve node");
    if (!XMLUtils::isValidQName(aQName))
        return NS_ERROR_FAILURE;

    PRInt32 idx = aQName.indexOf(':');
    if (idx != kNotFound) {
        String localName, prefixStr;
        aQName.subString(0, (PRUint32)idx, prefixStr);
        txAtom* prefix = TX_GET_ATOM(prefixStr);
        PRInt32 namespaceID = aResolver->lookupNamespaceID(prefix);
        if (namespaceID == kNameSpaceID_Unknown)
            return NS_ERROR_FAILURE;
        mNamespaceID = namespaceID;

        aQName.subString((PRUint32)idx + 1, localName);
        TX_IF_RELEASE_ATOM(mLocalName);
        mLocalName = TX_GET_ATOM(localName);
    }
    else {
        TX_IF_RELEASE_ATOM(mLocalName);
        mLocalName = TX_GET_ATOM(aQName);
        if (aUseDefault)
            mNamespaceID = aResolver->lookupNamespaceID(0);
        else
            mNamespaceID = kNameSpaceID_None;
    }
    return NS_OK;
}

  //------------------------------/
 //- Implementation of XMLUtils -/
//------------------------------/

void XMLUtils::getPrefix(const String& src, String& dest)
{
    // Anything preceding ':' is the namespace part of the name
    PRInt32 idx = src.indexOf(':');
    if (idx == kNotFound) {
        return;
    }
    // Use a temporary String to prevent any chars in dest
    // from being lost.
    NS_ASSERTION(idx > 0, "This QName looks invalid.");
    String tmp;
    src.subString(0, (PRUint32)idx, tmp);
    dest.append(tmp);
}

void XMLUtils::getLocalPart(const String& src, String& dest)
{
    // Anything after ':' is the local part of the name
    PRInt32 idx = src.indexOf(':');
    if (idx == kNotFound) {
        dest.append(src);
        return;
    }
    // Use a temporary String to prevent any chars in dest
    // from being lost.
    NS_ASSERTION(idx > 0, "This QName looks invalid.");
    String tmp;
    src.subString((PRUint32)idx + 1, tmp);
    dest.append(tmp);
}

MBool XMLUtils::isValidQName(const String& aName)
{
    if (aName.isEmpty()) {
        return MB_FALSE;
    }

    if (!isLetter(aName.charAt(0))) {
        return MB_FALSE;
    }

    PRUint32 size = aName.length();
    PRUint32 i;
    MBool foundColon = MB_FALSE;
    for (i = 1; i < size; ++i) {
        UNICODE_CHAR character = aName.charAt(i);
        if (character == ':') {
            foundColon = MB_TRUE;
            ++i;
            break;
        }
        if (!isNCNameChar(character)) {
            return MB_FALSE;
        }
    }
    if (i == size) {
        // If we found a colon as the last letter it is not
        // a valid QName.
        return !foundColon;
    }
    if (!isLetter(aName.charAt(i))) {
        return MB_FALSE;
    }
    for (++i; i < size; ++i) {
        if (!isNCNameChar(aName.charAt(i))) {
            return MB_FALSE;
        }
    }
    return MB_TRUE;
}

/**
 * Returns true if the given string has only whitespace characters
**/
MBool XMLUtils::isWhitespace(const String& aText)
{
    PRUint32 size = aText.length();
    PRUint32 i;
    for (i = 0; i < size; ++i) {
        if (!isWhitespace(aText.charAt(i))) {
            return MB_FALSE;
        }
    }
    return MB_TRUE;
}

/**
 * Normalizes the value of a XML processing instruction
**/
void XMLUtils::normalizePIValue(String& piValue)
{
    String origValue(piValue);
    PRUint32 origLength = origValue.length();
    PRUint32 conversionLoop = 0;
    UNICODE_CHAR prevCh = 0;
    piValue.clear();

    while (conversionLoop < origLength) {
        UNICODE_CHAR ch = origValue.charAt(conversionLoop);
        switch (ch) {
            case '>':
            {
                if (prevCh == '?') {
                    piValue.append(' ');
                }
                break;
            }
            default:
            {
                break;
            }
        }
        piValue.append(ch);
        prevCh = ch;
        ++conversionLoop;
    }
}

/*
 * Walks up the document tree and returns true if the closest xml:space
 * attribute is "preserve"
 */
//static
MBool XMLUtils::getXMLSpacePreserve(Node* aNode)
{
    NS_ASSERTION(aNode, "Calling preserveXMLSpace with NULL node!");

    String value;
    Node* parent = aNode;
    while (parent) {
        if (parent->getNodeType() == Node::ELEMENT_NODE) {
            Element* elem = (Element*)parent;
            if (elem->getAttr(txXMLAtoms::space, kNameSpaceID_XML, value)) {
                txAtom* val = TX_GET_ATOM(value);
                if (val == txXMLAtoms::preserve) {
                    TX_IF_RELEASE_ATOM(val);
                    return MB_TRUE;
                }
                if (val == txXMLAtoms::_default) {
                    TX_IF_RELEASE_ATOM(val);
                    return MB_FALSE;
                }
            }
        }
        parent = parent->getParentNode();
    }
    return MB_FALSE;
}

// macros for inclusion of char range headers
#define TX_CHAR_RANGE(ch, a, b) if (ch < a) return MB_FALSE; \
    if (ch <= b) return MB_TRUE
#define TX_MATCH_CHAR(ch, a) if (ch < a) return MB_FALSE; \
    if (ch == a) return MB_TRUE

MBool XMLUtils::isDigit(UNICODE_CHAR ch)
{
    TX_CHAR_RANGE(ch, 0x0030, 0x0039);  /* '0'-'9' */
    TX_CHAR_RANGE(ch, 0x0660, 0x0669);
    TX_CHAR_RANGE(ch, 0x06F0, 0x06F9);
    TX_CHAR_RANGE(ch, 0x0966, 0x096F);
    TX_CHAR_RANGE(ch, 0x09E6, 0x09EF);
    TX_CHAR_RANGE(ch, 0x0A66, 0x0A6F);
    TX_CHAR_RANGE(ch, 0x0AE6, 0x0AEF);
    TX_CHAR_RANGE(ch, 0x0B66, 0x0B6F);
    TX_CHAR_RANGE(ch, 0x0BE7, 0x0BEF);
    TX_CHAR_RANGE(ch, 0x0C66, 0x0C6F);
    TX_CHAR_RANGE(ch, 0x0CE6, 0x0CEF);
    TX_CHAR_RANGE(ch, 0x0D66, 0x0D6F);
    TX_CHAR_RANGE(ch, 0x0E50, 0x0E59);
    TX_CHAR_RANGE(ch, 0x0ED0, 0x0ED9);
    TX_CHAR_RANGE(ch, 0x0F20, 0x0F29);
    return MB_FALSE;
}

MBool XMLUtils::isLetter(UNICODE_CHAR ch)
{
/* Letter = BaseChar | Ideographic; and _ */
    TX_CHAR_RANGE(ch, 0x0041, 0x005A);
    TX_MATCH_CHAR(ch, 0x005F);        /* '_' */
    TX_CHAR_RANGE(ch, 0x0061, 0x007A);
    TX_CHAR_RANGE(ch, 0x00C0, 0x00D6);
    TX_CHAR_RANGE(ch, 0x00D8, 0x00F6);
    TX_CHAR_RANGE(ch, 0x00F8, 0x00FF);
    TX_CHAR_RANGE(ch, 0x0100, 0x0131);
    TX_CHAR_RANGE(ch, 0x0134, 0x013E);
    TX_CHAR_RANGE(ch, 0x0141, 0x0148);
    TX_CHAR_RANGE(ch, 0x014A, 0x017E);
    TX_CHAR_RANGE(ch, 0x0180, 0x01C3);
    TX_CHAR_RANGE(ch, 0x01CD, 0x01F0);
    TX_CHAR_RANGE(ch, 0x01F4, 0x01F5);
    TX_CHAR_RANGE(ch, 0x01FA, 0x0217);
    TX_CHAR_RANGE(ch, 0x0250, 0x02A8);
    TX_CHAR_RANGE(ch, 0x02BB, 0x02C1);
    TX_MATCH_CHAR(ch, 0x0386);
    TX_CHAR_RANGE(ch, 0x0388, 0x038A);
    TX_MATCH_CHAR(ch, 0x038C);
    TX_CHAR_RANGE(ch, 0x038E, 0x03A1);
    TX_CHAR_RANGE(ch, 0x03A3, 0x03CE);
    TX_CHAR_RANGE(ch, 0x03D0, 0x03D6);
    TX_MATCH_CHAR(ch, 0x03DA);
    TX_MATCH_CHAR(ch, 0x03DC);
    TX_MATCH_CHAR(ch, 0x03DE);
    TX_MATCH_CHAR(ch, 0x03E0);
    TX_CHAR_RANGE(ch, 0x03E2, 0x03F3);
    TX_CHAR_RANGE(ch, 0x0401, 0x040C);
    TX_CHAR_RANGE(ch, 0x040E, 0x044F);
    TX_CHAR_RANGE(ch, 0x0451, 0x045C);
    TX_CHAR_RANGE(ch, 0x045E, 0x0481);
    TX_CHAR_RANGE(ch, 0x0490, 0x04C4);
    TX_CHAR_RANGE(ch, 0x04C7, 0x04C8);
    TX_CHAR_RANGE(ch, 0x04CB, 0x04CC);
    TX_CHAR_RANGE(ch, 0x04D0, 0x04EB);
    TX_CHAR_RANGE(ch, 0x04EE, 0x04F5);
    TX_CHAR_RANGE(ch, 0x04F8, 0x04F9);
    TX_CHAR_RANGE(ch, 0x0531, 0x0556);
    TX_MATCH_CHAR(ch, 0x0559);
    TX_CHAR_RANGE(ch, 0x0561, 0x0586);
    TX_CHAR_RANGE(ch, 0x05D0, 0x05EA);
    TX_CHAR_RANGE(ch, 0x05F0, 0x05F2);
    TX_CHAR_RANGE(ch, 0x0621, 0x063A);
    TX_CHAR_RANGE(ch, 0x0641, 0x064A);
    TX_CHAR_RANGE(ch, 0x0671, 0x06B7);
    TX_CHAR_RANGE(ch, 0x06BA, 0x06BE);
    TX_CHAR_RANGE(ch, 0x06C0, 0x06CE);
    TX_CHAR_RANGE(ch, 0x06D0, 0x06D3);
    TX_MATCH_CHAR(ch, 0x06D5);
    TX_CHAR_RANGE(ch, 0x06E5, 0x06E6);
    TX_CHAR_RANGE(ch, 0x0905, 0x0939);
    TX_MATCH_CHAR(ch, 0x093D);
    TX_CHAR_RANGE(ch, 0x0958, 0x0961);
    TX_CHAR_RANGE(ch, 0x0985, 0x098C);
    TX_CHAR_RANGE(ch, 0x098F, 0x0990);
    TX_CHAR_RANGE(ch, 0x0993, 0x09A8);
    TX_CHAR_RANGE(ch, 0x09AA, 0x09B0);
    TX_MATCH_CHAR(ch, 0x09B2);
    TX_CHAR_RANGE(ch, 0x09B6, 0x09B9);
    TX_CHAR_RANGE(ch, 0x09DC, 0x09DD);
    TX_CHAR_RANGE(ch, 0x09DF, 0x09E1);
    TX_CHAR_RANGE(ch, 0x09F0, 0x09F1);
    TX_CHAR_RANGE(ch, 0x0A05, 0x0A0A);
    TX_CHAR_RANGE(ch, 0x0A0F, 0x0A10);
    TX_CHAR_RANGE(ch, 0x0A13, 0x0A28);
    TX_CHAR_RANGE(ch, 0x0A2A, 0x0A30);
    TX_CHAR_RANGE(ch, 0x0A32, 0x0A33);
    TX_CHAR_RANGE(ch, 0x0A35, 0x0A36);
    TX_CHAR_RANGE(ch, 0x0A38, 0x0A39);
    TX_CHAR_RANGE(ch, 0x0A59, 0x0A5C);
    TX_MATCH_CHAR(ch, 0x0A5E);
    TX_CHAR_RANGE(ch, 0x0A72, 0x0A74);
    TX_CHAR_RANGE(ch, 0x0A85, 0x0A8B);
    TX_MATCH_CHAR(ch, 0x0A8D);
    TX_CHAR_RANGE(ch, 0x0A8F, 0x0A91);
    TX_CHAR_RANGE(ch, 0x0A93, 0x0AA8);
    TX_CHAR_RANGE(ch, 0x0AAA, 0x0AB0);
    TX_CHAR_RANGE(ch, 0x0AB2, 0x0AB3);
    TX_CHAR_RANGE(ch, 0x0AB5, 0x0AB9);
    TX_MATCH_CHAR(ch, 0x0ABD);
    TX_MATCH_CHAR(ch, 0x0AE0);
    TX_CHAR_RANGE(ch, 0x0B05, 0x0B0C);
    TX_CHAR_RANGE(ch, 0x0B0F, 0x0B10);
    TX_CHAR_RANGE(ch, 0x0B13, 0x0B28);
    TX_CHAR_RANGE(ch, 0x0B2A, 0x0B30);
    TX_CHAR_RANGE(ch, 0x0B32, 0x0B33);
    TX_CHAR_RANGE(ch, 0x0B36, 0x0B39);
    TX_MATCH_CHAR(ch, 0x0B3D);
    TX_CHAR_RANGE(ch, 0x0B5C, 0x0B5D);
    TX_CHAR_RANGE(ch, 0x0B5F, 0x0B61);
    TX_CHAR_RANGE(ch, 0x0B85, 0x0B8A);
    TX_CHAR_RANGE(ch, 0x0B8E, 0x0B90);
    TX_CHAR_RANGE(ch, 0x0B92, 0x0B95);
    TX_CHAR_RANGE(ch, 0x0B99, 0x0B9A);
    TX_MATCH_CHAR(ch, 0x0B9C);
    TX_CHAR_RANGE(ch, 0x0B9E, 0x0B9F);
    TX_CHAR_RANGE(ch, 0x0BA3, 0x0BA4);
    TX_CHAR_RANGE(ch, 0x0BA8, 0x0BAA);
    TX_CHAR_RANGE(ch, 0x0BAE, 0x0BB5);
    TX_CHAR_RANGE(ch, 0x0BB7, 0x0BB9);
    TX_CHAR_RANGE(ch, 0x0C05, 0x0C0C);
    TX_CHAR_RANGE(ch, 0x0C0E, 0x0C10);
    TX_CHAR_RANGE(ch, 0x0C12, 0x0C28);
    TX_CHAR_RANGE(ch, 0x0C2A, 0x0C33);
    TX_CHAR_RANGE(ch, 0x0C35, 0x0C39);
    TX_CHAR_RANGE(ch, 0x0C60, 0x0C61);
    TX_CHAR_RANGE(ch, 0x0C85, 0x0C8C);
    TX_CHAR_RANGE(ch, 0x0C8E, 0x0C90);
    TX_CHAR_RANGE(ch, 0x0C92, 0x0CA8);
    TX_CHAR_RANGE(ch, 0x0CAA, 0x0CB3);
    TX_CHAR_RANGE(ch, 0x0CB5, 0x0CB9);
    TX_MATCH_CHAR(ch, 0x0CDE);
    TX_CHAR_RANGE(ch, 0x0CE0, 0x0CE1);
    TX_CHAR_RANGE(ch, 0x0D05, 0x0D0C);
    TX_CHAR_RANGE(ch, 0x0D0E, 0x0D10);
    TX_CHAR_RANGE(ch, 0x0D12, 0x0D28);
    TX_CHAR_RANGE(ch, 0x0D2A, 0x0D39);
    TX_CHAR_RANGE(ch, 0x0D60, 0x0D61);
    TX_CHAR_RANGE(ch, 0x0E01, 0x0E2E);
    TX_MATCH_CHAR(ch, 0x0E30);
    TX_CHAR_RANGE(ch, 0x0E32, 0x0E33);
    TX_CHAR_RANGE(ch, 0x0E40, 0x0E45);
    TX_CHAR_RANGE(ch, 0x0E81, 0x0E82);
    TX_MATCH_CHAR(ch, 0x0E84);
    TX_CHAR_RANGE(ch, 0x0E87, 0x0E88);
    TX_MATCH_CHAR(ch, 0x0E8A);
    TX_MATCH_CHAR(ch, 0x0E8D);
    TX_CHAR_RANGE(ch, 0x0E94, 0x0E97);
    TX_CHAR_RANGE(ch, 0x0E99, 0x0E9F);
    TX_CHAR_RANGE(ch, 0x0EA1, 0x0EA3);
    TX_MATCH_CHAR(ch, 0x0EA5);
    TX_MATCH_CHAR(ch, 0x0EA7);
    TX_CHAR_RANGE(ch, 0x0EAA, 0x0EAB);
    TX_CHAR_RANGE(ch, 0x0EAD, 0x0EAE);
    TX_MATCH_CHAR(ch, 0x0EB0);
    TX_CHAR_RANGE(ch, 0x0EB2, 0x0EB3);
    TX_MATCH_CHAR(ch, 0x0EBD);
    TX_CHAR_RANGE(ch, 0x0EC0, 0x0EC4);
    TX_CHAR_RANGE(ch, 0x0F40, 0x0F47);
    TX_CHAR_RANGE(ch, 0x0F49, 0x0F69);
    TX_CHAR_RANGE(ch, 0x10A0, 0x10C5);
    TX_CHAR_RANGE(ch, 0x10D0, 0x10F6);
    TX_MATCH_CHAR(ch, 0x1100);
    TX_CHAR_RANGE(ch, 0x1102, 0x1103);
    TX_CHAR_RANGE(ch, 0x1105, 0x1107);
    TX_MATCH_CHAR(ch, 0x1109);
    TX_CHAR_RANGE(ch, 0x110B, 0x110C);
    TX_CHAR_RANGE(ch, 0x110E, 0x1112);
    TX_MATCH_CHAR(ch, 0x113C);
    TX_MATCH_CHAR(ch, 0x113E);
    TX_MATCH_CHAR(ch, 0x1140);
    TX_MATCH_CHAR(ch, 0x114C);
    TX_MATCH_CHAR(ch, 0x114E);
    TX_MATCH_CHAR(ch, 0x1150);
    TX_CHAR_RANGE(ch, 0x1154, 0x1155);
    TX_MATCH_CHAR(ch, 0x1159);
    TX_CHAR_RANGE(ch, 0x115F, 0x1161);
    TX_MATCH_CHAR(ch, 0x1163);
    TX_MATCH_CHAR(ch, 0x1165);
    TX_MATCH_CHAR(ch, 0x1167);
    TX_MATCH_CHAR(ch, 0x1169);
    TX_CHAR_RANGE(ch, 0x116D, 0x116E);
    TX_CHAR_RANGE(ch, 0x1172, 0x1173);
    TX_MATCH_CHAR(ch, 0x1175);
    TX_MATCH_CHAR(ch, 0x119E);
    TX_MATCH_CHAR(ch, 0x11A8);
    TX_MATCH_CHAR(ch, 0x11AB);
    TX_CHAR_RANGE(ch, 0x11AE, 0x11AF);
    TX_CHAR_RANGE(ch, 0x11B7, 0x11B8);
    TX_MATCH_CHAR(ch, 0x11BA);
    TX_CHAR_RANGE(ch, 0x11BC, 0x11C2);
    TX_MATCH_CHAR(ch, 0x11EB);
    TX_MATCH_CHAR(ch, 0x11F0);
    TX_MATCH_CHAR(ch, 0x11F9);
    TX_CHAR_RANGE(ch, 0x1E00, 0x1E9B);
    TX_CHAR_RANGE(ch, 0x1EA0, 0x1EF9);
    TX_CHAR_RANGE(ch, 0x1F00, 0x1F15);
    TX_CHAR_RANGE(ch, 0x1F18, 0x1F1D);
    TX_CHAR_RANGE(ch, 0x1F20, 0x1F45);
    TX_CHAR_RANGE(ch, 0x1F48, 0x1F4D);
    TX_CHAR_RANGE(ch, 0x1F50, 0x1F57);
    TX_MATCH_CHAR(ch, 0x1F59);
    TX_MATCH_CHAR(ch, 0x1F5B);
    TX_MATCH_CHAR(ch, 0x1F5D);
    TX_CHAR_RANGE(ch, 0x1F5F, 0x1F7D);
    TX_CHAR_RANGE(ch, 0x1F80, 0x1FB4);
    TX_CHAR_RANGE(ch, 0x1FB6, 0x1FBC);
    TX_MATCH_CHAR(ch, 0x1FBE);
    TX_CHAR_RANGE(ch, 0x1FC2, 0x1FC4);
    TX_CHAR_RANGE(ch, 0x1FC6, 0x1FCC);
    TX_CHAR_RANGE(ch, 0x1FD0, 0x1FD3);
    TX_CHAR_RANGE(ch, 0x1FD6, 0x1FDB);
    TX_CHAR_RANGE(ch, 0x1FE0, 0x1FEC);
    TX_CHAR_RANGE(ch, 0x1FF2, 0x1FF4);
    TX_CHAR_RANGE(ch, 0x1FF6, 0x1FFC);
    TX_MATCH_CHAR(ch, 0x2126);
    TX_CHAR_RANGE(ch, 0x212A, 0x212B);
    TX_MATCH_CHAR(ch, 0x212E);
    TX_CHAR_RANGE(ch, 0x2180, 0x2182);
    TX_MATCH_CHAR(ch, 0x3007);
    TX_CHAR_RANGE(ch, 0x3021, 0x3029);
    TX_CHAR_RANGE(ch, 0x3041, 0x3094);
    TX_CHAR_RANGE(ch, 0x30A1, 0x30FA);
    TX_CHAR_RANGE(ch, 0x3105, 0x312C);
    TX_CHAR_RANGE(ch, 0x4E00, 0x9FA5);
    TX_CHAR_RANGE(ch, 0xAC00, 0xD7A3);
    return MB_FALSE;
}

MBool XMLUtils::isNCNameChar(UNICODE_CHAR ch)
{
/* NCNameChar = Letter | Digit | '.' | '-' | '_'  | 
   CombiningChar | Extender */
    if (isLetter(ch)) return MB_TRUE;  /* Letter | '_' */

    TX_CHAR_RANGE(ch, 0x002D, 0x002E); /* '-' | '.' */

    if (isDigit(ch)) return MB_TRUE;   /* Digit */

    TX_MATCH_CHAR(ch, 0x00B7);
    TX_MATCH_CHAR(ch, 0x02D0);
    TX_MATCH_CHAR(ch, 0x02D1);
    TX_CHAR_RANGE(ch, 0x0300, 0x0345);
    TX_CHAR_RANGE(ch, 0x0360, 0x0361);
    TX_MATCH_CHAR(ch, 0x0387);
    TX_CHAR_RANGE(ch, 0x0483, 0x0486);
    TX_CHAR_RANGE(ch, 0x0591, 0x05A1);
    TX_CHAR_RANGE(ch, 0x05A3, 0x05B9);
    TX_CHAR_RANGE(ch, 0x05BB, 0x05BD);
    TX_MATCH_CHAR(ch, 0x05BF);
    TX_CHAR_RANGE(ch, 0x05C1, 0x05C2);
    TX_MATCH_CHAR(ch, 0x05C4);
    TX_MATCH_CHAR(ch, 0x0640);
    TX_CHAR_RANGE(ch, 0x064B, 0x0652);
    TX_MATCH_CHAR(ch, 0x0670);
    TX_CHAR_RANGE(ch, 0x06D6, 0x06DC);
    TX_CHAR_RANGE(ch, 0x06DD, 0x06DF);
    TX_CHAR_RANGE(ch, 0x06E0, 0x06E4);
    TX_CHAR_RANGE(ch, 0x06E7, 0x06E8);
    TX_CHAR_RANGE(ch, 0x06EA, 0x06ED);
    TX_CHAR_RANGE(ch, 0x0901, 0x0903);
    TX_MATCH_CHAR(ch, 0x093C);
    TX_CHAR_RANGE(ch, 0x093E, 0x094C);
    TX_MATCH_CHAR(ch, 0x094D);
    TX_CHAR_RANGE(ch, 0x0951, 0x0954);
    TX_CHAR_RANGE(ch, 0x0962, 0x0963);
    TX_CHAR_RANGE(ch, 0x0981, 0x0983);
    TX_MATCH_CHAR(ch, 0x09BC);
    TX_MATCH_CHAR(ch, 0x09BE);
    TX_MATCH_CHAR(ch, 0x09BF);
    TX_CHAR_RANGE(ch, 0x09C0, 0x09C4);
    TX_CHAR_RANGE(ch, 0x09C7, 0x09C8);
    TX_CHAR_RANGE(ch, 0x09CB, 0x09CD);
    TX_MATCH_CHAR(ch, 0x09D7);
    TX_CHAR_RANGE(ch, 0x09E2, 0x09E3);
    TX_MATCH_CHAR(ch, 0x0A02);
    TX_MATCH_CHAR(ch, 0x0A3C);
    TX_MATCH_CHAR(ch, 0x0A3E);
    TX_MATCH_CHAR(ch, 0x0A3F);
    TX_CHAR_RANGE(ch, 0x0A40, 0x0A42);
    TX_CHAR_RANGE(ch, 0x0A47, 0x0A48);
    TX_CHAR_RANGE(ch, 0x0A4B, 0x0A4D);
    TX_CHAR_RANGE(ch, 0x0A70, 0x0A71);
    TX_CHAR_RANGE(ch, 0x0A81, 0x0A83);
    TX_MATCH_CHAR(ch, 0x0ABC);
    TX_CHAR_RANGE(ch, 0x0ABE, 0x0AC5);
    TX_CHAR_RANGE(ch, 0x0AC7, 0x0AC9);
    TX_CHAR_RANGE(ch, 0x0ACB, 0x0ACD);
    TX_CHAR_RANGE(ch, 0x0B01, 0x0B03);
    TX_MATCH_CHAR(ch, 0x0B3C);
    TX_CHAR_RANGE(ch, 0x0B3E, 0x0B43);
    TX_CHAR_RANGE(ch, 0x0B47, 0x0B48);
    TX_CHAR_RANGE(ch, 0x0B4B, 0x0B4D);
    TX_CHAR_RANGE(ch, 0x0B56, 0x0B57);
    TX_CHAR_RANGE(ch, 0x0B82, 0x0B83);
    TX_CHAR_RANGE(ch, 0x0BBE, 0x0BC2);
    TX_CHAR_RANGE(ch, 0x0BC6, 0x0BC8);
    TX_CHAR_RANGE(ch, 0x0BCA, 0x0BCD);
    TX_MATCH_CHAR(ch, 0x0BD7);
    TX_CHAR_RANGE(ch, 0x0C01, 0x0C03);
    TX_CHAR_RANGE(ch, 0x0C3E, 0x0C44);
    TX_CHAR_RANGE(ch, 0x0C46, 0x0C48);
    TX_CHAR_RANGE(ch, 0x0C4A, 0x0C4D);
    TX_CHAR_RANGE(ch, 0x0C55, 0x0C56);
    TX_CHAR_RANGE(ch, 0x0C82, 0x0C83);
    TX_CHAR_RANGE(ch, 0x0CBE, 0x0CC4);
    TX_CHAR_RANGE(ch, 0x0CC6, 0x0CC8);
    TX_CHAR_RANGE(ch, 0x0CCA, 0x0CCD);
    TX_CHAR_RANGE(ch, 0x0CD5, 0x0CD6);
    TX_CHAR_RANGE(ch, 0x0D02, 0x0D03);
    TX_CHAR_RANGE(ch, 0x0D3E, 0x0D43);
    TX_CHAR_RANGE(ch, 0x0D46, 0x0D48);
    TX_CHAR_RANGE(ch, 0x0D4A, 0x0D4D);
    TX_MATCH_CHAR(ch, 0x0D57);
    TX_MATCH_CHAR(ch, 0x0E31);
    TX_CHAR_RANGE(ch, 0x0E34, 0x0E3A);
    TX_MATCH_CHAR(ch, 0x0E46);
    TX_CHAR_RANGE(ch, 0x0E47, 0x0E4E);
    TX_MATCH_CHAR(ch, 0x0EB1);
    TX_CHAR_RANGE(ch, 0x0EB4, 0x0EB9);
    TX_CHAR_RANGE(ch, 0x0EBB, 0x0EBC);
    TX_MATCH_CHAR(ch, 0x0EC6);
    TX_CHAR_RANGE(ch, 0x0EC8, 0x0ECD);
    TX_CHAR_RANGE(ch, 0x0F18, 0x0F19);
    TX_MATCH_CHAR(ch, 0x0F35);
    TX_MATCH_CHAR(ch, 0x0F37);
    TX_MATCH_CHAR(ch, 0x0F39);
    TX_MATCH_CHAR(ch, 0x0F3E);
    TX_MATCH_CHAR(ch, 0x0F3F);
    TX_CHAR_RANGE(ch, 0x0F71, 0x0F84);
    TX_CHAR_RANGE(ch, 0x0F86, 0x0F8B);
    TX_CHAR_RANGE(ch, 0x0F90, 0x0F95);
    TX_MATCH_CHAR(ch, 0x0F97);
    TX_CHAR_RANGE(ch, 0x0F99, 0x0FAD);
    TX_CHAR_RANGE(ch, 0x0FB1, 0x0FB7);
    TX_MATCH_CHAR(ch, 0x0FB9);
    TX_CHAR_RANGE(ch, 0x20D0, 0x20DC);
    TX_MATCH_CHAR(ch, 0x20E1);
    TX_MATCH_CHAR(ch, 0x3005);
    TX_CHAR_RANGE(ch, 0x302A, 0x302F);
    TX_CHAR_RANGE(ch, 0x3031, 0x3035);
    TX_MATCH_CHAR(ch, 0x3099);
    TX_MATCH_CHAR(ch, 0x309A);
    TX_CHAR_RANGE(ch, 0x309D, 0x309E);
    TX_CHAR_RANGE(ch, 0x30FC, 0x30FE);
    return MB_FALSE;
}
