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
 * Bob Miller, kbob@oblix.com
 *    -- plugged core leak.
 *
 * $Id: printers.h,v 1.4 2000/04/12 22:32:13 nisheeth%netscape.com Exp $
 */


#include "dom.h"
#include "TxString.h"
#include "baseutils.h"
#include "NamedMap.h"
#include <iostream.h>

#ifndef TRANSFRMX_PRINTERS_H
#define TRANSFRMX_PRINTERS_H


/**
 * A class for printing XML nodes.
 * This class was ported from XSL:P Java source
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.4 $ $Date: 2000/04/12 22:32:13 $
**/
class XMLPrinter {

public:

    /**
     * The default indent size
    **/
    static const int DEFAULT_INDENT;

      //---------------/
     //- Contructors -/
    //---------------/

    /**
     * Default constructor. Uses stdout as the default ostream
    **/
    XMLPrinter();

    /**
     * Destructor must be virtual so subclasses are destroyed.
    **/

    virtual ~XMLPrinter();

    /**
     * Creates a new XML Printer using the given PrintWriter
     * for output
     * @param writer the PrintWriter to use for output
    **/
    XMLPrinter(ostream& os);

    /**
     * Creates a new XML Printer using the given PrintWriter
     * for output, and nodes are indenting using the specified
     * indent size
     * @param os the out stream to use for output
     * @param indent the number of spaces to indent
    **/
    XMLPrinter (ostream& os, int indent);

    /**
     * Prints the given Node
     * @param node the Node to print
    **/
    virtual void print(Node* node);

    /**
     * Sets the indent size
     * @param indent the number of spaces to indent
    **/
    virtual void setIndentSize(int indent);

    /**
     * Sets whether or not to "unwrap" CDATA Sections
     * when printing. By Default CDATA Sections are left as is.
     * @param unescape the boolean indicating whether or not
     * to unescape CDATA Sections
    **/
    virtual void setUnescapeCDATA(MBool unescape);


    virtual void setUseEmptyElementShorthand(MBool useShorthand);

    /**
     * Sets whether or not this XMLPrinter should add whitespace
     * to pretty print the XML tree
     * @param useFormat a boolean to indicate whether to allow the
     * XMLPrinter to add whitespace to the XML tree. (false by default)
    **/
    virtual void setUseFormat(MBool useFormat);

protected:

    static const String CDATA_END;
    static const String CDATA_START;
    static const String COMMENT_START;
    static const String COMMENT_END;
    static const String DOCTYPE_START;
    static const String DOCTYPE_END;
    static const String DOUBLE_QUOTE;
    static const String EQUALS;
    static const String FORWARD_SLASH;
    static const String L_ANGLE_BRACKET;
    static const String PI_START;
    static const String PI_END;
    static const String PUBLIC;
    static const String R_ANGLE_BRACKET;
    static const String SEMICOLON;
    static const String SPACE;
    static const String SYSTEM;
    static const String XML_DECL;

    // chars
    static const char   AMPERSAND;
    static const char   GT;
    static const char   LT;
    static const char   DASH;

      //---------------------/
     //- Protected Methods -/
    //---------------------/

    /**
     * prints the given node to this XMLPrinter's Writer. If the
     * useFormat flag has been set, the node will be printed with
     * indentation equal to currentIndent + indentSize
     * @param node the Node to print
     * @param currentIndent the current indent String
     * @return true, if and only if a new line was printed at
     * the end of printing the given node
    **/
    virtual MBool print(Node* node, String& currentIndent);

    /**
     * Prints the proper UTF8 character
    **/
    void printUTF8Char(DOM_CHAR ch) const;

    /**
     * Print the proper UTF8 characters
     * based on code submitted by Majkel Kretschmar
    **/
    void printUTF8Chars(const DOMString& data);


private:

    static const char CR;
    static const char LF;
    static const String AMP_ENTITY;
    static const String GT_ENTITY;
    static const String LT_ENTITY;
    static const String HEX_ENTITY;

    String version;
    String entityTokens;

    /**
     * The a string comprised of indentSize number of indentChar's
    **/
    String indent;

    /**
     * The character used for indentation
    **/
    char indentChar;

    /**
     * The size of the indentation
    **/
    int indentSize;


    /**
     * The out stream to print results to
    **/
    ostream* ostreamPtr;

    /**
     * A flag indicating whether or not to unescape CDATA sections
    **/
    MBool unescapeCDATA;

    MBool useEmptyElementShorthand;

    /**
     * A flag indicating whether or not to add whitespace
     * such as line breaks while printing
    **/
    MBool useFormat;



      //-------------------/
     //- Private Methods -/
    //-------------------/

    /**
     * Called by Constructor to initialize Object instance
    **/
    void initialize(ostream& os, int indentSize);

    /**
     * Replaces any occurances of the special characters with their
     * appropriate entity reference and prints the String
    **/
    void printWithXMLEntities(const DOMString& data);

    /**
     * Replaces any occurances of -- inside comment data with - -
     * and prints the String
     * @param data the comment data (does not include start and end tags)
    **/
    void printComment(const DOMString& data);

}; //-- XMLPrinter

/**
 * A class for printing an XML node as non-well-formed HTML
 * This class was ported from XSL:P Java source
 * @author Keith Visco (kvisco@ziplink.net)
 * @version $Revision: 1.4 $ $Date: 2000/04/12 22:32:13 $
**/
class HTMLPrinter : public XMLPrinter {

public:

      //---------------/
     //- Contructors -/
    //---------------/

    /**
     * Default constructor uses cout as the default ostream
    **/
    HTMLPrinter();

    /**
     * Creates a new XML Printer using the given PrintWriter
     * for output
     * @param writer the PrintWriter to use for output
    **/
    HTMLPrinter(ostream& os);

    /**
     * Creates a new XML Printer using the given PrintWriter
     * for output, and nodes are indenting using the specified
     * indent size
     * @param os the out stream to use for output
     * @param indent the number of spaces to indent
    **/
    HTMLPrinter (ostream& os, int indent);

    /**
     * Sets whether or not this XMLPrinter should add whitespace
     * to pretty print the XML tree
     * @param useFormat a boolean to indicate whether to allow the
     * XMLPrinter to add whitespace to the XML tree. (false by default)
    **/
    virtual void setUseFormat(MBool useFormat);

protected:

      //---------------------/
     //- Protected Methods -/
    //---------------------/

    /**
     * prints the given node to this XMLPrinter's Writer. If the
     * useFormat flag has been set, the node will be printed with
     * indentation equal to currentIndent + indentSize
     * @param node the Node to print
     * @param currentIndent the current indent String
     * @return true, if and only if a new line was printed at
     * the end of printing the given node
    **/
    virtual MBool print(Node* node, String& currentIndent);

private:

    NamedMap htmlEmptyTags;
    MBool useFormat;

    /**
     * The out stream to print results to
    **/
    ostream* ostreamPtr;

      //-------------------/
     //- Private Methods -/
    //-------------------/

    /**
     * Called by Constructor to initialize Object instance
    **/
    void initialize(ostream& os, int indentSize);

}; //-- HTMLPrinter

#endif
