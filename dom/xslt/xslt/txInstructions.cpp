/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Move.h"
#include "txInstructions.h"
#include "nsError.h"
#include "txExpr.h"
#include "txStylesheet.h"
#include "txNodeSetContext.h"
#include "txTextHandler.h"
#include "nsIConsoleService.h"
#include "nsServiceManagerUtils.h"
#include "txStringUtils.h"
#include "nsGkAtoms.h"
#include "txRtfHandler.h"
#include "txNodeSorter.h"
#include "txXSLTNumber.h"
#include "txExecutionState.h"

nsresult
txApplyDefaultElementTemplate::execute(txExecutionState& aEs)
{
    txExecutionState::TemplateRule* rule = aEs.getCurrentTemplateRule();
    txExpandedName mode(rule->mModeNsId, rule->mModeLocalName);
    txStylesheet::ImportFrame* frame = 0;
    txInstruction* templ;
    nsresult rv =
        aEs.mStylesheet->findTemplate(aEs.getEvalContext()->getContextNode(),
                                      mode, &aEs, nullptr, &templ, &frame);
    NS_ENSURE_SUCCESS(rv, rv);

    aEs.pushTemplateRule(frame, mode, aEs.mTemplateParams);

    return aEs.runTemplate(templ);
}

nsresult
txApplyImportsEnd::execute(txExecutionState& aEs)
{
    aEs.popTemplateRule();
    RefPtr<txParameterMap> paramMap = aEs.popParamMap();

    return NS_OK;
}

nsresult
txApplyImportsStart::execute(txExecutionState& aEs)
{
    txExecutionState::TemplateRule* rule = aEs.getCurrentTemplateRule();
    // The frame is set to null when there is no current template rule, or
    // when the current template rule is a default template. However this
    // instruction isn't used in default templates.
    if (!rule->mFrame) {
        // XXX ErrorReport: apply-imports instantiated without a current rule
        return NS_ERROR_XSLT_EXECUTION_FAILURE;
    }

    aEs.pushParamMap(rule->mParams);

    txStylesheet::ImportFrame* frame = 0;
    txExpandedName mode(rule->mModeNsId, rule->mModeLocalName);
    txInstruction* templ;
    nsresult rv = aEs.mStylesheet->findTemplate(aEs.getEvalContext()->getContextNode(),
                                                mode, &aEs, rule->mFrame, &templ,
                                                &frame);
    NS_ENSURE_SUCCESS(rv, rv);

    aEs.pushTemplateRule(frame, mode, rule->mParams);

    rv = aEs.runTemplate(templ);
    if (NS_FAILED(rv)) {
      aEs.popTemplateRule();
    }

    return rv;
}

txApplyTemplates::txApplyTemplates(const txExpandedName& aMode)
    : mMode(aMode)
{
}

nsresult
txApplyTemplates::execute(txExecutionState& aEs)
{
    txStylesheet::ImportFrame* frame = 0;
    txInstruction* templ;
    nsresult rv =
        aEs.mStylesheet->findTemplate(aEs.getEvalContext()->getContextNode(),
                                      mMode, &aEs, nullptr, &templ, &frame);
    NS_ENSURE_SUCCESS(rv, rv);

    aEs.pushTemplateRule(frame, mMode, aEs.mTemplateParams);

    return aEs.runTemplate(templ);
}

txAttribute::txAttribute(nsAutoPtr<Expr>&& aName, nsAutoPtr<Expr>&& aNamespace,
                         txNamespaceMap* aMappings)
    : mName(std::move(aName)), mNamespace(std::move(aNamespace)), mMappings(aMappings)
{
}

nsresult
txAttribute::execute(txExecutionState& aEs)
{
    nsAutoPtr<txTextHandler> handler(
        static_cast<txTextHandler*>(aEs.popResultHandler()));

    nsAutoString name;
    nsresult rv = mName->evaluateToString(aEs.getEvalContext(), name);
    NS_ENSURE_SUCCESS(rv, rv);

    const char16_t* colon;
    if (!XMLUtils::isValidQName(name, &colon) ||
        TX_StringEqualsAtom(name, nsGkAtoms::xmlns)) {
        return NS_OK;
    }

    RefPtr<nsAtom> prefix;
    uint32_t lnameStart = 0;
    if (colon) {
        prefix = NS_Atomize(Substring(name.get(), colon));
        lnameStart = colon - name.get() + 1;
    }

    int32_t nsId = kNameSpaceID_None;
    if (mNamespace) {
        nsAutoString nspace;
        rv = mNamespace->evaluateToString(aEs.getEvalContext(),
                                          nspace);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!nspace.IsEmpty()) {
            nsId = txNamespaceManager::getNamespaceID(nspace);
        }
    }
    else if (colon) {
        nsId = mMappings->lookupNamespace(prefix);
    }

    // add attribute if everything was ok
    return nsId != kNameSpaceID_Unknown ?
           aEs.mResultHandler->attribute(prefix, Substring(name, lnameStart),
                                         nsId, handler->mValue) :
           NS_OK;
}

txCallTemplate::txCallTemplate(const txExpandedName& aName)
    : mName(aName)
{
}

nsresult
txCallTemplate::execute(txExecutionState& aEs)
{
    txInstruction* instr = aEs.mStylesheet->getNamedTemplate(mName);
    NS_ENSURE_TRUE(instr, NS_ERROR_XSLT_EXECUTION_FAILURE);

    nsresult rv = aEs.runTemplate(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

txCheckParam::txCheckParam(const txExpandedName& aName)
    : mName(aName), mBailTarget(nullptr)
{
}

nsresult
txCheckParam::execute(txExecutionState& aEs)
{
    nsresult rv = NS_OK;
    if (aEs.mTemplateParams) {
        RefPtr<txAExprResult> exprRes;
        aEs.mTemplateParams->getVariable(mName, getter_AddRefs(exprRes));
        if (exprRes) {
            rv = aEs.bindVariable(mName, exprRes);
            NS_ENSURE_SUCCESS(rv, rv);

            aEs.gotoInstruction(mBailTarget);
        }
    }

    return NS_OK;
}

txConditionalGoto::txConditionalGoto(nsAutoPtr<Expr>&& aCondition,
                                     txInstruction* aTarget)
    : mCondition(std::move(aCondition)), mTarget(aTarget)
{
}

nsresult
txConditionalGoto::execute(txExecutionState& aEs)
{
    bool exprRes;
    nsresult rv = mCondition->evaluateToBool(aEs.getEvalContext(), exprRes);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!exprRes) {
        aEs.gotoInstruction(mTarget);
    }

    return NS_OK;
}

nsresult
txComment::execute(txExecutionState& aEs)
{
    nsAutoPtr<txTextHandler> handler(
        static_cast<txTextHandler*>(aEs.popResultHandler()));
    uint32_t length = handler->mValue.Length();
    int32_t pos = 0;
    while ((pos = handler->mValue.FindChar('-', (uint32_t)pos)) != kNotFound) {
        ++pos;
        if ((uint32_t)pos == length || handler->mValue.CharAt(pos) == '-') {
            handler->mValue.Insert(char16_t(' '), pos++);
            ++length;
        }
    }

    return aEs.mResultHandler->comment(handler->mValue);
}

nsresult
txCopyBase::copyNode(const txXPathNode& aNode, txExecutionState& aEs)
{
    switch (txXPathNodeUtils::getNodeType(aNode)) {
        case txXPathNodeType::ATTRIBUTE_NODE:
        {
            nsAutoString nodeValue;
            txXPathNodeUtils::appendNodeValue(aNode, nodeValue);

            RefPtr<nsAtom> localName =
                txXPathNodeUtils::getLocalName(aNode);
            return aEs.mResultHandler->
                attribute(txXPathNodeUtils::getPrefix(aNode),
                          localName, nullptr,
                          txXPathNodeUtils::getNamespaceID(aNode),
                          nodeValue);
        }
        case txXPathNodeType::COMMENT_NODE:
        {
            nsAutoString nodeValue;
            txXPathNodeUtils::appendNodeValue(aNode, nodeValue);
            return aEs.mResultHandler->comment(nodeValue);
        }
        case txXPathNodeType::DOCUMENT_NODE:
        case txXPathNodeType::DOCUMENT_FRAGMENT_NODE:
        {
            // Copy children
            txXPathTreeWalker walker(aNode);
            bool hasChild = walker.moveToFirstChild();
            while (hasChild) {
                copyNode(walker.getCurrentPosition(), aEs);
                hasChild = walker.moveToNextSibling();
            }
            break;
        }
        case txXPathNodeType::ELEMENT_NODE:
        {
            RefPtr<nsAtom> localName =
                txXPathNodeUtils::getLocalName(aNode);
            nsresult rv = aEs.mResultHandler->
                startElement(txXPathNodeUtils::getPrefix(aNode),
                             localName, nullptr,
                             txXPathNodeUtils::getNamespaceID(aNode));
            NS_ENSURE_SUCCESS(rv, rv);

            // Copy attributes
            txXPathTreeWalker walker(aNode);
            if (walker.moveToFirstAttribute()) {
                do {
                    nsAutoString nodeValue;
                    walker.appendNodeValue(nodeValue);

                    const txXPathNode& attr = walker.getCurrentPosition();
                    localName = txXPathNodeUtils::getLocalName(attr);
                    rv = aEs.mResultHandler->
                        attribute(txXPathNodeUtils::getPrefix(attr),
                                  localName, nullptr,
                                  txXPathNodeUtils::getNamespaceID(attr),
                                  nodeValue);
                    NS_ENSURE_SUCCESS(rv, rv);
                } while (walker.moveToNextAttribute());
                walker.moveToParent();
            }

            // Copy children
            bool hasChild = walker.moveToFirstChild();
            while (hasChild) {
                copyNode(walker.getCurrentPosition(), aEs);
                hasChild = walker.moveToNextSibling();
            }

            return aEs.mResultHandler->endElement();
        }
        case txXPathNodeType::PROCESSING_INSTRUCTION_NODE:
        {
            nsAutoString target, data;
            txXPathNodeUtils::getNodeName(aNode, target);
            txXPathNodeUtils::appendNodeValue(aNode, data);
            return aEs.mResultHandler->processingInstruction(target, data);
        }
        case txXPathNodeType::TEXT_NODE:
        case txXPathNodeType::CDATA_SECTION_NODE:
        {
            nsAutoString nodeValue;
            txXPathNodeUtils::appendNodeValue(aNode, nodeValue);
            return aEs.mResultHandler->characters(nodeValue, false);
        }
    }

    return NS_OK;
}

txCopy::txCopy()
    : mBailTarget(nullptr)
{
}

nsresult
txCopy::execute(txExecutionState& aEs)
{
    nsresult rv = NS_OK;
    const txXPathNode& node = aEs.getEvalContext()->getContextNode();

    switch (txXPathNodeUtils::getNodeType(node)) {
        case txXPathNodeType::DOCUMENT_NODE:
        case txXPathNodeType::DOCUMENT_FRAGMENT_NODE:
        {
            const nsString& empty = EmptyString();

            // "close" current element to ensure that no attributes are added
            rv = aEs.mResultHandler->characters(empty, false);
            NS_ENSURE_SUCCESS(rv, rv);

            rv = aEs.pushBool(false);
            NS_ENSURE_SUCCESS(rv, rv);

            break;
        }
        case txXPathNodeType::ELEMENT_NODE:
        {
            RefPtr<nsAtom> localName =
                txXPathNodeUtils::getLocalName(node);
            rv = aEs.mResultHandler->
                startElement(txXPathNodeUtils::getPrefix(node),
                             localName, nullptr,
                             txXPathNodeUtils::getNamespaceID(node));
            NS_ENSURE_SUCCESS(rv, rv);

            // XXX copy namespace nodes once we have them

            rv = aEs.pushBool(true);
            NS_ENSURE_SUCCESS(rv, rv);

            break;
        }
        default:
        {
            rv = copyNode(node, aEs);
            NS_ENSURE_SUCCESS(rv, rv);

            aEs.gotoInstruction(mBailTarget);
        }
    }

    return NS_OK;
}

txCopyOf::txCopyOf(nsAutoPtr<Expr>&& aSelect)
    : mSelect(std::move(aSelect))
{
}

nsresult
txCopyOf::execute(txExecutionState& aEs)
{
    RefPtr<txAExprResult> exprRes;
    nsresult rv = mSelect->evaluate(aEs.getEvalContext(),
                                    getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    switch (exprRes->getResultType()) {
        case txAExprResult::NODESET:
        {
            txNodeSet* nodes = static_cast<txNodeSet*>
                                          (static_cast<txAExprResult*>
                                                      (exprRes));
            int32_t i;
            for (i = 0; i < nodes->size(); ++i) {
                rv = copyNode(nodes->get(i), aEs);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            break;
        }
        case txAExprResult::RESULT_TREE_FRAGMENT:
        {
            txResultTreeFragment* rtf =
                static_cast<txResultTreeFragment*>
                           (static_cast<txAExprResult*>(exprRes));
            return rtf->flushToHandler(aEs.mResultHandler);
        }
        default:
        {
            nsAutoString value;
            exprRes->stringValue(value);
            if (!value.IsEmpty()) {
                return aEs.mResultHandler->characters(value, false);
            }
            break;
        }
    }

    return NS_OK;
}

nsresult
txEndElement::execute(txExecutionState& aEs)
{
    // This will return false if startElement was not called. This happens
    // when <xsl:element> produces a bad name, or when <xsl:copy> copies a
    // document node.
    if (aEs.popBool()) {
        return aEs.mResultHandler->endElement();
    }

    return NS_OK;
}

nsresult
txErrorInstruction::execute(txExecutionState& aEs)
{
    // XXX ErrorReport: unknown instruction executed
    return NS_ERROR_XSLT_EXECUTION_FAILURE;
}

txGoTo::txGoTo(txInstruction* aTarget)
    : mTarget(aTarget)
{
}

nsresult
txGoTo::execute(txExecutionState& aEs)
{
    aEs.gotoInstruction(mTarget);

    return NS_OK;
}

txInsertAttrSet::txInsertAttrSet(const txExpandedName& aName)
    : mName(aName)
{
}

nsresult
txInsertAttrSet::execute(txExecutionState& aEs)
{
    txInstruction* instr = aEs.mStylesheet->getAttributeSet(mName);
    NS_ENSURE_TRUE(instr, NS_ERROR_XSLT_EXECUTION_FAILURE);

    nsresult rv = aEs.runTemplate(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

txLoopNodeSet::txLoopNodeSet(txInstruction* aTarget)
    : mTarget(aTarget)
{
}

nsresult
txLoopNodeSet::execute(txExecutionState& aEs)
{
    aEs.popTemplateRule();
    txNodeSetContext* context =
        static_cast<txNodeSetContext*>(aEs.getEvalContext());
    if (!context->hasNext()) {
        delete aEs.popEvalContext();

        return NS_OK;
    }

    context->next();
    aEs.gotoInstruction(mTarget);

    return NS_OK;
}

txLREAttribute::txLREAttribute(int32_t aNamespaceID, nsAtom* aLocalName,
                               nsAtom* aPrefix, nsAutoPtr<Expr>&& aValue)
    : mNamespaceID(aNamespaceID),
      mLocalName(aLocalName),
      mPrefix(aPrefix),
      mValue(std::move(aValue))
{
    if (aNamespaceID == kNameSpaceID_None) {
        mLowercaseLocalName = TX_ToLowerCaseAtom(aLocalName);
    }
}

nsresult
txLREAttribute::execute(txExecutionState& aEs)
{
    RefPtr<txAExprResult> exprRes;
    nsresult rv = mValue->evaluate(aEs.getEvalContext(),
                                   getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    const nsString* value = exprRes->stringValuePointer();
    if (value) {
        return aEs.mResultHandler->attribute(mPrefix, mLocalName,
                                             mLowercaseLocalName,
                                             mNamespaceID, *value);
    }

    nsAutoString valueStr;
    exprRes->stringValue(valueStr);
    return aEs.mResultHandler->attribute(mPrefix, mLocalName,
                                         mLowercaseLocalName,
                                         mNamespaceID, valueStr);
}

txMessage::txMessage(bool aTerminate)
    : mTerminate(aTerminate)
{
}

nsresult
txMessage::execute(txExecutionState& aEs)
{
    nsAutoPtr<txTextHandler> handler(
        static_cast<txTextHandler*>(aEs.popResultHandler()));

    nsCOMPtr<nsIConsoleService> consoleSvc =
      do_GetService("@mozilla.org/consoleservice;1");
    if (consoleSvc) {
        nsAutoString logString(NS_LITERAL_STRING("xsl:message - "));
        logString.Append(handler->mValue);
        consoleSvc->LogStringMessage(logString.get());
    }

    return mTerminate ? NS_ERROR_XSLT_ABORTED : NS_OK;
}

txNumber::txNumber(txXSLTNumber::LevelType aLevel,
                   nsAutoPtr<txPattern>&& aCount, nsAutoPtr<txPattern>&& aFrom,
                   nsAutoPtr<Expr>&& aValue, nsAutoPtr<Expr>&& aFormat,
                   nsAutoPtr<Expr>&& aGroupingSeparator,
                   nsAutoPtr<Expr>&& aGroupingSize)
    : mLevel(aLevel), mCount(std::move(aCount)),
      mFrom(std::move(aFrom)),
      mValue(std::move(aValue)),
      mFormat(std::move(aFormat)),
      mGroupingSeparator(std::move(aGroupingSeparator)),
      mGroupingSize(std::move(aGroupingSize))
{
}

nsresult
txNumber::execute(txExecutionState& aEs)
{
    nsAutoString res;
    nsresult rv =
        txXSLTNumber::createNumber(mValue, mCount, mFrom, mLevel, mGroupingSize,
                                   mGroupingSeparator, mFormat,
                                   aEs.getEvalContext(), res);
    NS_ENSURE_SUCCESS(rv, rv);

    return aEs.mResultHandler->characters(res, false);
}

nsresult
txPopParams::execute(txExecutionState& aEs)
{
    RefPtr<txParameterMap> paramMap = aEs.popParamMap();

    return NS_OK;
}

txProcessingInstruction::txProcessingInstruction(nsAutoPtr<Expr>&& aName)
    : mName(std::move(aName))
{
}

nsresult
txProcessingInstruction::execute(txExecutionState& aEs)
{
    nsAutoPtr<txTextHandler> handler(
        static_cast<txTextHandler*>(aEs.popResultHandler()));
    XMLUtils::normalizePIValue(handler->mValue);

    nsAutoString name;
    nsresult rv = mName->evaluateToString(aEs.getEvalContext(), name);
    NS_ENSURE_SUCCESS(rv, rv);

    // Check name validity (must be valid NCName and a PITarget)
    // XXX Need to check for NCName and PITarget
    const char16_t* colon;
    if (!XMLUtils::isValidQName(name, &colon)) {
        // XXX ErrorReport: bad PI-target
        return NS_ERROR_FAILURE;
    }

    return aEs.mResultHandler->processingInstruction(name, handler->mValue);
}

txPushNewContext::txPushNewContext(nsAutoPtr<Expr>&& aSelect)
    : mSelect(std::move(aSelect)), mBailTarget(nullptr)
{
}

txPushNewContext::~txPushNewContext()
{
}

nsresult
txPushNewContext::execute(txExecutionState& aEs)
{
    RefPtr<txAExprResult> exprRes;
    nsresult rv = mSelect->evaluate(aEs.getEvalContext(),
                                    getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    if (exprRes->getResultType() != txAExprResult::NODESET) {
        // XXX ErrorReport: nodeset expected
        return NS_ERROR_XSLT_NODESET_EXPECTED;
    }

    txNodeSet* nodes = static_cast<txNodeSet*>
                                  (static_cast<txAExprResult*>
                                              (exprRes));

    if (nodes->isEmpty()) {
        aEs.gotoInstruction(mBailTarget);

        return NS_OK;
    }

    txNodeSorter sorter;
    uint32_t i, count = mSortKeys.Length();
    for (i = 0; i < count; ++i) {
        SortKey& sort = mSortKeys[i];
        rv = sorter.addSortElement(sort.mSelectExpr, sort.mLangExpr,
                                   sort.mDataTypeExpr, sort.mOrderExpr,
                                   sort.mCaseOrderExpr,
                                   aEs.getEvalContext());
        NS_ENSURE_SUCCESS(rv, rv);
    }
    RefPtr<txNodeSet> sortedNodes;
    rv = sorter.sortNodeSet(nodes, &aEs, getter_AddRefs(sortedNodes));
    NS_ENSURE_SUCCESS(rv, rv);

    txNodeSetContext* context = new txNodeSetContext(sortedNodes, &aEs);
    NS_ENSURE_TRUE(context, NS_ERROR_OUT_OF_MEMORY);

    context->next();

    rv = aEs.pushEvalContext(context);
    if (NS_FAILED(rv)) {
        delete context;
        return rv;
    }

    return NS_OK;
}

nsresult
txPushNewContext::addSort(nsAutoPtr<Expr>&& aSelectExpr,
                          nsAutoPtr<Expr>&& aLangExpr,
                          nsAutoPtr<Expr>&& aDataTypeExpr,
                          nsAutoPtr<Expr>&& aOrderExpr,
                          nsAutoPtr<Expr>&& aCaseOrderExpr)
{
    if (SortKey *key = mSortKeys.AppendElement()) {
        // workaround for not triggering the Copy Constructor
        key->mSelectExpr = std::move(aSelectExpr);
        key->mLangExpr = std::move(aLangExpr);
        key->mDataTypeExpr = std::move(aDataTypeExpr);
        key->mOrderExpr = std::move(aOrderExpr);
        key->mCaseOrderExpr = std::move(aCaseOrderExpr);
        return NS_OK;
    }
    return NS_ERROR_OUT_OF_MEMORY;
}

nsresult
txPushNullTemplateRule::execute(txExecutionState& aEs)
{
    aEs.pushTemplateRule(nullptr, txExpandedName(), nullptr);
    return NS_OK;
}

nsresult
txPushParams::execute(txExecutionState& aEs)
{
    aEs.pushParamMap(nullptr);
    return NS_OK;
}

nsresult
txPushRTFHandler::execute(txExecutionState& aEs)
{
    txAXMLEventHandler* handler = new txRtfHandler;
    nsresult rv = aEs.pushResultHandler(handler);
    if (NS_FAILED(rv)) {
        delete handler;
        return rv;
    }

    return NS_OK;
}

txPushStringHandler::txPushStringHandler(bool aOnlyText)
    : mOnlyText(aOnlyText)
{
}

nsresult
txPushStringHandler::execute(txExecutionState& aEs)
{
    txAXMLEventHandler* handler = new txTextHandler(mOnlyText);
    nsresult rv = aEs.pushResultHandler(handler);
    if (NS_FAILED(rv)) {
        delete handler;
        return rv;
    }

    return NS_OK;
}

txRemoveVariable::txRemoveVariable(const txExpandedName& aName)
    : mName(aName)
{
}

nsresult
txRemoveVariable::execute(txExecutionState& aEs)
{
    aEs.removeVariable(mName);

    return NS_OK;
}

nsresult
txReturn::execute(txExecutionState& aEs)
{
    NS_ASSERTION(!mNext, "instructions exist after txReturn");
    aEs.returnFromTemplate();

    return NS_OK;
}

txSetParam::txSetParam(const txExpandedName& aName, nsAutoPtr<Expr>&& aValue)
    : mName(aName), mValue(std::move(aValue))
{
}

nsresult
txSetParam::execute(txExecutionState& aEs)
{
    nsresult rv = NS_OK;
    if (!aEs.mTemplateParams) {
        aEs.mTemplateParams = new txParameterMap;
    }

    RefPtr<txAExprResult> exprRes;
    if (mValue) {
        rv = mValue->evaluate(aEs.getEvalContext(),
                              getter_AddRefs(exprRes));
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        nsAutoPtr<txRtfHandler> rtfHandler(
            static_cast<txRtfHandler*>(aEs.popResultHandler()));
        rv = rtfHandler->getAsRTF(getter_AddRefs(exprRes));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = aEs.mTemplateParams->bindVariable(mName, exprRes);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

txSetVariable::txSetVariable(const txExpandedName& aName,
                             nsAutoPtr<Expr>&& aValue)
    : mName(aName), mValue(std::move(aValue))
{
}

nsresult
txSetVariable::execute(txExecutionState& aEs)
{
    nsresult rv = NS_OK;
    RefPtr<txAExprResult> exprRes;
    if (mValue) {
        rv = mValue->evaluate(aEs.getEvalContext(), getter_AddRefs(exprRes));
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        nsAutoPtr<txRtfHandler> rtfHandler(
            static_cast<txRtfHandler*>(aEs.popResultHandler()));
        rv = rtfHandler->getAsRTF(getter_AddRefs(exprRes));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return aEs.bindVariable(mName, exprRes);
}

txStartElement::txStartElement(nsAutoPtr<Expr>&& aName,
                               nsAutoPtr<Expr>&& aNamespace,
                               txNamespaceMap* aMappings)
    : mName(std::move(aName)),
      mNamespace(std::move(aNamespace)),
      mMappings(aMappings)
{
}

nsresult
txStartElement::execute(txExecutionState& aEs)
{
    nsAutoString name;
    nsresult rv = mName->evaluateToString(aEs.getEvalContext(), name);
    NS_ENSURE_SUCCESS(rv, rv);


    int32_t nsId = kNameSpaceID_None;
    RefPtr<nsAtom> prefix;
    uint32_t lnameStart = 0;

    const char16_t* colon;
    if (XMLUtils::isValidQName(name, &colon)) {
        if (colon) {
            prefix = NS_Atomize(Substring(name.get(), colon));
            lnameStart = colon - name.get() + 1;
        }

        if (mNamespace) {
            nsAutoString nspace;
            rv = mNamespace->evaluateToString(aEs.getEvalContext(),
                                              nspace);
            NS_ENSURE_SUCCESS(rv, rv);

            if (!nspace.IsEmpty()) {
                nsId = txNamespaceManager::getNamespaceID(nspace);
            }
        }
        else {
            nsId = mMappings->lookupNamespace(prefix);
        }
    }
    else {
       nsId = kNameSpaceID_Unknown;
    }

    bool success = true;

    if (nsId != kNameSpaceID_Unknown) {
        rv = aEs.mResultHandler->startElement(prefix,
                                              Substring(name, lnameStart),
                                              nsId);
    }
    else {
        rv = NS_ERROR_XSLT_BAD_NODE_NAME;
    }

    if (rv == NS_ERROR_XSLT_BAD_NODE_NAME) {
        success = false;
        // we call characters with an empty string to "close" any element to
        // make sure that no attributes are added
        rv = aEs.mResultHandler->characters(EmptyString(), false);
    }
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aEs.pushBool(success);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}


txStartLREElement::txStartLREElement(int32_t aNamespaceID,
                                     nsAtom* aLocalName,
                                     nsAtom* aPrefix)
    : mNamespaceID(aNamespaceID),
      mLocalName(aLocalName),
      mPrefix(aPrefix)
{
    if (aNamespaceID == kNameSpaceID_None) {
        mLowercaseLocalName = TX_ToLowerCaseAtom(aLocalName);
    }
}

nsresult
txStartLREElement::execute(txExecutionState& aEs)
{
    nsresult rv = aEs.mResultHandler->startElement(mPrefix, mLocalName,
                                                   mLowercaseLocalName,
                                                   mNamespaceID);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aEs.pushBool(true);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

txText::txText(const nsAString& aStr, bool aDOE)
    : mStr(aStr),
      mDOE(aDOE)
{
}

nsresult
txText::execute(txExecutionState& aEs)
{
    return aEs.mResultHandler->characters(mStr, mDOE);
}

txValueOf::txValueOf(nsAutoPtr<Expr>&& aExpr, bool aDOE)
    : mExpr(std::move(aExpr)),
      mDOE(aDOE)
{
}

nsresult
txValueOf::execute(txExecutionState& aEs)
{
    RefPtr<txAExprResult> exprRes;
    nsresult rv = mExpr->evaluate(aEs.getEvalContext(),
                                  getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    const nsString* value = exprRes->stringValuePointer();
    if (value) {
        if (!value->IsEmpty()) {
            return aEs.mResultHandler->characters(*value, mDOE);
        }
    }
    else {
        nsAutoString valueStr;
        exprRes->stringValue(valueStr);
        if (!valueStr.IsEmpty()) {
            return aEs.mResultHandler->characters(valueStr, mDOE);
        }
    }

    return NS_OK;
}
