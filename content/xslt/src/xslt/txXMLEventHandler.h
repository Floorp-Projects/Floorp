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
class String;
class txOutputFormat;
#ifdef TX_EXE
#include <iostream.h>
#else
class nsIContent;
class nsIDOMDocument;
class nsIDOMHTMLScriptElement;
#endif

/*
 * An interface for handling XML documents, loosely modeled
 * after Dave Megginson's SAX 1.0 API.
 */

class txXMLEventHandler
{
public:
    virtual ~txXMLEventHandler() {};

    /*
     * Signals to receive the start of an attribute.
     *
     * @param aName the name of the attribute
     * @param aNsID the namespace ID of the attribute
     * @param aValue the value of the attribute
     */
    virtual void attribute(const String& aName,
                           const PRInt32 aNsID,
                           const String& aValue) = 0;

    /*
     * Signals to receive characters.
     *
     * @param aData the characters to receive
     */
    virtual void characters(const String& aData) = 0;

    /*
     * Signals to receive data that should be treated as a comment.
     *
     * @param data the comment data to receive
     */
    virtual void comment(const String& aData) = 0;

    /*
     * Signals the end of a document. It is an error to call
     * this method more than once.
     */
    virtual void endDocument() = 0;

    /*
     * Signals to receive the end of an element.
     *
     * @param aName the name of the element
     * @param aNsID the namespace ID of the element
     */
    virtual void endElement(const String& aName,
                            const PRInt32 aNsID) = 0;

    /*
     * Signals to receive a processing instruction.
     *
     * @param aTarget the target of the processing instruction
     * @param aData the data of the processing instruction
     */
    virtual void processingInstruction(const String& aTarget, 
                                       const String& aData) = 0;

    /*
     * Signals the start of a document.
     */
    virtual void startDocument() = 0;

    /*
     * Signals to receive the start of an element.
     *
     * @param aName the name of the element
     * @param aNsID the namespace ID of the element
     */
    virtual void startElement(const String& aName,
                              const PRInt32 aNsID) = 0;
};

class txOutputXMLEventHandler : public txXMLEventHandler
{
public:
    /*
     * Sets the output format.
     *
     * @param aOutputFormat the output format
     */
    virtual void setOutputFormat(txOutputFormat* aOutputFormat) = 0;
};

#ifdef TX_EXE
class txStreamXMLEventHandler : public txOutputXMLEventHandler
{
public:
    /**
     * Get the output stream.
     *
     * @param aOutputStream the current output stream
     */
    void getOutputStream(ostream** aOutputStream);

    /*
     * Sets the output stream.
     *
     * @param aOutputStream the output stream
     */
    virtual void setOutputStream(ostream* aOutputStream) = 0;

    /*
     * Signals to receive characters that don't need output escaping.
     *
     * @param aData the characters to receive
     */
    virtual void charactersNoOutputEscaping(const String& aData) = 0;
};
#else
class txMozillaXMLEventHandler : public txOutputXMLEventHandler
{
public:
    /*
     * Disables loading of stylesheets.
     */
    virtual void disableStylesheetLoad() = 0;

    /*
     * Returns the root content of the result.
     *
     * @param aReturn the root content
     */
    virtual nsresult getRootContent(nsIContent** aReturn) = 0;

    /*
     * Returns PR_TRUE if the event handler has finished anything
     * extra that had to happen after the transform has finished.
     */
    virtual PRBool isDone() = 0;

    /*
     * Removes a script element from the array of elements that are
     * still loading.
     *
     * @param aReturn the script element to remove
     */
    virtual void removeScriptElement(nsIDOMHTMLScriptElement *aElement) = 0;

    /*
     * Sets the Mozilla output document.
     *
     * @param aDocument the Mozilla output document
     */
    virtual void setOutputDocument(nsIDOMDocument* aDocument) = 0;
};
#endif

#endif
