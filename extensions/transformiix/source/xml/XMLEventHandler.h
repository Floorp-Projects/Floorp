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
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco
 * (C) 1999-2000 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */


#ifndef TRANSFRMX_XML_EVENT_HANDLER_H
#define TRANSFRMX_XML_EVENT_HANDLER_H


#include "TxString.h"


#ifndef UNICODE_CHAR
typedef unsigned short UNICODE_CHAR
#endif

//-- prototypes
class ErrorHandler;
class AttributeList;

/**
 * A interface for handling XML documents, modeled
 * after Dave Megginson's SAX 1.0 API. This also has methods
 * that allow handling CDATA characters, comments, etc.
 * I apologize for not throwing Exceptions, I am trying
 * to follow Mozilla.org's guidelines for writing portable C++.
**/
class XMLEventHandler {

public:


    /**
     * Default Destructor
    **/
    virtual ~XMLEventHandler();

    /**
     * Signals to recieve CDATA characters
     * This is useful when building a DOM tree, and the user
     * explicitly wants a CDATA section created in the tree.
     * @param chars the characters to receive
    **/
    virtual void cdata(UNICODE_CHAR* chars, int length) = 0;

    /**
     * Signals to recieve characters
     * @param chars the characters to recieve
     * @param length the number of characters to receive
    **/
    virtual void characters(UNICODE_CHAR* chars, int length) = 0;


    /**
     * Signals to recieve data that should be treated as a comment
     * @param data the comment data to recieve
    **/
    virtual void comment(String& data) = 0;

    /**
     * Signals the end of a document. It is an error to call this method more than once.
    **/
    virtual void endDocument() = 0;

    /**
     * Signals to recieve the end of an element
     * @param name the name of the element
     * @param nsURI the namespace URI of the element, a null
     * pointer indicates the default namespace
    **/
    virtual void endElement(const String& name, String* nsURI) = 0;


    /**
     * Signals to receive an entity reference
     * @param name the name of the entity reference
    **/
    virtual void entityReference(String& name) = 0;

    /**
     * Appends the current error message, if any, to the given String.
     * @param errMsg, the String in which to append the current error message
    **/
    virtual void getErrorMessage(String& errMsg) = 0;

    /**
     * Returns true if an error has occured
     * @return true if an error has occured
    **/
    virtual MBool hasError() = 0;

    /**
     * Signals to receive a processing instruction
     * @param target the target of the processing instruction
     * @param data the data of the processing instruction
    **/
    virtual void processingInstruction(const String& target, const String& data) = 0;


    /**
     * Sets the error handler for this XMLHandler
     * @param errorHandler a pointer to the ErrorHandler to use when
     * errors are encountered
    **/
    virtual void setErrorHandler(ErrorHandler* errorHandler) = 0;

    /**
     * Signals the start of a document.
    **/
    virtual void startDocument() = 0;

    /**
     * Signals to receive the start of an element.
     * @param name the name of the element
     * @param nsURI the namespace URI of the element, or 0 if the default namespace is to be used.
     * @param atts the AttributeList contain attribute name
     * value pairs
    **/
    virtual void startElement(const String& name, String* nsURI, AttributeList* atts) = 0;

    /**
     * Signals to receive the start of an element.
     * @param name the name of the element
     * @param nsPrefix, the desired namespace prefix for the element.
     * @param nsURI the namespace URI of the element
     * @param atts the AttributeList contain attribute name
     * value pairs
    **/
    virtual void startElement(const String& name, const String& nsPrefix, const String& nsURI, AttributeList* atts) = 0;


}; //-- XMLEventHandler


/**
 * An interface (abstract class) for dealing with a list of
 * Attributes. This is based off of SAX 1.0.
**/
class AttributeList {

 public:

    /**
     * Default destructor
    **/
    virtual ~AttributeList();

    /** 
     * @return the number of attributes in the list
    **/
    virtual int getLength() = 0;

    /**
     * @return the name of the attribute located at the given index
    **/
    virtual String& getName(int index) = 0;

    /**
     * Returns the namespace of the attribute located at the given index.
     * If there is no namespace defined for the attribute, 0 will be returned.
     * @return the namespace of the attribute located at the given index, or
     * 0 if no namespace has been defined.
    **/
    virtual String* getNamespace(int index) = 0;

    /**
     * @return the value of the attribute located at the given index
    **/
    virtual const String& getValue(int index) = 0;

    /**
     * @return the value of the attribute with the given name
    **/
    virtual const String& getValue(const String& name) = 0;

    /**
     * @return the value of the attribute with the given name
    **/
    virtual const String& getValue(const String& name, String* nsURI) = 0;


}; //-- AttributeList


/**
 * The ErrorHandler to use for dealing with errors
**/
class ErrorHandler {

public:

    /**
     * Signals a non fatal error
    **/
    virtual void error(String& message) = 0;

    /**
     * Signals a fatal error
    **/
    virtual void fatal(String& message) = 0;

    /**
     * Signals a warning message
    **/
    virtual void warning(String& message) = 0;


}; //-- ErrorHandler

#endif
