/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"

#include "txXSLTNumber.h"
#include "nsGkAtoms.h"
#include "txCore.h"
#include <math.h>
#include "txExpr.h"
#include "txXSLTPatterns.h"
#include "txIXPathContext.h"
#include "txXPathTreeWalker.h"

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
    PRInt32 value;
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
        nsRefPtr<txAExprResult> result;
        rv = aValueExpr->evaluate(aContext, getter_AddRefs(result));
        NS_ENSURE_SUCCESS(rv, rv);

        double value = result->numberValue();

        if (MOZ_DOUBLE_IS_INFINITE(value) || MOZ_DOUBLE_IS_NaN(value) ||
            value < 0.5) {
            txDouble::toString(value, aValueString);
            return NS_OK;
        }
        
        aValues.add(NS_INT32_TO_PTR((PRInt32)floor(value + 0.5)));
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
        PRUint16 nodeType = txXPathNodeUtils::getNodeType(currNode);
        switch (nodeType) {
            case txXPathNodeType::ELEMENT_NODE:
            {
                nsCOMPtr<nsIAtom> localName =
                    txXPathNodeUtils::getLocalName(currNode);
                PRInt32 namespaceID = txXPathNodeUtils::getNamespaceID(currNode);
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
                if (typeTest) {
                    nsAutoString nodeName;
                    txXPathNodeUtils::getNodeName(currNode, nodeName);
                    typeTest->setNodeName(nodeName);
                }
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
                nodeTest = new txNameTest(0, nsGkAtoms::_asterix, 0,
                                          nodeType);
                break;
            }
        }
        NS_ENSURE_TRUE(nodeTest, NS_ERROR_OUT_OF_MEMORY);

        countPattern = new txStepPattern(nodeTest, false);
        if (!countPattern) {
            // XXX error reporting
            delete nodeTest;
            return NS_ERROR_OUT_OF_MEMORY;
        }
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
        PRInt32 value = 0;
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
    PRInt32 groupSize = 0;
    if (aGroupSize && aGroupSeparator) {
        nsAutoString sizeStr;
        rv = aGroupSize->evaluateToString(aContext, sizeStr);
        NS_ENSURE_SUCCESS(rv, rv);

        double size = txDouble::toDouble(sizeStr);
        groupSize = (PRInt32)size;
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

    PRUint32 formatLen = format.Length();
    PRUint32 formatPos = 0;
    PRUnichar ch = 0;

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

        defaultCounter->mSeparator.AssignLiteral(".");
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
            sepToken.AssignLiteral(".");
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

PRInt32
txXSLTNumber::getSiblingCount(txXPathTreeWalker& aWalker,
                              txPattern* aCountPattern,
                              txIMatchContext* aContext)
{
    PRInt32 value = 1;
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

#define TX_CHAR_RANGE(ch, a, b) if (ch < a) return false; \
    if (ch <= b) return true
#define TX_MATCH_CHAR(ch, a) if (ch < a) return false; \
    if (ch == a) return true

bool txXSLTNumber::isAlphaNumeric(PRUnichar ch)
{
    TX_CHAR_RANGE(ch, 0x0030, 0x0039);
    TX_CHAR_RANGE(ch, 0x0041, 0x005A);
    TX_CHAR_RANGE(ch, 0x0061, 0x007A);
    TX_MATCH_CHAR(ch, 0x00AA);
    TX_CHAR_RANGE(ch, 0x00B2, 0x00B3);
    TX_MATCH_CHAR(ch, 0x00B5);
    TX_CHAR_RANGE(ch, 0x00B9, 0x00BA);
    TX_CHAR_RANGE(ch, 0x00BC, 0x00BE);
    TX_CHAR_RANGE(ch, 0x00C0, 0x00D6);
    TX_CHAR_RANGE(ch, 0x00D8, 0x00F6);
    TX_CHAR_RANGE(ch, 0x00F8, 0x021F);
    TX_CHAR_RANGE(ch, 0x0222, 0x0233);
    TX_CHAR_RANGE(ch, 0x0250, 0x02AD);
    TX_CHAR_RANGE(ch, 0x02B0, 0x02B8);
    TX_CHAR_RANGE(ch, 0x02BB, 0x02C1);
    TX_CHAR_RANGE(ch, 0x02D0, 0x02D1);
    TX_CHAR_RANGE(ch, 0x02E0, 0x02E4);
    TX_MATCH_CHAR(ch, 0x02EE);
    TX_MATCH_CHAR(ch, 0x037A);
    TX_MATCH_CHAR(ch, 0x0386);
    TX_CHAR_RANGE(ch, 0x0388, 0x038A);
    TX_MATCH_CHAR(ch, 0x038C);
    TX_CHAR_RANGE(ch, 0x038E, 0x03A1);
    TX_CHAR_RANGE(ch, 0x03A3, 0x03CE);
    TX_CHAR_RANGE(ch, 0x03D0, 0x03D7);
    TX_CHAR_RANGE(ch, 0x03DA, 0x03F3);
    TX_CHAR_RANGE(ch, 0x0400, 0x0481);
    TX_CHAR_RANGE(ch, 0x048C, 0x04C4);
    TX_CHAR_RANGE(ch, 0x04C7, 0x04C8);
    TX_CHAR_RANGE(ch, 0x04CB, 0x04CC);
    TX_CHAR_RANGE(ch, 0x04D0, 0x04F5);
    TX_CHAR_RANGE(ch, 0x04F8, 0x04F9);
    TX_CHAR_RANGE(ch, 0x0531, 0x0556);
    TX_MATCH_CHAR(ch, 0x0559);
    TX_CHAR_RANGE(ch, 0x0561, 0x0587);
    TX_CHAR_RANGE(ch, 0x05D0, 0x05EA);
    TX_CHAR_RANGE(ch, 0x05F0, 0x05F2);
    TX_CHAR_RANGE(ch, 0x0621, 0x063A);
    TX_CHAR_RANGE(ch, 0x0640, 0x064A);
    TX_CHAR_RANGE(ch, 0x0660, 0x0669);
    TX_CHAR_RANGE(ch, 0x0671, 0x06D3);
    TX_MATCH_CHAR(ch, 0x06D5);
    TX_CHAR_RANGE(ch, 0x06E5, 0x06E6);
    TX_CHAR_RANGE(ch, 0x06F0, 0x06FC);
    TX_MATCH_CHAR(ch, 0x0710);
    TX_CHAR_RANGE(ch, 0x0712, 0x072C);
    TX_CHAR_RANGE(ch, 0x0780, 0x07A5);
    TX_CHAR_RANGE(ch, 0x0905, 0x0939);
    TX_MATCH_CHAR(ch, 0x093D);
    TX_MATCH_CHAR(ch, 0x0950);
    TX_CHAR_RANGE(ch, 0x0958, 0x0961);
    TX_CHAR_RANGE(ch, 0x0966, 0x096F);
    TX_CHAR_RANGE(ch, 0x0985, 0x098C);
    TX_CHAR_RANGE(ch, 0x098F, 0x0990);
    TX_CHAR_RANGE(ch, 0x0993, 0x09A8);
    TX_CHAR_RANGE(ch, 0x09AA, 0x09B0);
    TX_MATCH_CHAR(ch, 0x09B2);
    TX_CHAR_RANGE(ch, 0x09B6, 0x09B9);
    TX_CHAR_RANGE(ch, 0x09DC, 0x09DD);
    TX_CHAR_RANGE(ch, 0x09DF, 0x09E1);
    TX_CHAR_RANGE(ch, 0x09E6, 0x09F1);
    TX_CHAR_RANGE(ch, 0x09F4, 0x09F9);
    TX_CHAR_RANGE(ch, 0x0A05, 0x0A0A);
    TX_CHAR_RANGE(ch, 0x0A0F, 0x0A10);
    TX_CHAR_RANGE(ch, 0x0A13, 0x0A28);
    TX_CHAR_RANGE(ch, 0x0A2A, 0x0A30);
    TX_CHAR_RANGE(ch, 0x0A32, 0x0A33);
    TX_CHAR_RANGE(ch, 0x0A35, 0x0A36);
    TX_CHAR_RANGE(ch, 0x0A38, 0x0A39);
    TX_CHAR_RANGE(ch, 0x0A59, 0x0A5C);
    TX_MATCH_CHAR(ch, 0x0A5E);
    TX_CHAR_RANGE(ch, 0x0A66, 0x0A6F);
    TX_CHAR_RANGE(ch, 0x0A72, 0x0A74);
    TX_CHAR_RANGE(ch, 0x0A85, 0x0A8B);
    TX_MATCH_CHAR(ch, 0x0A8D);
    TX_CHAR_RANGE(ch, 0x0A8F, 0x0A91);
    TX_CHAR_RANGE(ch, 0x0A93, 0x0AA8);
    TX_CHAR_RANGE(ch, 0x0AAA, 0x0AB0);
    TX_CHAR_RANGE(ch, 0x0AB2, 0x0AB3);
    TX_CHAR_RANGE(ch, 0x0AB5, 0x0AB9);
    TX_MATCH_CHAR(ch, 0x0ABD);
    TX_MATCH_CHAR(ch, 0x0AD0);
    TX_MATCH_CHAR(ch, 0x0AE0);
    TX_CHAR_RANGE(ch, 0x0AE6, 0x0AEF);
    TX_CHAR_RANGE(ch, 0x0B05, 0x0B0C);
    TX_CHAR_RANGE(ch, 0x0B0F, 0x0B10);
    TX_CHAR_RANGE(ch, 0x0B13, 0x0B28);
    TX_CHAR_RANGE(ch, 0x0B2A, 0x0B30);
    TX_CHAR_RANGE(ch, 0x0B32, 0x0B33);
    TX_CHAR_RANGE(ch, 0x0B36, 0x0B39);
    TX_MATCH_CHAR(ch, 0x0B3D);
    TX_CHAR_RANGE(ch, 0x0B5C, 0x0B5D);
    TX_CHAR_RANGE(ch, 0x0B5F, 0x0B61);
    TX_CHAR_RANGE(ch, 0x0B66, 0x0B6F);
    TX_CHAR_RANGE(ch, 0x0B85, 0x0B8A);
    TX_CHAR_RANGE(ch, 0x0B8E, 0x0B90);
    TX_CHAR_RANGE(ch, 0x0B92, 0x0B95);
    TX_CHAR_RANGE(ch, 0x0B99, 0x0B9A);
    TX_MATCH_CHAR(ch, 0x0B9C);
    TX_CHAR_RANGE(ch, 0x0B9E, 0x0B9F);
    TX_CHAR_RANGE(ch, 0x0BA3, 0x0BA4);
    TX_CHAR_RANGE(ch, 0x0BA8, 0x0BAA);
    TX_CHAR_RANGE(ch, 0x0BAE, 0x0BB5);
    TX_CHAR_RANGE(ch, 0x0BB7, 0x0BB9);
    TX_CHAR_RANGE(ch, 0x0BE7, 0x0BF2);
    TX_CHAR_RANGE(ch, 0x0C05, 0x0C0C);
    TX_CHAR_RANGE(ch, 0x0C0E, 0x0C10);
    TX_CHAR_RANGE(ch, 0x0C12, 0x0C28);
    TX_CHAR_RANGE(ch, 0x0C2A, 0x0C33);
    TX_CHAR_RANGE(ch, 0x0C35, 0x0C39);
    TX_CHAR_RANGE(ch, 0x0C60, 0x0C61);
    TX_CHAR_RANGE(ch, 0x0C66, 0x0C6F);
    TX_CHAR_RANGE(ch, 0x0C85, 0x0C8C);
    TX_CHAR_RANGE(ch, 0x0C8E, 0x0C90);
    TX_CHAR_RANGE(ch, 0x0C92, 0x0CA8);
    TX_CHAR_RANGE(ch, 0x0CAA, 0x0CB3);
    TX_CHAR_RANGE(ch, 0x0CB5, 0x0CB9);
    TX_MATCH_CHAR(ch, 0x0CDE);
    TX_CHAR_RANGE(ch, 0x0CE0, 0x0CE1);
    TX_CHAR_RANGE(ch, 0x0CE6, 0x0CEF);
    TX_CHAR_RANGE(ch, 0x0D05, 0x0D0C);
    TX_CHAR_RANGE(ch, 0x0D0E, 0x0D10);
    TX_CHAR_RANGE(ch, 0x0D12, 0x0D28);
    TX_CHAR_RANGE(ch, 0x0D2A, 0x0D39);
    TX_CHAR_RANGE(ch, 0x0D60, 0x0D61);
    TX_CHAR_RANGE(ch, 0x0D66, 0x0D6F);
    TX_CHAR_RANGE(ch, 0x0D85, 0x0D96);
    TX_CHAR_RANGE(ch, 0x0D9A, 0x0DB1);
    TX_CHAR_RANGE(ch, 0x0DB3, 0x0DBB);
    TX_MATCH_CHAR(ch, 0x0DBD);
    TX_CHAR_RANGE(ch, 0x0DC0, 0x0DC6);
    TX_CHAR_RANGE(ch, 0x0E01, 0x0E30);
    TX_CHAR_RANGE(ch, 0x0E32, 0x0E33);
    TX_CHAR_RANGE(ch, 0x0E40, 0x0E46);
    TX_CHAR_RANGE(ch, 0x0E50, 0x0E59);
    TX_CHAR_RANGE(ch, 0x0E81, 0x0E82);
    TX_MATCH_CHAR(ch, 0x0E84);
    TX_CHAR_RANGE(ch, 0x0E87, 0x0E88);
    TX_MATCH_CHAR(ch, 0x0E8A);
    TX_MATCH_CHAR(ch, 0x0E8D);
    TX_CHAR_RANGE(ch, 0x0E94, 0x0E97);
    TX_CHAR_RANGE(ch, 0x0E99, 0x0E9F);
    TX_CHAR_RANGE(ch, 0x0EA1, 0x0EA3);
    TX_MATCH_CHAR(ch, 0x0EA5);
    TX_MATCH_CHAR(ch, 0x0EA7);
    TX_CHAR_RANGE(ch, 0x0EAA, 0x0EAB);
    TX_CHAR_RANGE(ch, 0x0EAD, 0x0EB0);
    TX_CHAR_RANGE(ch, 0x0EB2, 0x0EB3);
    TX_MATCH_CHAR(ch, 0x0EBD);
    TX_CHAR_RANGE(ch, 0x0EC0, 0x0EC4);
    TX_MATCH_CHAR(ch, 0x0EC6);
    TX_CHAR_RANGE(ch, 0x0ED0, 0x0ED9);
    TX_CHAR_RANGE(ch, 0x0EDC, 0x0EDD);
    TX_MATCH_CHAR(ch, 0x0F00);
    TX_CHAR_RANGE(ch, 0x0F20, 0x0F33);
    TX_CHAR_RANGE(ch, 0x0F40, 0x0F47);
    TX_CHAR_RANGE(ch, 0x0F49, 0x0F6A);
    TX_CHAR_RANGE(ch, 0x0F88, 0x0F8B);
    TX_CHAR_RANGE(ch, 0x1000, 0x1021);
    TX_CHAR_RANGE(ch, 0x1023, 0x1027);
    TX_CHAR_RANGE(ch, 0x1029, 0x102A);
    TX_CHAR_RANGE(ch, 0x1040, 0x1049);
    TX_CHAR_RANGE(ch, 0x1050, 0x1055);
    TX_CHAR_RANGE(ch, 0x10A0, 0x10C5);
    TX_CHAR_RANGE(ch, 0x10D0, 0x10F6);
    TX_CHAR_RANGE(ch, 0x1100, 0x1159);
    TX_CHAR_RANGE(ch, 0x115F, 0x11A2);
    TX_CHAR_RANGE(ch, 0x11A8, 0x11F9);
    TX_CHAR_RANGE(ch, 0x1200, 0x1206);
    TX_CHAR_RANGE(ch, 0x1208, 0x1246);
    TX_MATCH_CHAR(ch, 0x1248);
    TX_CHAR_RANGE(ch, 0x124A, 0x124D);
    TX_CHAR_RANGE(ch, 0x1250, 0x1256);
    TX_MATCH_CHAR(ch, 0x1258);
    TX_CHAR_RANGE(ch, 0x125A, 0x125D);
    TX_CHAR_RANGE(ch, 0x1260, 0x1286);
    TX_MATCH_CHAR(ch, 0x1288);
    TX_CHAR_RANGE(ch, 0x128A, 0x128D);
    TX_CHAR_RANGE(ch, 0x1290, 0x12AE);
    TX_MATCH_CHAR(ch, 0x12B0);
    TX_CHAR_RANGE(ch, 0x12B2, 0x12B5);
    TX_CHAR_RANGE(ch, 0x12B8, 0x12BE);
    TX_MATCH_CHAR(ch, 0x12C0);
    TX_CHAR_RANGE(ch, 0x12C2, 0x12C5);
    TX_CHAR_RANGE(ch, 0x12C8, 0x12CE);
    TX_CHAR_RANGE(ch, 0x12D0, 0x12D6);
    TX_CHAR_RANGE(ch, 0x12D8, 0x12EE);
    TX_CHAR_RANGE(ch, 0x12F0, 0x130E);
    TX_MATCH_CHAR(ch, 0x1310);
    TX_CHAR_RANGE(ch, 0x1312, 0x1315);
    TX_CHAR_RANGE(ch, 0x1318, 0x131E);
    TX_CHAR_RANGE(ch, 0x1320, 0x1346);
    TX_CHAR_RANGE(ch, 0x1348, 0x135A);
    TX_CHAR_RANGE(ch, 0x1369, 0x137C);
    TX_CHAR_RANGE(ch, 0x13A0, 0x13F4);
    TX_CHAR_RANGE(ch, 0x1401, 0x166C);
    TX_CHAR_RANGE(ch, 0x166F, 0x1676);
    TX_CHAR_RANGE(ch, 0x1681, 0x169A);
    TX_CHAR_RANGE(ch, 0x16A0, 0x16EA);
    TX_CHAR_RANGE(ch, 0x16EE, 0x16F0);
    TX_CHAR_RANGE(ch, 0x1780, 0x17B3);
    TX_CHAR_RANGE(ch, 0x17E0, 0x17E9);
    TX_CHAR_RANGE(ch, 0x1810, 0x1819);
    TX_CHAR_RANGE(ch, 0x1820, 0x1877);
    TX_CHAR_RANGE(ch, 0x1880, 0x18A8);
    TX_CHAR_RANGE(ch, 0x1E00, 0x1E9B);
    TX_CHAR_RANGE(ch, 0x1EA0, 0x1EF9);
    TX_CHAR_RANGE(ch, 0x1F00, 0x1F15);
    TX_CHAR_RANGE(ch, 0x1F18, 0x1F1D);
    TX_CHAR_RANGE(ch, 0x1F20, 0x1F45);
    TX_CHAR_RANGE(ch, 0x1F48, 0x1F4D);
    TX_CHAR_RANGE(ch, 0x1F50, 0x1F57);
    TX_MATCH_CHAR(ch, 0x1F59);
    TX_MATCH_CHAR(ch, 0x1F5B);
    TX_MATCH_CHAR(ch, 0x1F5D);
    TX_CHAR_RANGE(ch, 0x1F5F, 0x1F7D);
    TX_CHAR_RANGE(ch, 0x1F80, 0x1FB4);
    TX_CHAR_RANGE(ch, 0x1FB6, 0x1FBC);
    TX_MATCH_CHAR(ch, 0x1FBE);
    TX_CHAR_RANGE(ch, 0x1FC2, 0x1FC4);
    TX_CHAR_RANGE(ch, 0x1FC6, 0x1FCC);
    TX_CHAR_RANGE(ch, 0x1FD0, 0x1FD3);
    TX_CHAR_RANGE(ch, 0x1FD6, 0x1FDB);
    TX_CHAR_RANGE(ch, 0x1FE0, 0x1FEC);
    TX_CHAR_RANGE(ch, 0x1FF2, 0x1FF4);
    TX_CHAR_RANGE(ch, 0x1FF6, 0x1FFC);
    TX_MATCH_CHAR(ch, 0x2070);
    TX_CHAR_RANGE(ch, 0x2074, 0x2079);
    TX_CHAR_RANGE(ch, 0x207F, 0x2089);
    TX_MATCH_CHAR(ch, 0x2102);
    TX_MATCH_CHAR(ch, 0x2107);
    TX_CHAR_RANGE(ch, 0x210A, 0x2113);
    TX_MATCH_CHAR(ch, 0x2115);
    TX_CHAR_RANGE(ch, 0x2119, 0x211D);
    TX_MATCH_CHAR(ch, 0x2124);
    TX_MATCH_CHAR(ch, 0x2126);
    TX_MATCH_CHAR(ch, 0x2128);
    TX_CHAR_RANGE(ch, 0x212A, 0x212D);
    TX_CHAR_RANGE(ch, 0x212F, 0x2131);
    TX_CHAR_RANGE(ch, 0x2133, 0x2139);
    TX_CHAR_RANGE(ch, 0x2153, 0x2183);
    TX_CHAR_RANGE(ch, 0x2460, 0x249B);
    TX_MATCH_CHAR(ch, 0x24EA);
    TX_CHAR_RANGE(ch, 0x2776, 0x2793);
    TX_CHAR_RANGE(ch, 0x3005, 0x3007);
    TX_CHAR_RANGE(ch, 0x3021, 0x3029);
    TX_CHAR_RANGE(ch, 0x3031, 0x3035);
    TX_CHAR_RANGE(ch, 0x3038, 0x303A);
    TX_CHAR_RANGE(ch, 0x3041, 0x3094);
    TX_CHAR_RANGE(ch, 0x309D, 0x309E);
    TX_CHAR_RANGE(ch, 0x30A1, 0x30FA);
    TX_CHAR_RANGE(ch, 0x30FC, 0x30FE);
    TX_CHAR_RANGE(ch, 0x3105, 0x312C);
    TX_CHAR_RANGE(ch, 0x3131, 0x318E);
    TX_CHAR_RANGE(ch, 0x3192, 0x3195);
    TX_CHAR_RANGE(ch, 0x31A0, 0x31B7);
    TX_CHAR_RANGE(ch, 0x3220, 0x3229);
    TX_CHAR_RANGE(ch, 0x3280, 0x3289);
    TX_MATCH_CHAR(ch, 0x3400);
    TX_MATCH_CHAR(ch, 0x4DB5);
    TX_MATCH_CHAR(ch, 0x4E00);
    TX_MATCH_CHAR(ch, 0x9FA5);
    TX_CHAR_RANGE(ch, 0xA000, 0xA48C);
    TX_MATCH_CHAR(ch, 0xAC00);
    TX_MATCH_CHAR(ch, 0xD7A3);
    TX_CHAR_RANGE(ch, 0xF900, 0xFA2D);
    TX_CHAR_RANGE(ch, 0xFB00, 0xFB06);
    TX_CHAR_RANGE(ch, 0xFB13, 0xFB17);
    TX_MATCH_CHAR(ch, 0xFB1D);
    TX_CHAR_RANGE(ch, 0xFB1F, 0xFB28);
    TX_CHAR_RANGE(ch, 0xFB2A, 0xFB36);
    TX_CHAR_RANGE(ch, 0xFB38, 0xFB3C);
    TX_MATCH_CHAR(ch, 0xFB3E);
    TX_CHAR_RANGE(ch, 0xFB40, 0xFB41);
    TX_CHAR_RANGE(ch, 0xFB43, 0xFB44);
    TX_CHAR_RANGE(ch, 0xFB46, 0xFBB1);
    TX_CHAR_RANGE(ch, 0xFBD3, 0xFD3D);
    TX_CHAR_RANGE(ch, 0xFD50, 0xFD8F);
    TX_CHAR_RANGE(ch, 0xFD92, 0xFDC7);
    TX_CHAR_RANGE(ch, 0xFDF0, 0xFDFB);
    TX_CHAR_RANGE(ch, 0xFE70, 0xFE72);
    TX_MATCH_CHAR(ch, 0xFE74);
    TX_CHAR_RANGE(ch, 0xFE76, 0xFEFC);
    TX_CHAR_RANGE(ch, 0xFF10, 0xFF19);
    TX_CHAR_RANGE(ch, 0xFF21, 0xFF3A);
    TX_CHAR_RANGE(ch, 0xFF41, 0xFF5A);
    TX_CHAR_RANGE(ch, 0xFF66, 0xFFBE);
    TX_CHAR_RANGE(ch, 0xFFC2, 0xFFC7);
    TX_CHAR_RANGE(ch, 0xFFCA, 0xFFCF);
    TX_CHAR_RANGE(ch, 0xFFD2, 0xFFD7);
    return false;
}
