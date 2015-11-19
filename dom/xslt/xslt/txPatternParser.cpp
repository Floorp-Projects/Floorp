/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txPatternParser.h"
#include "txExprLexer.h"
#include "nsGkAtoms.h"
#include "nsError.h"
#include "txStringUtils.h"
#include "txXSLTPatterns.h"
#include "txStylesheetCompiler.h"
#include "txPatternOptimizer.h"


nsresult txPatternParser::createPattern(const nsAFlatString& aPattern,
                                        txIParseContext* aContext,
                                        txPattern** aResult)
{
    txExprLexer lexer;
    nsresult rv = lexer.parse(aPattern);
    if (NS_FAILED(rv)) {
        // XXX error report parsing error
        return rv;
    }
    nsAutoPtr<txPattern> pattern;
    rv = createUnionPattern(lexer, aContext, *getter_Transfers(pattern));
    if (NS_FAILED(rv)) {
        // XXX error report parsing error
        return rv;
    }

    txPatternOptimizer optimizer;
    txPattern* newPattern = nullptr;
    rv = optimizer.optimize(pattern, &newPattern);
    NS_ENSURE_SUCCESS(rv, rv);

    *aResult = newPattern ? newPattern : pattern.forget();

    return NS_OK;
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

    bool isChild = true;
    bool isAbsolute = false;
    txPattern* stepPattern = 0;
    txLocPathPattern* pathPattern = 0;

    Token::Type type = aLexer.peek()->mType;
    switch (type) {
        case Token::ANCESTOR_OP:
            isChild = false;
            isAbsolute = true;
            aLexer.nextToken();
            break;
        case Token::PARENT_OP:
            aLexer.nextToken();
            isAbsolute = true;
            if (aLexer.peek()->mType == Token::END || 
                aLexer.peek()->mType == Token::UNION_OP) {
                aPattern = new txRootPattern();
                return NS_OK;
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
    if (isAbsolute) {
        txRootPattern* root = new txRootPattern();
#ifdef TX_TO_STRING
        root->setSerialize(false);
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
    return NS_OK;
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

    if (!aContext->allowed(txIParseContext::KEY_FUNCTION))
        return NS_ERROR_XSLT_CALL_TO_KEY_NOT_ALLOWED;

    const char16_t* colon;
    if (!XMLUtils::isValidQName(PromiseFlatString(key), &colon))
        return NS_ERROR_XPATH_PARSE_FAILURE;
    nsCOMPtr<nsIAtom> prefix, localName;
    int32_t namespaceID;
    nsresult rv = resolveQName(key, getter_AddRefs(prefix), aContext,
                               getter_AddRefs(localName), namespaceID);
    if (NS_FAILED(rv))
        return rv;

    aPattern  = new txKeyPattern(prefix, localName, namespaceID, value);
    return NS_OK;
}

nsresult txPatternParser::createStepPattern(txExprLexer& aLexer,
                                            txIParseContext* aContext,
                                            txPattern*& aPattern)
{
    nsresult rv = NS_OK;
    bool isAttr = false;
    Token* tok = aLexer.peek();
    if (tok->mType == Token::AXIS_IDENTIFIER) {
        if (TX_StringEqualsAtom(tok->Value(), nsGkAtoms::attribute)) {
            isAttr = true;
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
        isAttr = true;
    }

    txNodeTest* nodeTest;
    if (aLexer.peek()->mType == Token::CNAME) {
        tok = aLexer.nextToken();

        // resolve QName
        nsCOMPtr<nsIAtom> prefix, lName;
        int32_t nspace;
        rv = resolveQName(tok->Value(), getter_AddRefs(prefix), aContext,
                          getter_AddRefs(lName), nspace, true);
        if (NS_FAILED(rv)) {
            // XXX error report namespace resolve failed
            return rv;
        }

        uint16_t nodeType = isAttr ?
                            (uint16_t)txXPathNodeType::ATTRIBUTE_NODE :
                            (uint16_t)txXPathNodeType::ELEMENT_NODE;
        nodeTest = new txNameTest(prefix, lName, nspace, nodeType);
    }
    else {
        rv = createNodeTypeTest(aLexer, &nodeTest);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsAutoPtr<txStepPattern> step(new txStepPattern(nodeTest, isAttr));
    rv = parsePredicates(step, aLexer, aContext);
    NS_ENSURE_SUCCESS(rv, rv);

    aPattern = step.forget();

    return NS_OK;
}
