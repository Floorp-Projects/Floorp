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
 * The Original Code is XSL:P XSLT processor.
 * 
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco (C) 1999 Keith Visco. 
 * All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author
 * Majkel Kretschmar
 *    -- UTF-8 changes
 * Bob Miller, kbob@oblix.com
 *    -- plugged core leak.
 *
 * $Id: XMLPrinter.cpp,v 1.5 2000/07/23 06:45:59 kvisco%ziplink.net Exp $
 */

#include "printers.h"

  //--------------------------------/
 //- Implementation of XMLPrinter -/
//--------------------------------/

/**
 * A class for printing XML nodes.
 * This class was ported from XSL:P Java source
 * @author <a href="kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.5 $ $Date: 2000/07/23 06:45:59 $
**/

/**
 * The default indent size
**/
const int XMLPrinter::DEFAULT_INDENT = 2;


const String XMLPrinter::AMP_ENTITY    = "&amp;";
const String XMLPrinter::GT_ENTITY     = "&gt;";
const String XMLPrinter::LT_ENTITY     = "&lt;";
const String XMLPrinter::HEX_ENTITY    = "&#";

const String XMLPrinter::CDATA_END        = "]]>";
const String XMLPrinter::CDATA_START      = "<![CDATA[";
const String XMLPrinter::COMMENT_START    = "<!--";
const String XMLPrinter::COMMENT_END      = "-->";
const String XMLPrinter::DOCTYPE_START    = "<!DOCTYPE ";
const String XMLPrinter::DOCTYPE_END      = ">";
const String XMLPrinter::DOUBLE_QUOTE     = "\"";
const String XMLPrinter::EQUALS           = "=";
const String XMLPrinter::FORWARD_SLASH    = "/";
const String XMLPrinter::L_ANGLE_BRACKET  = "<";
const String XMLPrinter::PI_START         = "<?";
const String XMLPrinter::PI_END           = "?>";
const String XMLPrinter::PUBLIC           = "PUBLIC";
const String XMLPrinter::R_ANGLE_BRACKET  = ">";
const String XMLPrinter::SEMICOLON        = ";";
const String XMLPrinter::SPACE            = " ";
const String XMLPrinter::SYSTEM           = "SYSTEM";
const String XMLPrinter::XML_DECL         = "xml version=";

// chars
const char   XMLPrinter::AMPERSAND        = '&';
const char   XMLPrinter::GT               = '>';
const char   XMLPrinter::LT               = '<';
const char   XMLPrinter::DASH             = '-';
const char   XMLPrinter::TX_CR            = '\r';
const char   XMLPrinter::TX_LF            = '\n';


  //---------------/
 //- Contructors -/
//---------------/

/**
 * Default Constructor. Creates a new XMLPrinter using cout as the ostream.
**/
XMLPrinter::XMLPrinter() {
    initialize(cout, DEFAULT_INDENT);
} //-- XMLPrinter

/**
 * Creates a new XML Printer using the given ostream for output
 * @param os the out stream to use for output
**/
XMLPrinter::XMLPrinter(ostream& os) {
    initialize(os, DEFAULT_INDENT);
} //-- XMLPrinter

/**
 * Creates a new XML Printer using the given ostream
 * for output, and nodes are indenting using the specified
 * indent size
 * @param os the out stream to use for output
 * @param indent the number of spaces to indent
**/
XMLPrinter::XMLPrinter (ostream& os, int indent) {
    initialize(os, indent);
} //-- XMLPrinter

void XMLPrinter::initialize(ostream& os, int indentSize) {
    ostreamPtr = &os;
    indentChar = ' ';
    version = "1.0";
    entityTokens = "&<>";
    setIndentSize(indentSize);
    unescapeCDATA = MB_FALSE;
    useEmptyElementShorthand = MB_TRUE;
    useFormat = MB_FALSE;
} //-- initialize

// destructor is needed so that subclasses are destroyed.

XMLPrinter::~XMLPrinter()
{ }

/**
 * Prints the given Node
 * @param node the Node to print
**/
void XMLPrinter::print(Node* node) {
    String currentIndent;
    print(node,currentIndent);
    *ostreamPtr<<flush;
} //-- print

/* -- add later
void XMLPrinter::printDoctype(DocumentType docType);
*/

/**
 * Sets the indent size
 * @param indent the number of spaces to indent
**/
void XMLPrinter::setIndentSize(int indentSize) {
    this->indentSize = indentSize;
    indent.clear();
    for (int i = 0; i < indentSize; i++) {
        indent.append(indentChar);
    }
} //-- setIndentSize

/**
 * Sets whether or not to "unwrap" CDATA Sections
 * when printing. By Default CDATA Sections are left as is.
 * @param unescape the boolean indicating whether or not
 * to unescape CDATA Sections
**/
void XMLPrinter::setUnescapeCDATA(MBool unescape) {
    unescapeCDATA = unescape;
} //-- setUnescapeCDATA


void XMLPrinter::setUseEmptyElementShorthand(MBool useShorthand) {
    useEmptyElementShorthand = useShorthand;
} //-- setUseEmptyElementShorthand

/**
 * Sets whether or not this XMLPrinter should add whitespace
 * to pretty print the XML tree
 * @param useFormat a boolean to indicate whether to allow the
 * XMLPrinter to add whitespace to the XML tree. (false by default)
**/
void XMLPrinter::setUseFormat(MBool useFormat) {
    this->useFormat = useFormat;
} //-- setUseFormat

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
MBool XMLPrinter::print(Node* node, String& currentIndent) {

    ostream& out = *this->ostreamPtr;

    //-- if (node == null) return false;

    NodeList* nl;

    switch(node->getNodeType()) {

        //-- print Document Node
        case Node::DOCUMENT_NODE:
        {
            Document* doc = (Document*)node;
            out << PI_START << XML_DECL << DOUBLE_QUOTE;
            out << version;
            out << DOUBLE_QUOTE << PI_END << endl;
            //-- printDoctype(doc.getDoctype());
            nl = doc->getChildNodes();
            for (int i = 0; i < nl->getLength(); i++) {
                print(nl->item(i),currentIndent);
            }
            break;
        }
        //-- print Attribute Node
        case Node::ATTRIBUTE_NODE:
        {
            Attr* attr = (Attr*)node;
            //out << attr->getName();
            out << attr->getNodeName();
            const String& data = attr->getNodeValue();
            if (&data != &NULL_STRING) {
                out << EQUALS << DOUBLE_QUOTE;
                out << data;
                out << DOUBLE_QUOTE;
            }
           break;
        }
        //-- print Element
        case Node::ELEMENT_NODE:
        {
            Element* element = (Element*)node;
            out << L_ANGLE_BRACKET;
            out << element->getNodeName();

            NamedNodeMap* attList = element->getAttributes();
            if (attList) {
                //-- print attribute nodes
                Attr* att;
                for (int i = 0; i < attList->getLength(); i++) {
                    att = (Attr*)attList->item(i);
                    const String& data = att->getValue();
                    //out << SPACE << * (att->getName());
                    out << SPACE << att->getNodeName();
                    if (&data != &NULL_STRING) {
                        out << EQUALS << DOUBLE_QUOTE;
                        out << data;
                        out << DOUBLE_QUOTE;
                    }
                }
            }

            NodeList* childList = element->getChildNodes();
            int size = childList->getLength();
            if ((size == 0) && (useEmptyElementShorthand))
            {
                out << FORWARD_SLASH << R_ANGLE_BRACKET;
                if (useFormat) {
                    out << endl;
                    return MB_TRUE;
                }
            }
            else {
                // Either children, or no shorthand
                MBool newLine = MB_FALSE;
                out << R_ANGLE_BRACKET;
                if ((useFormat) && (size > 0)) {
                    // Fix formatting of PCDATA elements by Peter Marks and
                    // David King Lassman
                    // -- add if statement to check for text node before
                    //    adding line break
                    if (childList->item(0)->getNodeType() != Node::TEXT_NODE) {
                        out << endl;
                        newLine = MB_TRUE;
                    }
                }

                Node* child = 0;
                String newIndent(indent);
                newIndent.append(currentIndent);
                for (int i = 0; i < size; i++) {
                    child = childList->item(i);
                    if ((useFormat) && newLine)
                    {
                        out << newIndent;
                    }
                    newLine = print(child,newIndent);
                }
                if (useFormat) {
                    // Fix formatting of PCDATA elements by Peter Marks and
                    // David King Lassman
                    // -- add if statement to check for text node before
                    //    adding line break
                    if (child) {
                        if (child->getNodeType() != Node::TEXT_NODE) {
                            out << currentIndent;
                        }
                    }
                }
                out << L_ANGLE_BRACKET << FORWARD_SLASH;
                out << element->getNodeName();
                out << R_ANGLE_BRACKET;
                if (useFormat) {
                    Node* sibling = node->getNextSibling();
                    if ((!sibling) ||
                        (sibling->getNodeType() != Node::TEXT_NODE))
                    {
                        out<<endl;
                        return MB_TRUE;
                    }
                }
            } //-- end if
            break;
        }
        case Node::TEXT_NODE:
        {
            const String& data = ((Text*)node)->getData();
            printWithXMLEntities(data);
            break;
        }
        case Node::CDATA_SECTION_NODE:
            if (unescapeCDATA)
                printWithXMLEntities( ((Text*)node)->getData() );
            else {
                const String& data = ((Text*)node)->getData();
                out << CDATA_START;
                printUTF8Chars(data);
                out << CDATA_END;
            }
            break;
        case Node::COMMENT_NODE:
            out << COMMENT_START;
            printComment(((CharacterData*)node)->getData());
            out << COMMENT_END;
            if (useFormat) {
                out <<endl;
                return MB_TRUE;
            }
            break;
        case Node::ENTITY_REFERENCE_NODE:
            out << AMPERSAND << node->getNodeName() << SEMICOLON;
            break;
        case Node::PROCESSING_INSTRUCTION_NODE:
        {
            ProcessingInstruction* pi = (ProcessingInstruction*)node;
            out << PI_START;
            out << pi->getTarget();
            out << SPACE;
            out << pi->getData();
            out << PI_END;
            if (useFormat) {
                out <<endl;
                return MB_TRUE;
            }
            break;
        }
        case Node::DOCUMENT_TYPE_NODE:
            //--printDoctype((DocumentType*)node);
            break;
        default:
            break;
    } //-- switch

    //-- no new line, so return false;
    return MB_FALSE;
} //-- print

/**
 * Print the proper UTF8 character
 * based on code submitted by Majkel Kretschmar
**/
void XMLPrinter::printUTF8Char(DOM_CHAR ch) const {
    ostream& out = *this->ostreamPtr;
    if (ch >= 128) {
        out << HEX_ENTITY;
        if ( ch >= 256 ) out << (ch % 256);
        else out << ch;
        out << SEMICOLON;
    }
    else out << (char)ch;
} //-- printUTF8Char

/**
 * Print the proper UTF8 characters
 * based on code submitted by Majkel Kretschmar
**/
void XMLPrinter::printUTF8Chars(const String& data) {
    int i = 0;
    while(i < data.length()) printUTF8Char(data.charAt(i++));
} //-- printUTF8Chars

  //-------------------/
 //- Private Methods -/
//-------------------/


void XMLPrinter::printWithXMLEntities(const String& data) {
    DOM_CHAR currChar;

    if (&data == &NULL_STRING) return;

    for (int i = 0; i < data.length(); i++) {
        currChar = data.charAt(i);
        switch (currChar) {
            case AMPERSAND:
                *ostreamPtr << AMP_ENTITY;
                break;
            case LT:
                *ostreamPtr << LT_ENTITY;
                break;
            case GT:
                *ostreamPtr << GT_ENTITY;
                break;
            default:
                printUTF8Char(currChar);
                break;
        }
    }
    *ostreamPtr << flush;
} // -- printWithXMLEntities

/**
 * Replaces any occurances of -- inside comment data with - -
 * @param data the comment data (does not include start and end tags)
**/
void XMLPrinter::printComment(const String& data) {
    DOM_CHAR prevChar;
    DOM_CHAR currChar;

    if (&data == &NULL_STRING) return;

    //-- since comments will start with <!-- set prevChar to '-'
    prevChar = DASH;

    for (int i = 0; i < data.length(); i++) {
        currChar = data.charAt(i);

        if ((currChar == DASH) && (prevChar == DASH))
            *ostreamPtr << SPACE << DASH;
        else
            printUTF8Char(currChar);

        prevChar = currChar;
    }

    //-- handle last char as a dash
    if (prevChar == DASH) *ostreamPtr << SPACE;

} //-- printComment

