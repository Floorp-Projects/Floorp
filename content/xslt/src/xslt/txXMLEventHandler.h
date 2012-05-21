/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_XML_EVENT_HANDLER_H
#define TRANSFRMX_XML_EVENT_HANDLER_H

#include "txCore.h"
#include "nsIAtom.h"

#define kTXNameSpaceURI "http://www.mozilla.org/TransforMiix"
#define kTXWrapper "transformiix:result"

class txOutputFormat;
class nsIDOMDocument;

/**
 * An interface for handling XML documents, loosely modeled
 * after Dave Megginson's SAX 1.0 API.
 */

class txAXMLEventHandler
{
public:
    virtual ~txAXMLEventHandler() {}

    /**
     * Signals to receive the start of an attribute.
     *
     * @param aPrefix the prefix of the attribute
     * @param aLocalName the localname of the attribute
     * @param aLowercaseName the localname of the attribute in lower case
     * @param aNsID the namespace ID of the attribute
     * @param aValue the value of the attribute
     */
    virtual nsresult attribute(nsIAtom* aPrefix, nsIAtom* aLocalName,
                               nsIAtom* aLowercaseLocalName, PRInt32 aNsID,
                               const nsString& aValue) = 0;

    /**
     * Signals to receive the start of an attribute.
     *
     * @param aPrefix the prefix of the attribute
     * @param aLocalName the localname of the attribute
     * @param aNsID the namespace ID of the attribute
     * @param aValue the value of the attribute
     */
    virtual nsresult attribute(nsIAtom* aPrefix,
                               const nsSubstring& aLocalName,
                               const PRInt32 aNsID,
                               const nsString& aValue) = 0;

    /**
     * Signals to receive characters.
     *
     * @param aData the characters to receive
     * @param aDOE disable output escaping for these characters
     */
    virtual nsresult characters(const nsSubstring& aData, bool aDOE) = 0;

    /**
     * Signals to receive data that should be treated as a comment.
     *
     * @param data the comment data to receive
     */
    virtual nsresult comment(const nsString& aData) = 0;

    /**
     * Signals the end of a document. It is an error to call
     * this method more than once.
     */
    virtual nsresult endDocument(nsresult aResult) = 0;

    /**
     * Signals to receive the end of an element.
     */
    virtual nsresult endElement() = 0;

    /**
     * Signals to receive a processing instruction.
     *
     * @param aTarget the target of the processing instruction
     * @param aData the data of the processing instruction
     */
    virtual nsresult processingInstruction(const nsString& aTarget, 
                                           const nsString& aData) = 0;

    /**
     * Signals the start of a document.
     */
    virtual nsresult startDocument() = 0;

    /**
     * Signals to receive the start of an element.
     *
     * @param aPrefix the prefix of the element
     * @param aLocalName the localname of the element
     * @param aLowercaseName the localname of the element in lower case
     * @param aNsID the namespace ID of the element
     */
    virtual nsresult startElement(nsIAtom* aPrefix,
                                  nsIAtom* aLocalName,
                                  nsIAtom* aLowercaseLocalName,
                                  PRInt32 aNsID) = 0;

    /**
     * Signals to receive the start of an element. Can throw
     * NS_ERROR_XSLT_BAD_NODE_NAME if the name is invalid
     *
     * @param aPrefix the prefix of the element
     * @param aLocalName the localname of the element
     * @param aNsID the namespace ID of the element
     */
    virtual nsresult startElement(nsIAtom* aPrefix,
                                  const nsSubstring& aLocalName,
                                  const PRInt32 aNsID) = 0;
};

#define TX_DECL_TXAXMLEVENTHANDLER                                           \
    virtual nsresult attribute(nsIAtom* aPrefix, nsIAtom* aLocalName,        \
                               nsIAtom* aLowercaseLocalName, PRInt32 aNsID,  \
                               const nsString& aValue);                      \
    virtual nsresult attribute(nsIAtom* aPrefix,                             \
                               const nsSubstring& aLocalName,                \
                               const PRInt32 aNsID,                          \
                               const nsString& aValue);                      \
    virtual nsresult characters(const nsSubstring& aData, bool aDOE);      \
    virtual nsresult comment(const nsString& aData);                         \
    virtual nsresult endDocument(nsresult aResult = NS_OK);                  \
    virtual nsresult endElement();                                           \
    virtual nsresult processingInstruction(const nsString& aTarget,          \
                                           const nsString& aData);           \
    virtual nsresult startDocument();                                        \
    virtual nsresult startElement(nsIAtom* aPrefix,                          \
                                  nsIAtom* aLocalName,                       \
                                  nsIAtom* aLowercaseLocalName,              \
                                  PRInt32 aNsID);                            \
    virtual nsresult startElement(nsIAtom* aPrefix,                          \
                                  const nsSubstring& aName,                  \
                                  const PRInt32 aNsID);


class txAOutputXMLEventHandler : public txAXMLEventHandler
{
public:
    /**
     * Gets the Mozilla output document
     *
     * @param aDocument the Mozilla output document
     */
    virtual void getOutputDocument(nsIDOMDocument** aDocument) = 0;
};

#define TX_DECL_TXAOUTPUTXMLEVENTHANDLER                        \
    virtual void getOutputDocument(nsIDOMDocument** aDocument);

/**
 * Interface used to create the appropriate outputhandler
 */
class txAOutputHandlerFactory
{
public:
    virtual ~txAOutputHandlerFactory() {}

    /**
     * Creates an outputhandler for the specified format.
     * @param aFromat  format to get handler for
     * @param aHandler outparam. The created handler
     */
    virtual nsresult
    createHandlerWith(txOutputFormat* aFormat,
                      txAXMLEventHandler** aHandler) = 0;

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
                      const nsSubstring& aName,
                      PRInt32 aNsID,
                      txAXMLEventHandler** aHandler) = 0;
};

#define TX_DECL_TXAOUTPUTHANDLERFACTORY                        \
    nsresult createHandlerWith(txOutputFormat* aFormat,        \
                               txAXMLEventHandler** aHandler); \
    nsresult createHandlerWith(txOutputFormat* aFormat,        \
                               const nsSubstring& aName,       \
                               PRInt32 aNsID,                  \
                               txAXMLEventHandler** aHandler);

#endif
