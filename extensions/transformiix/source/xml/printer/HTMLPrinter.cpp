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
 *
 * Michel Casabianca, casa@sdv.fr
 *    -- added additional empty elements to the HTML tag list
 * $Id: HTMLPrinter.cpp,v 1.5 2001/04/08 14:37:24 peterv%netscape.com Exp $
 */

#include "printers.h"

  //--------------------------------/
 //- Implementation of HTMLPrinter -/
//--------------------------------/

/**
 * A class for printing XML nodes.
 * This class was ported from XSL:P Java source
 * @author <a href="kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.5 $ $Date: 2001/04/08 14:37:24 $
**/
  //---------------/
 //- Contructors -/
//---------------/
/**
 * Default Constructor. Creates a new HTMLPrinter using cout as the ostream.
**/
HTMLPrinter::HTMLPrinter() : XMLPrinter() {
    initialize(cout, DEFAULT_INDENT);
} //-- HTMLPrinter

/**
 * Creates a new HTML Printer using the given ostream for output
 * @param os the out stream to use for output
**/
HTMLPrinter::HTMLPrinter(ostream& os) : XMLPrinter(os) {
    initialize(os, DEFAULT_INDENT);
} //-- HTMLPrinter

/**
 * Creates a new HTML Printer using the given ostream
 * for output, and nodes are indenting using the specified
 * indent size
 * @param os the out stream to use for output
 * @param indent the number of spaces to indent
**/
HTMLPrinter::HTMLPrinter (ostream& os, int indent) : XMLPrinter(os, indent) {
    initialize(os, indent);
} //-- HTMLPrinter

void HTMLPrinter::initialize(ostream& os, int indentSize) {
    ostreamPtr = &os;
    XMLPrinter::setUseEmptyElementShorthand(MB_FALSE);
    setUseFormat(MB_TRUE);

    TxObject* nonNull = &htmlEmptyTags;
    htmlEmptyTags.put("AREA",       nonNull);
    htmlEmptyTags.put("BASE",       nonNull);
    htmlEmptyTags.put("BASEFONT",   nonNull);
    htmlEmptyTags.put("BR",         nonNull);
    htmlEmptyTags.put("FRAME",      nonNull);
    htmlEmptyTags.put("HR",         nonNull);    
    htmlEmptyTags.put("IMAGE",      nonNull);
    htmlEmptyTags.put("IMG",        nonNull);
    htmlEmptyTags.put("INPUT",      nonNull);
    htmlEmptyTags.put("ISINDEX",    nonNull);
    htmlEmptyTags.put("LI",         nonNull);
    htmlEmptyTags.put("LINK",       nonNull);
    htmlEmptyTags.put("META",       nonNull);
    htmlEmptyTags.put("P",          nonNull);
    htmlEmptyTags.put("PARAM",      nonNull);

} //-- initialize

/**
 * Sets whether or not this HTMLPrinter should add whitespace
 * to pretty print the XML tree
 * @param useFormat a boolean to indicate whether to allow the
 * Printer to add whitespace to the XML tree. (true by default)
**/

void HTMLPrinter::setUseFormat(MBool useFormat) {
    this->useFormat = useFormat;
    XMLPrinter::setUseFormat(useFormat);
} //-- setUseFormat

  //---------------------/
 //- Protected Methods -/
//---------------------/

/**
 * prints the given node to this HTMLPrinter's Writer. If the
 * useFormat flag has been set, the node will be printed with
 * indentation equal to currentIndent + indentSize
 * @param node the Node to print
 * @param currentIndent the current indent String
 * @return true, if and only if a new line was printed at
 * the end of printing the given node
**/
MBool HTMLPrinter::print(Node* node, String& currentIndent) {

    ostream& out = *this->ostreamPtr;

    //-- if (node == null) return false;

    NodeList* nl;

    switch(node->getNodeType()) {

        //-- print Document Node
        case Node::DOCUMENT_NODE:
        {
            out << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"";
            out <<endl<< "    \"http://www.w3.org/TR/REC-html40/loose.dtd\">" <<endl;
            Document* doc = (Document*)node;
            //-- printDoctype(doc.getDoctype());
            nl = doc->getChildNodes();
            for (int i = 0; i < nl->getLength(); i++) {
                print(nl->item(i),currentIndent);
            }
            break;
        }
        case Node::ELEMENT_NODE :
        {
            String nodeName = node->getNodeName();
            nodeName.toUpperCase();

            //-- ALL THIS CODE WILL BE CHANGING I AM CURRENTLY
            //-- WRITING THE SAX-LIKE PRINTER, WHICH WILL BE USED
            //-- BY THIS PRINTER
            if (nodeName.isEqual("SCRIPT")) {
	       Element* element = (Element*)node;
	       out << L_ANGLE_BRACKET;
               out << nodeName;
                NamedNodeMap* attList = element->getAttributes();
                int size = 0;
                if (attList) size = attList->getLength();
                Attr* att = 0;
                int i = 0;
                for ( ; i < size; i++) {
                    att = (Attr*) attList->item(i);
                    out << SPACE;
                    out << att->getName();
                    const String& data = att->getValue();
                    if (&data != &NULL_STRING) {
                        out << EQUALS << DOUBLE_QUOTE;
                        out << data;
                        out << DOUBLE_QUOTE;
                    }
                }
                out << R_ANGLE_BRACKET;
                NodeList* nl = element->getChildNodes();
                if (useFormat) out<<endl;
                for (i = 0; i < nl->getLength(); i++) {
		  Node* child = nl->item(i);
		  switch(child->getNodeType()) {
		      case Node::COMMENT_NODE:
		      {
                          out << COMMENT_START;
                          out << ((CharacterData*)child)->getData();
                          out << COMMENT_END;
                          break;
		      }
		      case Node::TEXT_NODE:
                      case Node::CDATA_SECTION_NODE:
                      {
			  out << ((Text*)child)->getData();
                          break;
                      }
		      default:
			  break;
		  }
                }
                out << flush;
                if (useFormat) {
		    out << endl << currentIndent;                    
		}
                out << L_ANGLE_BRACKET << FORWARD_SLASH;
                out << nodeName;
                out << R_ANGLE_BRACKET;
		if (useFormat) {
                    out << endl;
                    return MB_TRUE;
		}
                return MB_FALSE;

	    }
            //-- handle special elements
            else if (node->hasChildNodes() || 
		     ( !htmlEmptyTags.get(nodeName)))  
            {
                return XMLPrinter::print(node, currentIndent);
            }
            else {
                Element* element = (Element*)node;
                out << L_ANGLE_BRACKET;
                out << element->getNodeName();
                NamedNodeMap* attList = element->getAttributes();
                int size = 0;
                if (attList) size = attList->getLength();
                Attr* att = 0;
                for (int i = 0; i < size; i++) {
                    att = (Attr*) attList->item(i);
                    out << SPACE;
                    out << att->getName();
                    const String& data = att->getValue();
                    if (&data != &NULL_STRING) {
                        out << EQUALS << DOUBLE_QUOTE;
                        out << data;
                        out << DOUBLE_QUOTE;
                    }
                }
                out << R_ANGLE_BRACKET;
                if (useFormat) {
                    Node* sibling = node->getNextSibling();
                    if ((!sibling) ||
                        (sibling->getNodeType() != Node::TEXT_NODE))
                    {
                        out <<endl;
                        return MB_TRUE;
                    }
                }

            }
            break;
        }
        default:
            return XMLPrinter::print(node, currentIndent);
    } //-- switch

    //-- no new line, so return false;
    return MB_FALSE;
} //-- print

