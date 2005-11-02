/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco
 * (C) 1999-2000 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 *   Keith Visco <kvisco@ziplink.net>
 *
 */

#ifndef TRANSFRMX_XML_EVENT_HANDLER_H
#define TRANSFRMX_XML_EVENT_HANDLER_H

#include "baseutils.h"
#include "txError.h"

class nsAString;
class txOutputFormat;

#ifdef TX_EXE
#include <iostream.h>
#else
#include "nsISupports.h"
#define kTXNameSpaceURI "http://www.mozilla.org/TransforMiix"
#define kTXWrapper "transformiix:result"

class nsIContent;
class nsIDOMDocument;
class nsIDOMHTMLScriptElement;
class nsITransformObserver;
#endif

/**
 * An interface for handling XML documents, loosely modeled
 * after Dave Megginson's SAX 1.0 API.
 */

class txXMLEventHandler
{
public:
    virtual ~txXMLEventHandler() {};

    /**
     * Signals to receive the start of an attribute.
     *
     * @param aName the name of the attribute
     * @param aNsID the namespace ID of the attribute
     * @param aValue the value of the attribute
     */
    virtual void attribute(const nsAString& aName,
                           const PRInt32 aNsID,
                           const nsAString& aValue) = 0;

    /**
     * Signals to receive characters.
     *
     * @param aData the characters to receive
     */
    virtual void characters(const nsAString& aData) = 0;

    /**
     * Signals to receive data that should be treated as a comment.
     *
     * @param data the comment data to receive
     */
    virtual void comment(const nsAString& aData) = 0;

    /**
     * Signals the end of a document. It is an error to call
     * this method more than once.
     */
    virtual void endDocument() = 0;

    /**
     * Signals to receive the end of an element.
     *
     * @param aName the name of the element
     * @param aNsID the namespace ID of the element
     */
    virtual void endElement(const nsAString& aName,
                            const PRInt32 aNsID) = 0;

    /**
     * Signals to receive a processing instruction.
     *
     * @param aTarget the target of the processing instruction
     * @param aData the data of the processing instruction
     */
    virtual void processingInstruction(const nsAString& aTarget, 
                                       const nsAString& aData) = 0;

    /**
     * Signals the start of a document.
     */
    virtual void startDocument() = 0;

    /**
     * Signals to receive the start of an element.
     *
     * @param aName the name of the element
     * @param aNsID the namespace ID of the element
     */
    virtual void startElement(const nsAString& aName,
                              const PRInt32 aNsID) = 0;
};

#ifdef TX_EXE
class txIOutputXMLEventHandler : public txXMLEventHandler
#else
#define TX_IOUTPUTXMLEVENTHANDLER_IID \
{ 0x80e5e802, 0x8c88, 0x11d6, \
  { 0xa7, 0xf2, 0xc5, 0xc3, 0x85, 0x6b, 0xbb, 0xbc }}

class txIOutputXMLEventHandler : public nsISupports,
                                 public txXMLEventHandler
#endif
{
public:
    /**
     * Signals to receive characters that don't need output escaping.
     *
     * @param aData the characters to receive
     */
    virtual void charactersNoOutputEscaping(const nsAString& aData) = 0;

    /**
     * Returns whether the output handler supports
     * disable-output-escaping.
     *
     * @return MB_TRUE if this handler supports
     *                 disable-output-escaping
     */
    virtual MBool hasDisableOutputEscaping() = 0;

#ifndef TX_EXE
    NS_DEFINE_STATIC_IID_ACCESSOR(TX_IOUTPUTXMLEVENTHANDLER_IID)

    /**
     * Gets the Mozilla output document
     *
     * @param aDocument the Mozilla output document
     */
    virtual void getOutputDocument(nsIDOMDocument** aDocument) = 0;
#endif
};

/**
 * Interface used to create the appropriate outputhandler
 */
class txIOutputHandlerFactory
{
public:
    virtual ~txIOutputHandlerFactory() {};

    /**
     * Creates an outputhandler for the specified format.
     * @param aFromat  format to get handler for
     * @param aHandler outparam. The created handler
     */
    virtual nsresult
    createHandlerWith(txOutputFormat* aFormat,
                      txIOutputXMLEventHandler** aHandler) = 0;

    /**
     * Creates an outputhandler for the specified format, with the specified
     * name and namespace for the root element.
     * @param aFromat  format to get handler for
     * @param aName    name of the root element
     * @param aNsID    namespace-id of the root element
     * @param aHandler outparam. The created handler
     */
    virtual nsresult
    createHandlerWith(txOutputFormat* aFormat,
                      const nsAString& aName,
                      PRInt32 aNsID,
                      txIOutputXMLEventHandler** aHandler) = 0;
};

#define TX_DECL_TXIOUTPUTHANDLERFACTORY                               \
    nsresult createHandlerWith(txOutputFormat* aFormat,               \
                               txIOutputXMLEventHandler** aHandler);  \
    nsresult createHandlerWith(txOutputFormat* aFormat,               \
                               const nsAString& aName,                \
                               PRInt32 aNsID,                         \
                               txIOutputXMLEventHandler** aHandler)   \

#endif
