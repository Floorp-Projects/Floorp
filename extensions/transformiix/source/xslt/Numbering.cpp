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
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */

/**
 * Numbering methods
**/

#include "Numbering.h"
#include "Names.h"
#include "txAtoms.h"
#include "XMLUtils.h"

void Numbering::doNumbering(Element* xslNumber, String& dest,
                            ProcessorState* ps)
{
    if (!xslNumber)
        return;

    int* counts = 0;
    int nbrOfCounts = 0;

    //-- check for expr
    if (xslNumber->hasAttr(txXSLTAtoms::value, kNameSpaceID_None)) {
        Expr* expr = ps->getExpr(xslNumber, ProcessorState::ValueAttr);
        if (!expr) {
            // XXX error reporting, parse failed
            return;
        }
        ExprResult* result = expr->evaluate(ps->getEvalContext());
        double dbl = Double::NaN;
        if (result)
            dbl = result->numberValue();
        delete result;
        nbrOfCounts = 1;
        counts = new int[1];
        counts[0] = (int)dbl;
    }
    else {
        Node* context = ps->getEvalContext()->getContextNode();

        // create count pattern
        String countAttr;
        xslNumber->getAttr(txXSLTAtoms::count, kNameSpaceID_None,
                           countAttr);

        txPattern* countPattern = 0;
        MBool ownsPattern;
        
        if (!countAttr.isEmpty()) {
            countPattern = ps->getPattern(xslNumber,
                                          ProcessorState::CountAttr);
            ownsPattern = MB_FALSE;
        }
        else {
            MBool isAttr = MB_FALSE;
            Node::NodeType type = Node::ELEMENT_NODE;
            txNodeTypeTest::NodeType nodetype;
            switch(context->getNodeType()) {
                case Node::ATTRIBUTE_NODE:
                    isAttr = MB_TRUE;
                    type = Node::ATTRIBUTE_NODE;
                case Node::ELEMENT_NODE:
                    {
                        const String& name = context->getNodeName();
                        String prefix, lname;
                        XMLUtils::getPrefix(name, prefix);
                        XMLUtils::getLocalPart(name, lname);
                        txAtom* prefixAtom = 0;
                        if (!prefix.isEmpty()) {
                            prefixAtom = TX_GET_ATOM(prefix);
                        }
                        txAtom* lNameAtom = TX_GET_ATOM(lname);
                        PRInt32 NSid = context->getNamespaceID();
                        txNameTest* nt = new txNameTest(prefixAtom, lNameAtom,
                                                        NSid, type);
                        TX_IF_RELEASE_ATOM(prefixAtom);
                        TX_IF_RELEASE_ATOM(lNameAtom);
                        countPattern = new txStepPattern(nt, isAttr);
                    }
                    break;
                case Node::CDATA_SECTION_NODE :
                case Node::TEXT_NODE :
                    nodetype = txNodeTypeTest::TEXT_TYPE;
                    break;
                case Node::COMMENT_NODE :
                    nodetype = txNodeTypeTest::COMMENT_TYPE;
                    break;
                case Node::PROCESSING_INSTRUCTION_NODE :
                    nodetype = txNodeTypeTest::PI_TYPE;
                    break;
                default:
                    NS_ASSERTION(0, "Unexpected Node type");
                    delete counts;
                    return;
            }
            if (!countPattern) {
                // not a nametest
                txNodeTypeTest* nt  = new txNodeTypeTest(nodetype);
                countPattern = new txStepPattern(nt, MB_FALSE);
            }
            ownsPattern = MB_TRUE;
        }
        if (!countPattern) {
            delete counts;
            return;
        }

        NodeSet* nodes = 0;
        int cnum = 0;

        String level;
        xslNumber->getAttr(txXSLTAtoms::level, kNameSpaceID_None,
                           level);
        String fromAttr;
        xslNumber->getAttr(txXSLTAtoms::from, kNameSpaceID_None,
                           fromAttr);
        txPattern* from = 0;

        if (MULTIPLE_VALUE.isEqual(level))
            nodes = getAncestorsOrSelf(countPattern,
                                       from,
                                       context,
                                       ps,
                                       MB_FALSE);
        //else if (ANY_VALUE.isEqual(level))
        //    nodes = getAnyPreviousNodes(countExpr, context, ps);
        else
            nodes = getAncestorsOrSelf(countPattern,
                                       from,
                                       context,
                                       ps,
                                       MB_TRUE);

        nbrOfCounts = nodes->size();
        counts = new int[nbrOfCounts];
        cnum = 0;
        for (int i = nodes->size()-1; i >= 0; i--) {
            counts[cnum++] =
                countPreceedingSiblings(countPattern, nodes->get(i), ps);
        }
        delete nodes;
        if (ownsPattern)
            delete countPattern;
    }
    //-- format counts
    for ( int i = 0; i < nbrOfCounts; i++) {
        Double::toString(counts[i], dest);
    }
    delete counts;
} //-- doNumbering

int Numbering::countPreceedingSiblings
    (txPattern* patternExpr, Node* context, ProcessorState* ps)
{
    int count = 1;

    if (!context) return 0;

    Node* sibling = context;
    while ((sibling = sibling->getPreviousSibling())) {
        if (patternExpr->matches(sibling, ps))
            ++count;
    }
    return count;
} //-- countPreceedingSiblings

NodeSet* Numbering::getAncestorsOrSelf
    (txPattern* countExpr,
     txPattern* from,
     Node* context,
     ProcessorState* ps,
     MBool findNearest)
{
    NodeSet* nodeSet = new NodeSet();
    Node* parent = context;
    while ((parent)  && (parent->getNodeType() == Node::ELEMENT_NODE))
    {
        if (from && from->matches(parent, ps))
            break;

        if (countExpr->matches(parent, ps)) {
            nodeSet->append(parent);
            if (findNearest) break;
        }
        parent = parent->getParentNode();
    }
    return nodeSet;
} //-- fromAncestorsOrSelf
