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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Axel Hecht <axel@pike.org>
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
#include "txExprLexer.h"
#include "nsGkAtoms.h"
#include "txError.h"
#include "txStringUtils.h"
#include "txXSLTPatterns.h"
#include "txIXPathContext.h"
#include "txPatternOptimizer.h"


txPattern* txPatternParser::createPattern(const nsAFlatString& aPattern,
                                          txIParseContext* aContext)
{
    txExprLexer lexer;
    nsresult rv = lexer.parse(aPattern);
    if (NS_FAILED(rv)) {
        // XXX error report parsing error
        return 0;
    }
    nsAutoPtr<txPattern> pattern;
    rv = createUnionPattern(lexer, aContext, *getter_Transfers(pattern));
    if (NS_FAILED(rv)) {
        // XXX error report parsing error
        return 0;
    }

    txPatternOptimizer optimizer;
    txPattern* newPattern = nsnull;
    rv = optimizer.optimize(pattern, &newPattern);
    NS_ENSURE_SUCCESS(rv, nsnull);

    return newPattern ? newPattern : pattern.forget();
}

nsresult txPatternParser::createUnionPattern(txExprLexer& aLexer,
                                             txIParseContext* aContext,
                                             txPattern*& aPattern)
{
    nsresult rv = NS_OK;
    txPattern* locPath = 0;

    rv = createLocPathPattern(aLexer, aContext, locPath);
    if (NS_FAILED(rv))
        return rv;

    Token::Type type = aLexer.peek()->mType;
    if (type == Token::END) {
        aPattern = locPath;
        return NS_OK;
    }

    if (type != Token::UNION_OP) {
        delete locPath;
        return NS_ERROR_XPATH_PARSE_FAILURE;
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
        rv = createLocPathPattern(aLexer, aContext, locPath);
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
        type = aLexer.nextToken()->mType;
    } while (type == Token::UNION_OP);

    if (type != Token::END) {
        delete unionPattern;
        return NS_ERROR_XPATH_PARSE_FAILURE;
    }

    aPattern = unionPattern;
    return NS_OK;
}

nsresult txPatternParser::createLocPathPattern(txExprLexer& aLexer,
                                               txIParseContext* aContext,
                                               txPattern*& aPattern)
{
    nsresult rv = NS_OK;

    MBool isChild = MB_TRUE;
    MBool isAbsolute = MB_FALSE;
    txPattern* stepPattern = 0;
    txLocPathPattern* pathPattern = 0;

    Token::Type type = aLexer.peek()->mType;
    switch (type) {
        case Token::ANCESTOR_OP:
            isChild = MB_FALSE;
            isAbsolute = MB_TRUE;
            aLexer.nextToken();
            break;
        case Token::PARENT_OP:
            aLexer.nextToken();
            isAbsolute = MB_TRUE;
            if (aLexer.peek()->mType == Token::END || 
                aLexer.peek()->mType == Token::UNION_OP) {
                aPattern = new txRootPattern();

                return aPattern ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
            }
            break;
        case Token::FUNCTION_NAME_AND_PAREN:
            // id(Literal) or key(Literal, Literal)
            {
                nsCOMPtr<nsIAtom> nameAtom =
                    do_GetAtom(aLexer.nextToken()->Value());
                if (nameAtom == nsGkAtoms::id) {
                    rv = createIdPattern(aLexer, stepPattern);
                }
                else if (nameAtom == nsGkAtoms::key) {
                    rv = createKeyPattern(aLexer, aContext, stepPattern);
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

    type = aLexer.peek()->mType;
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
        txRootPattern* root = new txRootPattern();
        if (!root) {
            delete stepPattern;
            delete pathPattern;
            return NS_ERROR_OUT_OF_MEMORY;
        }

#ifdef TX_TO_STRING
        root->setSerialize(PR_FALSE);
#endif

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
        type = aLexer.peek()->mType;
    }
    aPattern = pathPattern;
    return rv;
}

nsresult txPatternParser::createIdPattern(txExprLexer& aLexer,
                                          txPattern*& aPattern)
{
    // check for '(' Literal ')'
    if (aLexer.peek()->mType != Token::LITERAL)
        return NS_ERROR_XPATH_PARSE_FAILURE;
    const nsDependentSubstring& value =
        aLexer.nextToken()->Value();
    if (aLexer.nextToken()->mType != Token::R_PAREN)
        return NS_ERROR_XPATH_PARSE_FAILURE;
    aPattern  = new txIdPattern(value);
    return aPattern ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult txPatternParser::createKeyPattern(txExprLexer& aLexer,
                                           txIParseContext* aContext,
                                           txPattern*& aPattern)
{
    // check for '(' Literal, Literal ')'
    if (aLexer.peek()->mType != Token::LITERAL)
        return NS_ERROR_XPATH_PARSE_FAILURE;
    const nsDependentSubstring& key =
        aLexer.nextToken()->Value();
    if (aLexer.nextToken()->mType != Token::COMMA && 
        aLexer.peek()->mType != Token::LITERAL)
        return NS_ERROR_XPATH_PARSE_FAILURE;
    const nsDependentSubstring& value =
        aLexer.nextToken()->Value();
    if (aLexer.nextToken()->mType != Token::R_PAREN)
        return NS_ERROR_XPATH_PARSE_FAILURE;

    const PRUnichar* colon;
    if (!XMLUtils::isValidQName(PromiseFlatString(key), &colon))
        return NS_ERROR_XPATH_PARSE_FAILURE;
    nsCOMPtr<nsIAtom> prefix, localName;
    PRInt32 namespaceID;
    nsresult rv = resolveQName(key, getter_AddRefs(prefix), aContext,
                               getter_AddRefs(localName), namespaceID);
    if (NS_FAILED(rv))
        return rv;

    aPattern  = new txKeyPattern(prefix, localName, namespaceID, value);

    return aPattern ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult txPatternParser::createStepPattern(txExprLexer& aLexer,
                                            txIParseContext* aContext,
                                            txPattern*& aPattern)
{
    nsresult rv = NS_OK;
    MBool isAttr = MB_FALSE;
    Token* tok = aLexer.peek();
    if (tok->mType == Token::AXIS_IDENTIFIER) {
        if (TX_StringEqualsAtom(tok->Value(), nsGkAtoms::attribute)) {
            isAttr = MB_TRUE;
        }
        else if (!TX_StringEqualsAtom(tok->Value(), nsGkAtoms::child)) {
            // all done already for CHILD_AXIS, for all others
            // XXX report unexpected axis error
            return NS_ERROR_XPATH_PARSE_FAILURE;
        }
        aLexer.nextToken();
    }
    else if (tok->mType == Token::AT_SIGN) {
        aLexer.nextToken();
        isAttr = MB_TRUE;
    }
    tok = aLexer.nextToken();

    txNodeTest* nodeTest;
    if (tok->mType == Token::CNAME) {
        // resolve QName
        nsCOMPtr<nsIAtom> prefix, lName;
        PRInt32 nspace;
        rv = resolveQName(tok->Value(), getter_AddRefs(prefix), aContext,
                          getter_AddRefs(lName), nspace, PR_TRUE);
        if (NS_FAILED(rv)) {
            // XXX error report namespace resolve failed
            return rv;
        }

        PRUint16 nodeType = isAttr ?
                            (PRUint16)txXPathNodeType::ATTRIBUTE_NODE :
                            (PRUint16)txXPathNodeType::ELEMENT_NODE;
        nodeTest = new txNameTest(prefix, lName, nspace, nodeType);
        if (!nodeTest) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    else {
        aLexer.pushBack();
        rv = createNodeTypeTest(aLexer, &nodeTest);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsAutoPtr<txStepPattern> step(new txStepPattern(nodeTest, isAttr));
    if (!step) {
        delete nodeTest;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = parsePredicates(step, aLexer, aContext);
    NS_ENSURE_SUCCESS(rv, rv);

    aPattern = step.forget();

    return NS_OK;
}
