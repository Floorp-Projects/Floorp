/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Initial Developer of the Original Code is Axel Hecht.
 * Portions created by Keith Visco (C) 2000 Axel Hecht. 
 * All Rights Reserved.
 * 
 * Contributor(s): 
 * Axel Hecht <axel@pike.org>
 *    -- original author
 *
 */

#include "printers.h"

  //--------------------------------/
 //- Implementation of TEXTPrinter -/
//--------------------------------/

/**
 * A class for printing XML nodes as plain text.
 * See http://www.w3.org/TR/xslt, 16.3
 * Just output the stringValue of text nodes.
 *
 * bad ripoff from HTMLPrinter.cpp :-)
 *
 * @author <a href="mailto:axel@pike.org">Axel Hecht</a>
**/
  //---------------/
 //- Contructors -/
//---------------/
/**
 * Default Constructor. Creates a new TEXTPrinter using cout as the ostream.
**/
TEXTPrinter::TEXTPrinter() : XMLPrinter() {
    initialize(cout);
} //-- TEXTPrinter

/**
 * Creates a new text Printer using the given ostream for output
 * @param os the out stream to use for output
**/
TEXTPrinter::TEXTPrinter(ostream& os) : XMLPrinter(os) {
    initialize(os);
} //-- TEXTPrinter

/**
 * Creates a new text Printer using the given ostream
 * for output, setting indenting to 0 (brute force)
 * @param os the out stream to use for output
 * @param indent the number of spaces to indent
**/
TEXTPrinter::TEXTPrinter (ostream& os, int indent) : XMLPrinter(os, 0) {
    initialize(os);
} //-- TEXTPrinter

void TEXTPrinter::initialize(ostream& os) {
    ostreamPtr = &os;

} //-- initialize

/**
 * prints the given node to this TEXTPrinter's Writer. If the
 * useFormat flag has been set, the node will be printed with
 * indentation equal to currentIndent + indentSize
 * @param node the Node to print
 * @param currentIndent the current indent String
 * @return true, if and only if a new line was printed at
 * the end of printing the given node
**/
void TEXTPrinter::print(Node* node) {

    ostream& out = *this->ostreamPtr;

    while (node){
      if (node->getNodeType() == Node::TEXT_NODE){
	out << ((Text*)node)->getData();
      }
      if (node->getFirstChild())
        print (node->getFirstChild());
      node = node->getNextSibling();
    }
    return;
} //-- print

