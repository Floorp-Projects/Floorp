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
 * Portions created by Keith Visco (C) 1999-2000 Keith Visco.
 * All Rights Reserved..
 *
 * Contributor(s):
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * $Id: NodeSorter.h,v 1.3 2001/04/08 14:38:57 peterv%netscape.com Exp $
 */


#ifndef TRANSFRMX_NODESORTER_H
#define TRANSFRMX_NODESORTER_H

#include "TxString.h"
#include "dom.h"
#include "NodeSet.h"
#include "ProcessorState.h"

/*
 * Sorts Nodes as specified by the W3C XSLT 1.0 Recommendation
 */

class NodeSorter {

public:

    //-- static variables
    static const String DEFAULT_LANG;
    static const String ASCENDING_ORDER;
    static const String DESCENDING_ORDER;
    static const String TEXT_TYPE;
    static const String NUMBER_TYPE;

    /**
     * Sorts the given Node Array using this XSLSort's properties as a
     * basis for the sort
     * @param nodes the Array of nodes to sort
     * @param pState the ProcessorState to evaluate the Select pattern with
     * @return the a new Array of sorted nodes
    **/
    static void sort
        (NodeSet* nodes, Element* sortElement, Node* context, ProcessorState* ps);

private:
    
    /**
     * Sorts the given String[]
    **/
    static void sortKeys
        (String** keys, int length, MBool ascending, const String& dataType, const String& lang);


};

#endif
