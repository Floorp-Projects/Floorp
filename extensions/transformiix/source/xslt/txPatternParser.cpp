/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT Processor.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Axel Hecht <axel@pike.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "txPatternParser.h"
#include "ExprLexer.h"
#include "txAtoms.h"
#include "txStringUtils.h"
#include "txXSLTPatterns.h"

txPattern* txPatternParser::createPattern(const nsAFlatString& aPattern,
                                          txIParseContext* aContext,
                                          ProcessorState* aPs)
{
    txPattern* pattern = 0;
    ExprLexer lexer(aPattern);
    nsresult rv = createUnionPattern(lexer, aContext, aPs, pattern);
    if (NS_FAILED(rv)) {
        // XXX error report parsing error
        return 0;
    }
    return pattern;
}

nsresult txPatternParser::createUnionPattern(ExprLexer& aLexer,
                                             txIParseContext* aContext,
                                             ProcessorState* aPs,
                                             txPattern*& aPattern)
{
    nsresult rv = NS_OK;
    txPattern* locPath = 0;

    rv = createLocPathPattern(aLexer, aContext, aPs, locPath);
    if (NS_FAILED(rv))
        return rv;

    short type = aLexer.peek()->type;
    if (type == Token::END) {
        aPattern = locPath;
        return NS_OK;
    }

    if (type != Token::UNION_OP) {
        delete locPath;
        return NS_ERROR_XPATH_PARSE_FAILED;
    }

    txUnionPattern* unionPattern = new txUnionPattern();
    if (!unionPattern) {
        delete locPath;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    rv = unionPattern->addPattern(locPath);
#if 0 // XXX addPattern can't fail yet, it doesn't check for mem
    if (NS_FAILED(rv)) {
        delete unionPattern;
        delete locPath;
        return rv;
    }
#endif

    aLexer.nextToken();
    do {
        rv = createLocPathPattern(aLexer, aContext, aPs, locPath);
        if (NS_FAILED(rv)) {
            delete unionPattern;
            return rv;
        }
        rv = unionPattern->addPattern(locPath);
#if 0 // XXX addPattern can't fail yet, it doesn't check for mem
        if (NS_FAILED(rv)) {
            delete unionPattern;
            delete locPath;
            return rv;
        }
#endif
        type = aLexer.nextToken()->type;
    } while (type == Token::UNION_OP);

    if (type != Token::END) {
        delete unionPattern;
        return NS_ERROR_XPATH_PARSE_FAILED;
    }

    aPattern = unionPattern;
    return NS_OK;
}

nsresult txPatternParser::createLocPathPattern(ExprLexer& aLexer,
                                               txIParseContext* aContext,
                                               ProcessorState* aPs,
                                               txPattern*& aPattern)
{
    nsresult rv = NS_OK;

    MBool isChild = MB_TRUE;
    MBool isAbsolute = MB_FALSE;
    txPattern* stepPattern = 0;
    txLocPathPattern* pathPattern = 0;

    short type = aLexer.peek()->type;
    switch (type) {
        case Token::ANCESTOR_OP:
            isChild = MB_FALSE;
            isAbsolute = MB_TRUE;
            aLexer.nextToken();
            break;
        case Token::PARENT_OP:
            aLexer.nextToken();
            isAbsolute = MB_TRUE;
            if (aLexer.peek()->type == Token::END || 
                aLexer.peek()->type == Token::UNION_OP) {
                aPattern = new txRootPattern(MB_TRUE);
                return aPattern ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
            }
            break;
        case Token::FUNCTION_NAME:
            // id(Literal) or key(Literal, Literal)
            {
                nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aLexer.nextToken()->value);
                if (nameAtom == txXPathAtoms::id) {
                    rv = createIdPattern(aLexer, stepPattern);
                }
                else if (nameAtom == txXSLTAtoms::key) {
                    rv = createKeyPattern(aLexer, aContext, aPs, stepPattern);
                }
                if (NS_FAILED(rv))
                    return rv;
            }
            break;
        default:
            break;
    }
    if (!stepPattern) {
        rv = createStepPattern(aLexer, aContext, stepPattern);
        if (NS_FAILED(rv))
            return rv;
    }

    type = aLexer.peek()->type;
    if (!isAbsolute && type != Token::PARENT_OP
        && type != Token::ANCESTOR_OP) {
        aPattern = stepPattern;
        return NS_OK;
    }

    pathPattern = new txLocPathPattern();
    if (!pathPattern) {
        delete stepPattern;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (isAbsolute) {
        txRootPattern* root = new txRootPattern(MB_FALSE);
        if (!root) {
            delete stepPattern;
            delete pathPattern;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        rv = pathPattern->addStep(root, isChild);
        if (NS_FAILED(rv)) {
            delete stepPattern;
            delete pathPattern;
            delete root;
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    rv = pathPattern->addStep(stepPattern, isChild);
    if (NS_FAILED(rv)) {
        delete stepPattern;
        delete pathPattern;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    stepPattern = 0; // stepPattern is part of pathPattern now

    while (type == Token::PARENT_OP || type == Token::ANCESTOR_OP) {
        isChild = type == Token::PARENT_OP;
        aLexer.nextToken();
        rv = createStepPattern(aLexer, aContext, stepPattern);
        if (NS_FAILED(rv)) {
            delete pathPattern;
            return rv;
        }
        rv = pathPattern->addStep(stepPattern, isChild);
        if (NS_FAILED(rv)) {
            delete stepPattern;
            delete pathPattern;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        stepPattern = 0; // stepPattern is part of pathPattern now
        type = aLexer.peek()->type;
    }
    aPattern = pathPattern;
    return rv;
}

nsresult txPatternParser::createIdPattern(ExprLexer& aLexer,
                                          txPattern*& aPattern)
{
    // check for '(' Literal ')'
    if (aLexer.nextToken()->type != Token::L_PAREN && 
        aLexer.peek()->type != Token::LITERAL)
        return NS_ERROR_XPATH_PARSE_FAILED;
    const nsString& value = aLexer.nextToken()->value;
    if (aLexer.nextToken()->type != Token::R_PAREN)
        return NS_ERROR_XPATH_PARSE_FAILED;
    aPattern  = new txIdPattern(value);
    return aPattern ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult txPatternParser::createKeyPattern(ExprLexer& aLexer,
                                           txIParseContext* aContext,
                                           ProcessorState* aPs,
                                           txPattern*& aPattern)
{
    // check for '(' Literal, Literal ')'
    if (aLexer.nextToken()->type != Token::L_PAREN && 
        aLexer.peek()->type != Token::LITERAL)
        return NS_ERROR_XPATH_PARSE_FAILED;
    const nsString& key = aLexer.nextToken()->value;
    if (aLexer.nextToken()->type != Token::COMMA && 
        aLexer.peek()->type != Token::LITERAL)
        return NS_ERROR_XPATH_PARSE_FAILED;
    const nsString& value = aLexer.nextToken()->value;
    if (aLexer.nextToken()->type != Token::R_PAREN)
        return NS_ERROR_XPATH_PARSE_FAILED;

    if (!XMLUtils::isValidQName(key))
        return NS_ERROR_XPATH_PARSE_FAILED;
    nsCOMPtr<nsIAtom> prefix, localName;
    PRInt32 namespaceID;
    nsresult rv = resolveQName(key, getter_AddRefs(prefix), aContext,
                               getter_AddRefs(localName), namespaceID);
    if (NS_FAILED(rv))
        return rv;

    aPattern  = new txKeyPattern(aPs, prefix, localName, namespaceID, value);

    return aPattern ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult txPatternParser::createStepPattern(ExprLexer& aLexer,
                                            txIParseContext* aContext,
                                            txPattern*& aPattern)
{
    nsresult rv = NS_OK;
    MBool isAttr = MB_FALSE;
    Token* tok = aLexer.peek();
    if (tok->type == Token::AXIS_IDENTIFIER) {
        if (TX_StringEqualsAtom(tok->value, txXPathAtoms::attribute)) {
            isAttr = MB_TRUE;
        }
        else if (!TX_StringEqualsAtom(tok->value, txXPathAtoms::child)) {
            // all done already for CHILD_AXIS, for all others
            // XXX report unexpected axis error
            return NS_ERROR_XPATH_PARSE_FAILED;
        }
        aLexer.nextToken();
    }
    else if (tok->type == Token::AT_SIGN) {
        aLexer.nextToken();
        isAttr = MB_TRUE;
    }
    tok = aLexer.nextToken();

    txNodeTest* nodeTest = 0;
    if (tok->type == Token::CNAME) {
        // resolve QName
        nsCOMPtr<nsIAtom> prefix, lName;
        PRInt32 nspace;
        rv = resolveQName(tok->value, getter_AddRefs(prefix), aContext,
                          getter_AddRefs(lName), nspace);
        if (NS_FAILED(rv)) {
            // XXX error report namespace resolve failed
            return rv;
        }
        if (isAttr) {
            nodeTest = new txNameTest(prefix, lName, nspace,
                                      Node::ATTRIBUTE_NODE);
        }
        else {
            nodeTest = new txNameTest(prefix, lName, nspace,
                                      Node::ELEMENT_NODE);
        }
        if (!nodeTest) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    else {
        aLexer.pushBack();
        nodeTest = createNodeTypeTest(aLexer);
        if (!nodeTest) {
            // XXX error report NodeTest expected
            return NS_ERROR_XPATH_PARSE_FAILED;
        }
    }

    txStepPattern* step = new txStepPattern(nodeTest, isAttr);
    if (!step) {
        delete nodeTest;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nodeTest = 0;
    if (!parsePredicates(step, aLexer, aContext)) {
        delete step;
        return NS_ERROR_XPATH_PARSE_FAILED;
    }

    aPattern = step;
    return NS_OK;
}
