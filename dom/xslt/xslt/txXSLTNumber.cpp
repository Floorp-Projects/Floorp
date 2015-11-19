/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/FloatingPoint.h"

#include "txXSLTNumber.h"
#include "nsGkAtoms.h"
#include "txCore.h"
#include <math.h>
#include "txExpr.h"
#include "txXSLTPatterns.h"
#include "txIXPathContext.h"
#include "txXPathTreeWalker.h"

#include <algorithm>

nsresult txXSLTNumber::createNumber(Expr* aValueExpr, txPattern* aCountPattern,
                                    txPattern* aFromPattern, LevelType aLevel,
                                    Expr* aGroupSize, Expr* aGroupSeparator,
                                    Expr* aFormat, txIEvalContext* aContext,
                                    nsAString& aResult)
{
    aResult.Truncate();
    nsresult rv = NS_OK;

    // Parse format
    txList counters;
    nsAutoString head, tail;
    rv = getCounters(aGroupSize, aGroupSeparator, aFormat, aContext, counters,
                     head, tail);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // Create list of values to format
    txList values;
    nsAutoString valueString;
    rv = getValueList(aValueExpr, aCountPattern, aFromPattern, aLevel,
                      aContext, values, valueString);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!valueString.IsEmpty()) {
        aResult = valueString;

        return NS_OK;
    }

    // Create resulting string
    aResult = head;
    bool first = true;
    txListIterator valueIter(&values);
    txListIterator counterIter(&counters);
    valueIter.resetToEnd();
    int32_t value;
    txFormattedCounter* counter = 0;
    while ((value = NS_PTR_TO_INT32(valueIter.previous()))) {
        if (counterIter.hasNext()) {
            counter = (txFormattedCounter*)counterIter.next();
        }

        if (!first) {
            aResult.Append(counter->mSeparator);
        }

        counter->appendNumber(value, aResult);
        first = false;
    }
    
    aResult.Append(tail);
    
    txListIterator iter(&counters);
    while (iter.hasNext()) {
        delete (txFormattedCounter*)iter.next();
    }

    return NS_OK;
}

nsresult
txXSLTNumber::getValueList(Expr* aValueExpr, txPattern* aCountPattern,
                           txPattern* aFromPattern, LevelType aLevel,
                           txIEvalContext* aContext, txList& aValues,
                           nsAString& aValueString)
{
    aValueString.Truncate();
    nsresult rv = NS_OK;

    // If the value attribute exists then use that
    if (aValueExpr) {
        RefPtr<txAExprResult> result;
        rv = aValueExpr->evaluate(aContext, getter_AddRefs(result));
        NS_ENSURE_SUCCESS(rv, rv);

        double value = result->numberValue();

        if (mozilla::IsInfinite(value) || mozilla::IsNaN(value) ||
            value < 0.5) {
            txDouble::toString(value, aValueString);
            return NS_OK;
        }
        
        aValues.add(NS_INT32_TO_PTR((int32_t)floor(value + 0.5)));
        return NS_OK;
    }


    // Otherwise use count/from/level

    txPattern* countPattern = aCountPattern;
    bool ownsCountPattern = false;
    const txXPathNode& currNode = aContext->getContextNode();

    // Parse count- and from-attributes

    if (!aCountPattern) {
        ownsCountPattern = true;
        txNodeTest* nodeTest;
        uint16_t nodeType = txXPathNodeUtils::getNodeType(currNode);
        switch (nodeType) {
            case txXPathNodeType::ELEMENT_NODE:
            {
                nsCOMPtr<nsIAtom> localName =
                    txXPathNodeUtils::getLocalName(currNode);
                int32_t namespaceID = txXPathNodeUtils::getNamespaceID(currNode);
                nodeTest = new txNameTest(0, localName, namespaceID,
                                          txXPathNodeType::ELEMENT_NODE);
                break;
            }
            case txXPathNodeType::TEXT_NODE:
            case txXPathNodeType::CDATA_SECTION_NODE:
            {
                nodeTest = new txNodeTypeTest(txNodeTypeTest::TEXT_TYPE);
                break;
            }
            case txXPathNodeType::PROCESSING_INSTRUCTION_NODE:
            {
                txNodeTypeTest* typeTest;
                typeTest = new txNodeTypeTest(txNodeTypeTest::PI_TYPE);
                nsAutoString nodeName;
                txXPathNodeUtils::getNodeName(currNode, nodeName);
                typeTest->setNodeName(nodeName);
                nodeTest = typeTest;
                break;
            }
            case txXPathNodeType::COMMENT_NODE:
            {
                nodeTest = new txNodeTypeTest(txNodeTypeTest::COMMENT_TYPE);
                break;
            }
            case txXPathNodeType::DOCUMENT_NODE:
            case txXPathNodeType::ATTRIBUTE_NODE:
            default:
            {
                // this won't match anything as we walk up the tree
                // but it's what the spec says to do
                nodeTest = new txNameTest(0, nsGkAtoms::_asterisk, 0,
                                          nodeType);
                break;
            }
        }
        MOZ_ASSERT(nodeTest);
        countPattern = new txStepPattern(nodeTest, false);
    }


    // Generate list of values depending on the value of the level-attribute

    // level = "single"
    if (aLevel == eLevelSingle) {
        txXPathTreeWalker walker(currNode);
        do {
            if (aFromPattern && !walker.isOnNode(currNode) &&
                aFromPattern->matches(walker.getCurrentPosition(), aContext)) {
                break;
            }

            if (countPattern->matches(walker.getCurrentPosition(), aContext)) {
                aValues.add(NS_INT32_TO_PTR(getSiblingCount(walker, countPattern,
                                                            aContext)));
                break;
            }

        } while (walker.moveToParent());

        // Spec says to only match ancestors that are decendants of the
        // ancestor that matches the from-pattern, so keep going to make
        // sure that there is an ancestor that does.
        if (aFromPattern && aValues.getLength()) {
            bool hasParent;
            while ((hasParent = walker.moveToParent())) {
                if (aFromPattern->matches(walker.getCurrentPosition(), aContext)) {
                    break;
                }
            }

            if (!hasParent) {
                aValues.clear();
            }
        }
    }
    // level = "multiple"
    else if (aLevel == eLevelMultiple) {
        // find all ancestor-or-selfs that matches count until...
        txXPathTreeWalker walker(currNode);
        bool matchedFrom = false;
        do {
            if (aFromPattern && !walker.isOnNode(currNode) &&
                aFromPattern->matches(walker.getCurrentPosition(), aContext)) {
                //... we find one that matches from
                matchedFrom = true;
                break;
            }

            if (countPattern->matches(walker.getCurrentPosition(), aContext)) {
                aValues.add(NS_INT32_TO_PTR(getSiblingCount(walker, countPattern,
                                                            aContext)));
            }
        } while (walker.moveToParent());

        // Spec says to only match ancestors that are decendants of the
        // ancestor that matches the from-pattern, so if none did then
        // we shouldn't search anything
        if (aFromPattern && !matchedFrom) {
            aValues.clear();
        }
    }
    // level = "any"
    else if (aLevel == eLevelAny) {
        int32_t value = 0;
        bool matchedFrom = false;

        txXPathTreeWalker walker(currNode);
        do {
            if (aFromPattern && !walker.isOnNode(currNode) &&
                aFromPattern->matches(walker.getCurrentPosition(), aContext)) {
                matchedFrom = true;
                break;
            }

            if (countPattern->matches(walker.getCurrentPosition(), aContext)) {
                ++value;
            }

        } while (getPrevInDocumentOrder(walker));

        // Spec says to only count nodes that follows the first node that
        // matches the from pattern. So so if none did then we shouldn't
        // count any
        if (aFromPattern && !matchedFrom) {
            value = 0;
        }

        if (value) {
            aValues.add(NS_INT32_TO_PTR(value));
        }
    }

    if (ownsCountPattern) {
        delete countPattern;
    }
    
    return NS_OK;
}


nsresult
txXSLTNumber::getCounters(Expr* aGroupSize, Expr* aGroupSeparator,
                          Expr* aFormat, txIEvalContext* aContext,
                          txList& aCounters, nsAString& aHead,
                          nsAString& aTail)
{
    aHead.Truncate();
    aTail.Truncate();

    nsresult rv = NS_OK;

    nsAutoString groupSeparator;
    int32_t groupSize = 0;
    if (aGroupSize && aGroupSeparator) {
        nsAutoString sizeStr;
        rv = aGroupSize->evaluateToString(aContext, sizeStr);
        NS_ENSURE_SUCCESS(rv, rv);

        double size = txDouble::toDouble(sizeStr);
        groupSize = (int32_t)size;
        if ((double)groupSize != size) {
            groupSize = 0;
        }

        rv = aGroupSeparator->evaluateToString(aContext, groupSeparator);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsAutoString format;
    if (aFormat) {
        rv = aFormat->evaluateToString(aContext, format);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    uint32_t formatLen = format.Length();
    uint32_t formatPos = 0;
    char16_t ch = 0;

    // start with header
    while (formatPos < formatLen &&
           !isAlphaNumeric(ch = format.CharAt(formatPos))) {
        aHead.Append(ch);
        ++formatPos;
    }

    // If there are no formatting tokens we need to create a default one.
    if (formatPos == formatLen) {
        txFormattedCounter* defaultCounter;
        rv = txFormattedCounter::getCounterFor(NS_LITERAL_STRING("1"), groupSize,
                                               groupSeparator, defaultCounter);
        NS_ENSURE_SUCCESS(rv, rv);

        defaultCounter->mSeparator.Assign('.');
        rv = aCounters.add(defaultCounter);
        if (NS_FAILED(rv)) {
            // XXX ErrorReport: out of memory
            delete defaultCounter;
            return rv;
        }

        return NS_OK;
    }

    while (formatPos < formatLen) {
        nsAutoString sepToken;
        // parse separator token
        if (!aCounters.getLength()) {
            // Set the first counters separator to default value so that if
            // there is only one formatting token and we're formatting a
            // value-list longer then one we use the default separator. This
            // won't be used when formatting the first value anyway.
            sepToken.Assign('.');
        }
        else {
            while (formatPos < formatLen &&
                   !isAlphaNumeric(ch = format.CharAt(formatPos))) {
                sepToken.Append(ch);
                ++formatPos;
            }
        }

        // if we're at the end of the string then the previous token was the tail
        if (formatPos == formatLen) {
            aTail = sepToken;
            return NS_OK;
        }

        // parse formatting token
        nsAutoString numToken;
        while (formatPos < formatLen &&
               isAlphaNumeric(ch = format.CharAt(formatPos))) {
            numToken.Append(ch);
            ++formatPos;
        }
        
        txFormattedCounter* counter = 0;
        rv = txFormattedCounter::getCounterFor(numToken, groupSize,
                                               groupSeparator, counter);
        if (NS_FAILED(rv)) {
            txListIterator iter(&aCounters);
            while (iter.hasNext()) {
                delete (txFormattedCounter*)iter.next();
            }
            aCounters.clear();
            return rv;
        }

        // Add to list of counters
        counter->mSeparator = sepToken;
        rv = aCounters.add(counter);
        if (NS_FAILED(rv)) {
            // XXX ErrorReport: out of memory
            txListIterator iter(&aCounters);
            while (iter.hasNext()) {
                delete (txFormattedCounter*)iter.next();
            }
            aCounters.clear();
            return rv;
        }
    }
    
    return NS_OK;
}

int32_t
txXSLTNumber::getSiblingCount(txXPathTreeWalker& aWalker,
                              txPattern* aCountPattern,
                              txIMatchContext* aContext)
{
    int32_t value = 1;
    while (aWalker.moveToPreviousSibling()) {
        if (aCountPattern->matches(aWalker.getCurrentPosition(), aContext)) {
            ++value;
        }
    }
    return value;
}

bool
txXSLTNumber::getPrevInDocumentOrder(txXPathTreeWalker& aWalker)
{
    if (aWalker.moveToPreviousSibling()) {
        while (aWalker.moveToLastChild()) {
            // do nothing
        }
        return true;
    }
    return aWalker.moveToParent();
}

struct CharRange {
    char16_t lower;             // inclusive
    char16_t upper;             // inclusive

    bool operator<(const CharRange& other) const {
        return upper < other.lower;
    }
};

bool txXSLTNumber::isAlphaNumeric(char16_t ch)
{
    static const CharRange alphanumericRanges[] = {
        { 0x0030, 0x0039 },
        { 0x0041, 0x005A },
        { 0x0061, 0x007A },
        { 0x00AA, 0x00AA },
        { 0x00B2, 0x00B3 },
        { 0x00B5, 0x00B5 },
        { 0x00B9, 0x00BA },
        { 0x00BC, 0x00BE },
        { 0x00C0, 0x00D6 },
        { 0x00D8, 0x00F6 },
        { 0x00F8, 0x021F },
        { 0x0222, 0x0233 },
        { 0x0250, 0x02AD },
        { 0x02B0, 0x02B8 },
        { 0x02BB, 0x02C1 },
        { 0x02D0, 0x02D1 },
        { 0x02E0, 0x02E4 },
        { 0x02EE, 0x02EE },
        { 0x037A, 0x037A },
        { 0x0386, 0x0386 },
        { 0x0388, 0x038A },
        { 0x038C, 0x038C },
        { 0x038E, 0x03A1 },
        { 0x03A3, 0x03CE },
        { 0x03D0, 0x03D7 },
        { 0x03DA, 0x03F3 },
        { 0x0400, 0x0481 },
        { 0x048C, 0x04C4 },
        { 0x04C7, 0x04C8 },
        { 0x04CB, 0x04CC },
        { 0x04D0, 0x04F5 },
        { 0x04F8, 0x04F9 },
        { 0x0531, 0x0556 },
        { 0x0559, 0x0559 },
        { 0x0561, 0x0587 },
        { 0x05D0, 0x05EA },
        { 0x05F0, 0x05F2 },
        { 0x0621, 0x063A },
        { 0x0640, 0x064A },
        { 0x0660, 0x0669 },
        { 0x0671, 0x06D3 },
        { 0x06D5, 0x06D5 },
        { 0x06E5, 0x06E6 },
        { 0x06F0, 0x06FC },
        { 0x0710, 0x0710 },
        { 0x0712, 0x072C },
        { 0x0780, 0x07A5 },
        { 0x0905, 0x0939 },
        { 0x093D, 0x093D },
        { 0x0950, 0x0950 },
        { 0x0958, 0x0961 },
        { 0x0966, 0x096F },
        { 0x0985, 0x098C },
        { 0x098F, 0x0990 },
        { 0x0993, 0x09A8 },
        { 0x09AA, 0x09B0 },
        { 0x09B2, 0x09B2 },
        { 0x09B6, 0x09B9 },
        { 0x09DC, 0x09DD },
        { 0x09DF, 0x09E1 },
        { 0x09E6, 0x09F1 },
        { 0x09F4, 0x09F9 },
        { 0x0A05, 0x0A0A },
        { 0x0A0F, 0x0A10 },
        { 0x0A13, 0x0A28 },
        { 0x0A2A, 0x0A30 },
        { 0x0A32, 0x0A33 },
        { 0x0A35, 0x0A36 },
        { 0x0A38, 0x0A39 },
        { 0x0A59, 0x0A5C },
        { 0x0A5E, 0x0A5E },
        { 0x0A66, 0x0A6F },
        { 0x0A72, 0x0A74 },
        { 0x0A85, 0x0A8B },
        { 0x0A8D, 0x0A8D },
        { 0x0A8F, 0x0A91 },
        { 0x0A93, 0x0AA8 },
        { 0x0AAA, 0x0AB0 },
        { 0x0AB2, 0x0AB3 },
        { 0x0AB5, 0x0AB9 },
        { 0x0ABD, 0x0ABD },
        { 0x0AD0, 0x0AD0 },
        { 0x0AE0, 0x0AE0 },
        { 0x0AE6, 0x0AEF },
        { 0x0B05, 0x0B0C },
        { 0x0B0F, 0x0B10 },
        { 0x0B13, 0x0B28 },
        { 0x0B2A, 0x0B30 },
        { 0x0B32, 0x0B33 },
        { 0x0B36, 0x0B39 },
        { 0x0B3D, 0x0B3D },
        { 0x0B5C, 0x0B5D },
        { 0x0B5F, 0x0B61 },
        { 0x0B66, 0x0B6F },
        { 0x0B85, 0x0B8A },
        { 0x0B8E, 0x0B90 },
        { 0x0B92, 0x0B95 },
        { 0x0B99, 0x0B9A },
        { 0x0B9C, 0x0B9C },
        { 0x0B9E, 0x0B9F },
        { 0x0BA3, 0x0BA4 },
        { 0x0BA8, 0x0BAA },
        { 0x0BAE, 0x0BB5 },
        { 0x0BB7, 0x0BB9 },
        { 0x0BE7, 0x0BF2 },
        { 0x0C05, 0x0C0C },
        { 0x0C0E, 0x0C10 },
        { 0x0C12, 0x0C28 },
        { 0x0C2A, 0x0C33 },
        { 0x0C35, 0x0C39 },
        { 0x0C60, 0x0C61 },
        { 0x0C66, 0x0C6F },
        { 0x0C85, 0x0C8C },
        { 0x0C8E, 0x0C90 },
        { 0x0C92, 0x0CA8 },
        { 0x0CAA, 0x0CB3 },
        { 0x0CB5, 0x0CB9 },
        { 0x0CDE, 0x0CDE },
        { 0x0CE0, 0x0CE1 },
        { 0x0CE6, 0x0CEF },
        { 0x0D05, 0x0D0C },
        { 0x0D0E, 0x0D10 },
        { 0x0D12, 0x0D28 },
        { 0x0D2A, 0x0D39 },
        { 0x0D60, 0x0D61 },
        { 0x0D66, 0x0D6F },
        { 0x0D85, 0x0D96 },
        { 0x0D9A, 0x0DB1 },
        { 0x0DB3, 0x0DBB },
        { 0x0DBD, 0x0DBD },
        { 0x0DC0, 0x0DC6 },
        { 0x0E01, 0x0E30 },
        { 0x0E32, 0x0E33 },
        { 0x0E40, 0x0E46 },
        { 0x0E50, 0x0E59 },
        { 0x0E81, 0x0E82 },
        { 0x0E84, 0x0E84 },
        { 0x0E87, 0x0E88 },
        { 0x0E8A, 0x0E8A },
        { 0x0E8D, 0x0E8D },
        { 0x0E94, 0x0E97 },
        { 0x0E99, 0x0E9F },
        { 0x0EA1, 0x0EA3 },
        { 0x0EA5, 0x0EA5 },
        { 0x0EA7, 0x0EA7 },
        { 0x0EAA, 0x0EAB },
        { 0x0EAD, 0x0EB0 },
        { 0x0EB2, 0x0EB3 },
        { 0x0EBD, 0x0EBD },
        { 0x0EC0, 0x0EC4 },
        { 0x0EC6, 0x0EC6 },
        { 0x0ED0, 0x0ED9 },
        { 0x0EDC, 0x0EDD },
        { 0x0F00, 0x0F00 },
        { 0x0F20, 0x0F33 },
        { 0x0F40, 0x0F47 },
        { 0x0F49, 0x0F6A },
        { 0x0F88, 0x0F8B },
        { 0x1000, 0x1021 },
        { 0x1023, 0x1027 },
        { 0x1029, 0x102A },
        { 0x1040, 0x1049 },
        { 0x1050, 0x1055 },
        { 0x10A0, 0x10C5 },
        { 0x10D0, 0x10F6 },
        { 0x1100, 0x1159 },
        { 0x115F, 0x11A2 },
        { 0x11A8, 0x11F9 },
        { 0x1200, 0x1206 },
        { 0x1208, 0x1246 },
        { 0x1248, 0x1248 },
        { 0x124A, 0x124D },
        { 0x1250, 0x1256 },
        { 0x1258, 0x1258 },
        { 0x125A, 0x125D },
        { 0x1260, 0x1286 },
        { 0x1288, 0x1288 },
        { 0x128A, 0x128D },
        { 0x1290, 0x12AE },
        { 0x12B0, 0x12B0 },
        { 0x12B2, 0x12B5 },
        { 0x12B8, 0x12BE },
        { 0x12C0, 0x12C0 },
        { 0x12C2, 0x12C5 },
        { 0x12C8, 0x12CE },
        { 0x12D0, 0x12D6 },
        { 0x12D8, 0x12EE },
        { 0x12F0, 0x130E },
        { 0x1310, 0x1310 },
        { 0x1312, 0x1315 },
        { 0x1318, 0x131E },
        { 0x1320, 0x1346 },
        { 0x1348, 0x135A },
        { 0x1369, 0x137C },
        { 0x13A0, 0x13F4 },
        { 0x1401, 0x166C },
        { 0x166F, 0x1676 },
        { 0x1681, 0x169A },
        { 0x16A0, 0x16EA },
        { 0x16EE, 0x16F0 },
        { 0x1780, 0x17B3 },
        { 0x17E0, 0x17E9 },
        { 0x1810, 0x1819 },
        { 0x1820, 0x1877 },
        { 0x1880, 0x18A8 },
        { 0x1E00, 0x1E9B },
        { 0x1EA0, 0x1EF9 },
        { 0x1F00, 0x1F15 },
        { 0x1F18, 0x1F1D },
        { 0x1F20, 0x1F45 },
        { 0x1F48, 0x1F4D },
        { 0x1F50, 0x1F57 },
        { 0x1F59, 0x1F59 },
        { 0x1F5B, 0x1F5B },
        { 0x1F5D, 0x1F5D },
        { 0x1F5F, 0x1F7D },
        { 0x1F80, 0x1FB4 },
        { 0x1FB6, 0x1FBC },
        { 0x1FBE, 0x1FBE },
        { 0x1FC2, 0x1FC4 },
        { 0x1FC6, 0x1FCC },
        { 0x1FD0, 0x1FD3 },
        { 0x1FD6, 0x1FDB },
        { 0x1FE0, 0x1FEC },
        { 0x1FF2, 0x1FF4 },
        { 0x1FF6, 0x1FFC },
        { 0x2070, 0x2070 },
        { 0x2074, 0x2079 },
        { 0x207F, 0x2089 },
        { 0x2102, 0x2102 },
        { 0x2107, 0x2107 },
        { 0x210A, 0x2113 },
        { 0x2115, 0x2115 },
        { 0x2119, 0x211D },
        { 0x2124, 0x2124 },
        { 0x2126, 0x2126 },
        { 0x2128, 0x2128 },
        { 0x212A, 0x212D },
        { 0x212F, 0x2131 },
        { 0x2133, 0x2139 },
        { 0x2153, 0x2183 },
        { 0x2460, 0x249B },
        { 0x24EA, 0x24EA },
        { 0x2776, 0x2793 },
        { 0x3005, 0x3007 },
        { 0x3021, 0x3029 },
        { 0x3031, 0x3035 },
        { 0x3038, 0x303A },
        { 0x3041, 0x3094 },
        { 0x309D, 0x309E },
        { 0x30A1, 0x30FA },
        { 0x30FC, 0x30FE },
        { 0x3105, 0x312C },
        { 0x3131, 0x318E },
        { 0x3192, 0x3195 },
        { 0x31A0, 0x31B7 },
        { 0x3220, 0x3229 },
        { 0x3280, 0x3289 },
        { 0x3400, 0x3400 },
        { 0x4DB5, 0x4DB5 },
        { 0x4E00, 0x4E00 },
        { 0x9FA5, 0x9FA5 },
        { 0xA000, 0xA48C },
        { 0xAC00, 0xAC00 },
        { 0xD7A3, 0xD7A3 },
        { 0xF900, 0xFA2D },
        { 0xFB00, 0xFB06 },
        { 0xFB13, 0xFB17 },
        { 0xFB1D, 0xFB1D },
        { 0xFB1F, 0xFB28 },
        { 0xFB2A, 0xFB36 },
        { 0xFB38, 0xFB3C },
        { 0xFB3E, 0xFB3E },
        { 0xFB40, 0xFB41 },
        { 0xFB43, 0xFB44 },
        { 0xFB46, 0xFBB1 },
        { 0xFBD3, 0xFD3D },
        { 0xFD50, 0xFD8F },
        { 0xFD92, 0xFDC7 },
        { 0xFDF0, 0xFDFB },
        { 0xFE70, 0xFE72 },
        { 0xFE74, 0xFE74 },
        { 0xFE76, 0xFEFC },
        { 0xFF10, 0xFF19 },
        { 0xFF21, 0xFF3A },
        { 0xFF41, 0xFF5A },
        { 0xFF66, 0xFFBE },
        { 0xFFC2, 0xFFC7 },
        { 0xFFCA, 0xFFCF },
        { 0xFFD2, 0xFFD7 }
    };

    CharRange search = { ch, ch };
    const CharRange* end = mozilla::ArrayEnd(alphanumericRanges);
    const CharRange* element = std::lower_bound(&alphanumericRanges[0], end, search);
    if (element == end) {
        return false;
    }
    return element->lower <= ch && ch <= element->upper;
}
