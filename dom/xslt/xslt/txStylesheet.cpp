/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txStylesheet.h"

#include <utility>

#include "mozilla/FloatingPoint.h"
#include "txExpr.h"
#include "txInstructions.h"
#include "txKey.h"
#include "txLog.h"
#include "txToplevelItems.h"
#include "txXPathTreeWalker.h"
#include "txXSLTFunctions.h"
#include "txXSLTPatterns.h"

using mozilla::LogLevel;
using mozilla::MakeUnique;
using mozilla::UniquePtr;
using mozilla::Unused;
using mozilla::WrapUnique;

txStylesheet::txStylesheet() : mRootFrame(nullptr) {}

nsresult txStylesheet::init() {
  mRootFrame = new ImportFrame;

  // Create default templates
  // element/root template
  mContainerTemplate = MakeUnique<txPushParams>();

  UniquePtr<txNodeTest> nt(new txNodeTypeTest(txNodeTypeTest::NODE_TYPE));
  UniquePtr<Expr> nodeExpr(
      new LocationStep(nt.get(), LocationStep::CHILD_AXIS));
  Unused << nt.release();

  txPushNewContext* pushContext = new txPushNewContext(std::move(nodeExpr));
  mContainerTemplate->mNext = WrapUnique(pushContext);

  txApplyDefaultElementTemplate* applyTemplates =
      new txApplyDefaultElementTemplate;
  pushContext->mNext = WrapUnique(applyTemplates);

  txLoopNodeSet* loopNodeSet = new txLoopNodeSet(applyTemplates);
  applyTemplates->mNext = WrapUnique(loopNodeSet);

  txPopParams* popParams = new txPopParams;
  loopNodeSet->mNext = WrapUnique(popParams);
  pushContext->mBailTarget = loopNodeSet->mNext.get();

  popParams->mNext = MakeUnique<txReturn>();

  // attribute/textnode template
  nt = MakeUnique<txNodeTypeTest>(txNodeTypeTest::NODE_TYPE);
  nodeExpr = MakeUnique<LocationStep>(nt.get(), LocationStep::SELF_AXIS);
  Unused << nt.release();

  mCharactersTemplate = MakeUnique<txValueOf>(std::move(nodeExpr), false);
  mCharactersTemplate->mNext = MakeUnique<txReturn>();

  // pi/comment/namespace template
  mEmptyTemplate = MakeUnique<txReturn>();

  return NS_OK;
}

txStylesheet::~txStylesheet() {
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

nsresult txStylesheet::findTemplate(const txXPathNode& aNode,
                                    const txExpandedName& aMode,
                                    txIMatchContext* aContext,
                                    ImportFrame* aImportedBy,
                                    txInstruction** aTemplate,
                                    ImportFrame** aImportFrame) {
  NS_ASSERTION(aImportFrame, "missing ImportFrame pointer");

  *aTemplate = nullptr;
  *aImportFrame = nullptr;
  ImportFrame* endFrame = nullptr;
  txListIterator frameIter(&mImportFrames);

  if (aImportedBy) {
    ImportFrame* curr = static_cast<ImportFrame*>(frameIter.next());
    while (curr != aImportedBy) {
      curr = static_cast<ImportFrame*>(frameIter.next());
    }
    endFrame = aImportedBy->mFirstNotImported;
  }

#if defined(TX_TO_STRING)
  txPattern* match = 0;
#endif

  ImportFrame* frame;
  while (!*aTemplate && (frame = static_cast<ImportFrame*>(frameIter.next())) &&
         frame != endFrame) {
    // get templatelist for this mode
    nsTArray<MatchableTemplate>* templates =
        frame->mMatchableTemplates.get(aMode);

    if (templates) {
      // Find template with highest priority
      uint32_t i, len = templates->Length();
      for (i = 0; i < len && !*aTemplate; ++i) {
        MatchableTemplate& templ = (*templates)[i];
        bool matched;
        nsresult rv = templ.mMatch->matches(aNode, aContext, matched);
        NS_ENSURE_SUCCESS(rv, rv);

        if (matched) {
          *aTemplate = templ.mFirstInstruction;
          *aImportFrame = frame;
#if defined(TX_TO_STRING)
          match = templ.mMatch.get();
#endif
        }
      }
    }
  }

  if (MOZ_LOG_TEST(txLog::xslt, LogLevel::Debug)) {
    nsAutoString mode, nodeName;
    if (aMode.mLocalName) {
      aMode.mLocalName->ToString(mode);
    }
    txXPathNodeUtils::getNodeName(aNode, nodeName);
    if (*aTemplate) {
      nsAutoString matchAttr;
#ifdef TX_TO_STRING
      match->toString(matchAttr);
#endif
      MOZ_LOG(txLog::xslt, LogLevel::Debug,
              ("MatchTemplate, Pattern %s, Mode %s, Node %s\n",
               NS_LossyConvertUTF16toASCII(matchAttr).get(),
               NS_LossyConvertUTF16toASCII(mode).get(),
               NS_LossyConvertUTF16toASCII(nodeName).get()));
    } else {
      MOZ_LOG(txLog::xslt, LogLevel::Debug,
              ("No match, Node %s, Mode %s\n",
               NS_LossyConvertUTF16toASCII(nodeName).get(),
               NS_LossyConvertUTF16toASCII(mode).get()));
    }
  }

  if (!*aTemplate) {
    // Test for these first since a node can be both a text node
    // and a root (if it is orphaned)
    if (txXPathNodeUtils::isAttribute(aNode) ||
        txXPathNodeUtils::isText(aNode)) {
      *aTemplate = mCharactersTemplate.get();
    } else if (txXPathNodeUtils::isElement(aNode) ||
               txXPathNodeUtils::isRoot(aNode)) {
      *aTemplate = mContainerTemplate.get();
    } else {
      *aTemplate = mEmptyTemplate.get();
    }
  }

  return NS_OK;
}

txDecimalFormat* txStylesheet::getDecimalFormat(const txExpandedName& aName) {
  return mDecimalFormats.get(aName);
}

txInstruction* txStylesheet::getAttributeSet(const txExpandedName& aName) {
  return mAttributeSets.get(aName);
}

txInstruction* txStylesheet::getNamedTemplate(const txExpandedName& aName) {
  return mNamedTemplates.get(aName);
}

txOutputFormat* txStylesheet::getOutputFormat() { return &mOutputFormat; }

txStylesheet::GlobalVariable* txStylesheet::getGlobalVariable(
    const txExpandedName& aName) {
  return mGlobalVariables.get(aName);
}

const txOwningExpandedNameMap<txXSLKey>& txStylesheet::getKeyMap() {
  return mKeys;
}

nsresult txStylesheet::isStripSpaceAllowed(const txXPathNode& aNode,
                                           txIMatchContext* aContext,
                                           bool& aAllowed) {
  int32_t frameCount = mStripSpaceTests.Length();
  if (frameCount == 0) {
    aAllowed = false;

    return NS_OK;
  }

  txXPathTreeWalker walker(aNode);

  if (txXPathNodeUtils::isText(walker.getCurrentPosition()) &&
      (!txXPathNodeUtils::isWhitespace(aNode) || !walker.moveToParent())) {
    aAllowed = false;

    return NS_OK;
  }

  const txXPathNode& node = walker.getCurrentPosition();

  if (!txXPathNodeUtils::isElement(node)) {
    aAllowed = false;

    return NS_OK;
  }

  // check Whitespace stipping handling list against given Node
  int32_t i;
  for (i = 0; i < frameCount; ++i) {
    const auto& sst = mStripSpaceTests[i];
    bool matched;
    nsresult rv = sst->matches(node, aContext, matched);
    NS_ENSURE_SUCCESS(rv, rv);

    if (matched) {
      aAllowed = sst->stripsSpace() && !XMLUtils::getXMLSpacePreserve(node);

      return NS_OK;
    }
  }

  aAllowed = false;

  return NS_OK;
}

nsresult txStylesheet::doneCompiling() {
  nsresult rv = NS_OK;
  // Collect all importframes into a single ordered list
  txListIterator frameIter(&mImportFrames);
  frameIter.addAfter(mRootFrame);

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
        case txToplevelItem::attributeSet: {
          rv = addAttributeSet(static_cast<txAttributeSetItem*>(item));
          NS_ENSURE_SUCCESS(rv, rv);
          break;
        }
        case txToplevelItem::dummy:
        case txToplevelItem::import: {
          break;
        }
        case txToplevelItem::output: {
          mOutputFormat.merge(static_cast<txOutputItem*>(item)->mFormat);
          break;
        }
        case txToplevelItem::stripSpace: {
          rv = addStripSpace(static_cast<txStripSpaceItem*>(item),
                             frameStripSpaceTests);
          NS_ENSURE_SUCCESS(rv, rv);
          break;
        }
        case txToplevelItem::templ: {
          rv = addTemplate(static_cast<txTemplateItem*>(item), frame);
          NS_ENSURE_SUCCESS(rv, rv);

          break;
        }
        case txToplevelItem::variable: {
          rv = addGlobalVariable(static_cast<txVariableItem*>(item));
          NS_ENSURE_SUCCESS(rv, rv);

          break;
        }
      }
      delete item;
      itemIter.remove();  // remove() moves to the previous
      itemIter.next();
    }
    mStripSpaceTests.AppendElements(frameStripSpaceTests);
    frameStripSpaceTests.Clear();
  }

  if (!mDecimalFormats.get(txExpandedName())) {
    UniquePtr<txDecimalFormat> format(new txDecimalFormat);
    rv = mDecimalFormats.add(txExpandedName(), format.get());
    NS_ENSURE_SUCCESS(rv, rv);

    Unused << format.release();
  }

  return NS_OK;
}

nsresult txStylesheet::addTemplate(txTemplateItem* aTemplate,
                                   ImportFrame* aImportFrame) {
  NS_ASSERTION(aTemplate, "missing template");

  txInstruction* instr = aTemplate->mFirstInstruction.get();
  mTemplateInstructions.add(instr);

  // mTemplateInstructions now owns the instructions
  Unused << aTemplate->mFirstInstruction.release();

  if (!aTemplate->mName.isNull()) {
    nsresult rv = mNamedTemplates.add(aTemplate->mName, instr);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) || rv == NS_ERROR_XSLT_ALREADY_SET, rv);
  }

  if (!aTemplate->mMatch) {
    // This is no error, see section 6 Named Templates

    return NS_OK;
  }

  // get the txList for the right mode
  nsTArray<MatchableTemplate>* templates =
      aImportFrame->mMatchableTemplates.get(aTemplate->mMode);

  if (!templates) {
    UniquePtr<nsTArray<MatchableTemplate>> newList(
        new nsTArray<MatchableTemplate>);
    nsresult rv =
        aImportFrame->mMatchableTemplates.set(aTemplate->mMode, newList.get());
    NS_ENSURE_SUCCESS(rv, rv);

    templates = newList.release();
  }

  // Add the simple patterns to the list of matchable templates, according
  // to default priority
  UniquePtr<txPattern> simple = std::move(aTemplate->mMatch);
  UniquePtr<txPattern> unionPattern;
  if (simple->getType() == txPattern::UNION_PATTERN) {
    unionPattern = std::move(simple);
    simple = WrapUnique(unionPattern->getSubPatternAt(0));
    unionPattern->setSubPatternAt(0, nullptr);
  }

  uint32_t unionPos = 1;  // only used when unionPattern is set
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
    nt->mFirstInstruction = instr;
    nt->mMatch = std::move(simple);
    nt->mPriority = priority;

    if (unionPattern) {
      simple = WrapUnique(unionPattern->getSubPatternAt(unionPos));
      if (simple) {
        unionPattern->setSubPatternAt(unionPos, nullptr);
      }
      ++unionPos;
    }
  }

  return NS_OK;
}

nsresult txStylesheet::addFrames(txListIterator& aInsertIter) {
  ImportFrame* frame = static_cast<ImportFrame*>(aInsertIter.current());
  nsresult rv = NS_OK;
  txListIterator iter(&frame->mToplevelItems);
  txToplevelItem* item;
  while ((item = static_cast<txToplevelItem*>(iter.next()))) {
    if (item->getType() == txToplevelItem::import) {
      txImportItem* import = static_cast<txImportItem*>(item);
      import->mFrame->mFirstNotImported =
          static_cast<ImportFrame*>(aInsertIter.next());
      aInsertIter.addBefore(import->mFrame.release());
      aInsertIter.previous();
      rv = addFrames(aInsertIter);
      NS_ENSURE_SUCCESS(rv, rv);
      aInsertIter.previous();
    }
  }

  return NS_OK;
}

nsresult txStylesheet::addStripSpace(
    txStripSpaceItem* aStripSpaceItem,
    nsTArray<txStripSpaceTest*>& aFrameStripSpaceTests) {
  int32_t testCount = aStripSpaceItem->mStripSpaceTests.Length();
  for (; testCount > 0; --testCount) {
    txStripSpaceTest* sst = aStripSpaceItem->mStripSpaceTests[testCount - 1];
    double priority = sst->getDefaultPriority();
    int32_t i, frameCount = aFrameStripSpaceTests.Length();
    for (i = 0; i < frameCount; ++i) {
      if (aFrameStripSpaceTests[i]->getDefaultPriority() < priority) {
        break;
      }
    }
    aFrameStripSpaceTests.InsertElementAt(i, sst);
    aStripSpaceItem->mStripSpaceTests.RemoveElementAt(testCount - 1);
  }

  return NS_OK;
}

nsresult txStylesheet::addAttributeSet(txAttributeSetItem* aAttributeSetItem) {
  nsresult rv = NS_OK;
  txInstruction* oldInstr = mAttributeSets.get(aAttributeSetItem->mName);
  if (!oldInstr) {
    rv = mAttributeSets.add(aAttributeSetItem->mName,
                            aAttributeSetItem->mFirstInstruction.get());
    NS_ENSURE_SUCCESS(rv, rv);

    Unused << aAttributeSetItem->mFirstInstruction.release();

    return NS_OK;
  }

  // We need to prepend the new instructions before the existing ones.
  txInstruction* instr = aAttributeSetItem->mFirstInstruction.get();
  txInstruction* lastNonReturn = nullptr;
  while (instr->mNext) {
    lastNonReturn = instr;
    instr = instr->mNext.get();
  }

  if (!lastNonReturn) {
    // The new attributeset is empty, so lets just ignore it.
    return NS_OK;
  }

  rv = mAttributeSets.set(aAttributeSetItem->mName,
                          aAttributeSetItem->mFirstInstruction.get());
  NS_ENSURE_SUCCESS(rv, rv);

  Unused << aAttributeSetItem->mFirstInstruction.release();

  lastNonReturn->mNext =
      WrapUnique(oldInstr);  // ...and link up the old instructions.

  return NS_OK;
}

nsresult txStylesheet::addGlobalVariable(txVariableItem* aVariable) {
  if (mGlobalVariables.get(aVariable->mName)) {
    return NS_OK;
  }
  UniquePtr<GlobalVariable> var(new GlobalVariable(
      std::move(aVariable->mValue), std::move(aVariable->mFirstInstruction),
      aVariable->mIsParam));
  nsresult rv = mGlobalVariables.add(aVariable->mName, var.get());
  NS_ENSURE_SUCCESS(rv, rv);

  Unused << var.release();

  return NS_OK;
}

nsresult txStylesheet::addKey(const txExpandedName& aName,
                              UniquePtr<txPattern> aMatch,
                              UniquePtr<Expr> aUse) {
  nsresult rv = NS_OK;

  txXSLKey* xslKey = mKeys.get(aName);
  if (!xslKey) {
    xslKey = new txXSLKey(aName);
    rv = mKeys.add(aName, xslKey);
    if (NS_FAILED(rv)) {
      delete xslKey;
      return rv;
    }
  }
  if (!xslKey->addKey(std::move(aMatch), std::move(aUse))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult txStylesheet::addDecimalFormat(const txExpandedName& aName,
                                        UniquePtr<txDecimalFormat>&& aFormat) {
  txDecimalFormat* existing = mDecimalFormats.get(aName);
  if (existing) {
    NS_ENSURE_TRUE(existing->isEqual(aFormat.get()),
                   NS_ERROR_XSLT_PARSE_FAILURE);

    return NS_OK;
  }

  nsresult rv = mDecimalFormats.add(aName, aFormat.get());
  NS_ENSURE_SUCCESS(rv, rv);

  Unused << aFormat.release();

  return NS_OK;
}

txStylesheet::ImportFrame::~ImportFrame() {
  txListIterator tlIter(&mToplevelItems);
  while (tlIter.hasNext()) {
    delete static_cast<txToplevelItem*>(tlIter.next());
  }
}

txStylesheet::GlobalVariable::GlobalVariable(UniquePtr<Expr>&& aExpr,
                                             UniquePtr<txInstruction>&& aInstr,
                                             bool aIsParam)
    : mExpr(std::move(aExpr)),
      mFirstInstruction(std::move(aInstr)),
      mIsParam(aIsParam) {}
