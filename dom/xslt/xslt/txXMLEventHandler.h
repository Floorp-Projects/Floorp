/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_XML_EVENT_HANDLER_H
#define TRANSFRMX_XML_EVENT_HANDLER_H

#include "txCore.h"
#include "nsAtom.h"

#define kTXNameSpaceURI "http://www.mozilla.org/TransforMiix"
#define kTXWrapper "transformiix:result"

class txOutputFormat;
class nsIDocument;

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
    virtual nsresult attribute(nsAtom* aPrefix, nsAtom* aLocalName,
                               nsAtom* aLowercaseLocalName, int32_t aNsID,
                               const nsString& aValue) = 0;

    /**
     * Signals to receive the start of an attribute.
     *
     * @param aPrefix the prefix of the attribute
     * @param aLocalName the localname of the attribute
     * @param aNsID the namespace ID of the attribute
     * @param aValue the value of the attribute
     */
    virtual nsresult attribute(nsAtom* aPrefix,
                               const nsAString& aLocalName,
                               const int32_t aNsID,
                               const nsString& aValue) = 0;

    /**
     * Signals to receive characters.
     *
     * @param aData the characters to receive
     * @param aDOE disable output escaping for these characters
     */
    virtual nsresult characters(const nsAString& aData, bool aDOE) = 0;

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
    virtual nsresult startElement(nsAtom* aPrefix,
                                  nsAtom* aLocalName,
                                  nsAtom* aLowercaseLocalName,
                                  int32_t aNsID) = 0;

    /**
     * Signals to receive the start of an element. Can throw
     * NS_ERROR_XSLT_BAD_NODE_NAME if the name is invalid
     *
     * @param aPrefix the prefix of the element
     * @param aLocalName the localname of the element
     * @param aNsID the namespace ID of the element
     */
    virtual nsresult startElement(nsAtom* aPrefix,
                                  const nsAString& aLocalName,
                                  const int32_t aNsID) = 0;
};

#define TX_DECL_TXAXMLEVENTHANDLER                                           \
    virtual nsresult attribute(nsAtom* aPrefix, nsAtom* aLocalName,        \
                               nsAtom* aLowercaseLocalName, int32_t aNsID,  \
                               const nsString& aValue) override;             \
    virtual nsresult attribute(nsAtom* aPrefix,                             \
                               const nsAString& aLocalName,                  \
                               const int32_t aNsID,                          \
                               const nsString& aValue) override;             \
    virtual nsresult characters(const nsAString& aData, bool aDOE) override; \
    virtual nsresult comment(const nsString& aData) override;                \
    virtual nsresult endDocument(nsresult aResult = NS_OK) override;         \
    virtual nsresult endElement() override;                                  \
    virtual nsresult processingInstruction(const nsString& aTarget,          \
                                           const nsString& aData) override;  \
    virtual nsresult startDocument() override;                               \
    virtual nsresult startElement(nsAtom* aPrefix,                          \
                                  nsAtom* aLocalName,                       \
                                  nsAtom* aLowercaseLocalName,              \
                                  int32_t aNsID) override;                   \
    virtual nsresult startElement(nsAtom* aPrefix,                          \
                                  const nsAString& aName,                    \
                                  const int32_t aNsID) override;


class txAOutputXMLEventHandler : public txAXMLEventHandler
{
public:
    /**
     * Gets the Mozilla output document
     *
     * @param aDocument the Mozilla output document
     */
    virtual void getOutputDocument(nsIDocument** aDocument) = 0;
};

#define TX_DECL_TXAOUTPUTXMLEVENTHANDLER                        \
    virtual void getOutputDocument(nsIDocument** aDocument) override;

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
                      const nsAString& aName,
                      int32_t aNsID,
                      txAXMLEventHandler** aHandler) = 0;
};

#define TX_DECL_TXAOUTPUTHANDLERFACTORY                        \
    nsresult createHandlerWith(txOutputFormat* aFormat,        \
                               txAXMLEventHandler** aHandler) override; \
    nsresult createHandlerWith(txOutputFormat* aFormat,        \
                               const nsAString& aName,         \
                               int32_t aNsID,                  \
                               txAXMLEventHandler** aHandler) override;

#endif
