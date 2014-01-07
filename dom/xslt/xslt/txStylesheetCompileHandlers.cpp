/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/FloatingPoint.h"

#include "txStylesheetCompiler.h"
#include "txStylesheetCompileHandlers.h"
#include "nsWhitespaceTokenizer.h"
#include "txInstructions.h"
#include "nsGkAtoms.h"
#include "txCore.h"
#include "txStringUtils.h"
#include "txStylesheet.h"
#include "txToplevelItems.h"
#include "txPatternParser.h"
#include "txNamespaceMap.h"
#include "txURIUtils.h"
#include "txXSLTFunctions.h"

using namespace mozilla;

txHandlerTable* gTxIgnoreHandler = 0;
txHandlerTable* gTxRootHandler = 0;
txHandlerTable* gTxEmbedHandler = 0;
txHandlerTable* gTxTopHandler = 0;
txHandlerTable* gTxTemplateHandler = 0;
txHandlerTable* gTxTextHandler = 0;
txHandlerTable* gTxApplyTemplatesHandler = 0;
txHandlerTable* gTxCallTemplateHandler = 0;
txHandlerTable* gTxVariableHandler = 0;
txHandlerTable* gTxForEachHandler = 0;
txHandlerTable* gTxTopVariableHandler = 0;
txHandlerTable* gTxChooseHandler = 0;
txHandlerTable* gTxParamHandler = 0;
txHandlerTable* gTxImportHandler = 0;
txHandlerTable* gTxAttributeSetHandler = 0;
txHandlerTable* gTxFallbackHandler = 0;

static nsresult
txFnStartLRE(int32_t aNamespaceID,
             nsIAtom* aLocalName,
             nsIAtom* aPrefix,
             txStylesheetAttr* aAttributes,
             int32_t aAttrCount,
             txStylesheetCompilerState& aState);
static nsresult
txFnEndLRE(txStylesheetCompilerState& aState);


#define TX_RETURN_IF_WHITESPACE(_str, _state)                               \
    do {                                                                    \
      if (!_state.mElementContext->mPreserveWhitespace &&                   \
          XMLUtils::isWhitespace(PromiseFlatString(_str))) {                \
          return NS_OK;                                                     \
      }                                                                     \
    } while(0)


static nsresult
getStyleAttr(txStylesheetAttr* aAttributes,
             int32_t aAttrCount,
             int32_t aNamespace,
             nsIAtom* aName,
             bool aRequired,
             txStylesheetAttr** aAttr)
{
    int32_t i;
    for (i = 0; i < aAttrCount; ++i) {
        txStylesheetAttr* attr = aAttributes + i;
        if (attr->mNamespaceID == aNamespace &&
            attr->mLocalName == aName) {
            attr->mLocalName = nullptr;
            *aAttr = attr;

            return NS_OK;
        }
    }
    *aAttr = nullptr;
    
    if (aRequired) {
        // XXX ErrorReport: missing required attribute
        return NS_ERROR_XSLT_PARSE_FAILURE;
    }
    
    return NS_OK;
}

static nsresult
parseUseAttrSets(txStylesheetAttr* aAttributes,
                 int32_t aAttrCount,
                 bool aInXSLTNS,
                 txStylesheetCompilerState& aState)
{
    txStylesheetAttr* attr = nullptr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount,
                               aInXSLTNS ? kNameSpaceID_XSLT
                                         : kNameSpaceID_None,
                               nsGkAtoms::useAttributeSets, false,
                               &attr);
    if (!attr) {
        return rv;
    }

    nsWhitespaceTokenizer tok(attr->mValue);
    while (tok.hasMoreTokens()) {
        txExpandedName name;
        rv = name.init(tok.nextToken(), aState.mElementContext->mMappings,
                       false);
        NS_ENSURE_SUCCESS(rv, rv);

        nsAutoPtr<txInstruction> instr(new txInsertAttrSet(name));
        NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

        rv = aState.addInstruction(instr);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
}

static nsresult
parseExcludeResultPrefixes(txStylesheetAttr* aAttributes,
                           int32_t aAttrCount,
                           int32_t aNamespaceID)
{
    txStylesheetAttr* attr = nullptr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, aNamespaceID,
                               nsGkAtoms::excludeResultPrefixes, false,
                               &attr);
    if (!attr) {
        return rv;
    }

    // XXX Needs to be implemented.

    return NS_OK;
}

static nsresult
getQNameAttr(txStylesheetAttr* aAttributes,
             int32_t aAttrCount,
             nsIAtom* aName,
             bool aRequired,
             txStylesheetCompilerState& aState,
             txExpandedName& aExpName)
{
    aExpName.reset();
    txStylesheetAttr* attr = nullptr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                               aName, aRequired, &attr);
    if (!attr) {
        return rv;
    }

    rv = aExpName.init(attr->mValue, aState.mElementContext->mMappings,
                       false);
    if (!aRequired && NS_FAILED(rv) && aState.fcp()) {
        aExpName.reset();
        rv = NS_OK;
    }

    return rv;
}

static nsresult
getExprAttr(txStylesheetAttr* aAttributes,
            int32_t aAttrCount,
            nsIAtom* aName,
            bool aRequired,
            txStylesheetCompilerState& aState,
            nsAutoPtr<Expr>& aExpr)
{
    aExpr = nullptr;
    txStylesheetAttr* attr = nullptr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                               aName, aRequired, &attr);
    if (!attr) {
        return rv;
    }

    rv = txExprParser::createExpr(attr->mValue, &aState,
                                  getter_Transfers(aExpr));
    if (NS_FAILED(rv) && aState.fcp()) {
        // use default value in fcp for not required exprs
        if (aRequired) {
            aExpr = new txErrorExpr(
#ifdef TX_TO_STRING
                                    attr->mValue
#endif
                                    );
            NS_ENSURE_TRUE(aExpr, NS_ERROR_OUT_OF_MEMORY);
        }
        else {
            aExpr = nullptr;
        }
        return NS_OK;
    }

    return rv;
}

static nsresult
getAVTAttr(txStylesheetAttr* aAttributes,
           int32_t aAttrCount,
           nsIAtom* aName,
           bool aRequired,
           txStylesheetCompilerState& aState,
           nsAutoPtr<Expr>& aAVT)
{
    aAVT = nullptr;
    txStylesheetAttr* attr = nullptr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                               aName, aRequired, &attr);
    if (!attr) {
        return rv;
    }

    rv = txExprParser::createAVT(attr->mValue, &aState,
                                 getter_Transfers(aAVT));
    if (NS_FAILED(rv) && aState.fcp()) {
        // use default value in fcp for not required exprs
        if (aRequired) {
            aAVT = new txErrorExpr(
#ifdef TX_TO_STRING
                                   attr->mValue
#endif
                                   );
            NS_ENSURE_TRUE(aAVT, NS_ERROR_OUT_OF_MEMORY);
        }
        else {
            aAVT = nullptr;
        }
        return NS_OK;
    }

    return rv;
}

static nsresult
getPatternAttr(txStylesheetAttr* aAttributes,
               int32_t aAttrCount,
               nsIAtom* aName,
               bool aRequired,
               txStylesheetCompilerState& aState,
               nsAutoPtr<txPattern>& aPattern)
{
    aPattern = nullptr;
    txStylesheetAttr* attr = nullptr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                               aName, aRequired, &attr);
    if (!attr) {
        return rv;
    }

    aPattern = txPatternParser::createPattern(attr->mValue, &aState);
    if (!aPattern && (aRequired || !aState.fcp())) {
        // XXX ErrorReport: XSLT-Pattern parse failure
        return NS_ERROR_XPATH_PARSE_FAILURE;
    }

    return NS_OK;
}

static nsresult
getNumberAttr(txStylesheetAttr* aAttributes,
              int32_t aAttrCount,
              nsIAtom* aName,
              bool aRequired,
              txStylesheetCompilerState& aState,
              double& aNumber)
{
    aNumber = UnspecifiedNaN();
    txStylesheetAttr* attr = nullptr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                               aName, aRequired, &attr);
    if (!attr) {
        return rv;
    }

    aNumber = txDouble::toDouble(attr->mValue);
    if (mozilla::IsNaN(aNumber) && (aRequired || !aState.fcp())) {
        // XXX ErrorReport: number parse failure
        return NS_ERROR_XSLT_PARSE_FAILURE;
    }

    return NS_OK;
}

static nsresult
getAtomAttr(txStylesheetAttr* aAttributes,
            int32_t aAttrCount,
            nsIAtom* aName,
            bool aRequired,
            txStylesheetCompilerState& aState,
            nsIAtom** aAtom)
{
    *aAtom = nullptr;
    txStylesheetAttr* attr = nullptr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                               aName, aRequired, &attr);
    if (!attr) {
        return rv;
    }

    *aAtom = NS_NewAtom(attr->mValue).get();
    NS_ENSURE_TRUE(*aAtom, NS_ERROR_OUT_OF_MEMORY);

    return NS_OK;
}

static nsresult
getYesNoAttr(txStylesheetAttr* aAttributes,
             int32_t aAttrCount,
             nsIAtom* aName,
             bool aRequired,
             txStylesheetCompilerState& aState,
             txThreeState& aRes)
{
    aRes = eNotSet;
    nsCOMPtr<nsIAtom> atom;
    nsresult rv = getAtomAttr(aAttributes, aAttrCount, aName, aRequired,
                              aState, getter_AddRefs(atom));
    if (!atom) {
        return rv;
    }

    if (atom == nsGkAtoms::yes) {
        aRes = eTrue;
    }
    else if (atom == nsGkAtoms::no) {
        aRes = eFalse;
    }
    else if (aRequired || !aState.fcp()) {
        // XXX ErrorReport: unknown values
        return NS_ERROR_XSLT_PARSE_FAILURE;
    }

    return NS_OK;
}

static nsresult
getCharAttr(txStylesheetAttr* aAttributes,
            int32_t aAttrCount,
            nsIAtom* aName,
            bool aRequired,
            txStylesheetCompilerState& aState,
            char16_t& aChar)
{
    // Don't reset aChar since it contains the default value
    txStylesheetAttr* attr = nullptr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                               aName, aRequired, &attr);
    if (!attr) {
        return rv;
    }

    if (attr->mValue.Length() == 1) {
        aChar = attr->mValue.CharAt(0);
    }
    else if (aRequired || !aState.fcp()) {
        // XXX ErrorReport: not a character
        return NS_ERROR_XSLT_PARSE_FAILURE;
    }

    return NS_OK;
}


/**
 * Ignore and error handlers
 */
static nsresult
txFnTextIgnore(const nsAString& aStr, txStylesheetCompilerState& aState)
{
    return NS_OK;
}

static nsresult
txFnTextError(const nsAString& aStr, txStylesheetCompilerState& aState)
{
    TX_RETURN_IF_WHITESPACE(aStr, aState);

    return NS_ERROR_XSLT_PARSE_FAILURE;
}

void
clearAttributes(txStylesheetAttr* aAttributes,
                     int32_t aAttrCount)
{
    int32_t i;
    for (i = 0; i < aAttrCount; ++i) {
        aAttributes[i].mLocalName = nullptr;
    }
}

static nsresult
txFnStartElementIgnore(int32_t aNamespaceID,
                       nsIAtom* aLocalName,
                       nsIAtom* aPrefix,
                       txStylesheetAttr* aAttributes,
                       int32_t aAttrCount,
                       txStylesheetCompilerState& aState)
{
    if (!aState.fcp()) {
        clearAttributes(aAttributes, aAttrCount);
    }

    return NS_OK;
}

static nsresult
txFnEndElementIgnore(txStylesheetCompilerState& aState)
{
    return NS_OK;
}

static nsresult
txFnStartElementSetIgnore(int32_t aNamespaceID,
                          nsIAtom* aLocalName,
                          nsIAtom* aPrefix,
                          txStylesheetAttr* aAttributes,
                          int32_t aAttrCount,
                          txStylesheetCompilerState& aState)
{
    if (!aState.fcp()) {
        clearAttributes(aAttributes, aAttrCount);
    }

    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndElementSetIgnore(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();
    return NS_OK;
}

static nsresult
txFnStartElementError(int32_t aNamespaceID,
                      nsIAtom* aLocalName,
                      nsIAtom* aPrefix,
                      txStylesheetAttr* aAttributes,
                      int32_t aAttrCount,
                      txStylesheetCompilerState& aState)
{
    return NS_ERROR_XSLT_PARSE_FAILURE;
}

static nsresult
txFnEndElementError(txStylesheetCompilerState& aState)
{
    NS_ERROR("txFnEndElementError shouldn't be called"); 
    return NS_ERROR_XSLT_PARSE_FAILURE;
}


/**
 * Root handlers
 */
static nsresult
txFnStartStylesheet(int32_t aNamespaceID,
                    nsIAtom* aLocalName,
                    nsIAtom* aPrefix,
                    txStylesheetAttr* aAttributes,
                    int32_t aAttrCount,
                    txStylesheetCompilerState& aState)
{
    // extension-element-prefixes is handled in
    // txStylesheetCompiler::startElementInternal

    txStylesheetAttr* attr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                               nsGkAtoms::id, false, &attr);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = parseExcludeResultPrefixes(aAttributes, aAttrCount, kNameSpaceID_None);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                      nsGkAtoms::version, true, &attr);
    NS_ENSURE_SUCCESS(rv, rv);

    return aState.pushHandlerTable(gTxImportHandler);
}

static nsresult
txFnEndStylesheet(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();
    return NS_OK;
}

static nsresult
txFnStartElementContinueTopLevel(int32_t aNamespaceID,
                                nsIAtom* aLocalName,
                                nsIAtom* aPrefix,
                                txStylesheetAttr* aAttributes,
                                int32_t aAttrCount,
                                txStylesheetCompilerState& aState)
{
    aState.mHandlerTable = gTxTopHandler;

    return NS_XSLT_GET_NEW_HANDLER;
}

static nsresult
txFnStartLREStylesheet(int32_t aNamespaceID,
                       nsIAtom* aLocalName,
                       nsIAtom* aPrefix,
                       txStylesheetAttr* aAttributes,
                       int32_t aAttrCount,
                       txStylesheetCompilerState& aState)
{
    txStylesheetAttr* attr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_XSLT,
                               nsGkAtoms::version, true, &attr);
    NS_ENSURE_SUCCESS(rv, rv);

    txExpandedName nullExpr;
    double prio = UnspecifiedNaN();

    nsAutoPtr<txPattern> match(new txRootPattern());
    NS_ENSURE_TRUE(match, NS_ERROR_OUT_OF_MEMORY);

    nsAutoPtr<txTemplateItem> templ(new txTemplateItem(match, nullExpr,
                                                       nullExpr, prio));
    NS_ENSURE_TRUE(templ, NS_ERROR_OUT_OF_MEMORY);

    aState.openInstructionContainer(templ);
    rv = aState.addToplevelItem(templ);
    NS_ENSURE_SUCCESS(rv, rv);

    templ.forget();

    rv = aState.pushHandlerTable(gTxTemplateHandler);
    NS_ENSURE_SUCCESS(rv, rv);
    
    return txFnStartLRE(aNamespaceID, aLocalName, aPrefix, aAttributes,
                        aAttrCount, aState);
}

static nsresult
txFnEndLREStylesheet(txStylesheetCompilerState& aState)
{
    nsresult rv = txFnEndLRE(aState);
    NS_ENSURE_SUCCESS(rv, rv);

    aState.popHandlerTable();

    nsAutoPtr<txInstruction> instr(new txReturn());
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    aState.closeInstructionContainer();

    return NS_OK;
}

static nsresult
txFnStartEmbed(int32_t aNamespaceID,
               nsIAtom* aLocalName,
               nsIAtom* aPrefix,
               txStylesheetAttr* aAttributes,
               int32_t aAttrCount,
               txStylesheetCompilerState& aState)
{
    if (!aState.handleEmbeddedSheet()) {
        return NS_OK;
    }
    if (aNamespaceID != kNameSpaceID_XSLT ||
        (aLocalName != nsGkAtoms::stylesheet &&
         aLocalName != nsGkAtoms::transform)) {
        return NS_ERROR_XSLT_PARSE_FAILURE;
    }
    return txFnStartStylesheet(aNamespaceID, aLocalName, aPrefix,
                               aAttributes, aAttrCount, aState);
}

static nsresult
txFnEndEmbed(txStylesheetCompilerState& aState)
{
    if (!aState.handleEmbeddedSheet()) {
        return NS_OK;
    }
    nsresult rv = txFnEndStylesheet(aState);
    aState.doneEmbedding();
    return rv;
}


/**
 * Top handlers
 */
static nsresult
txFnStartOtherTop(int32_t aNamespaceID,
                  nsIAtom* aLocalName,
                  nsIAtom* aPrefix,
                  txStylesheetAttr* aAttributes,
                  int32_t aAttrCount,
                  txStylesheetCompilerState& aState)
{
    if (aNamespaceID == kNameSpaceID_None ||
        (aNamespaceID == kNameSpaceID_XSLT && !aState.fcp())) {
        return NS_ERROR_XSLT_PARSE_FAILURE;
    }

    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndOtherTop(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();
    return NS_OK;
}


// xsl:attribute-set
static nsresult
txFnStartAttributeSet(int32_t aNamespaceID,
                      nsIAtom* aLocalName,
                      nsIAtom* aPrefix,
                      txStylesheetAttr* aAttributes,
                      int32_t aAttrCount,
                      txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;
    txExpandedName name;
    rv = getQNameAttr(aAttributes, aAttrCount, nsGkAtoms::name, true,
                      aState, name);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txAttributeSetItem> attrSet(new txAttributeSetItem(name));
    NS_ENSURE_TRUE(attrSet, NS_ERROR_OUT_OF_MEMORY);

    aState.openInstructionContainer(attrSet);

    rv = aState.addToplevelItem(attrSet);
    NS_ENSURE_SUCCESS(rv, rv);

    attrSet.forget();

    rv = parseUseAttrSets(aAttributes, aAttrCount, false, aState);
    NS_ENSURE_SUCCESS(rv, rv);

    return aState.pushHandlerTable(gTxAttributeSetHandler);
}

static nsresult
txFnEndAttributeSet(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    nsAutoPtr<txInstruction> instr(new txReturn());
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    aState.closeInstructionContainer();

    return NS_OK;
}


// xsl:decimal-format
static nsresult
txFnStartDecimalFormat(int32_t aNamespaceID,
                       nsIAtom* aLocalName,
                       nsIAtom* aPrefix,
                       txStylesheetAttr* aAttributes,
                       int32_t aAttrCount,
                       txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;
    txExpandedName name;
    rv = getQNameAttr(aAttributes, aAttrCount, nsGkAtoms::name, false,
                      aState, name);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txDecimalFormat> format(new txDecimalFormat);
    NS_ENSURE_TRUE(format, NS_ERROR_OUT_OF_MEMORY);

    rv = getCharAttr(aAttributes, aAttrCount, nsGkAtoms::decimalSeparator,
                     false, aState, format->mDecimalSeparator);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = getCharAttr(aAttributes, aAttrCount, nsGkAtoms::groupingSeparator,
                     false, aState, format->mGroupingSeparator);
    NS_ENSURE_SUCCESS(rv, rv);

    txStylesheetAttr* attr = nullptr;
    rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                      nsGkAtoms::infinity, false, &attr);
    NS_ENSURE_SUCCESS(rv, rv);

    if (attr) {
        format->mInfinity = attr->mValue;
    }

    rv = getCharAttr(aAttributes, aAttrCount, nsGkAtoms::minusSign,
                     false, aState, format->mMinusSign);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                      nsGkAtoms::NaN, false, &attr);
    NS_ENSURE_SUCCESS(rv, rv);

    if (attr) {
        format->mNaN = attr->mValue;
    }

    rv = getCharAttr(aAttributes, aAttrCount, nsGkAtoms::percent,
                     false, aState, format->mPercent);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = getCharAttr(aAttributes, aAttrCount, nsGkAtoms::perMille,
                     false, aState, format->mPerMille);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = getCharAttr(aAttributes, aAttrCount, nsGkAtoms::zeroDigit,
                     false, aState, format->mZeroDigit);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = getCharAttr(aAttributes, aAttrCount, nsGkAtoms::digit,
                     false, aState, format->mDigit);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = getCharAttr(aAttributes, aAttrCount, nsGkAtoms::patternSeparator,
                     false, aState, format->mPatternSeparator);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aState.mStylesheet->addDecimalFormat(name, format);
    NS_ENSURE_SUCCESS(rv, rv);
    
    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndDecimalFormat(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    return NS_OK;
}

// xsl:import
static nsresult
txFnStartImport(int32_t aNamespaceID,
                nsIAtom* aLocalName,
                nsIAtom* aPrefix,
                txStylesheetAttr* aAttributes,
                int32_t aAttrCount,
                txStylesheetCompilerState& aState)
{
    nsAutoPtr<txImportItem> import(new txImportItem);
    NS_ENSURE_TRUE(import, NS_ERROR_OUT_OF_MEMORY);
    
    import->mFrame = new txStylesheet::ImportFrame;
    NS_ENSURE_TRUE(import->mFrame, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addToplevelItem(import);
    NS_ENSURE_SUCCESS(rv, rv);
    
    txImportItem* importPtr = import.forget();
    
    txStylesheetAttr* attr = nullptr;
    rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                      nsGkAtoms::href, true, &attr);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString absUri;
    URIUtils::resolveHref(attr->mValue, aState.mElementContext->mBaseURI,
                          absUri);
    rv = aState.loadImportedStylesheet(absUri, importPtr->mFrame);
    NS_ENSURE_SUCCESS(rv, rv);

    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndImport(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    return NS_OK;
}

// xsl:include
static nsresult
txFnStartInclude(int32_t aNamespaceID,
                 nsIAtom* aLocalName,
                 nsIAtom* aPrefix,
                 txStylesheetAttr* aAttributes,
                 int32_t aAttrCount,
                 txStylesheetCompilerState& aState)
{
    txStylesheetAttr* attr = nullptr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                               nsGkAtoms::href, true, &attr);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString absUri;
    URIUtils::resolveHref(attr->mValue, aState.mElementContext->mBaseURI,
                          absUri);
    rv = aState.loadIncludedStylesheet(absUri);
    NS_ENSURE_SUCCESS(rv, rv);

    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndInclude(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    return NS_OK;
}

// xsl:key
static nsresult
txFnStartKey(int32_t aNamespaceID,
             nsIAtom* aLocalName,
             nsIAtom* aPrefix,
             txStylesheetAttr* aAttributes,
             int32_t aAttrCount,
             txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;
    txExpandedName name;
    rv = getQNameAttr(aAttributes, aAttrCount, nsGkAtoms::name, true,
                      aState, name);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txPattern> match;
    rv = getPatternAttr(aAttributes, aAttrCount, nsGkAtoms::match, true,
                        aState, match);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> use;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::use, true,
                     aState, use);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aState.mStylesheet->addKey(name, match, use);
    NS_ENSURE_SUCCESS(rv, rv);
    
    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndKey(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    return NS_OK;
}

// xsl:namespace-alias
static nsresult
txFnStartNamespaceAlias(int32_t aNamespaceID,
             nsIAtom* aLocalName,
             nsIAtom* aPrefix,
             txStylesheetAttr* aAttributes,
             int32_t aAttrCount,
             txStylesheetCompilerState& aState)
{
    txStylesheetAttr* attr = nullptr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                               nsGkAtoms::stylesheetPrefix, true, &attr);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                      nsGkAtoms::resultPrefix, true, &attr);
    NS_ENSURE_SUCCESS(rv, rv);

    // XXX Needs to be implemented.

    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndNamespaceAlias(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    return NS_OK;
}

// xsl:output
static nsresult
txFnStartOutput(int32_t aNamespaceID,
                nsIAtom* aLocalName,
                nsIAtom* aPrefix,
                txStylesheetAttr* aAttributes,
                int32_t aAttrCount,
                txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsAutoPtr<txOutputItem> item(new txOutputItem);
    NS_ENSURE_TRUE(item, NS_ERROR_OUT_OF_MEMORY);

    txExpandedName methodExpName;
    rv = getQNameAttr(aAttributes, aAttrCount, nsGkAtoms::method, false,
                      aState, methodExpName);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!methodExpName.isNull()) {
        if (methodExpName.mNamespaceID != kNameSpaceID_None) {
            // The spec doesn't say what to do here so we'll just ignore the
            // value. We could possibly warn.
        }
        else if (methodExpName.mLocalName == nsGkAtoms::html) {
            item->mFormat.mMethod = eHTMLOutput;
        }
        else if (methodExpName.mLocalName == nsGkAtoms::text) {
            item->mFormat.mMethod = eTextOutput;
        }
        else if (methodExpName.mLocalName == nsGkAtoms::xml) {
            item->mFormat.mMethod = eXMLOutput;
        }
        else {
            return NS_ERROR_XSLT_PARSE_FAILURE;
        }
    }

    txStylesheetAttr* attr = nullptr;
    getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                 nsGkAtoms::version, false, &attr);
    if (attr) {
        item->mFormat.mVersion = attr->mValue;
    }

    getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                 nsGkAtoms::encoding, false, &attr);
    if (attr) {
        item->mFormat.mEncoding = attr->mValue;
    }

    rv = getYesNoAttr(aAttributes, aAttrCount,
                      nsGkAtoms::omitXmlDeclaration, false, aState,
                      item->mFormat.mOmitXMLDeclaration);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = getYesNoAttr(aAttributes, aAttrCount,
                      nsGkAtoms::standalone, false, aState,
                      item->mFormat.mStandalone);
    NS_ENSURE_SUCCESS(rv, rv);

    getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                 nsGkAtoms::doctypePublic, false, &attr);
    if (attr) {
        item->mFormat.mPublicId = attr->mValue;
    }

    getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                 nsGkAtoms::doctypeSystem, false, &attr);
    if (attr) {
        item->mFormat.mSystemId = attr->mValue;
    }

    getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                 nsGkAtoms::cdataSectionElements, false, &attr);
    if (attr) {
        nsWhitespaceTokenizer tokens(attr->mValue);
        while (tokens.hasMoreTokens()) {
            nsAutoPtr<txExpandedName> qname(new txExpandedName());
            NS_ENSURE_TRUE(qname, NS_ERROR_OUT_OF_MEMORY);

            rv = qname->init(tokens.nextToken(),
                             aState.mElementContext->mMappings, false);
            NS_ENSURE_SUCCESS(rv, rv);

            rv = item->mFormat.mCDATASectionElements.add(qname);
            NS_ENSURE_SUCCESS(rv, rv);
            qname.forget();
        }
    }

    rv = getYesNoAttr(aAttributes, aAttrCount,
                      nsGkAtoms::indent, false, aState,
                      item->mFormat.mIndent);
    NS_ENSURE_SUCCESS(rv, rv);

    getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                 nsGkAtoms::mediaType, false, &attr);
    if (attr) {
        item->mFormat.mMediaType = attr->mValue;
    }

    rv = aState.addToplevelItem(item);
    NS_ENSURE_SUCCESS(rv, rv);
    
    item.forget();

    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndOutput(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    return NS_OK;
}

// xsl:strip-space/xsl:preserve-space
static nsresult
txFnStartStripSpace(int32_t aNamespaceID,
                    nsIAtom* aLocalName,
                    nsIAtom* aPrefix,
                    txStylesheetAttr* aAttributes,
                    int32_t aAttrCount,
                    txStylesheetCompilerState& aState)
{
    txStylesheetAttr* attr = nullptr;
    nsresult rv = getStyleAttr(aAttributes, aAttrCount, kNameSpaceID_None,
                               nsGkAtoms::elements, true, &attr);
    NS_ENSURE_SUCCESS(rv, rv);

    bool strip = aLocalName == nsGkAtoms::stripSpace;

    nsAutoPtr<txStripSpaceItem> stripItem(new txStripSpaceItem);
    NS_ENSURE_TRUE(stripItem, NS_ERROR_OUT_OF_MEMORY);

    nsWhitespaceTokenizer tokenizer(attr->mValue);
    while (tokenizer.hasMoreTokens()) {
        const nsASingleFragmentString& name = tokenizer.nextToken();
        int32_t ns = kNameSpaceID_None;
        nsCOMPtr<nsIAtom> prefix, localName;
        rv = XMLUtils::splitQName(name, getter_AddRefs(prefix),
                                  getter_AddRefs(localName));
        if (NS_FAILED(rv)) {
            // check for "*" or "prefix:*"
            uint32_t length = name.Length();
            const char16_t* c;
            name.BeginReading(c);
            if (length == 2 || c[length-1] != '*') {
                // these can't work
                return NS_ERROR_XSLT_PARSE_FAILURE;
            }
            if (length > 1) {
                // Check for a valid prefix, that is, the returned prefix
                // should be empty and the real prefix is returned in
                // localName.
                if (c[length-2] != ':') {
                    return NS_ERROR_XSLT_PARSE_FAILURE;
                }
                rv = XMLUtils::splitQName(StringHead(name, length - 2), 
                                          getter_AddRefs(prefix),
                                          getter_AddRefs(localName));
                if (NS_FAILED(rv) || prefix) {
                    // bad chars or two ':'
                    return NS_ERROR_XSLT_PARSE_FAILURE;
                }
                prefix = localName;
            }
            localName = nsGkAtoms::_asterix;
        }
        if (prefix) {
            ns = aState.mElementContext->mMappings->lookupNamespace(prefix);
            NS_ENSURE_TRUE(ns != kNameSpaceID_Unknown, NS_ERROR_FAILURE);
        }
        nsAutoPtr<txStripSpaceTest> sst(new txStripSpaceTest(prefix, localName,
                                                             ns, strip));
        NS_ENSURE_TRUE(sst, NS_ERROR_OUT_OF_MEMORY);

        rv = stripItem->addStripSpaceTest(sst);
        NS_ENSURE_SUCCESS(rv, rv);
        
        sst.forget();
    }

    rv = aState.addToplevelItem(stripItem);
    NS_ENSURE_SUCCESS(rv, rv);

    stripItem.forget();

    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndStripSpace(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    return NS_OK;
}

// xsl:template
static nsresult
txFnStartTemplate(int32_t aNamespaceID,
                  nsIAtom* aLocalName,
                  nsIAtom* aPrefix,
                  txStylesheetAttr* aAttributes,
                  int32_t aAttrCount,
                  txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;
    txExpandedName name;
    rv = getQNameAttr(aAttributes, aAttrCount, nsGkAtoms::name, false,
                      aState, name);
    NS_ENSURE_SUCCESS(rv, rv);

    txExpandedName mode;
    rv = getQNameAttr(aAttributes, aAttrCount, nsGkAtoms::mode, false,
                      aState, mode);
    NS_ENSURE_SUCCESS(rv, rv);

    double prio = UnspecifiedNaN();
    rv = getNumberAttr(aAttributes, aAttrCount, nsGkAtoms::priority,
                       false, aState, prio);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txPattern> match;
    rv = getPatternAttr(aAttributes, aAttrCount, nsGkAtoms::match,
                        name.isNull(), aState, match);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txTemplateItem> templ(new txTemplateItem(match, name, mode, prio));
    NS_ENSURE_TRUE(templ, NS_ERROR_OUT_OF_MEMORY);

    aState.openInstructionContainer(templ);
    rv = aState.addToplevelItem(templ);
    NS_ENSURE_SUCCESS(rv, rv);
    
    templ.forget();

    return aState.pushHandlerTable(gTxParamHandler);
}

static nsresult
txFnEndTemplate(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    nsAutoPtr<txInstruction> instr(new txReturn());
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    aState.closeInstructionContainer();

    return NS_OK;
}

// xsl:variable, xsl:param
static nsresult
txFnStartTopVariable(int32_t aNamespaceID,
                     nsIAtom* aLocalName,
                     nsIAtom* aPrefix,
                     txStylesheetAttr* aAttributes,
                     int32_t aAttrCount,
                     txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;
    txExpandedName name;
    rv = getQNameAttr(aAttributes, aAttrCount, nsGkAtoms::name, true,
                      aState, name);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> select;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::select, false,
                     aState, select);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txVariableItem> var(
        new txVariableItem(name, select, aLocalName == nsGkAtoms::param));
    NS_ENSURE_TRUE(var, NS_ERROR_OUT_OF_MEMORY);

    aState.openInstructionContainer(var);
    rv = aState.pushPtr(var, aState.eVariableItem);
    NS_ENSURE_SUCCESS(rv, rv);

    if (var->mValue) {
        // XXX should be gTxErrorHandler?
        rv = aState.pushHandlerTable(gTxIgnoreHandler);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        rv = aState.pushHandlerTable(gTxTopVariableHandler);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = aState.addToplevelItem(var);
    NS_ENSURE_SUCCESS(rv, rv);
    
    var.forget();

    return NS_OK;
}

static nsresult
txFnEndTopVariable(txStylesheetCompilerState& aState)
{
    txHandlerTable* prev = aState.mHandlerTable;
    aState.popHandlerTable();
    txVariableItem* var =
        static_cast<txVariableItem*>(aState.popPtr(aState.eVariableItem));

    if (prev == gTxTopVariableHandler) {
        // No children were found.
        NS_ASSERTION(!var->mValue,
                     "There shouldn't be a select-expression here");
        var->mValue = new txLiteralExpr(EmptyString());
        NS_ENSURE_TRUE(var->mValue, NS_ERROR_OUT_OF_MEMORY);
    }
    else if (!var->mValue) {
        // If we don't have a select-expression there mush be children.
        nsAutoPtr<txInstruction> instr(new txReturn());
        NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

        nsresult rv = aState.addInstruction(instr);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    aState.closeInstructionContainer();

    return NS_OK;
}

static nsresult
txFnStartElementStartTopVar(int32_t aNamespaceID,
                            nsIAtom* aLocalName,
                            nsIAtom* aPrefix,
                            txStylesheetAttr* aAttributes,
                            int32_t aAttrCount,
                            txStylesheetCompilerState& aState)
{
    aState.mHandlerTable = gTxTemplateHandler;

    return NS_XSLT_GET_NEW_HANDLER;
}

static nsresult
txFnTextStartTopVar(const nsAString& aStr, txStylesheetCompilerState& aState)
{
    TX_RETURN_IF_WHITESPACE(aStr, aState);

    aState.mHandlerTable = gTxTemplateHandler;

    return NS_XSLT_GET_NEW_HANDLER;
}

/**
 * Template Handlers
 */

/*
  LRE

  txStartLREElement
  txInsertAttrSet        one for each qname in xsl:use-attribute-sets
  txLREAttribute         one for each attribute
  [children]
  txEndElement
*/
static nsresult
txFnStartLRE(int32_t aNamespaceID,
             nsIAtom* aLocalName,
             nsIAtom* aPrefix,
             txStylesheetAttr* aAttributes,
             int32_t aAttrCount,
             txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsAutoPtr<txInstruction> instr(new txStartLREElement(aNamespaceID,
                                                         aLocalName, aPrefix));
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = parseExcludeResultPrefixes(aAttributes, aAttrCount, kNameSpaceID_XSLT);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = parseUseAttrSets(aAttributes, aAttrCount, true, aState);
    NS_ENSURE_SUCCESS(rv, rv);

    txStylesheetAttr* attr = nullptr;
    int32_t i;
    for (i = 0; i < aAttrCount; ++i) {
        attr = aAttributes + i;
        
        if (attr->mNamespaceID == kNameSpaceID_XSLT) {
            if (attr->mLocalName == nsGkAtoms::version) {
                attr->mLocalName = nullptr;
            }

            continue;
        }

        nsAutoPtr<Expr> avt;
        rv = txExprParser::createAVT(attr->mValue, &aState,
                                     getter_Transfers(avt));
        NS_ENSURE_SUCCESS(rv, rv);

        instr = new txLREAttribute(attr->mNamespaceID, attr->mLocalName,
                                   attr->mPrefix, avt);
        NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);
        
        rv = aState.addInstruction(instr);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

static nsresult
txFnEndLRE(txStylesheetCompilerState& aState)
{
    nsAutoPtr<txInstruction> instr(new txEndElement);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
  "LRE text"

  txText
*/
static nsresult
txFnText(const nsAString& aStr, txStylesheetCompilerState& aState)
{
    TX_RETURN_IF_WHITESPACE(aStr, aState);

    nsAutoPtr<txInstruction> instr(new txText(aStr, false));
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
  xsl:apply-imports

  txApplyImportsStart
  txApplyImportsEnd
*/
static nsresult
txFnStartApplyImports(int32_t aNamespaceID,
                      nsIAtom* aLocalName,
                      nsIAtom* aPrefix,
                      txStylesheetAttr* aAttributes,
                      int32_t aAttrCount,
                      txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsAutoPtr<txInstruction> instr(new txApplyImportsStart);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    instr = new txApplyImportsEnd;
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndApplyImports(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    return NS_OK;
}

/*
  xsl:apply-templates

  txPushParams
  [params]
  txPushNewContext    -+   (holds <xsl:sort>s)
  txApplyTemplate <-+  |
  txLoopNodeSet    -+  |
  txPopParams        <-+
*/
static nsresult
txFnStartApplyTemplates(int32_t aNamespaceID,
                        nsIAtom* aLocalName,
                        nsIAtom* aPrefix,
                        txStylesheetAttr* aAttributes,
                        int32_t aAttrCount,
                        txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsAutoPtr<txInstruction> instr(new txPushParams);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    txExpandedName mode;
    rv = getQNameAttr(aAttributes, aAttrCount, nsGkAtoms::mode, false,
                      aState, mode);
    NS_ENSURE_SUCCESS(rv, rv);

    instr = new txApplyTemplates(mode);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.pushObject(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    instr.forget();

    nsAutoPtr<Expr> select;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::select, false,
                     aState, select);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!select) {
        nsAutoPtr<txNodeTest> nt(
            new txNodeTypeTest(txNodeTypeTest::NODE_TYPE));
        NS_ENSURE_TRUE(nt, NS_ERROR_OUT_OF_MEMORY);

        select = new LocationStep(nt, LocationStep::CHILD_AXIS);
        NS_ENSURE_TRUE(select, NS_ERROR_OUT_OF_MEMORY);

        nt.forget();
    }

    nsAutoPtr<txPushNewContext> pushcontext(new txPushNewContext(select));
    NS_ENSURE_TRUE(pushcontext, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.pushSorter(pushcontext);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aState.pushObject(pushcontext);
    NS_ENSURE_SUCCESS(rv, rv);
    
    pushcontext.forget();

    return aState.pushHandlerTable(gTxApplyTemplatesHandler);
}

static nsresult
txFnEndApplyTemplates(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    txPushNewContext* pushcontext =
        static_cast<txPushNewContext*>(aState.popObject());
    nsAutoPtr<txInstruction> instr(pushcontext); // txPushNewContext
    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    aState.popSorter();

    instr = static_cast<txInstruction*>(aState.popObject()); // txApplyTemplates
    nsAutoPtr<txLoopNodeSet> loop(new txLoopNodeSet(instr));
    NS_ENSURE_TRUE(loop, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    instr = loop.forget();
    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    instr = new txPopParams;
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);
    
    pushcontext->mBailTarget = instr;
    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
  xsl:attribute

  txPushStringHandler
  [children]
  txAttribute
*/
static nsresult
txFnStartAttribute(int32_t aNamespaceID,
                   nsIAtom* aLocalName,
                   nsIAtom* aPrefix,
                   txStylesheetAttr* aAttributes,
                   int32_t aAttrCount,
                   txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsAutoPtr<txInstruction> instr(new txPushStringHandler(true));
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> name;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::name, true,
                    aState, name);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> nspace;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::_namespace, false,
                    aState, nspace);
    NS_ENSURE_SUCCESS(rv, rv);

    instr = new txAttribute(name, nspace, aState.mElementContext->mMappings);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.pushObject(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    instr.forget();

    // We need to push the template-handler since the current might be
    // the attributeset-handler
    return aState.pushHandlerTable(gTxTemplateHandler);
}

static nsresult
txFnEndAttribute(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();
    nsAutoPtr<txInstruction> instr(static_cast<txInstruction*>
                                              (aState.popObject()));
    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
  xsl:call-template

  txPushParams
  [params]
  txCallTemplate
  txPopParams
*/
static nsresult
txFnStartCallTemplate(int32_t aNamespaceID,
                      nsIAtom* aLocalName,
                      nsIAtom* aPrefix,
                      txStylesheetAttr* aAttributes,
                      int32_t aAttrCount,
                      txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsAutoPtr<txInstruction> instr(new txPushParams);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    txExpandedName name;
    rv = getQNameAttr(aAttributes, aAttrCount, nsGkAtoms::name, true,
                      aState, name);
    NS_ENSURE_SUCCESS(rv, rv);

    instr = new txCallTemplate(name);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.pushObject(instr);
    NS_ENSURE_SUCCESS(rv, rv);
    
    instr.forget();

    return aState.pushHandlerTable(gTxCallTemplateHandler);
}

static nsresult
txFnEndCallTemplate(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    // txCallTemplate
    nsAutoPtr<txInstruction> instr(static_cast<txInstruction*>(aState.popObject()));
    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    instr = new txPopParams;
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
  xsl:choose

  txCondotionalGoto      --+        \
  [children]               |         | one for each xsl:when
  txGoTo           --+     |        /
                     |     |
  txCondotionalGoto  |   <-+  --+
  [children]         |          |
  txGoTo           --+          |
                     |          |
  [children]         |        <-+      for the xsl:otherwise, if there is one
                   <-+
*/
static nsresult
txFnStartChoose(int32_t aNamespaceID,
                 nsIAtom* aLocalName,
                 nsIAtom* aPrefix,
                 txStylesheetAttr* aAttributes,
                 int32_t aAttrCount,
                 txStylesheetCompilerState& aState)
{
    nsresult rv = aState.pushChooseGotoList();
    NS_ENSURE_SUCCESS(rv, rv);
    
    return aState.pushHandlerTable(gTxChooseHandler);
}

static nsresult
txFnEndChoose(txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;
    aState.popHandlerTable();
    txListIterator iter(aState.mChooseGotoList);
    txGoTo* gotoinstr;
    while ((gotoinstr = static_cast<txGoTo*>(iter.next()))) {
        rv = aState.addGotoTarget(&gotoinstr->mTarget);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    aState.popChooseGotoList();

    return NS_OK;
}

/*
  xsl:comment

  txPushStringHandler
  [children]
  txComment
*/
static nsresult
txFnStartComment(int32_t aNamespaceID,
                 nsIAtom* aLocalName,
                 nsIAtom* aPrefix,
                 txStylesheetAttr* aAttributes,
                 int32_t aAttrCount,
                 txStylesheetCompilerState& aState)
{
    nsAutoPtr<txInstruction> instr(new txPushStringHandler(true));
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

static nsresult
txFnEndComment(txStylesheetCompilerState& aState)
{
    nsAutoPtr<txInstruction> instr(new txComment);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
  xsl:copy

  txCopy          -+
  txInsertAttrSet  |     one for each qname in use-attribute-sets
  [children]       |
  txEndElement     |
                 <-+
*/
static nsresult
txFnStartCopy(int32_t aNamespaceID,
              nsIAtom* aLocalName,
              nsIAtom* aPrefix,
              txStylesheetAttr* aAttributes,
              int32_t aAttrCount,
              txStylesheetCompilerState& aState)
{
    nsAutoPtr<txCopy> copy(new txCopy);
    NS_ENSURE_TRUE(copy, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.pushPtr(copy, aState.eCopy);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txInstruction> instr(copy.forget());
    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = parseUseAttrSets(aAttributes, aAttrCount, false, aState);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

static nsresult
txFnEndCopy(txStylesheetCompilerState& aState)
{
    nsAutoPtr<txInstruction> instr(new txEndElement);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    txCopy* copy = static_cast<txCopy*>(aState.popPtr(aState.eCopy));
    rv = aState.addGotoTarget(&copy->mBailTarget);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
  xsl:copy-of

  txCopyOf
*/
static nsresult
txFnStartCopyOf(int32_t aNamespaceID,
                nsIAtom* aLocalName,
                nsIAtom* aPrefix,
                txStylesheetAttr* aAttributes,
                int32_t aAttrCount,
                txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsAutoPtr<Expr> select;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::select, true,
                    aState, select);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txInstruction> instr(new txCopyOf(select));
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);
    
    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndCopyOf(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();
    return NS_OK;
}

/*
  xsl:element

  txStartElement
  txInsertAttrSet        one for each qname in use-attribute-sets
  [children]
  txEndElement
*/
static nsresult
txFnStartElement(int32_t aNamespaceID,
                 nsIAtom* aLocalName,
                 nsIAtom* aPrefix,
                 txStylesheetAttr* aAttributes,
                 int32_t aAttrCount,
                 txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsAutoPtr<Expr> name;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::name, true,
                    aState, name);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> nspace;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::_namespace, false,
                    aState, nspace);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txInstruction> instr(
        new txStartElement(name, nspace, aState.mElementContext->mMappings));
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = parseUseAttrSets(aAttributes, aAttrCount, false, aState);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

static nsresult
txFnEndElement(txStylesheetCompilerState& aState)
{
    nsAutoPtr<txInstruction> instr(new txEndElement);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
    xsl:fallback

    [children]
*/
static nsresult
txFnStartFallback(int32_t aNamespaceID,
                  nsIAtom* aLocalName,
                  nsIAtom* aPrefix,
                  txStylesheetAttr* aAttributes,
                  int32_t aAttrCount,
                  txStylesheetCompilerState& aState)
{
    aState.mSearchingForFallback = false;

    return aState.pushHandlerTable(gTxTemplateHandler);
}

static nsresult
txFnEndFallback(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    NS_ASSERTION(!aState.mSearchingForFallback,
                 "bad nesting of unknown-instruction and fallback handlers");
    return NS_OK;
}

/*
  xsl:for-each

  txPushNewContext            -+    (holds <xsl:sort>s)
  txPushNullTemplateRule  <-+  |
  [children]                |  |
  txLoopNodeSet            -+  |
                             <-+
*/
static nsresult
txFnStartForEach(int32_t aNamespaceID,
                 nsIAtom* aLocalName,
                 nsIAtom* aPrefix,
                 txStylesheetAttr* aAttributes,
                 int32_t aAttrCount,
                 txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsAutoPtr<Expr> select;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::select, true,
                     aState, select);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txPushNewContext> pushcontext(new txPushNewContext(select));
    NS_ENSURE_TRUE(pushcontext, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.pushPtr(pushcontext, aState.ePushNewContext);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aState.pushSorter(pushcontext);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txInstruction> instr(pushcontext.forget());
    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);
    
    instr = new txPushNullTemplateRule;
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.pushPtr(instr, aState.ePushNullTemplateRule);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return aState.pushHandlerTable(gTxForEachHandler);
}

static nsresult
txFnEndForEach(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    // This is a txPushNullTemplateRule
    txInstruction* pnullrule =
        static_cast<txInstruction*>(aState.popPtr(aState.ePushNullTemplateRule));

    nsAutoPtr<txInstruction> instr(new txLoopNodeSet(pnullrule));
    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    aState.popSorter();
    txPushNewContext* pushcontext =
        static_cast<txPushNewContext*>(aState.popPtr(aState.ePushNewContext));
    aState.addGotoTarget(&pushcontext->mBailTarget);

    return NS_OK;
}

static nsresult
txFnStartElementContinueTemplate(int32_t aNamespaceID,
                                nsIAtom* aLocalName,
                                nsIAtom* aPrefix,
                                txStylesheetAttr* aAttributes,
                                int32_t aAttrCount,
                                txStylesheetCompilerState& aState)
{
    aState.mHandlerTable = gTxTemplateHandler;

    return NS_XSLT_GET_NEW_HANDLER;
}

static nsresult
txFnTextContinueTemplate(const nsAString& aStr,
                        txStylesheetCompilerState& aState)
{
    TX_RETURN_IF_WHITESPACE(aStr, aState);

    aState.mHandlerTable = gTxTemplateHandler;

    return NS_XSLT_GET_NEW_HANDLER;
}

/*
  xsl:if

  txConditionalGoto  -+
  [children]          |
                    <-+
*/
static nsresult
txFnStartIf(int32_t aNamespaceID,
            nsIAtom* aLocalName,
            nsIAtom* aPrefix,
            txStylesheetAttr* aAttributes,
            int32_t aAttrCount,
            txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsAutoPtr<Expr> test;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::test, true,
                     aState, test);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txConditionalGoto> condGoto(new txConditionalGoto(test, nullptr));
    NS_ENSURE_TRUE(condGoto, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.pushPtr(condGoto, aState.eConditionalGoto);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txInstruction> instr(condGoto.forget());
    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

static nsresult
txFnEndIf(txStylesheetCompilerState& aState)
{
    txConditionalGoto* condGoto =
        static_cast<txConditionalGoto*>(aState.popPtr(aState.eConditionalGoto));
    return aState.addGotoTarget(&condGoto->mTarget);
}

/*
  xsl:message

  txPushStringHandler
  [children]
  txMessage
*/
static nsresult
txFnStartMessage(int32_t aNamespaceID,
                 nsIAtom* aLocalName,
                 nsIAtom* aPrefix,
                 txStylesheetAttr* aAttributes,
                 int32_t aAttrCount,
                 txStylesheetCompilerState& aState)
{
    nsAutoPtr<txInstruction> instr(new txPushStringHandler(false));
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    txThreeState term;
    rv = getYesNoAttr(aAttributes, aAttrCount, nsGkAtoms::terminate,
                      false, aState, term);
    NS_ENSURE_SUCCESS(rv, rv);

    instr = new txMessage(term == eTrue);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.pushObject(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    instr.forget();

    return NS_OK;
}

static nsresult
txFnEndMessage(txStylesheetCompilerState& aState)
{
    nsAutoPtr<txInstruction> instr(static_cast<txInstruction*>(aState.popObject()));
    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
  xsl:number

  txNumber
*/
static nsresult
txFnStartNumber(int32_t aNamespaceID,
                nsIAtom* aLocalName,
                nsIAtom* aPrefix,
                txStylesheetAttr* aAttributes,
                int32_t aAttrCount,
                txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIAtom> levelAtom;
    rv = getAtomAttr(aAttributes, aAttrCount, nsGkAtoms::level, false,
                     aState, getter_AddRefs(levelAtom));
    NS_ENSURE_SUCCESS(rv, rv);
    
    txXSLTNumber::LevelType level = txXSLTNumber::eLevelSingle;
    if (levelAtom == nsGkAtoms::multiple) {
        level = txXSLTNumber::eLevelMultiple;
    }
    else if (levelAtom == nsGkAtoms::any) {
        level = txXSLTNumber::eLevelAny;
    }
    else if (levelAtom && levelAtom != nsGkAtoms::single && !aState.fcp()) {
        return NS_ERROR_XSLT_PARSE_FAILURE;
    }
    
    nsAutoPtr<txPattern> count;
    rv = getPatternAttr(aAttributes, aAttrCount, nsGkAtoms::count, false,
                        aState, count);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsAutoPtr<txPattern> from;
    rv = getPatternAttr(aAttributes, aAttrCount, nsGkAtoms::from, false,
                        aState, from);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> value;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::value, false,
                     aState, value);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> format;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::format, false,
                    aState, format);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsAutoPtr<Expr> lang;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::lang, false,
                      aState, lang);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsAutoPtr<Expr> letterValue;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::letterValue, false,
                    aState, letterValue);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsAutoPtr<Expr> groupingSeparator;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::groupingSeparator,
                    false, aState, groupingSeparator);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsAutoPtr<Expr> groupingSize;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::groupingSize,
                    false, aState, groupingSize);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsAutoPtr<txInstruction> instr(new txNumber(level, count, from, value,
                                                format,groupingSeparator,
                                                groupingSize));
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);
    
    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndNumber(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    return NS_OK;
}

/*
    xsl:otherwise
    
    (see xsl:choose)
*/
static nsresult
txFnStartOtherwise(int32_t aNamespaceID,
                   nsIAtom* aLocalName,
                   nsIAtom* aPrefix,
                   txStylesheetAttr* aAttributes,
                   int32_t aAttrCount,
                   txStylesheetCompilerState& aState)
{
    return aState.pushHandlerTable(gTxTemplateHandler);
}

static nsresult
txFnEndOtherwise(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();
    aState.mHandlerTable = gTxIgnoreHandler; // XXX should be gTxErrorHandler

    return NS_OK;
}

/*
    xsl:param
    
    txCheckParam    --+
    txPushRTFHandler  |  --- (for RTF-parameters)
    [children]        |  /
    txSetVariable     |
                    <-+
*/
static nsresult
txFnStartParam(int32_t aNamespaceID,
               nsIAtom* aLocalName,
               nsIAtom* aPrefix,
               txStylesheetAttr* aAttributes,
               int32_t aAttrCount,
               txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    txExpandedName name;
    rv = getQNameAttr(aAttributes, aAttrCount, nsGkAtoms::name, true,
                      aState, name);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txCheckParam> checkParam(new txCheckParam(name));
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = aState.pushPtr(checkParam, aState.eCheckParam);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txInstruction> instr(checkParam.forget());
    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> select;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::select, false,
                     aState, select);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txSetVariable> var(new txSetVariable(name, select));
    NS_ENSURE_TRUE(var, NS_ERROR_OUT_OF_MEMORY);

    if (var->mValue) {
        // XXX should be gTxErrorHandler?
        rv = aState.pushHandlerTable(gTxIgnoreHandler);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        rv = aState.pushHandlerTable(gTxVariableHandler);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = aState.pushObject(var);
    NS_ENSURE_SUCCESS(rv, rv);
    
    var.forget();

    return NS_OK;
}

static nsresult
txFnEndParam(txStylesheetCompilerState& aState)
{
    nsAutoPtr<txSetVariable> var(static_cast<txSetVariable*>
                                            (aState.popObject()));
    txHandlerTable* prev = aState.mHandlerTable;
    aState.popHandlerTable();

    if (prev == gTxVariableHandler) {
        // No children were found.
        NS_ASSERTION(!var->mValue,
                     "There shouldn't be a select-expression here");
        var->mValue = new txLiteralExpr(EmptyString());
        NS_ENSURE_TRUE(var->mValue, NS_ERROR_OUT_OF_MEMORY);
    }

    nsresult rv = aState.addVariable(var->mName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txInstruction> instr(var.forget());
    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    txCheckParam* checkParam =
        static_cast<txCheckParam*>(aState.popPtr(aState.eCheckParam));
    aState.addGotoTarget(&checkParam->mBailTarget);

    return NS_OK;
}

/*
  xsl:processing-instruction

  txPushStringHandler
  [children]
  txProcessingInstruction
*/
static nsresult
txFnStartPI(int32_t aNamespaceID,
            nsIAtom* aLocalName,
            nsIAtom* aPrefix,
            txStylesheetAttr* aAttributes,
            int32_t aAttrCount,
            txStylesheetCompilerState& aState)
{
    nsAutoPtr<txInstruction> instr(new txPushStringHandler(true));
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> name;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::name, true,
                    aState, name);
    NS_ENSURE_SUCCESS(rv, rv);

    instr = new txProcessingInstruction(name);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.pushObject(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    instr.forget();

    return NS_OK;
}

static nsresult
txFnEndPI(txStylesheetCompilerState& aState)
{
    nsAutoPtr<txInstruction> instr(static_cast<txInstruction*>
                                              (aState.popObject()));
    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
    xsl:sort
    
    (no instructions)
*/
static nsresult
txFnStartSort(int32_t aNamespaceID,
              nsIAtom* aLocalName,
              nsIAtom* aPrefix,
              txStylesheetAttr* aAttributes,
              int32_t aAttrCount,
              txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsAutoPtr<Expr> select;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::select, false,
                     aState, select);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!select) {
        nsAutoPtr<txNodeTest> nt(
              new txNodeTypeTest(txNodeTypeTest::NODE_TYPE));
        NS_ENSURE_TRUE(nt, NS_ERROR_OUT_OF_MEMORY);

        select = new LocationStep(nt, LocationStep::SELF_AXIS);
        NS_ENSURE_TRUE(select, NS_ERROR_OUT_OF_MEMORY);

        nt.forget();
    }

    nsAutoPtr<Expr> lang;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::lang, false,
                    aState, lang);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> dataType;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::dataType, false,
                    aState, dataType);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> order;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::order, false,
                    aState, order);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> caseOrder;
    rv = getAVTAttr(aAttributes, aAttrCount, nsGkAtoms::caseOrder, false,
                    aState, caseOrder);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aState.mSorter->addSort(select, lang, dataType, order, caseOrder);
    NS_ENSURE_SUCCESS(rv, rv);

    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndSort(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    return NS_OK;
}

/*
  xsl:text

  [children]     (only txText)
*/
static nsresult
txFnStartText(int32_t aNamespaceID,
              nsIAtom* aLocalName,
              nsIAtom* aPrefix,
              txStylesheetAttr* aAttributes,
              int32_t aAttrCount,
              txStylesheetCompilerState& aState)
{
    NS_ASSERTION(!aState.mDOE, "nested d-o-e elements should not happen");

    nsresult rv = NS_OK;
    txThreeState doe;
    rv = getYesNoAttr(aAttributes, aAttrCount,
                      nsGkAtoms::disableOutputEscaping, false, aState,
                      doe);
    NS_ENSURE_SUCCESS(rv, rv);

    aState.mDOE = doe == eTrue;

    return aState.pushHandlerTable(gTxTextHandler);
}

static nsresult
txFnEndText(txStylesheetCompilerState& aState)
{
    aState.mDOE = false;
    aState.popHandlerTable();
    return NS_OK;
}

static nsresult
txFnTextText(const nsAString& aStr, txStylesheetCompilerState& aState)
{
    nsAutoPtr<txInstruction> instr(new txText(aStr, aState.mDOE));
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
  xsl:value-of

  txValueOf
*/
static nsresult
txFnStartValueOf(int32_t aNamespaceID,
                 nsIAtom* aLocalName,
                 nsIAtom* aPrefix,
                 txStylesheetAttr* aAttributes,
                 int32_t aAttrCount,
                 txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    txThreeState doe;
    rv = getYesNoAttr(aAttributes, aAttrCount,
                     nsGkAtoms::disableOutputEscaping, false, aState,
                     doe);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> select;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::select, true,
                     aState, select);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txInstruction> instr(new txValueOf(select, doe == eTrue));
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return aState.pushHandlerTable(gTxIgnoreHandler);
}

static nsresult
txFnEndValueOf(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();
    return NS_OK;
}

/*
    xsl:variable
    
    txPushRTFHandler     --- (for RTF-parameters)
    [children]           /
    txSetVariable      
*/
static nsresult
txFnStartVariable(int32_t aNamespaceID,
                  nsIAtom* aLocalName,
                  nsIAtom* aPrefix,
                  txStylesheetAttr* aAttributes,
                  int32_t aAttrCount,
                  txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    txExpandedName name;
    rv = getQNameAttr(aAttributes, aAttrCount, nsGkAtoms::name, true,
                      aState, name);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> select;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::select, false,
                     aState, select);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txSetVariable> var(new txSetVariable(name, select));
    NS_ENSURE_TRUE(var, NS_ERROR_OUT_OF_MEMORY);

    if (var->mValue) {
        // XXX should be gTxErrorHandler?
        rv = aState.pushHandlerTable(gTxIgnoreHandler);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        rv = aState.pushHandlerTable(gTxVariableHandler);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = aState.pushObject(var);
    NS_ENSURE_SUCCESS(rv, rv);

    var.forget();

    return NS_OK;
}

static nsresult
txFnEndVariable(txStylesheetCompilerState& aState)
{
    nsAutoPtr<txSetVariable> var(static_cast<txSetVariable*>
                                            (aState.popObject()));

    txHandlerTable* prev = aState.mHandlerTable;
    aState.popHandlerTable();

    if (prev == gTxVariableHandler) {
        // No children were found.
        NS_ASSERTION(!var->mValue,
                     "There shouldn't be a select-expression here");
        var->mValue = new txLiteralExpr(EmptyString());
        NS_ENSURE_TRUE(var->mValue, NS_ERROR_OUT_OF_MEMORY);
    }

    nsresult rv = aState.addVariable(var->mName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txInstruction> instr(var.forget());
    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

static nsresult
txFnStartElementStartRTF(int32_t aNamespaceID,
                         nsIAtom* aLocalName,
                         nsIAtom* aPrefix,
                         txStylesheetAttr* aAttributes,
                         int32_t aAttrCount,
                         txStylesheetCompilerState& aState)
{
    nsAutoPtr<txInstruction> instr(new txPushRTFHandler);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    aState.mHandlerTable = gTxTemplateHandler;

    return NS_XSLT_GET_NEW_HANDLER;
}

static nsresult
txFnTextStartRTF(const nsAString& aStr, txStylesheetCompilerState& aState)
{
    TX_RETURN_IF_WHITESPACE(aStr, aState);

    nsAutoPtr<txInstruction> instr(new txPushRTFHandler);
    NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    aState.mHandlerTable = gTxTemplateHandler;

    return NS_XSLT_GET_NEW_HANDLER;
}

/*
    xsl:when
    
    (see xsl:choose)
*/
static nsresult
txFnStartWhen(int32_t aNamespaceID,
              nsIAtom* aLocalName,
              nsIAtom* aPrefix,
              txStylesheetAttr* aAttributes,
              int32_t aAttrCount,
              txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    nsAutoPtr<Expr> test;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::test, true,
                     aState, test);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txConditionalGoto> condGoto(new txConditionalGoto(test, nullptr));
    NS_ENSURE_TRUE(condGoto, NS_ERROR_OUT_OF_MEMORY);

    rv = aState.pushPtr(condGoto, aState.eConditionalGoto);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txInstruction> instr(condGoto.forget());
    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return aState.pushHandlerTable(gTxTemplateHandler);
}

static nsresult
txFnEndWhen(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();
    nsAutoPtr<txGoTo> gotoinstr(new txGoTo(nullptr));
    NS_ENSURE_TRUE(gotoinstr, NS_ERROR_OUT_OF_MEMORY);
    
    nsresult rv = aState.mChooseGotoList->add(gotoinstr);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txInstruction> instr(gotoinstr.forget());
    rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    txConditionalGoto* condGoto =
        static_cast<txConditionalGoto*>(aState.popPtr(aState.eConditionalGoto));
    rv = aState.addGotoTarget(&condGoto->mTarget);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
    xsl:with-param
    
    txPushRTFHandler   -- for RTF-parameters
    [children]         /
    txSetParam
*/
static nsresult
txFnStartWithParam(int32_t aNamespaceID,
                   nsIAtom* aLocalName,
                   nsIAtom* aPrefix,
                   txStylesheetAttr* aAttributes,
                   int32_t aAttrCount,
                   txStylesheetCompilerState& aState)
{
    nsresult rv = NS_OK;

    txExpandedName name;
    rv = getQNameAttr(aAttributes, aAttrCount, nsGkAtoms::name, true,
                      aState, name);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<Expr> select;
    rv = getExprAttr(aAttributes, aAttrCount, nsGkAtoms::select, false,
                     aState, select);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<txSetParam> var(new txSetParam(name, select));
    NS_ENSURE_TRUE(var, NS_ERROR_OUT_OF_MEMORY);

    if (var->mValue) {
        // XXX should be gTxErrorHandler?
        rv = aState.pushHandlerTable(gTxIgnoreHandler);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        rv = aState.pushHandlerTable(gTxVariableHandler);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = aState.pushObject(var);
    NS_ENSURE_SUCCESS(rv, rv);

    var.forget();

    return NS_OK;
}

static nsresult
txFnEndWithParam(txStylesheetCompilerState& aState)
{
    nsAutoPtr<txSetParam> var(static_cast<txSetParam*>(aState.popObject()));
    txHandlerTable* prev = aState.mHandlerTable;
    aState.popHandlerTable();

    if (prev == gTxVariableHandler) {
        // No children were found.
        NS_ASSERTION(!var->mValue,
                     "There shouldn't be a select-expression here");
        var->mValue = new txLiteralExpr(EmptyString());
        NS_ENSURE_TRUE(var->mValue, NS_ERROR_OUT_OF_MEMORY);
    }

    nsAutoPtr<txInstruction> instr(var.forget());
    nsresult rv = aState.addInstruction(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

/*
    Unknown instruction

    [fallbacks]           if one or more xsl:fallbacks are found
    or
    txErrorInstruction    otherwise
*/
static nsresult
txFnStartUnknownInstruction(int32_t aNamespaceID,
                            nsIAtom* aLocalName,
                            nsIAtom* aPrefix,
                            txStylesheetAttr* aAttributes,
                            int32_t aAttrCount,
                            txStylesheetCompilerState& aState)
{
    NS_ASSERTION(!aState.mSearchingForFallback,
                 "bad nesting of unknown-instruction and fallback handlers");

    if (aNamespaceID == kNameSpaceID_XSLT && !aState.fcp()) {
        return NS_ERROR_XSLT_PARSE_FAILURE;
    }

    aState.mSearchingForFallback = true;

    return aState.pushHandlerTable(gTxFallbackHandler);
}

static nsresult
txFnEndUnknownInstruction(txStylesheetCompilerState& aState)
{
    aState.popHandlerTable();

    if (aState.mSearchingForFallback) {
        nsAutoPtr<txInstruction> instr(new txErrorInstruction());
        NS_ENSURE_TRUE(instr, NS_ERROR_OUT_OF_MEMORY);

        nsresult rv = aState.addInstruction(instr);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    aState.mSearchingForFallback = false;

    return NS_OK;
}

/**
 * Table Datas
 */

struct txHandlerTableData {
    txElementHandler mOtherHandler;
    txElementHandler mLREHandler;
    HandleTextFn mTextHandler;
};

const txHandlerTableData gTxIgnoreTableData = {
  // Other
  { 0, 0, txFnStartElementIgnore, txFnEndElementIgnore },
  // LRE
  { 0, 0, txFnStartElementIgnore, txFnEndElementIgnore },
  // Text
  txFnTextIgnore
};

const txElementHandler gTxRootElementHandlers[] = {
  { kNameSpaceID_XSLT, "stylesheet", txFnStartStylesheet, txFnEndStylesheet },
  { kNameSpaceID_XSLT, "transform", txFnStartStylesheet, txFnEndStylesheet }
};

const txHandlerTableData gTxRootTableData = {
  // Other
  { 0, 0, txFnStartElementError, txFnEndElementError },
  // LRE
  { 0, 0, txFnStartLREStylesheet, txFnEndLREStylesheet },
  // Text
  txFnTextError
};

const txHandlerTableData gTxEmbedTableData = {
  // Other
  { 0, 0, txFnStartEmbed, txFnEndEmbed },
  // LRE
  { 0, 0, txFnStartEmbed, txFnEndEmbed },
  // Text
  txFnTextIgnore
};

const txElementHandler gTxTopElementHandlers[] = {
  { kNameSpaceID_XSLT, "attribute-set", txFnStartAttributeSet, txFnEndAttributeSet },
  { kNameSpaceID_XSLT, "decimal-format", txFnStartDecimalFormat, txFnEndDecimalFormat },
  { kNameSpaceID_XSLT, "include", txFnStartInclude, txFnEndInclude },
  { kNameSpaceID_XSLT, "key", txFnStartKey, txFnEndKey },
  { kNameSpaceID_XSLT, "namespace-alias", txFnStartNamespaceAlias,
    txFnEndNamespaceAlias },
  { kNameSpaceID_XSLT, "output", txFnStartOutput, txFnEndOutput },
  { kNameSpaceID_XSLT, "param", txFnStartTopVariable, txFnEndTopVariable },
  { kNameSpaceID_XSLT, "preserve-space", txFnStartStripSpace, txFnEndStripSpace },
  { kNameSpaceID_XSLT, "strip-space", txFnStartStripSpace, txFnEndStripSpace },
  { kNameSpaceID_XSLT, "template", txFnStartTemplate, txFnEndTemplate },
  { kNameSpaceID_XSLT, "variable", txFnStartTopVariable, txFnEndTopVariable }
};

const txHandlerTableData gTxTopTableData = {
  // Other
  { 0, 0, txFnStartOtherTop, txFnEndOtherTop },
  // LRE
  { 0, 0, txFnStartOtherTop, txFnEndOtherTop },
  // Text
  txFnTextIgnore
};

const txElementHandler gTxTemplateElementHandlers[] = {
  { kNameSpaceID_XSLT, "apply-imports", txFnStartApplyImports, txFnEndApplyImports },
  { kNameSpaceID_XSLT, "apply-templates", txFnStartApplyTemplates, txFnEndApplyTemplates },
  { kNameSpaceID_XSLT, "attribute", txFnStartAttribute, txFnEndAttribute },
  { kNameSpaceID_XSLT, "call-template", txFnStartCallTemplate, txFnEndCallTemplate },
  { kNameSpaceID_XSLT, "choose", txFnStartChoose, txFnEndChoose },
  { kNameSpaceID_XSLT, "comment", txFnStartComment, txFnEndComment },
  { kNameSpaceID_XSLT, "copy", txFnStartCopy, txFnEndCopy },
  { kNameSpaceID_XSLT, "copy-of", txFnStartCopyOf, txFnEndCopyOf },
  { kNameSpaceID_XSLT, "element", txFnStartElement, txFnEndElement },
  { kNameSpaceID_XSLT, "fallback", txFnStartElementSetIgnore, txFnEndElementSetIgnore },
  { kNameSpaceID_XSLT, "for-each", txFnStartForEach, txFnEndForEach },
  { kNameSpaceID_XSLT, "if", txFnStartIf, txFnEndIf },
  { kNameSpaceID_XSLT, "message", txFnStartMessage, txFnEndMessage },
  { kNameSpaceID_XSLT, "number", txFnStartNumber, txFnEndNumber },
  { kNameSpaceID_XSLT, "processing-instruction", txFnStartPI, txFnEndPI },
  { kNameSpaceID_XSLT, "text", txFnStartText, txFnEndText },
  { kNameSpaceID_XSLT, "value-of", txFnStartValueOf, txFnEndValueOf },
  { kNameSpaceID_XSLT, "variable", txFnStartVariable, txFnEndVariable }
};

const txHandlerTableData gTxTemplateTableData = {
  // Other
  { 0, 0, txFnStartUnknownInstruction, txFnEndUnknownInstruction },
  // LRE
  { 0, 0, txFnStartLRE, txFnEndLRE },
  // Text
  txFnText
};

const txHandlerTableData gTxTextTableData = {
  // Other
  { 0, 0, txFnStartElementError, txFnEndElementError },
  // LRE
  { 0, 0, txFnStartElementError, txFnEndElementError },
  // Text
  txFnTextText
};

const txElementHandler gTxApplyTemplatesElementHandlers[] = {
  { kNameSpaceID_XSLT, "sort", txFnStartSort, txFnEndSort },
  { kNameSpaceID_XSLT, "with-param", txFnStartWithParam, txFnEndWithParam }
};

const txHandlerTableData gTxApplyTemplatesTableData = {
  // Other
  { 0, 0, txFnStartElementSetIgnore, txFnEndElementSetIgnore }, // should this be error?
  // LRE
  { 0, 0, txFnStartElementSetIgnore, txFnEndElementSetIgnore },
  // Text
  txFnTextIgnore
};

const txElementHandler gTxCallTemplateElementHandlers[] = {
  { kNameSpaceID_XSLT, "with-param", txFnStartWithParam, txFnEndWithParam }
};

const txHandlerTableData gTxCallTemplateTableData = {
  // Other
  { 0, 0, txFnStartElementSetIgnore, txFnEndElementSetIgnore }, // should this be error?
  // LRE
  { 0, 0, txFnStartElementSetIgnore, txFnEndElementSetIgnore },
  // Text
  txFnTextIgnore
};

const txHandlerTableData gTxVariableTableData = {
  // Other
  { 0, 0, txFnStartElementStartRTF, 0 },
  // LRE
  { 0, 0, txFnStartElementStartRTF, 0 },
  // Text
  txFnTextStartRTF
};

const txElementHandler gTxForEachElementHandlers[] = {
  { kNameSpaceID_XSLT, "sort", txFnStartSort, txFnEndSort }
};

const txHandlerTableData gTxForEachTableData = {
  // Other
  { 0, 0, txFnStartElementContinueTemplate, 0 },
  // LRE
  { 0, 0, txFnStartElementContinueTemplate, 0 },
  // Text
  txFnTextContinueTemplate
};

const txHandlerTableData gTxTopVariableTableData = {
  // Other
  { 0, 0, txFnStartElementStartTopVar, 0 },
  // LRE
  { 0, 0, txFnStartElementStartTopVar, 0 },
  // Text
  txFnTextStartTopVar
};

const txElementHandler gTxChooseElementHandlers[] = {
  { kNameSpaceID_XSLT, "otherwise", txFnStartOtherwise, txFnEndOtherwise },
  { kNameSpaceID_XSLT, "when", txFnStartWhen, txFnEndWhen }
};

const txHandlerTableData gTxChooseTableData = {
  // Other
  { 0, 0, txFnStartElementError, 0 },
  // LRE
  { 0, 0, txFnStartElementError, 0 },
  // Text
  txFnTextError
};

const txElementHandler gTxParamElementHandlers[] = {
  { kNameSpaceID_XSLT, "param", txFnStartParam, txFnEndParam }
};

const txHandlerTableData gTxParamTableData = {
  // Other
  { 0, 0, txFnStartElementContinueTemplate, 0 },
  // LRE
  { 0, 0, txFnStartElementContinueTemplate, 0 },
  // Text
  txFnTextContinueTemplate
};

const txElementHandler gTxImportElementHandlers[] = {
  { kNameSpaceID_XSLT, "import", txFnStartImport, txFnEndImport }
};

const txHandlerTableData gTxImportTableData = {
  // Other
  { 0, 0, txFnStartElementContinueTopLevel, 0 },
  // LRE
  { 0, 0, txFnStartOtherTop, txFnEndOtherTop }, // XXX what should we do here?
  // Text
  txFnTextIgnore  // XXX what should we do here?
};

const txElementHandler gTxAttributeSetElementHandlers[] = {
  { kNameSpaceID_XSLT, "attribute", txFnStartAttribute, txFnEndAttribute }
};

const txHandlerTableData gTxAttributeSetTableData = {
  // Other
  { 0, 0, txFnStartElementError, 0 },
  // LRE
  { 0, 0, txFnStartElementError, 0 },
  // Text
  txFnTextError
};

const txElementHandler gTxFallbackElementHandlers[] = {
  { kNameSpaceID_XSLT, "fallback", txFnStartFallback, txFnEndFallback }
};

const txHandlerTableData gTxFallbackTableData = {
  // Other
  { 0, 0, txFnStartElementSetIgnore, txFnEndElementSetIgnore },
  // LRE
  { 0, 0, txFnStartElementSetIgnore, txFnEndElementSetIgnore },
  // Text
  txFnTextIgnore
};



/**
 * txHandlerTable
 */
txHandlerTable::txHandlerTable(const HandleTextFn aTextHandler,
                               const txElementHandler* aLREHandler,
                               const txElementHandler* aOtherHandler)
  : mTextHandler(aTextHandler),
    mLREHandler(aLREHandler),
    mOtherHandler(aOtherHandler)
{
}

nsresult
txHandlerTable::init(const txElementHandler* aHandlers, uint32_t aCount)
{
    nsresult rv = NS_OK;

    uint32_t i;
    for (i = 0; i < aCount; ++i) {
        nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aHandlers->mLocalName);
        txExpandedName name(aHandlers->mNamespaceID, nameAtom);
        rv = mHandlers.add(name, aHandlers);
        NS_ENSURE_SUCCESS(rv, rv);

        ++aHandlers;
    }
    return NS_OK;
}

const txElementHandler*
txHandlerTable::find(int32_t aNamespaceID, nsIAtom* aLocalName)
{
    txExpandedName name(aNamespaceID, aLocalName);
    const txElementHandler* handler = mHandlers.get(name);
    if (!handler) {
        handler = mOtherHandler;
    }
    return handler;
}

#define INIT_HANDLER(_name)                                          \
    gTx##_name##Handler =                                            \
        new txHandlerTable(gTx##_name##TableData.mTextHandler,       \
                           &gTx##_name##TableData.mLREHandler,       \
                           &gTx##_name##TableData.mOtherHandler);    \
    if (!gTx##_name##Handler)                                        \
        return false

#define INIT_HANDLER_WITH_ELEMENT_HANDLERS(_name)                    \
    INIT_HANDLER(_name);                                             \
                                                                     \
    rv = gTx##_name##Handler->init(gTx##_name##ElementHandlers,      \
                                   ArrayLength(gTx##_name##ElementHandlers)); \
    if (NS_FAILED(rv))                                               \
        return false

#define SHUTDOWN_HANDLER(_name)                                      \
    delete gTx##_name##Handler;                                      \
    gTx##_name##Handler = nullptr

// static
bool
txHandlerTable::init()
{
    nsresult rv = NS_OK;

    INIT_HANDLER_WITH_ELEMENT_HANDLERS(Root);
    INIT_HANDLER(Embed);
    INIT_HANDLER_WITH_ELEMENT_HANDLERS(Top);
    INIT_HANDLER(Ignore);
    INIT_HANDLER_WITH_ELEMENT_HANDLERS(Template);
    INIT_HANDLER(Text);
    INIT_HANDLER_WITH_ELEMENT_HANDLERS(ApplyTemplates);
    INIT_HANDLER_WITH_ELEMENT_HANDLERS(CallTemplate);
    INIT_HANDLER(Variable);
    INIT_HANDLER_WITH_ELEMENT_HANDLERS(ForEach);
    INIT_HANDLER(TopVariable);
    INIT_HANDLER_WITH_ELEMENT_HANDLERS(Choose);
    INIT_HANDLER_WITH_ELEMENT_HANDLERS(Param);
    INIT_HANDLER_WITH_ELEMENT_HANDLERS(Import);
    INIT_HANDLER_WITH_ELEMENT_HANDLERS(AttributeSet);
    INIT_HANDLER_WITH_ELEMENT_HANDLERS(Fallback);

    return true;
}

// static
void
txHandlerTable::shutdown()
{
    SHUTDOWN_HANDLER(Root);
    SHUTDOWN_HANDLER(Embed);
    SHUTDOWN_HANDLER(Top);
    SHUTDOWN_HANDLER(Ignore);
    SHUTDOWN_HANDLER(Template);
    SHUTDOWN_HANDLER(Text);
    SHUTDOWN_HANDLER(ApplyTemplates);
    SHUTDOWN_HANDLER(CallTemplate);
    SHUTDOWN_HANDLER(Variable);
    SHUTDOWN_HANDLER(ForEach);
    SHUTDOWN_HANDLER(TopVariable);
    SHUTDOWN_HANDLER(Choose);
    SHUTDOWN_HANDLER(Param);
    SHUTDOWN_HANDLER(Import);
    SHUTDOWN_HANDLER(AttributeSet);
    SHUTDOWN_HANDLER(Fallback);
}
