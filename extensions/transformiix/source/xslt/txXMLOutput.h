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
 * The Original Code is the TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com>
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
#include "dom.h"
#include "List.h"
#include "Stack.h"
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

class txAttribute {
public:
    txAttribute(PRInt32 aNsID, txAtom* aLocalName, const String& aValue);
    txExpandedName mName;
    String mValue;
    MBool mShorthand;
};

class txXMLOutput : public txStreamXMLEventHandler
{
public:
    txXMLOutput();
    virtual ~txXMLOutput();

    static const int DEFAULT_INDENT;

    /*
     * Signals to receive the start of an attribute.
     *
     * @param aName the name of the attribute
     * @param aNsID the namespace ID of the attribute
     * @param aValue the value of the attribute
     */
    virtual void attribute(const String& aName,
                           const PRInt32 aNsID,
                           const String& aValue);

    /*
     * Signals to receive characters.
     *
     * @param aData the characters to receive
     */
    virtual void characters(const String& aData);

    /*
     * Signals to receive characters that don't need output escaping.
     *
     * @param aData the characters to receive
     */
    virtual void charactersNoOutputEscaping(const String& aData);

    /*
     * Signals to receive data that should be treated as a comment.
     *
     * @param data the comment data to receive
     */
    void comment(const String& aData);

    /*
     * Signals the end of a document. It is an error to call
     * this method more than once.
     */
    virtual void endDocument();

    /*
     * Signals to receive the end of an element.
     *
     * @param aName the name of the element
     * @param aNsID the namespace ID of the element
     */
    virtual void endElement(const String& aName,
                            const PRInt32 aNsID);

    /*
     * Signals to receive a processing instruction.
     *
     * @param aTarget the target of the processing instruction
     * @param aData the data of the processing instruction
     */
    virtual void processingInstruction(const String& aTarget,
                                       const String& aData);

    /*
     * Signals the start of a document.
     */
    virtual void startDocument();

    /*
     * Signals to receive the start of an element.
     *
     * @param aName the name of the element
     * @param aNsID the namespace ID of the element
     */
    virtual void startElement(const String& aName,
                              const PRInt32 aNsID);

    /*
     * Sets the output format.
     *
     * @param aOutputFormat the output format
     */
    void setOutputFormat(txOutputFormat* aOutputFormat);

    /**
     * Get the output stream.
     *
     * @param aOutputStream the current output stream
     */
    void getOutputStream(ostream** aOutputStream);

    /**
     * Sets the output stream.
     *
     * @param aOutputStream the new output stream
     */
    void setOutputStream(ostream* aOutputStream);

protected:
    virtual void closeStartTag(MBool aUseEmptyElementShorthand);
    void printUTF8Char(DOM_CHAR& ch);
    void printUTF8Chars(const String& aData);
    void printWithXMLEntities(const String& aData, MBool aAttribute = MB_FALSE);

    ostream* mOut;
    txOutputFormat mOutputFormat;
    MBool mUseEmptyElementShorthand;
    MBool mHaveDocumentElement;
    MBool mStartTagOpen;
    MBool mAfterEndTag;
    MBool mInCDATASection;
    PRUint32 mIndentLevel;
    txList mAttributes;
    Stack mCDATASections;

private:
    DOM_CHAR mBuffer[4];
};

#endif
