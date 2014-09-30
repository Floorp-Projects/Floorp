/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"
#include "mozilla/Move.h"

#include "txStylesheet.h"
#include "txExpr.h"
#include "txXSLTPatterns.h"
#include "txToplevelItems.h"
#include "txInstructions.h"
#include "txXSLTFunctions.h"
#include "txLog.h"
#include "txKey.h"
#include "txXPathTreeWalker.h"

using mozilla::Move;

txStylesheet::txStylesheet()
    : mRootFrame(nullptr)
{
}

nsresult
txStylesheet::init()
{
    mRootFrame = new ImportFrame;
    NS_ENSURE_TRUE(mRootFrame, NS_ERROR_OUT_OF_MEMORY);
    
    // Create default templates
    // element/root template
    mContainerTemplate = new txPushParams;
    NS_ENSURE_TRUE(mContainerTemplate, NS_ERROR_OUT_OF_MEMORY);

    nsAutoPtr<txNodeTest> nt(new txNodeTypeTest(txNodeTypeTest::NODE_TYPE));
    NS_ENSURE_TRUE(nt, NS_ERROR_OUT_OF_MEMORY);

    nsAutoPtr<Expr> nodeExpr(new LocationStep(nt, LocationStep::CHILD_AXIS));
    NS_ENSURE_TRUE(nodeExpr, NS_ERROR_OUT_OF_MEMORY);

    nt.forget();

    txPushNewContext* pushContext = new txPushNewContext(Move(nodeExpr));
    mContainerTemplate->mNext = pushContext;
    NS_ENSURE_TRUE(pushContext, NS_ERROR_OUT_OF_MEMORY);

    txApplyDefaultElementTemplate* applyTemplates =
        new txApplyDefaultElementTemplate;
    pushContext->mNext = applyTemplates;
    NS_ENSURE_TRUE(applyTemplates, NS_ERROR_OUT_OF_MEMORY);

    txLoopNodeSet* loopNodeSet = new txLoopNodeSet(applyTemplates);
    applyTemplates->mNext = loopNodeSet;
    NS_ENSURE_TRUE(loopNodeSet, NS_ERROR_OUT_OF_MEMORY);

    txPopParams* popParams = new txPopParams;
    pushContext->mBailTarget = loopNodeSet->mNext = popParams;
    NS_ENSURE_TRUE(popParams, NS_ERROR_OUT_OF_MEMORY);

    popParams->mNext = new txReturn();
    NS_ENSURE_TRUE(popParams->mNext, NS_ERROR_OUT_OF_MEMORY);

    // attribute/textnode template
    nt = new txNodeTypeTest(txNodeTypeTest::NODE_TYPE);
    NS_ENSURE_TRUE(nt, NS_ERROR_OUT_OF_MEMORY);

    nodeExpr = new LocationStep(nt, LocationStep::SELF_AXIS);
    NS_ENSURE_TRUE(nodeExpr, NS_ERROR_OUT_OF_MEMORY);

    nt.forget();

    mCharactersTemplate = new txValueOf(Move(nodeExpr), false);
    NS_ENSURE_TRUE(mCharactersTemplate, NS_ERROR_OUT_OF_MEMORY);

    mCharactersTemplate->mNext = new txReturn();
    NS_ENSURE_TRUE(mCharactersTemplate->mNext, NS_ERROR_OUT_OF_MEMORY);

    // pi/comment/namespace template
    mEmptyTemplate = new txReturn();
    NS_ENSURE_TRUE(mEmptyTemplate, NS_ERROR_OUT_OF_MEMORY);

    return NS_OK;
}

txStylesheet::~txStylesheet()
{
    // Delete all ImportFrames
    delete mRootFrame;
    txListIterator frameIter(&mImportFrames);
    while (frameIter.hasNext()) {
        delete static_cast<ImportFrame*>(frameIter.next());
    }

    txListIterator instrIter(&mTemplateInstructions);
    while (instrIter.hasNext()) {
        delete static_cast<txInstruction*>(instrIter.next());
    }
    
    // We can't make the map own its values because then we wouldn't be able
    // to merge attributesets of the same name
    txExpandedNameMap<txInstruction>::iterator attrSetIter(mAttributeSets);
    while (attrSetIter.next()) {
        delete attrSetIter.value();
    }
}

txInstruction*
txStylesheet::findTemplate(const txXPathNode& aNode,
                           const txExpandedName& aMode,
                           txIMatchContext* aContext,
                           ImportFrame* aImportedBy,
                           ImportFrame** aImportFrame)
{
    NS_ASSERTION(aImportFrame, "missing ImportFrame pointer");

    *aImportFrame = nullptr;
    txInstruction* matchTemplate = nullptr;
    ImportFrame* endFrame = nullptr;
    txListIterator frameIter(&mImportFrames);

    if (aImportedBy) {
        ImportFrame* curr = static_cast<ImportFrame*>(frameIter.next());
        while (curr != aImportedBy) {
               curr = static_cast<ImportFrame*>(frameIter.next());
        }
        endFrame = aImportedBy->mFirstNotImported;
    }

#if defined(PR_LOGGING) && defined(TX_TO_STRING)
    txPattern* match = 0;
#endif

    ImportFrame* frame;
    while (!matchTemplate &&
           (frame = static_cast<ImportFrame*>(frameIter.next())) &&
           frame != endFrame) {

        // get templatelist for this mode
        nsTArray<MatchableTemplate>* templates =
            frame->mMatchableTemplates.get(aMode);

        if (templates) {
            // Find template with highest priority
            uint32_t i, len = templates->Length();
            for (i = 0; i < len && !matchTemplate; ++i) {
                MatchableTemplate& templ = (*templates)[i];
                if (templ.mMatch->matches(aNode, aContext)) {
                    matchTemplate = templ.mFirstInstruction;
                    *aImportFrame = frame;
#if defined(PR_LOGGING) && defined(TX_TO_STRING)
                    match = templ.mMatch;
#endif
                }
            }
        }
    }

#ifdef PR_LOGGING
    nsAutoString mode, nodeName;
    if (aMode.mLocalName) {
        aMode.mLocalName->ToString(mode);
    }
    txXPathNodeUtils::getNodeName(aNode, nodeName);
    if (matchTemplate) {
        nsAutoString matchAttr;
#ifdef TX_TO_STRING
        match->toString(matchAttr);
#endif
        PR_LOG(txLog::xslt, PR_LOG_DEBUG,
               ("MatchTemplate, Pattern %s, Mode %s, Node %s\n",
                NS_LossyConvertUTF16toASCII(matchAttr).get(),
                NS_LossyConvertUTF16toASCII(mode).get(),
                NS_LossyConvertUTF16toASCII(nodeName).get()));
    }
    else {
        PR_LOG(txLog::xslt, PR_LOG_DEBUG,
               ("No match, Node %s, Mode %s\n", 
                NS_LossyConvertUTF16toASCII(nodeName).get(),
                NS_LossyConvertUTF16toASCII(mode).get()));
    }
#endif

    if (!matchTemplate) {
        // Test for these first since a node can be both a text node
        // and a root (if it is orphaned)
        if (txXPathNodeUtils::isAttribute(aNode) ||
            txXPathNodeUtils::isText(aNode)) {
            matchTemplate = mCharactersTemplate;
        }
        else if (txXPathNodeUtils::isElement(aNode) ||
                 txXPathNodeUtils::isRoot(aNode)) {
            matchTemplate = mContainerTemplate;
        }
        else {
            matchTemplate = mEmptyTemplate;
        }
    }

    return matchTemplate;
}

txDecimalFormat*
txStylesheet::getDecimalFormat(const txExpandedName& aName)
{
    return mDecimalFormats.get(aName);
}

txInstruction*
txStylesheet::getAttributeSet(const txExpandedName& aName)
{
    return mAttributeSets.get(aName);
}

txInstruction*
txStylesheet::getNamedTemplate(const txExpandedName& aName)
{
    return mNamedTemplates.get(aName);
}

txOutputFormat*
txStylesheet::getOutputFormat()
{
    return &mOutputFormat;
}

txStylesheet::GlobalVariable*
txStylesheet::getGlobalVariable(const txExpandedName& aName)
{
    return mGlobalVariables.get(aName);
}

const txOwningExpandedNameMap<txXSLKey>&
txStylesheet::getKeyMap()
{
    return mKeys;
}

bool
txStylesheet::isStripSpaceAllowed(const txXPathNode& aNode, txIMatchContext* aContext)
{
    int32_t frameCount = mStripSpaceTests.Length();
    if (frameCount == 0) {
        return false;
    }

    txXPathTreeWalker walker(aNode);

    if (txXPathNodeUtils::isText(walker.getCurrentPosition()) &&
        (!txXPathNodeUtils::isWhitespace(aNode) || !walker.moveToParent())) {
        return false;
    }

    const txXPathNode& node = walker.getCurrentPosition();

    if (!txXPathNodeUtils::isElement(node)) {
        return false;
    }

    // check Whitespace stipping handling list against given Node
    int32_t i;
    for (i = 0; i < frameCount; ++i) {
        txStripSpaceTest* sst = mStripSpaceTests[i];
        if (sst->matches(node, aContext)) {
            return sst->stripsSpace() && !XMLUtils::getXMLSpacePreserve(node);
        }
    }

    return false;
}

nsresult
txStylesheet::doneCompiling()
{
    nsresult rv = NS_OK;
    // Collect all importframes into a single ordered list
    txListIterator frameIter(&mImportFrames);
    rv = frameIter.addAfter(mRootFrame);
    NS_ENSURE_SUCCESS(rv, rv);
    
    mRootFrame = nullptr;
    frameIter.next();
    rv = addFrames(frameIter);
    NS_ENSURE_SUCCESS(rv, rv);

    // Loop through importframes in decreasing-precedence-order and process
    // all items
    frameIter.reset();
    ImportFrame* frame;
    while ((frame = static_cast<ImportFrame*>(frameIter.next()))) {
        nsTArray<txStripSpaceTest*> frameStripSpaceTests;

        txListIterator itemIter(&frame->mToplevelItems);
        itemIter.resetToEnd();
        txToplevelItem* item;
        while ((item = static_cast<txToplevelItem*>(itemIter.previous()))) {
            switch (item->getType()) {
                case txToplevelItem::attributeSet:
                {
                    rv = addAttributeSet(static_cast<txAttributeSetItem*>
                                                    (item));
                    NS_ENSURE_SUCCESS(rv, rv);
                    break;
                }
                case txToplevelItem::dummy:
                case txToplevelItem::import:
                {
                    break;
                }
                case txToplevelItem::output:
                {
                    mOutputFormat.merge(static_cast<txOutputItem*>(item)->mFormat);
                    break;
                }
                case txToplevelItem::stripSpace:
                {
                    rv = addStripSpace(static_cast<txStripSpaceItem*>(item),
                                       frameStripSpaceTests);
                    NS_ENSURE_SUCCESS(rv, rv);
                    break;
                }
                case txToplevelItem::templ:
                {
                    rv = addTemplate(static_cast<txTemplateItem*>(item),
                                     frame);
                    NS_ENSURE_SUCCESS(rv, rv);

                    break;
                }
                case txToplevelItem::variable:
                {
                    rv = addGlobalVariable(static_cast<txVariableItem*>
                                                      (item));
                    NS_ENSURE_SUCCESS(rv, rv);

                    break;
                }
            }
            delete item;
            itemIter.remove(); //remove() moves to the previous
            itemIter.next();
        }
        if (!mStripSpaceTests.AppendElements(frameStripSpaceTests)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        
        frameStripSpaceTests.Clear();
    }

    if (!mDecimalFormats.get(txExpandedName())) {
        nsAutoPtr<txDecimalFormat> format(new txDecimalFormat);
        NS_ENSURE_TRUE(format, NS_ERROR_OUT_OF_MEMORY);
        
        rv = mDecimalFormats.add(txExpandedName(), format);
        NS_ENSURE_SUCCESS(rv, rv);
        
        format.forget();
    }

    return NS_OK;
}

nsresult
txStylesheet::addTemplate(txTemplateItem* aTemplate,
                          ImportFrame* aImportFrame)
{
    NS_ASSERTION(aTemplate, "missing template");

    txInstruction* instr = aTemplate->mFirstInstruction;
    nsresult rv = mTemplateInstructions.add(instr);
    NS_ENSURE_SUCCESS(rv, rv);

    // mTemplateInstructions now owns the instructions
    aTemplate->mFirstInstruction.forget();

    if (!aTemplate->mName.isNull()) {
        rv = mNamedTemplates.add(aTemplate->mName, instr);
        NS_ENSURE_TRUE(NS_SUCCEEDED(rv) || rv == NS_ERROR_XSLT_ALREADY_SET,
                       rv);
    }

    if (!aTemplate->mMatch) {
        // This is no error, see section 6 Named Templates

        return NS_OK;
    }

    // get the txList for the right mode
    nsTArray<MatchableTemplate>* templates =
        aImportFrame->mMatchableTemplates.get(aTemplate->mMode);

    if (!templates) {
        nsAutoPtr< nsTArray<MatchableTemplate> > newList(
            new nsTArray<MatchableTemplate>);
        NS_ENSURE_TRUE(newList, NS_ERROR_OUT_OF_MEMORY);

        rv = aImportFrame->mMatchableTemplates.set(aTemplate->mMode, newList);
        NS_ENSURE_SUCCESS(rv, rv);

        templates = newList.forget();
    }

    // Add the simple patterns to the list of matchable templates, according
    // to default priority
    nsAutoPtr<txPattern> simple = Move(aTemplate->mMatch);
    nsAutoPtr<txPattern> unionPattern;
    if (simple->getType() == txPattern::UNION_PATTERN) {
        unionPattern = Move(simple);
        simple = unionPattern->getSubPatternAt(0);
        unionPattern->setSubPatternAt(0, nullptr);
    }

    uint32_t unionPos = 1; // only used when unionPattern is set
    while (simple) {
        double priority = aTemplate->mPrio;
        if (mozilla::IsNaN(priority)) {
            priority = simple->getDefaultPriority();
            NS_ASSERTION(!mozilla::IsNaN(priority),
                         "simple pattern without default priority");
        }

        uint32_t i, len = templates->Length();
        for (i = 0; i < len; ++i) {
            if (priority > (*templates)[i].mPriority) {
                break;
            }
        }

        MatchableTemplate* nt = templates->InsertElementAt(i);
        NS_ENSURE_TRUE(nt, NS_ERROR_OUT_OF_MEMORY);

        nt->mFirstInstruction = instr;
        nt->mMatch = Move(simple);
        nt->mPriority = priority;

        if (unionPattern) {
            simple = unionPattern->getSubPatternAt(unionPos);
            if (simple) {
                unionPattern->setSubPatternAt(unionPos, nullptr);
            }
            ++unionPos;
        }
    }

    return NS_OK;
}

nsresult
txStylesheet::addFrames(txListIterator& aInsertIter)
{
    ImportFrame* frame = static_cast<ImportFrame*>(aInsertIter.current());
    nsresult rv = NS_OK;
    txListIterator iter(&frame->mToplevelItems);
    txToplevelItem* item;
    while ((item = static_cast<txToplevelItem*>(iter.next()))) {
        if (item->getType() == txToplevelItem::import) {
            txImportItem* import = static_cast<txImportItem*>(item);
            import->mFrame->mFirstNotImported =
                static_cast<ImportFrame*>(aInsertIter.next());
            rv = aInsertIter.addBefore(import->mFrame);
            NS_ENSURE_SUCCESS(rv, rv);

            import->mFrame.forget();
            aInsertIter.previous();
            rv = addFrames(aInsertIter);
            NS_ENSURE_SUCCESS(rv, rv);
            aInsertIter.previous();
        }
    }
    
    return NS_OK;
}

nsresult
txStylesheet::addStripSpace(txStripSpaceItem* aStripSpaceItem,
                            nsTArray<txStripSpaceTest*>& aFrameStripSpaceTests)
{
    int32_t testCount = aStripSpaceItem->mStripSpaceTests.Length();
    for (; testCount > 0; --testCount) {
        txStripSpaceTest* sst = aStripSpaceItem->mStripSpaceTests[testCount-1];
        double priority = sst->getDefaultPriority();
        int32_t i, frameCount = aFrameStripSpaceTests.Length();
        for (i = 0; i < frameCount; ++i) {
            if (aFrameStripSpaceTests[i]->getDefaultPriority() < priority) {
                break;
            }
        }
        if (!aFrameStripSpaceTests.InsertElementAt(i, sst)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        aStripSpaceItem->mStripSpaceTests.RemoveElementAt(testCount-1);
    }

    return NS_OK;
}

nsresult
txStylesheet::addAttributeSet(txAttributeSetItem* aAttributeSetItem)
{
    nsresult rv = NS_OK;
    txInstruction* oldInstr = mAttributeSets.get(aAttributeSetItem->mName);
    if (!oldInstr) {
        rv = mAttributeSets.add(aAttributeSetItem->mName,
                                aAttributeSetItem->mFirstInstruction);
        NS_ENSURE_SUCCESS(rv, rv);
        
        aAttributeSetItem->mFirstInstruction.forget();
        
        return NS_OK;
    }
    
    // We need to prepend the new instructions before the existing ones.
    txInstruction* instr = aAttributeSetItem->mFirstInstruction;
    txInstruction* lastNonReturn = nullptr;
    while (instr->mNext) {
        lastNonReturn = instr;
        instr = instr->mNext;
    }
    
    if (!lastNonReturn) {
        // The new attributeset is empty, so lets just ignore it.
        return NS_OK;
    }

    rv = mAttributeSets.set(aAttributeSetItem->mName,
                            aAttributeSetItem->mFirstInstruction);
    NS_ENSURE_SUCCESS(rv, rv);

    aAttributeSetItem->mFirstInstruction.forget();

    lastNonReturn->mNext = oldInstr;  // ...and link up the old instructions.

    return NS_OK;
}

nsresult
txStylesheet::addGlobalVariable(txVariableItem* aVariable)
{
    if (mGlobalVariables.get(aVariable->mName)) {
        return NS_OK;
    }
    nsAutoPtr<GlobalVariable> var(
        new GlobalVariable(Move(aVariable->mValue),
                           Move(aVariable->mFirstInstruction),
                           aVariable->mIsParam));
    NS_ENSURE_TRUE(var, NS_ERROR_OUT_OF_MEMORY);
    
    nsresult rv = mGlobalVariables.add(aVariable->mName, var);
    NS_ENSURE_SUCCESS(rv, rv);
    
    var.forget();
    
    return NS_OK;
    
}

nsresult
txStylesheet::addKey(const txExpandedName& aName,
                     nsAutoPtr<txPattern> aMatch, nsAutoPtr<Expr> aUse)
{
    nsresult rv = NS_OK;

    txXSLKey* xslKey = mKeys.get(aName);
    if (!xslKey) {
        xslKey = new txXSLKey(aName);
        NS_ENSURE_TRUE(xslKey, NS_ERROR_OUT_OF_MEMORY);

        rv = mKeys.add(aName, xslKey);
        if (NS_FAILED(rv)) {
            delete xslKey;
            return rv;
        }
    }
    if (!xslKey->addKey(Move(aMatch), Move(aUse))) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

nsresult
txStylesheet::addDecimalFormat(const txExpandedName& aName,
                               nsAutoPtr<txDecimalFormat>&& aFormat)
{
    txDecimalFormat* existing = mDecimalFormats.get(aName);
    if (existing) {
        NS_ENSURE_TRUE(existing->isEqual(aFormat),
                       NS_ERROR_XSLT_PARSE_FAILURE);

        return NS_OK;
    }

    nsresult rv = mDecimalFormats.add(aName, aFormat);
    NS_ENSURE_SUCCESS(rv, rv);
    
    aFormat.forget();

    return NS_OK;
}

txStylesheet::ImportFrame::~ImportFrame()
{
    txListIterator tlIter(&mToplevelItems);
    while (tlIter.hasNext()) {
        delete static_cast<txToplevelItem*>(tlIter.next());
    }
}

txStylesheet::GlobalVariable::GlobalVariable(nsAutoPtr<Expr>&& aExpr,
                                             nsAutoPtr<txInstruction>&& aInstr,
                                             bool aIsParam)
    : mExpr(Move(aExpr)),
    mFirstInstruction(Move(aInstr)),
    mIsParam(aIsParam)
{
}
