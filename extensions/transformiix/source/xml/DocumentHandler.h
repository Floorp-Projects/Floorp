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
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * $Id: DocumentHandler.h,v 1.2 2000/04/12 22:31:33 nisheeth%netscape.com Exp $
 */


#ifndef TRANSFRMX_DOCUMENT_HANDLER_H
#define TRANSFRMX_DOCUMENT_HANDLER_H


#include "TxString.h"


#ifndef UNICODE_CHAR
typedef unsigned short UNICODE_CHAR
#endif

/**
 * A interface for handling XML documents, closely modelled
 * after Dave Megginson's SAX API. This also has methods
 * that allow handling CDATA characters, comments, etc.
 * I apologize for not throwing SAXExceptions, I am trying
 * to follow Mozilla.org's guidelines for writing portable C++.
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.2 $ $Date: 2000/04/12 22:31:33 $
**/
class DocumentHandler {

public:


    /**
     * Default Destructor
    **/
    virtual ~DocumentHandler();

    /**
     * Signals to recieve CDATA characters
     * This is useful when building a DOM tree, and the user
     * explicitly wants a CDATA section created in the tree.
     * @param chars the characters to receive
     * @return the status code, a positive number, or 0 indicates
     * sucessful
    **/
    virtual int cdata(UNICODE_CHAR* chars, int start, int length) = 0;

    /**
     * Signals to recieve characters
     * @param chars the characters to recieve
     * @param start the start of the characters to receive
     * @param length the number of characters to receive
     * @return the status code, a positive number, or 0 indicates
     * sucessful
    **/
    virtual int characters(UNICODE_CHAR* chars, int start, int length) = 0;


    /**
     * Signals to recieve data that should be treated as a comment
     * @param data the comment data to recieve
     * @return the status code, a positive number, or 0 indicates
     * sucessful
    **/
    virtual int comment(String data) = 0;

    /**
     * Signals the end of a document
     * @return the status code, a positive number, or 0 indicates
     * sucessful
    **/
    virtual int endDocument() = 0;

    /**
     * Signals to recieve the end of an element
     * @param name the name of the element
     * @return the status code, a positive number, or 0 indicates
     * sucessful
    **/
    virtual int endElement(String name) = 0;


    /**
     * Signals to receive an entity reference
     * @param name the name of the entity reference
     * @return the status code, a positive number, or 0 indicates
     * sucessful
    **/    
    virtual int entityReference(String name) = 0;

    /**
     * Signals to receive a processing instruction
     * @param target the target of the processing instruction
     * @param data the data of the processing instruction
     * @return the status code, a positive number, or 0 indicates
     * sucessful
    **/
    virtual int processingInstruction(String target, String data) = 0;

    /**
     * Signals the start of a document
     * @return the status code, a positive number, or 0 indicates
     * sucessful
    **/
    virtual int startDocument() = 0;

    /**
     * Signals to receive the start of an element.
     * @param name the name of the element
     * @param atts the AttributeList contain attribute name
     * value pairs
     * @return the status code, a positive number, or 0 indicates
     * sucessful   
    **/
    virtual int startElement(String name, AttributeList* atts) = 0;

}; //-- DocumentHandler


/**
 * An interface (abstract class) for dealing with a list of
 * Attributes. This is compliant with SAX 1.0.
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
     * @return the type of the attribute located at the given index
    **/
    virtual const String& getType(int index) = 0;

    /** 
     * @return the type of the attribute with the given name
    **/
    virtual const String& getType(const String& name) = 0;

    /**
     * @return the value of the attribute located at the given index
    **/
    virtual const String& getValue(int index) = 0;

    /**
     * @return the value of the attribute with the given name
    **/
    virtual const String& getValue(const String& name) = 0;


}; //-- AttributeList

#endif
