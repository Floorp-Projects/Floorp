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

#include "txStylesheet.h"
#include "txExpr.h"
#include "txXSLTPatterns.h"
#include "txToplevelItems.h"
#include "txInstructions.h"
#include "txXSLTFunctions.h"
#include "txLog.h"
#include "txKey.h"
#include "txXPathTreeWalker.h"

txStylesheet::txStylesheet()
    : mRootFrame(nsnull)
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

    txPushNewContext* pushContext = new txPushNewContext(nodeExpr);
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

    mCharactersTemplate = new txValueOf(nodeExpr, PR_FALSE);
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

    *aImportFrame = nsnull;
    txInstruction* matchTemplate = nsnull;
    ImportFrame* endFrame = nsnull;
    txListIterator frameIter(&mImportFrames);

    if (aImportedBy) {
        ImportFrame* curr = static_cast<ImportFrame*>(frameIter.next());
        while (curr != aImportedBy) {
               curr = static_cast<ImportFrame*>(frameIter.next());
        }
        endFrame = aImportedBy->mFirstNotImported;
    }

#ifdef PR_LOGGING
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
            PRUint32 i, len = templates->Length();
            for (i = 0; i < len && !matchTemplate; ++i) {
                MatchableTemplate& templ = (*templates)[i];
                if (templ.mMatch->matches(aNode, aContext)) {
                    matchTemplate = templ.mFirstInstruction;
                    *aImportFrame = frame;
#ifdef PR_LOGGING
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
    PRInt32 frameCount = mStripSpaceTests.Length();
    if (frameCount == 0) {
        return PR_FALSE;
    }

    txXPathTreeWalker walker(aNode);

    if (txXPathNodeUtils::isText(walker.getCurrentPosition()) &&
        (!txXPathNodeUtils::isWhitespace(aNode) || !walker.moveToParent())) {
        return PR_FALSE;
    }

    const txXPathNode& node = walker.getCurrentPosition();

    if (!txXPathNodeUtils::isElement(node)) {
        return PR_FALSE;
    }

    // check Whitespace stipping handling list against given Node
    PRInt32 i;
    for (i = 0; i < frameCount; ++i) {
        txStripSpaceTest* sst = mStripSpaceTests[i];
        if (sst->matches(node, aContext)) {
            return sst->stripsSpace() && !XMLUtils::getXMLSpacePreserve(node);
        }
    }

    return PR_FALSE;
}

nsresult
txStylesheet::doneCompiling()
{
    nsresult rv = NS_OK;
    // Collect all importframes into a single ordered list
    txListIterator frameIter(&mImportFrames);
    rv = frameIter.addAfter(mRootFrame);
    NS_ENSURE_SUCCESS(rv, rv);
    
    mRootFrame = nsnull;
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
    nsAutoPtr<txPattern> simple = aTemplate->mMatch;
    nsAutoPtr<txPattern> unionPattern;
    if (simple->getType() == txPattern::UNION_PATTERN) {
        unionPattern = simple;
        simple = unionPattern->getSubPatternAt(0);
        unionPattern->setSubPatternAt(0, nsnull);
    }

    PRUint32 unionPos = 1; // only used when unionPattern is set
    while (simple) {
        double priority = aTemplate->mPrio;
        if (Double::isNaN(priority)) {
            priority = simple->getDefaultPriority();
            NS_ASSERTION(!Double::isNaN(priority),
                         "simple pattern without default priority");
        }

        PRUint32 i, len = templates->Length();
        for (i = 0; i < len; ++i) {
            if (priority > (*templates)[i].mPriority) {
                break;
            }
        }

        MatchableTemplate* nt = templates->InsertElementAt(i);
        NS_ENSURE_TRUE(nt, NS_ERROR_OUT_OF_MEMORY);

        nt->mFirstInstruction = instr;
        nt->mMatch = simple;
        nt->mPriority = priority;

        if (unionPattern) {
            simple = unionPattern->getSubPatternAt(unionPos);
            if (simple) {
                unionPattern->setSubPatternAt(unionPos, nsnull);
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
    PRInt32 testCount = aStripSpaceItem->mStripSpaceTests.Length();
    for (; testCount > 0; --testCount) {
        txStripSpaceTest* sst = aStripSpaceItem->mStripSpaceTests[testCount-1];
        double priority = sst->getDefaultPriority();
        PRInt32 i, frameCount = aFrameStripSpaceTests.Length();
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
    txInstruction* lastNonReturn = nsnull;
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
        new GlobalVariable(aVariable->mValue, aVariable->mFirstInstruction,
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
    if (!xslKey->addKey(aMatch, aUse)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

nsresult
txStylesheet::addDecimalFormat(const txExpandedName& aName,
                               nsAutoPtr<txDecimalFormat> aFormat)
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

txStylesheet::GlobalVariable::GlobalVariable(nsAutoPtr<Expr> aExpr,
                                             nsAutoPtr<txInstruction> aFirstInstruction,
                                             bool aIsParam)
    : mExpr(aExpr), mFirstInstruction(aFirstInstruction), mIsParam(aIsParam)
{
}
