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
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc>
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

#include "txInstructions.h"
#include "txError.h"
#include "Expr.h"
#include "ExprResult.h"
#include "txStylesheet.h"
#include "txNodeSetContext.h"
#include "txTextHandler.h"
#include "nsIConsoleService.h"
#include "nsIServiceManagerUtils.h"
#include "txStringUtils.h"
#include "txAtoms.h"
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
    txInstruction* templ =
        aEs.mStylesheet->findTemplate(aEs.getEvalContext()->getContextNode(),
                                      mode, &aEs, nsnull, &frame);

    nsresult rv = aEs.pushTemplateRule(frame, mode, aEs.mTemplateParams);
    NS_ENSURE_SUCCESS(rv, rv);

    return aEs.runTemplate(templ);
}

nsresult
txApplyImportsEnd::execute(txExecutionState& aEs)
{
    aEs.popTemplateRule();
    aEs.popParamMap();
    
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

    nsresult rv = aEs.pushParamMap(rule->mParams);
    NS_ENSURE_SUCCESS(rv, rv);

    txStylesheet::ImportFrame* frame = 0;
    txExpandedName mode(rule->mModeNsId, rule->mModeLocalName);
    txInstruction* templ =
        aEs.mStylesheet->findTemplate(aEs.getEvalContext()->getContextNode(),
                                      mode, &aEs, rule->mFrame, &frame);

    rv = aEs.pushTemplateRule(frame, mode, rule->mParams);
    NS_ENSURE_SUCCESS(rv, rv);

    return aEs.runTemplate(templ);
}

txApplyTemplates::txApplyTemplates(const txExpandedName& aMode)
    : mMode(aMode)
{
}

nsresult
txApplyTemplates::execute(txExecutionState& aEs)
{
    txStylesheet::ImportFrame* frame = 0;
    txInstruction* templ =
        aEs.mStylesheet->findTemplate(aEs.getEvalContext()->getContextNode(),
                                      mMode, &aEs, nsnull, &frame);

    nsresult rv = aEs.pushTemplateRule(frame, mMode, aEs.mTemplateParams);
    NS_ENSURE_SUCCESS(rv, rv);

    return aEs.runTemplate(templ);
}

txAttribute::txAttribute(nsAutoPtr<Expr> aName, nsAutoPtr<Expr> aNamespace,
                         txNamespaceMap* aMappings)
    : mName(aName),
      mNamespace(aNamespace),
      mMappings(aMappings)
{
}

nsresult
txAttribute::execute(txExecutionState& aEs)
{
    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = mName->evaluate(aEs.getEvalContext(),
                                  getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString name;
    exprRes->stringValue(name);

    const PRUnichar* colon;
    if (!XMLUtils::isValidQName(name, &colon) ||
        TX_StringEqualsAtom(name, txXMLAtoms::xmlns)) {
        // truncate name to indicate failure
        name.Truncate();
    }

    nsCOMPtr<nsIAtom> prefix;
    if (colon) {
        prefix = do_GetAtom(Substring(name.get(), colon));
    }

    PRInt32 nsId = kNameSpaceID_None;
    if (!name.IsEmpty()) {
        if (mNamespace) {
            rv = mNamespace->evaluate(aEs.getEvalContext(),
                                      getter_AddRefs(exprRes));
            NS_ENSURE_SUCCESS(rv, rv);

            nsAutoString nspace;
            exprRes->stringValue(nspace);

            if (!nspace.IsEmpty()) {
                nsId = txNamespaceManager::getNamespaceID(nspace);
                NS_ENSURE_FALSE(nsId == kNameSpaceID_Unknown,
                                NS_ERROR_FAILURE);
            }
        }
        else if (prefix) {
            nsId = mMappings->lookupNamespace(prefix);
            if (nsId == kNameSpaceID_Unknown) {
                // tunkate name to indicate failure
                name.Truncate();
            }
        }
    }

    if (prefix == txXMLAtoms::xmlns) {
        // Cut xmlns: (6 characters)
        name.Cut(0, 6);
    }

    nsAutoPtr<txTextHandler> handler(
        NS_STATIC_CAST(txTextHandler*, aEs.popResultHandler()));
    if (!name.IsEmpty()) {
        // add attribute if everything was ok
        aEs.mResultHandler->attribute(name, nsId, handler->mValue);
    }

    return NS_OK;
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
    : mName(aName), mBailTarget(nsnull)
{
}

nsresult
txCheckParam::execute(txExecutionState& aEs)
{
    nsresult rv = NS_OK;
    if (aEs.mTemplateParams) {
        nsRefPtr<txAExprResult> exprRes;
        aEs.mTemplateParams->getVariable(mName, getter_AddRefs(exprRes));
        if (exprRes) {
            rv = aEs.bindVariable(mName, exprRes);
            NS_ENSURE_SUCCESS(rv, rv);

            aEs.gotoInstruction(mBailTarget);
        }
    }
    
    return NS_OK;
}

txConditionalGoto::txConditionalGoto(nsAutoPtr<Expr> aCondition,
                                     txInstruction* aTarget)
    : mCondition(aCondition),
      mTarget(aTarget)
{
}

nsresult
txConditionalGoto::execute(txExecutionState& aEs)
{
    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = mCondition->evaluate(aEs.getEvalContext(),
                                       getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!exprRes->booleanValue()) {
        aEs.gotoInstruction(mTarget);
    }

    return NS_OK;
}

nsresult
txComment::execute(txExecutionState& aEs)
{
    nsAutoPtr<txTextHandler> handler(
        NS_STATIC_CAST(txTextHandler*, aEs.popResultHandler()));
    PRUint32 length = handler->mValue.Length();
    PRInt32 pos = 0;
    while ((pos = handler->mValue.FindChar('-', (PRUint32)pos)) != kNotFound) {
        ++pos;
        if ((PRUint32)pos == length || handler->mValue.CharAt(pos) == '-') {
            handler->mValue.Insert(PRUnichar(' '), pos++);
            ++length;
        }
    }

    aEs.mResultHandler->comment(handler->mValue);

    return NS_OK;
}

nsresult
txCopyBase::copyNode(const txXPathNode& aNode, txExecutionState& aEs)
{
    switch (txXPathNodeUtils::getNodeType(aNode)) {
        case txXPathNodeType::ATTRIBUTE_NODE:
        {
            nsAutoString nodeName, nodeValue;
            txXPathNodeUtils::getNodeName(aNode, nodeName);
            txXPathNodeUtils::appendNodeValue(aNode, nodeValue);
            aEs.mResultHandler->attribute(nodeName,
                                          txXPathNodeUtils::getNamespaceID(aNode),
                                          nodeValue);
            break;
        }
        case txXPathNodeType::COMMENT_NODE:
        {
            nsAutoString nodeValue;
            txXPathNodeUtils::appendNodeValue(aNode, nodeValue);
            aEs.mResultHandler->comment(nodeValue);
            break;
        }
        case txXPathNodeType::DOCUMENT_NODE:
        case txXPathNodeType::DOCUMENT_FRAGMENT_NODE:
        {
            // Copy children
            txXPathTreeWalker walker(aNode);
            PRBool hasChild = walker.moveToFirstChild();
            while (hasChild) {
                copyNode(walker.getCurrentPosition(), aEs);
                hasChild = walker.moveToNextSibling();
            }
            break;
        }
        case txXPathNodeType::ELEMENT_NODE:
        {
            nsAutoString name;
            txXPathNodeUtils::getNodeName(aNode, name);
            PRInt32 nsID = txXPathNodeUtils::getNamespaceID(aNode);
            aEs.mResultHandler->startElement(name, nsID);

            // Copy attributes
            txXPathTreeWalker walker(aNode);
            if (walker.moveToFirstAttribute()) {
                do {
                    nsAutoString nodeName, nodeValue;
                    walker.getNodeName(nodeName);
                    walker.appendNodeValue(nodeValue);
                    aEs.mResultHandler->attribute(nodeName,
                                                  walker.getNamespaceID(),
                                                  nodeValue);
                } while (walker.moveToNextAttribute());
                walker.moveToParent();
            }

            // Copy children
            PRBool hasChild = walker.moveToFirstChild();
            while (hasChild) {
                copyNode(walker.getCurrentPosition(), aEs);
                hasChild = walker.moveToNextSibling();
            }

            aEs.mResultHandler->endElement(name, nsID);
            break;
        }
        case txXPathNodeType::PROCESSING_INSTRUCTION_NODE:
        {
            nsAutoString target, data;
            txXPathNodeUtils::getNodeName(aNode, target);
            txXPathNodeUtils::appendNodeValue(aNode, data);
            aEs.mResultHandler->processingInstruction(target, data);
            break;
        }
        case txXPathNodeType::TEXT_NODE:
        case txXPathNodeType::CDATA_SECTION_NODE:
        {
            nsAutoString nodeValue;
            txXPathNodeUtils::appendNodeValue(aNode, nodeValue);
            aEs.mResultHandler->characters(nodeValue, PR_FALSE);
            break;
        }
    }
    
    return NS_OK;
}

txCopy::txCopy()
    : mBailTarget(nsnull)
{
}

nsresult
txCopy::execute(txExecutionState& aEs)
{
    nsresult rv = NS_OK;
    const txXPathNode& node = aEs.getEvalContext()->getContextNode();

    switch (txXPathNodeUtils::getNodeType(node)) {
        case txXPathNodeType::DOCUMENT_NODE:
        {
            const nsAFlatString& empty = EmptyString();

            // "close" current element to ensure that no attributes are added
            aEs.mResultHandler->characters(empty, PR_FALSE);

            rv = aEs.pushString(empty);
            NS_ENSURE_SUCCESS(rv, rv);

            rv = aEs.pushInt(kNameSpaceID_None);
            NS_ENSURE_SUCCESS(rv, rv);

            break;
        }
        case txXPathNodeType::ELEMENT_NODE:
        {
            nsAutoString nodeName;
            txXPathNodeUtils::getNodeName(node, nodeName);
            PRInt32 nsID = txXPathNodeUtils::getNamespaceID(node);

            aEs.mResultHandler->startElement(nodeName, nsID);
            // XXX copy namespace nodes once we have them

            rv = aEs.pushString(nodeName);
            NS_ENSURE_SUCCESS(rv, rv);

            rv = aEs.pushInt(nsID);
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

txCopyOf::txCopyOf(nsAutoPtr<Expr> aSelect)
    : mSelect(aSelect)
{
}

nsresult
txCopyOf::execute(txExecutionState& aEs)
{
    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = mSelect->evaluate(aEs.getEvalContext(),
                                    getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    switch (exprRes->getResultType()) {
        case txAExprResult::NODESET:
        {
            txNodeSet* nodes = NS_STATIC_CAST(txNodeSet*,
                                              NS_STATIC_CAST(txAExprResult*,
                                                             exprRes));
            PRInt32 i;
            for (i = 0; i < nodes->size(); ++i) {
                rv = copyNode(nodes->get(i), aEs);
                NS_ENSURE_SUCCESS(rv, rv);
            }
            break;
        }
        case txAExprResult::RESULT_TREE_FRAGMENT:
        {
            txResultTreeFragment* rtf =
                NS_STATIC_CAST(txResultTreeFragment*,
                               NS_STATIC_CAST(txAExprResult*, exprRes));
            rv = rtf->flushToHandler(aEs.mResultHandler);
            NS_ENSURE_SUCCESS(rv, rv);

            break;
        }
        default:
        {
            nsAutoString value;
            exprRes->stringValue(value);
            if (!value.IsEmpty()) {
                aEs.mResultHandler->characters(value, PR_FALSE);
            }
            break;
        }
    }
    
    return NS_OK;
}

nsresult
txEndElement::execute(txExecutionState& aEs)
{
    PRInt32 namespaceID = aEs.popInt();
    nsAutoString nodeName;
    aEs.popString(nodeName);
    
    
    // For xsl:elements with a bad name we push an empty name
    if (!nodeName.IsEmpty()) {
        aEs.mResultHandler->endElement(nodeName, namespaceID);
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
        NS_STATIC_CAST(txNodeSetContext*, aEs.getEvalContext());
    if (!context->hasNext()) {
        delete aEs.popEvalContext();

        return NS_OK;
    }

    context->next();
    aEs.gotoInstruction(mTarget);
    
    return NS_OK;
}

txLREAttribute::txLREAttribute(PRInt32 aNamespaceID, nsIAtom* aLocalName,
                               nsIAtom* aPrefix, nsAutoPtr<Expr> aValue)
    : mNamespaceID(aNamespaceID),
      mLocalName(aLocalName),
      mPrefix(aPrefix),
      mValue(aValue)
{
}

nsresult
txLREAttribute::execute(txExecutionState& aEs)
{
    // We should atomize the resulthandler
    nsAutoString nodeName;
    if (mPrefix) {
        mPrefix->ToString(nodeName);
        nsAutoString localName;
        nodeName.Append(PRUnichar(':'));
        mLocalName->ToString(localName);
        nodeName.Append(localName);
    }
    else {
        mLocalName->ToString(nodeName);
    }

    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = mValue->evaluate(aEs.getEvalContext(),
                                   getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAString* value = exprRes->stringValuePointer();
    if (value) {
        aEs.mResultHandler->attribute(nodeName, mNamespaceID, *value);
    }
    else {
        nsAutoString valueStr;
        exprRes->stringValue(valueStr);
        aEs.mResultHandler->attribute(nodeName, mNamespaceID, valueStr);
    }

    return NS_OK;
}

txMessage::txMessage(PRBool aTerminate)
    : mTerminate(aTerminate)
{
}

nsresult
txMessage::execute(txExecutionState& aEs)
{
    nsAutoPtr<txTextHandler> handler(
        NS_STATIC_CAST(txTextHandler*, aEs.popResultHandler()));

    nsCOMPtr<nsIConsoleService> consoleSvc = 
      do_GetService("@mozilla.org/consoleservice;1");
    if (consoleSvc) {
        nsAutoString logString(NS_LITERAL_STRING("xsl:message - "));
        logString.Append(handler->mValue);
        consoleSvc->LogStringMessage(logString.get());
    }

    return mTerminate ? NS_ERROR_XSLT_ABORTED : NS_OK;
}

txNumber::txNumber(txXSLTNumber::LevelType aLevel, nsAutoPtr<txPattern> aCount,
                   nsAutoPtr<txPattern> aFrom, nsAutoPtr<Expr> aValue,
                   nsAutoPtr<Expr> aFormat, nsAutoPtr<Expr> aGroupingSeparator,
                   nsAutoPtr<Expr> aGroupingSize)
    : mLevel(aLevel), mCount(aCount), mFrom(aFrom), mValue(aValue),
      mFormat(aFormat), mGroupingSeparator(aGroupingSeparator),
      mGroupingSize(aGroupingSize)
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
    
    aEs.mResultHandler->characters(res, PR_FALSE);

    return NS_OK;
}

nsresult
txPopParams::execute(txExecutionState& aEs)
{
    delete aEs.popParamMap();

    return NS_OK;
}

txProcessingInstruction::txProcessingInstruction(nsAutoPtr<Expr> aName)
    : mName(aName)
{
}

nsresult
txProcessingInstruction::execute(txExecutionState& aEs)
{
    nsAutoPtr<txTextHandler> handler(
        NS_STATIC_CAST(txTextHandler*, aEs.popResultHandler()));
    XMLUtils::normalizePIValue(handler->mValue);

    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = mName->evaluate(aEs.getEvalContext(),
                                  getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString name;
    exprRes->stringValue(name);

    // Check name validity (must be valid NCName and a PITarget)
    // XXX Need to check for NCName and PITarget
    const PRUnichar* colon;
    if (!XMLUtils::isValidQName(name, &colon)) {
        // XXX ErrorReport: bad PI-target
        return NS_ERROR_FAILURE;
    }

    aEs.mResultHandler->processingInstruction(name, handler->mValue);

    return NS_OK;
}

txPushNewContext::txPushNewContext(nsAutoPtr<Expr> aSelect)
    : mSelect(aSelect), mBailTarget(nsnull)
{
}

txPushNewContext::~txPushNewContext()
{
    PRInt32 i;
    for (i = 0; i < mSortKeys.Count(); ++i)
    {
        delete NS_STATIC_CAST(SortKey*, mSortKeys[i]);
    }
}

nsresult
txPushNewContext::execute(txExecutionState& aEs)
{
    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = mSelect->evaluate(aEs.getEvalContext(),
                                    getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    if (exprRes->getResultType() != txAExprResult::NODESET) {
        // XXX ErrorReport: nodeset expected
        return NS_ERROR_XSLT_NODESET_EXPECTED;
    }
    
    txNodeSet* nodes = NS_STATIC_CAST(txNodeSet*,
                                      NS_STATIC_CAST(txAExprResult*,
                                                     exprRes));
    
    if (nodes->isEmpty()) {
        aEs.gotoInstruction(mBailTarget);
        
        return NS_OK;
    }

    txNodeSorter sorter;
    PRInt32 i, count = mSortKeys.Count();
    for (i = 0; i < count; ++i) {
        SortKey* sort = NS_STATIC_CAST(SortKey*, mSortKeys[i]);
        rv = sorter.addSortElement(sort->mSelectExpr, sort->mLangExpr,
                                   sort->mDataTypeExpr, sort->mOrderExpr,
                                   sort->mCaseOrderExpr,
                                   aEs.getEvalContext());
        NS_ENSURE_SUCCESS(rv, rv);
    }
    nsRefPtr<txNodeSet> sortedNodes;
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
txPushNewContext::addSort(nsAutoPtr<Expr> aSelectExpr,
                          nsAutoPtr<Expr> aLangExpr,
                          nsAutoPtr<Expr> aDataTypeExpr,
                          nsAutoPtr<Expr> aOrderExpr,
                          nsAutoPtr<Expr> aCaseOrderExpr)
{
    SortKey* sort = new SortKey(aSelectExpr, aLangExpr, aDataTypeExpr,
                                aOrderExpr, aCaseOrderExpr);
    NS_ENSURE_TRUE(sort, NS_ERROR_OUT_OF_MEMORY);

    if (!mSortKeys.AppendElement(sort)) {
        delete sort;
        return NS_ERROR_OUT_OF_MEMORY;
    }
   
    return NS_OK;
}

txPushNewContext::SortKey::SortKey(nsAutoPtr<Expr> aSelectExpr,
                                   nsAutoPtr<Expr> aLangExpr,
                                   nsAutoPtr<Expr> aDataTypeExpr,
                                   nsAutoPtr<Expr> aOrderExpr,
                                   nsAutoPtr<Expr> aCaseOrderExpr)
    : mSelectExpr(aSelectExpr), mLangExpr(aLangExpr),
      mDataTypeExpr(aDataTypeExpr), mOrderExpr(aOrderExpr),
      mCaseOrderExpr(aCaseOrderExpr)
{
}

nsresult
txPushNullTemplateRule::execute(txExecutionState& aEs)
{
    return aEs.pushTemplateRule(nsnull, txExpandedName(), nsnull);
}

nsresult
txPushParams::execute(txExecutionState& aEs)
{
    return aEs.pushParamMap(nsnull);
}

nsresult
txPushRTFHandler::execute(txExecutionState& aEs)
{
    txAXMLEventHandler* handler = new txRtfHandler;
    NS_ENSURE_TRUE(handler, NS_ERROR_OUT_OF_MEMORY);
    
    nsresult rv = aEs.pushResultHandler(handler);
    if (NS_FAILED(rv)) {
        delete handler;
        return rv;
    }

    return NS_OK;
}

txPushStringHandler::txPushStringHandler(PRBool aOnlyText)
    : mOnlyText(aOnlyText)
{
}

nsresult
txPushStringHandler::execute(txExecutionState& aEs)
{
    txAXMLEventHandler* handler = new txTextHandler(mOnlyText);
    NS_ENSURE_TRUE(handler, NS_ERROR_OUT_OF_MEMORY);
    
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

txSetParam::txSetParam(const txExpandedName& aName, nsAutoPtr<Expr> aValue)
    : mName(aName), mValue(aValue)
{
}

nsresult
txSetParam::execute(txExecutionState& aEs)
{
    nsresult rv = NS_OK;
    if (!aEs.mTemplateParams) {
        aEs.mTemplateParams = new txVariableMap;
        NS_ENSURE_TRUE(aEs.mTemplateParams, NS_ERROR_OUT_OF_MEMORY);
    }

    nsRefPtr<txAExprResult> exprRes;
    if (mValue) {
        rv = mValue->evaluate(aEs.getEvalContext(),
                              getter_AddRefs(exprRes));
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        nsAutoPtr<txRtfHandler> rtfHandler(
            NS_STATIC_CAST(txRtfHandler*, aEs.popResultHandler()));
        rv = rtfHandler->getAsRTF(getter_AddRefs(exprRes));
        NS_ENSURE_SUCCESS(rv, rv);
    }
    
    rv = aEs.mTemplateParams->bindVariable(mName, exprRes);
    NS_ENSURE_SUCCESS(rv, rv);
    
    return NS_OK;
}

txSetVariable::txSetVariable(const txExpandedName& aName,
                             nsAutoPtr<Expr> aValue)
    : mName(aName), mValue(aValue)
{
}

nsresult
txSetVariable::execute(txExecutionState& aEs)
{
    nsresult rv = NS_OK;
    nsRefPtr<txAExprResult> exprRes;
    if (mValue) {
        rv = mValue->evaluate(aEs.getEvalContext(), getter_AddRefs(exprRes));
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        nsAutoPtr<txRtfHandler> rtfHandler(
            NS_STATIC_CAST(txRtfHandler*, aEs.popResultHandler()));
        rv = rtfHandler->getAsRTF(getter_AddRefs(exprRes));
        NS_ENSURE_SUCCESS(rv, rv);
    }
    
    return aEs.bindVariable(mName, exprRes);
}

txStartElement::txStartElement(nsAutoPtr<Expr> aName,
                               nsAutoPtr<Expr> aNamespace,
                               txNamespaceMap* aMappings)
    : mName(aName),
      mNamespace(aNamespace),
      mMappings(aMappings)
{
}

nsresult
txStartElement::execute(txExecutionState& aEs)
{
    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = mName->evaluate(aEs.getEvalContext(),
                                  getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString name;
    exprRes->stringValue(name);

    const PRUnichar* colon;
    if (!XMLUtils::isValidQName(name, &colon)) {
        // tunkate name to indicate failure
        name.Truncate();
    }

    PRInt32 nsId = kNameSpaceID_None;
    if (!name.IsEmpty()) {
        if (mNamespace) {
            rv = mNamespace->evaluate(aEs.getEvalContext(),
                                      getter_AddRefs(exprRes));
            NS_ENSURE_SUCCESS(rv, rv);

            nsAutoString nspace;
            exprRes->stringValue(nspace);

            if (!nspace.IsEmpty()) {
                nsId = txNamespaceManager::getNamespaceID(nspace);
                NS_ENSURE_FALSE(nsId == kNameSpaceID_Unknown,
                                NS_ERROR_FAILURE);
            }
        }
        else {
            nsCOMPtr<nsIAtom> prefix;
            if (colon) {
                prefix = do_GetAtom(Substring(name.get(), colon));
            }
            nsId = mMappings->lookupNamespace(prefix);
            if (nsId == kNameSpaceID_Unknown) {
                // truncate name to indicate failure
                name.Truncate();
            }
        }
    }

    if (!name.IsEmpty()) {
        // add element if everything was ok
        aEs.mResultHandler->startElement(name, nsId);
    }
    else {
        // we call characters with an empty string to "close" any element to
        // make sure that no attributes are added
        aEs.mResultHandler->characters(EmptyString(), PR_FALSE);
    }

    rv = aEs.pushString(name);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aEs.pushInt(nsId);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}


txStartLREElement::txStartLREElement(PRInt32 aNamespaceID,
                                     nsIAtom* aLocalName,
                                     nsIAtom* aPrefix)
    : mNamespaceID(aNamespaceID),
      mLocalName(aLocalName),
      mPrefix(aPrefix)
{
}

nsresult
txStartLREElement::execute(txExecutionState& aEs)
{
    // We should atomize the resulthandler
    nsAutoString nodeName;
    if (mPrefix) {
        mPrefix->ToString(nodeName);
        nsAutoString localName;
        nodeName.Append(PRUnichar(':'));
        mLocalName->ToString(localName);
        nodeName.Append(localName);
    }
    else {
        mLocalName->ToString(nodeName);
    }

    aEs.mResultHandler->startElement(nodeName, mNamespaceID);

    nsresult rv = aEs.pushString(nodeName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aEs.pushInt(mNamespaceID);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

txText::txText(const nsAString& aStr, PRBool aDOE)
    : mStr(aStr),
      mDOE(aDOE)
{
}

nsresult
txText::execute(txExecutionState& aEs)
{
    aEs.mResultHandler->characters(mStr, mDOE);
    return NS_OK;
}

txValueOf::txValueOf(nsAutoPtr<Expr> aExpr, PRBool aDOE)
    : mExpr(aExpr),
      mDOE(aDOE)
{
}

nsresult
txValueOf::execute(txExecutionState& aEs)
{
    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = mExpr->evaluate(aEs.getEvalContext(),
                                  getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAString* value = exprRes->stringValuePointer();
    if (value) {
        if (!value->IsEmpty()) {
            aEs.mResultHandler->characters(*value, mDOE);
        }
    }
    else {
        nsAutoString valueStr;
        exprRes->stringValue(valueStr);
        if (!valueStr.IsEmpty()) {
            aEs.mResultHandler->characters(valueStr, mDOE);
        }
    }

    return NS_OK;
}
