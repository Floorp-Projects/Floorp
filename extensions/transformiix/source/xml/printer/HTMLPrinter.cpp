/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

#include "Printers.h"

  //--------------------------------/
 //- Implementation of HTMLPrinter -/
//--------------------------------/

/**
 * A class for printing XML nodes.
 * This class was ported from XSL:P Java source
 * @author <a href="kvisco@mitre.org">Keith Visco</a>
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

    MITREObject* nonNull = &htmlEmptyTags;
    htmlEmptyTags.put("BR",         nonNull);
    htmlEmptyTags.put("HR",         nonNull);
    htmlEmptyTags.put("IMAGE",      nonNull);
    htmlEmptyTags.put("INPUT",      nonNull);
    htmlEmptyTags.put("LI",         nonNull);
    htmlEmptyTags.put("META",       nonNull);
    htmlEmptyTags.put("P",          nonNull);

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

            //-- handle special elements
            if (node->hasChildNodes() || ( !htmlEmptyTags.get(nodeName) ))  {
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
                    const DOMString& data = att->getValue();
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

