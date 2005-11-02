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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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

#ifndef TRANSFRMX_XML_OUTPUT_H
#define TRANSFRMX_XML_OUTPUT_H

#include "txXMLEventHandler.h"
#include "List.h"
#include "txStack.h"
#include "txOutputFormat.h"
#include "XMLUtils.h"

#define DASH            '-'
#define TX_CR           '\r'
#define TX_LF           '\n'

#define AMPERSAND       '&'
#define APOSTROPHE      '\''
#define GT              '>'
#define LT              '<'
#define QUOTE           '"'

#define AMP_ENTITY      "&amp;"
#define APOS_ENTITY     "&apos;"
#define GT_ENTITY       "&gt;"
#define LT_ENTITY       "&lt;"
#define QUOT_ENTITY     "&quot;"
#define HEX_ENTITY      "&#"

#define CDATA_END       "]]>"
#define CDATA_START     "<![CDATA["
#define COMMENT_START   "<!--"
#define COMMENT_END     "-->"
#define DOCTYPE_START   "<!DOCTYPE "
#define DOCTYPE_END     ">"
#define DOUBLE_QUOTE    "\""
#define EQUALS          "="
#define FORWARD_SLASH   "/"
#define L_ANGLE_BRACKET "<"
#define PI_START        "<?"
#define PI_END          "?>"
#define PUBLIC          "PUBLIC"
#define R_ANGLE_BRACKET ">"
#define SEMICOLON       ";"
#define SPACE           " "
#define SYSTEM          "SYSTEM"
#define XML_DECL        "xml version="
#define XML_VERSION     "1.0"

class txOutAttr {
public:
    txOutAttr(PRInt32 aNsID, nsIAtom* aLocalName, const nsAString& aValue);
    txExpandedName mName;
    nsString mValue;
    MBool mShorthand;
};

class txXMLOutput : public txAOutputXMLEventHandler
{
public:
    txXMLOutput(txOutputFormat* aFormat, ostream* aOut);
    virtual ~txXMLOutput();

    static const int DEFAULT_INDENT;

    TX_DECL_TXAXMLEVENTHANDLER
    TX_DECL_TXAOUTPUTXMLEVENTHANDLER

protected:
    virtual void closeStartTag(MBool aUseEmptyElementShorthand);
    void printUTF8Char(PRUnichar& ch);
    void printUTF8Chars(const nsAString& aData);
    void printWithXMLEntities(const nsAString& aData,
                              MBool aAttribute = MB_FALSE);

    ostream* mOut;
    txOutputFormat mOutputFormat;
    MBool mUseEmptyElementShorthand;
    MBool mHaveDocumentElement;
    MBool mStartTagOpen;
    MBool mAfterEndTag;
    MBool mInCDATASection;
    PRUint32 mIndentLevel;
    txList mAttributes;
    txStack mCDATASections;

private:
    PRUnichar mBuffer[4];
};

#endif
