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
#include "Expr.h"
#include "txXSLTPatterns.h"
#include "txToplevelItems.h"
#include "txInstructions.h"
#include "XSLTFunctions.h"
#include "TxLog.h"
#include "txKey.h"

txStylesheet::txStylesheet()
    : mRootFrame(nsnull),
      mNamedTemplates(PR_FALSE),
      mDecimalFormats(PR_TRUE),
      mAttributeSets(PR_FALSE),
      mGlobalVariables(PR_TRUE),
      mKeys(PR_TRUE)
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
        delete NS_STATIC_CAST(ImportFrame*, frameIter.next());
    }

    txListIterator instrIter(&mTemplateInstructions);
    while (instrIter.hasNext()) {
        delete NS_STATIC_CAST(txInstruction*, instrIter.next());
    }
    
    // We can't make the map own its values because then we wouldn't be able
    // to merge attributesets of the same name
    txExpandedNameMap::iterator attrSetIter(mAttributeSets);
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
        ImportFrame* curr = NS_STATIC_CAST(ImportFrame*, frameIter.next());
        while (curr != aImportedBy) {
               curr = NS_STATIC_CAST(ImportFrame*, frameIter.next());
        }
        endFrame = aImportedBy->mFirstNotImported;
    }

#ifdef PR_LOGGING
    txPattern* match = 0;
#endif

    ImportFrame* frame;
    while (!matchTemplate &&
           (frame = NS_STATIC_CAST(ImportFrame*, frameIter.next())) &&
           frame != endFrame) {

        // get templatelist for this mode
        txList* templates =
            NS_STATIC_CAST(txList*, frame->mMatchableTemplates.get(aMode));

        if (templates) {
            txListIterator templateIter(templates);

            // Find template with highest priority
            MatchableTemplate* templ;
            while (!matchTemplate &&
                   (templ =
                    NS_STATIC_CAST(MatchableTemplate*, templateIter.next()))) {
                if (templ->mMatch->matches(aNode, aContext)) {
                    matchTemplate = templ->mFirstInstruction;
                    *aImportFrame = frame;
#ifdef PR_LOGGING
                    match = templ->mMatch;
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
                NS_LossyConvertUCS2toASCII(matchAttr).get(),
                NS_LossyConvertUCS2toASCII(mode).get(),
                NS_LossyConvertUCS2toASCII(nodeName).get()));
    }
    else {
        PR_LOG(txLog::xslt, PR_LOG_DEBUG,
               ("No match, Node %s, Mode %s\n", 
                NS_LossyConvertUCS2toASCII(nodeName).get(),
                NS_LossyConvertUCS2toASCII(mode).get()));
    }
#endif

    if (!matchTemplate) {
        switch (txXPathNodeUtils::getNodeType(aNode)) {
            case txXPathNodeType::ELEMENT_NODE:
            case txXPathNodeType::DOCUMENT_NODE:
            {
                matchTemplate = mContainerTemplate;
                break;
            }
            case txXPathNodeType::ATTRIBUTE_NODE:
            case txXPathNodeType::TEXT_NODE:
            case txXPathNodeType::CDATA_SECTION_NODE:
            {
                matchTemplate = mCharactersTemplate;
                break;
            }
            default:
            {
                matchTemplate = mEmptyTemplate;
                break;
            }
        }
    }

    return matchTemplate;
}

txDecimalFormat*
txStylesheet::getDecimalFormat(const txExpandedName& aName)
{
    return NS_STATIC_CAST(txDecimalFormat*, mDecimalFormats.get(aName));
}

txInstruction*
txStylesheet::getAttributeSet(const txExpandedName& aName)
{
    return NS_STATIC_CAST(txInstruction*, mAttributeSets.get(aName));
}

txInstruction*
txStylesheet::getNamedTemplate(const txExpandedName& aName)
{
    return NS_STATIC_CAST(txInstruction*, mNamedTemplates.get(aName));
}

txOutputFormat*
txStylesheet::getOutputFormat()
{
    return &mOutputFormat;
}

txStylesheet::GlobalVariable*
txStylesheet::getGlobalVariable(const txExpandedName& aName)
{
    return NS_STATIC_CAST(GlobalVariable*, mGlobalVariables.get(aName));
}

const txExpandedNameMap&
txStylesheet::getKeyMap()
{
    return mKeys;
}

PRBool
txStylesheet::isStripSpaceAllowed(const txXPathNode& aNode, txIMatchContext* aContext)
{
    PRInt32 frameCount = mStripSpaceTests.Count();
    if (frameCount == 0) {
        return PR_FALSE;
    }

    txXPathTreeWalker walker(aNode);

    PRUint16 nodeType = walker.getNodeType();
    if (nodeType == txXPathNodeType::TEXT_NODE ||
        nodeType == txXPathNodeType::CDATA_SECTION_NODE) {
        if (!txXPathNodeUtils::isWhitespace(aNode) || !walker.moveToParent()) {
            return PR_FALSE;
        }
        nodeType = walker.getNodeType();
    }

    if (nodeType != txXPathNodeType::ELEMENT_NODE) {
        return PR_FALSE;
    }

    const txXPathNode& node = walker.getCurrentPosition();

    // check Whitespace stipping handling list against given Node
    PRInt32 i;
    for (i = 0; i < frameCount; ++i) {
        txStripSpaceTest* sst =
            NS_STATIC_CAST(txStripSpaceTest*, mStripSpaceTests[i]);
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
    while ((frame = NS_STATIC_CAST(ImportFrame*, frameIter.next()))) {
        nsVoidArray frameStripSpaceTests;

        txListIterator itemIter(&frame->mToplevelItems);
        itemIter.resetToEnd();
        txToplevelItem* item;
        while ((item = NS_STATIC_CAST(txToplevelItem*, itemIter.previous()))) {
            switch (item->getType()) {
                case txToplevelItem::attributeSet:
                {
                    rv = addAttributeSet(NS_STATIC_CAST(txAttributeSetItem*,
                                                        item));
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
                    mOutputFormat.merge(NS_STATIC_CAST(txOutputItem*, item)->mFormat);
                    break;
                }
                case txToplevelItem::stripSpace:
                {
                    rv = addStripSpace(NS_STATIC_CAST(txStripSpaceItem*, item),
                                       frameStripSpaceTests);
                    NS_ENSURE_SUCCESS(rv, rv);
                    break;
                }
                case txToplevelItem::templ:
                {
                    rv = addTemplate(NS_STATIC_CAST(txTemplateItem*, item),
                                     frame);
                    NS_ENSURE_SUCCESS(rv, rv);

                    break;
                }
                case txToplevelItem::variable:
                {
                    rv = addGlobalVariable(NS_STATIC_CAST(txVariableItem*,
                                                          item));
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
    txList* templates =
        NS_STATIC_CAST(txList*,
                       aImportFrame->mMatchableTemplates.get(aTemplate->mMode));

    if (!templates) {
        nsAutoPtr<txList> newList(new txList);
        NS_ENSURE_TRUE(newList, NS_ERROR_OUT_OF_MEMORY);

        rv = aImportFrame->mMatchableTemplates.add(aTemplate->mMode, newList);
        NS_ENSURE_SUCCESS(rv, rv);
        
        templates = newList.forget();
    }

    // Add the simple patterns to the list of matchable templates, according
    // to default priority
    txList simpleMatches;
    rv = aTemplate->mMatch->getSimplePatterns(simpleMatches);
    if (simpleMatches.get(0) == aTemplate->mMatch) {
        aTemplate->mMatch.forget();
    }
    
    txListIterator simples(&simpleMatches);
    while (simples.hasNext()) {
        // XXX if we fail in this loop, we leak the remaining simple patterns
        nsAutoPtr<txPattern> simple(NS_STATIC_CAST(txPattern*, simples.next()));
        double priority = aTemplate->mPrio;
        if (Double::isNaN(priority)) {
            priority = simple->getDefaultPriority();
            NS_ASSERTION(!Double::isNaN(priority),
                         "simple pattern without default priority");
        }
        nsAutoPtr<MatchableTemplate>
            nt(new MatchableTemplate(instr, simple, priority));
        NS_ENSURE_TRUE(nt, NS_ERROR_OUT_OF_MEMORY);

        txListIterator templ(templates);
        while (templ.hasNext()) {
            MatchableTemplate* mt = NS_STATIC_CAST(MatchableTemplate*,
                                                   templ.next());
            if (priority > mt->mPriority) {
                rv = templ.addBefore(nt);
                NS_ENSURE_SUCCESS(rv, rv);

                nt.forget();
                break;
            }
        }
        if (nt) {
            rv = templates->add(nt);
            NS_ENSURE_SUCCESS(rv, rv);

            nt.forget();
        }
    }

    return NS_OK;
}

nsresult
txStylesheet::addFrames(txListIterator& aInsertIter)
{
    ImportFrame* frame = NS_STATIC_CAST(ImportFrame*, aInsertIter.current());
    nsresult rv = NS_OK;
    txListIterator iter(&frame->mToplevelItems);
    txToplevelItem* item;
    while ((item = NS_STATIC_CAST(txToplevelItem*, iter.next()))) {
        if (item->getType() == txToplevelItem::import) {
            txImportItem* import = NS_STATIC_CAST(txImportItem*, item);
            import->mFrame->mFirstNotImported =
                NS_STATIC_CAST(ImportFrame*, aInsertIter.next());
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
                            nsVoidArray& frameStripSpaceTests)
{
    PRInt32 testCount = aStripSpaceItem->mStripSpaceTests.Count();
    for (; testCount > 0; --testCount) {
        txStripSpaceTest* sst =
            NS_STATIC_CAST(txStripSpaceTest*,
                           aStripSpaceItem->mStripSpaceTests[testCount-1]);
        double priority = sst->getDefaultPriority();
        PRInt32 i, frameCount = frameStripSpaceTests.Count();
        for (i = 0; i < frameCount; ++i) {
            txStripSpaceTest* fsst =
                NS_STATIC_CAST(txStripSpaceTest*, frameStripSpaceTests[i]);
            if (fsst->getDefaultPriority() < priority) {
                break;
            }
        }
        if (!frameStripSpaceTests.InsertElementAt(sst, i)) {
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
    txInstruction* oldInstr =
        NS_STATIC_CAST(txInstruction*,
                       mAttributeSets.get(aAttributeSetItem->mName));
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

    delete lastNonReturn->mNext;      // Delete the txReturn...
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

    txXSLKey* xslKey = NS_STATIC_CAST(txXSLKey*, mKeys.get(aName));
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
    txDecimalFormat* existing =
        NS_STATIC_CAST(txDecimalFormat*, mDecimalFormats.get(aName));
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

txStylesheet::ImportFrame::ImportFrame()
    : mMatchableTemplates(MB_TRUE),
      mFirstNotImported(nsnull)
{
}

txStylesheet::ImportFrame::~ImportFrame()
{
    // Delete templates in mMatchableTemplates
    txExpandedNameMap::iterator mapIter(mMatchableTemplates);
    while (mapIter.next()) {
        txListIterator templIter(NS_STATIC_CAST(txList*, mapIter.value()));
        MatchableTemplate* templ;
        while ((templ = NS_STATIC_CAST(MatchableTemplate*, templIter.next()))) {
            delete templ;
        }
    }
    
    txListIterator tlIter(&mToplevelItems);
    while (tlIter.hasNext()) {
        delete NS_STATIC_CAST(txToplevelItem*, tlIter.next());
    }
}

txStylesheet::GlobalVariable::GlobalVariable(nsAutoPtr<Expr> aExpr,
                                             nsAutoPtr<txInstruction> aFirstInstruction,
                                             PRBool aIsParam)
    : mExpr(aExpr), mFirstInstruction(aFirstInstruction), mIsParam(aIsParam)
{
}
