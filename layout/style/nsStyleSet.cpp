/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman <glazman@netscape.com>
 *   Brian Ryner    <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsStyleSet.h"
#include "nsNetUtil.h"
#include "nsICSSStyleSheet.h"
#include "nsIDocument.h"
#include "nsRuleWalker.h"
#include "nsStyleContext.h"
#include "nsICSSStyleRule.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSRuleProcessor.h"
#include "nsIContent.h"
#include "nsIFrame.h"

nsIURI *nsStyleSet::gQuirkURI = 0;

nsStyleSet::nsStyleSet()
  : mRuleTree(nsnull),
    mRuleWalker(nsnull),
    mDestroyedCount(0),
    mBatching(0),
    mInShutdown(PR_FALSE),
    mAuthorStyleDisabled(PR_FALSE),
    mDirty(0)
{
}

nsresult
nsStyleSet::Init(nsPresContext *aPresContext)
{
  if (!gQuirkURI) {
    static const char kQuirk_href[] = "resource://gre/res/quirk.css";
    NS_NewURI(&gQuirkURI, kQuirk_href);
    NS_ENSURE_TRUE(gQuirkURI, NS_ERROR_OUT_OF_MEMORY);
  }

  if (!BuildDefaultStyleData(aPresContext)) {
    mDefaultStyleData.Destroy(0, aPresContext);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mRuleTree = nsRuleNode::CreateRootNode(aPresContext);
  if (!mRuleTree) {
    mDefaultStyleData.Destroy(0, aPresContext);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mRuleWalker = new nsRuleWalker(mRuleTree);
  if (!mRuleWalker) {
    mRuleTree->Destroy();
    mDefaultStyleData.Destroy(0, aPresContext);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult
nsStyleSet::GatherRuleProcessors(sheetType aType)
{
  mRuleProcessors[aType] = nsnull;
  if (mAuthorStyleDisabled && (aType == eDocSheet || 
                               aType == ePresHintSheet ||
                               aType == eHTMLPresHintSheet ||
                               aType == eStyleAttrSheet)) {
    //don't regather if this level is disabled
    return NS_OK;
  }
  if (mSheets[aType].Count()) {
    switch (aType) {
      case eAgentSheet:
      case eUserSheet:
      case eDocSheet:
      case eOverrideSheet: {
        // levels containing CSS stylesheets
        nsCOMArray<nsIStyleSheet>& sheets = mSheets[aType];
        nsCOMArray<nsICSSStyleSheet> cssSheets(sheets.Count());
        for (PRInt32 i = 0, i_end = sheets.Count(); i < i_end; ++i) {
          nsCOMPtr<nsICSSStyleSheet> cssSheet = do_QueryInterface(sheets[i]);
          NS_ASSERTION(cssSheet, "not a CSS sheet");
          cssSheets.AppendObject(cssSheet);
        }
        mRuleProcessors[aType] = new nsCSSRuleProcessor(cssSheets);
      } break;

      default:
        // levels containing non-CSS stylesheets
        NS_ASSERTION(mSheets[aType].Count() == 1, "only one sheet per level");
        mRuleProcessors[aType] = do_QueryInterface(mSheets[aType][0]);
        break;
    }
  }

  return NS_OK;
}

#ifdef DEBUG
#define CHECK_APPLICABLE \
PR_BEGIN_MACRO \
  PRBool applicable = PR_TRUE; \
  aSheet->GetApplicable(applicable); \
  NS_ASSERTION(applicable, "Inapplicable sheet being placed in style set"); \
PR_END_MACRO
#else
#define CHECK_APPLICABLE
#endif

nsresult
nsStyleSet::AppendStyleSheet(sheetType aType, nsIStyleSheet *aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  CHECK_APPLICABLE;
  mSheets[aType].RemoveObject(aSheet);
  if (!mSheets[aType].AppendObject(aSheet))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mBatching)
    return GatherRuleProcessors(aType);

  mDirty |= 1 << aType;
  return NS_OK;
}

nsresult
nsStyleSet::PrependStyleSheet(sheetType aType, nsIStyleSheet *aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
  CHECK_APPLICABLE;
  mSheets[aType].RemoveObject(aSheet);
  if (!mSheets[aType].InsertObjectAt(aSheet, 0))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mBatching)
    return GatherRuleProcessors(aType);

  mDirty |= 1 << aType;
  return NS_OK;
}

nsresult
nsStyleSet::RemoveStyleSheet(sheetType aType, nsIStyleSheet *aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");
#ifdef DEBUG
  PRBool complete = PR_TRUE;
  aSheet->GetComplete(complete);
  NS_ASSERTION(complete, "Incomplete sheet being removed from style set");
#endif
  mSheets[aType].RemoveObject(aSheet);
  if (!mBatching)
    return GatherRuleProcessors(aType);

  mDirty |= 1 << aType;
  return NS_OK;
}

nsresult
nsStyleSet::ReplaceSheets(sheetType aType,
                          const nsCOMArray<nsIStyleSheet> &aNewSheets)
{
  mSheets[aType].Clear();
  if (!mSheets[aType].AppendObjects(aNewSheets))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mBatching)
    return GatherRuleProcessors(aType);

  mDirty |= 1 << aType;
  return NS_OK;
}

PRBool
nsStyleSet::GetAuthorStyleDisabled()
{
  return mAuthorStyleDisabled;
}

nsresult
nsStyleSet::SetAuthorStyleDisabled(PRBool aStyleDisabled)
{
  if (aStyleDisabled == !mAuthorStyleDisabled) {
    mAuthorStyleDisabled = aStyleDisabled;
    BeginUpdate();
    mDirty |= 1 << eDocSheet |
              1 << ePresHintSheet |
              1 << eHTMLPresHintSheet |
              1 << eStyleAttrSheet;
    return EndUpdate();
  }
  return NS_OK;
}

// -------- Doc Sheets

nsresult
nsStyleSet::AddDocStyleSheet(nsIStyleSheet* aSheet, nsIDocument* aDocument)
{
  NS_PRECONDITION(aSheet && aDocument, "null arg");
  CHECK_APPLICABLE;

  nsCOMArray<nsIStyleSheet>& docSheets = mSheets[eDocSheet];

  docSheets.RemoveObject(aSheet);
  // lowest index first
  PRInt32 newDocIndex = aDocument->GetIndexOfStyleSheet(aSheet);
  PRInt32 count = docSheets.Count();
  PRInt32 index;
  for (index = 0; index < count; index++) {
    nsIStyleSheet* sheet = docSheets.ObjectAt(index);
    PRInt32 sheetDocIndex = aDocument->GetIndexOfStyleSheet(sheet);
    if (sheetDocIndex > newDocIndex)
      break;
  }
  if (!docSheets.InsertObjectAt(aSheet, index))
    return NS_ERROR_OUT_OF_MEMORY;
  if (!mBatching)
    return GatherRuleProcessors(eDocSheet);

  mDirty |= 1 << eDocSheet;
  return NS_OK;
}

#undef CHECK_APPLICABLE

// Batching
void
nsStyleSet::BeginUpdate()
{
  ++mBatching;
}

nsresult
nsStyleSet::EndUpdate()
{
  NS_ASSERTION(mBatching > 0, "Unbalanced EndUpdate");
  if (--mBatching) {
    // We're not completely done yet.
    return NS_OK;
  }

  for (int i = 0; i < eSheetTypeCount; ++i) {
    if (mDirty & (1 << i)) {
      nsresult rv = GatherRuleProcessors(sheetType(i));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  mDirty = 0;
  return NS_OK;
}

void
nsStyleSet::EnableQuirkStyleSheet(PRBool aEnable)
{
  if (!mQuirkStyleSheet) {
    // first find the quirk sheet:
    // - run through all of the agent sheets and check for a CSSStyleSheet that
    //   has the URL we want
    PRInt32 nSheets = mSheets[eAgentSheet].Count();
    for (PRInt32 i = 0; i < nSheets; ++i) {
      nsIStyleSheet *sheet = mSheets[eAgentSheet].ObjectAt(i);
      NS_ASSERTION(sheet, "mAgentSheets should not contain null sheets");

      nsICSSStyleSheet *cssSheet = NS_STATIC_CAST(nsICSSStyleSheet*, sheet);
      NS_ASSERTION(nsCOMPtr<nsICSSStyleSheet>(do_QueryInterface(sheet)) == cssSheet,
                   "Agent sheet must be a CSSStyleSheet");

      nsCOMPtr<nsIStyleSheet> quirkSheet;
      PRBool bHasSheet = PR_FALSE;
      if (NS_SUCCEEDED(cssSheet->ContainsStyleSheet(gQuirkURI, bHasSheet, 
                                                    getter_AddRefs(quirkSheet))) 
          && bHasSheet) {
        NS_ASSERTION(quirkSheet, "QuirkSheet must be set: ContainsStyleSheet is hosed");
        // cache the sheet for faster lookup next time
        mQuirkStyleSheet = quirkSheet;
        // only one quirk style sheet can exist, so stop looking
        break;
      }
    }
  }
  NS_ASSERTION(mQuirkStyleSheet, "no quirk stylesheet");
  if (mQuirkStyleSheet) {
#ifdef DEBUG_dbaron_off // XXX Make this |DEBUG| once it stops firing.
    PRBool applicableNow;
    mQuirkStyleSheet->GetApplicable(applicableNow);
    NS_ASSERTION(!mRuleProcessors[eAgentSheet] || aEnable == applicableNow,
                 "enabling/disabling quirk stylesheet too late or incomplete quirk stylesheet");
    if (mRuleProcessors[eAgentSheet] && aEnable == applicableNow)
      printf("WARNING: We set the quirks mode too many times.\n"); // we do!
#endif
    mQuirkStyleSheet->SetEnabled(aEnable);
  }
}

static PRBool
EnumRulesMatching(nsIStyleRuleProcessor* aProcessor, void* aData)
{
  ElementRuleProcessorData* data =
    NS_STATIC_CAST(ElementRuleProcessorData*, aData);

  aProcessor->RulesMatching(data);
  return PR_TRUE;
}

/**
 * |GetContext| implements sharing of style contexts (not just the data
 * on the rule nodes) between siblings and cousins of the same
 * generation.  (It works for cousins of the same generation since
 * |aParentContext| could itself be a shared context.)
 */
already_AddRefed<nsStyleContext>
nsStyleSet::GetContext(nsPresContext* aPresContext, 
                       nsStyleContext* aParentContext, 
                       nsIAtom* aPseudoTag)
{
  nsStyleContext* result = nsnull;
  nsRuleNode* ruleNode = mRuleWalker->GetCurrentNode();
      
  if (aParentContext)
    result = aParentContext->FindChildWithRules(aPseudoTag, ruleNode).get();

#ifdef NOISY_DEBUG
  if (result)
    fprintf(stdout, "--- SharedSC %d ---\n", ++gSharedCount);
  else
    fprintf(stdout, "+++ NewSC %d +++\n", ++gNewCount);
#endif

  if (!result) {
    result = NS_NewStyleContext(aParentContext, aPseudoTag, ruleNode,
                                aPresContext).get();
    if (!aParentContext && result)
      mRoots.AppendElement(result);
  }

  return result;
}

void
nsStyleSet::AddImportantRules(nsRuleNode* aCurrLevelNode,
                              nsRuleNode* aLastPrevLevelNode)
{
  if (!aCurrLevelNode || aCurrLevelNode == aLastPrevLevelNode)
    return;

  AddImportantRules(aCurrLevelNode->GetParent(), aLastPrevLevelNode);

  nsIStyleRule *rule = aCurrLevelNode->GetRule();
  nsCOMPtr<nsICSSStyleRule> cssRule(do_QueryInterface(rule));
  if (cssRule) {
    nsCOMPtr<nsIStyleRule> impRule = cssRule->GetImportantRule();
    if (impRule)
      mRuleWalker->Forward(impRule);
  }
}

#ifdef DEBUG
void
nsStyleSet::AssertNoImportantRules(nsRuleNode* aCurrLevelNode,
                                   nsRuleNode* aLastPrevLevelNode)
{
  if (!aCurrLevelNode || aCurrLevelNode == aLastPrevLevelNode)
    return;

  AssertNoImportantRules(aCurrLevelNode->GetParent(), aLastPrevLevelNode);

  nsIStyleRule *rule = aCurrLevelNode->GetRule();
  nsCOMPtr<nsICSSStyleRule> cssRule(do_QueryInterface(rule));
  if (cssRule) {
    nsCOMPtr<nsIStyleRule> impRule = cssRule->GetImportantRule();
    NS_ASSERTION(!impRule, "Unexpected important rule");
  }
}

void
nsStyleSet::AssertNoCSSRules(nsRuleNode* aCurrLevelNode,
                             nsRuleNode* aLastPrevLevelNode)
{
  if (!aCurrLevelNode || aCurrLevelNode == aLastPrevLevelNode)
    return;

  AssertNoImportantRules(aCurrLevelNode->GetParent(), aLastPrevLevelNode);

  nsIStyleRule *rule = aCurrLevelNode->GetRule();
  nsCOMPtr<nsICSSStyleRule> cssRule(do_QueryInterface(rule));
  NS_ASSERTION(!cssRule, "Unexpected CSS rule");
}
#endif

// Enumerate the rules in a way that cares about the order of the rules.
void
nsStyleSet::FileRules(nsIStyleRuleProcessor::EnumFunc aCollectorFunc, 
                      RuleProcessorData* aData)
{
  // Cascading order:
  // [least important]
  //  1. UA normal rules                    = Agent        normal
  //  2. Presentation hints                 = PresHint     normal
  //  3. User normal rules                  = User         normal
  //  2. HTML Presentation hints            = HTMLPresHint normal
  //  5. Author normal rules                = Document     normal
  //  6. Override normal rules              = Override     normal
  //  7. Author !important rules            = Document     !important
  //  8. Override !important rules          = Override     !important
  //  9. User !important rules              = User         !important
  // 10. UA !important rules                = Agent        !important
  // [most important]

  NS_PRECONDITION(SheetCount(ePresHintSheet) == 0 ||
                  SheetCount(eHTMLPresHintSheet) == 0,
                  "Can't have both types of preshint sheets at once!");
  
  if (mRuleProcessors[eAgentSheet])
    (*aCollectorFunc)(mRuleProcessors[eAgentSheet], aData);
  nsRuleNode* lastAgentRN = mRuleWalker->GetCurrentNode();

  if (mRuleProcessors[ePresHintSheet])
    (*aCollectorFunc)(mRuleProcessors[ePresHintSheet], aData);
  nsRuleNode* lastPresHintRN = mRuleWalker->GetCurrentNode();
  
  if (mRuleProcessors[eUserSheet])
    (*aCollectorFunc)(mRuleProcessors[eUserSheet], aData);
  nsRuleNode* lastUserRN = mRuleWalker->GetCurrentNode();

  if (mRuleProcessors[eHTMLPresHintSheet])
    (*aCollectorFunc)(mRuleProcessors[eHTMLPresHintSheet], aData);
  nsRuleNode* lastHTMLPresHintRN = mRuleWalker->GetCurrentNode();
  
  PRBool cutOffInheritance = PR_FALSE;
  if (mStyleRuleSupplier) {
    // We can supply additional document-level sheets that should be walked.
    mStyleRuleSupplier->WalkRules(this, aCollectorFunc, aData,
                                  &cutOffInheritance);
  }
  if (!cutOffInheritance && mRuleProcessors[eDocSheet]) // NOTE: different
    (*aCollectorFunc)(mRuleProcessors[eDocSheet], aData);
  if (mRuleProcessors[eStyleAttrSheet])
    (*aCollectorFunc)(mRuleProcessors[eStyleAttrSheet], aData);

  if (mRuleProcessors[eOverrideSheet])
    (*aCollectorFunc)(mRuleProcessors[eOverrideSheet], aData);
  nsRuleNode* lastOvrRN = mRuleWalker->GetCurrentNode();

  // There should be no important rules in the preshint or HTMLpreshint level
  AddImportantRules(lastOvrRN, lastHTMLPresHintRN);  // doc and override
#ifdef DEBUG
  AssertNoCSSRules(lastHTMLPresHintRN, lastUserRN);
  AssertNoImportantRules(lastHTMLPresHintRN, lastUserRN); // HTML preshints
#endif
  AddImportantRules(lastUserRN, lastPresHintRN); //user
#ifdef DEBUG
  AssertNoImportantRules(lastPresHintRN, lastAgentRN); // preshints
#endif
  AddImportantRules(lastAgentRN, nsnull);     //agent

}

// Enumerate all the rules in a way that doesn't care about the order
// of the rules and doesn't walk !important-rules.
void
nsStyleSet::WalkRuleProcessors(nsIStyleRuleProcessor::EnumFunc aFunc,
                               RuleProcessorData* aData)
{
  NS_PRECONDITION(SheetCount(ePresHintSheet) == 0 ||
                  SheetCount(eHTMLPresHintSheet) == 0,
                  "Can't have both types of preshint sheets at once!");
  
  if (mRuleProcessors[eAgentSheet])
    (*aFunc)(mRuleProcessors[eAgentSheet], aData);
  if (mRuleProcessors[ePresHintSheet])
    (*aFunc)(mRuleProcessors[ePresHintSheet], aData);
  if (mRuleProcessors[eUserSheet])
    (*aFunc)(mRuleProcessors[eUserSheet], aData);
  if (mRuleProcessors[eHTMLPresHintSheet])
    (*aFunc)(mRuleProcessors[eHTMLPresHintSheet], aData);
  
  PRBool cutOffInheritance = PR_FALSE;
  if (mStyleRuleSupplier) {
    // We can supply additional document-level sheets that should be walked.
    mStyleRuleSupplier->WalkRules(this, aFunc, aData, &cutOffInheritance);
  }
  if (!cutOffInheritance && mRuleProcessors[eDocSheet]) // NOTE: different
    (*aFunc)(mRuleProcessors[eDocSheet], aData);
  if (mRuleProcessors[eStyleAttrSheet])
    (*aFunc)(mRuleProcessors[eStyleAttrSheet], aData);
  if (mRuleProcessors[eOverrideSheet])
    (*aFunc)(mRuleProcessors[eOverrideSheet], aData);
}

PRBool nsStyleSet::BuildDefaultStyleData(nsPresContext* aPresContext)
{
  NS_ASSERTION(!mDefaultStyleData.mResetData &&
               !mDefaultStyleData.mInheritedData,
               "leaking default style data");
  mDefaultStyleData.mResetData = new (aPresContext) nsResetStyleData;
  if (!mDefaultStyleData.mResetData)
    return PR_FALSE;
  mDefaultStyleData.mInheritedData = new (aPresContext) nsInheritedStyleData;
  if (!mDefaultStyleData.mInheritedData)
    return PR_FALSE;

#define SSARG_PRESCONTEXT aPresContext

#define CREATE_DATA(name, type, args) \
  if (!(mDefaultStyleData.m##type##Data->m##name##Data = \
          new (aPresContext) nsStyle##name args)) \
    return PR_FALSE;

#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args) \
  CREATE_DATA(name, Inherited, ctor_args)
#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args) \
  CREATE_DATA(name, Reset, ctor_args)

#include "nsStyleStructList.h"

#undef STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_RESET
#undef SSARG_PRESCONTEXT

  return PR_TRUE;
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveStyleFor(nsIContent* aContent,
                            nsStyleContext* aParentContext)
{
  nsStyleContext*  result = nsnull;
  nsPresContext* presContext = PresContext();

  NS_ASSERTION(aContent, "must have content");
  NS_ASSERTION(aContent->IsContentOfType(nsIContent::eELEMENT),
               "content must be element");

  if (aContent && presContext) {
    if (mRuleProcessors[eAgentSheet]        ||
        mRuleProcessors[ePresHintSheet]     ||
        mRuleProcessors[eUserSheet]         ||
        mRuleProcessors[eHTMLPresHintSheet] ||
        mRuleProcessors[eDocSheet]          ||
        mRuleProcessors[eStyleAttrSheet]    ||
        mRuleProcessors[eOverrideSheet]) {
      ElementRuleProcessorData data(presContext, aContent, mRuleWalker);
      FileRules(EnumRulesMatching, &data);
      result = GetContext(presContext, aParentContext, nsnull).get();

      // Now reset the walker back to the root of the tree.
      mRuleWalker->Reset();
    }
  }

  return result;
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolveStyleForNonElement(nsStyleContext* aParentContext)
{
  nsStyleContext* result = nsnull;
  nsPresContext *presContext = PresContext();

  if (presContext) {
    if (mRuleProcessors[eAgentSheet]        ||
        mRuleProcessors[ePresHintSheet]     ||
        mRuleProcessors[eUserSheet]         ||
        mRuleProcessors[eHTMLPresHintSheet] ||
        mRuleProcessors[eDocSheet]          ||
        mRuleProcessors[eStyleAttrSheet]    ||
        mRuleProcessors[eOverrideSheet]) {
      result = GetContext(presContext, aParentContext,
                          nsCSSAnonBoxes::mozNonElement).get();
      NS_ASSERTION(mRuleWalker->AtRoot(), "rule walker must be at root");
    }
  }

  return result;
}


static PRBool
EnumPseudoRulesMatching(nsIStyleRuleProcessor* aProcessor, void* aData)
{
  PseudoRuleProcessorData* data =
    NS_STATIC_CAST(PseudoRuleProcessorData*, aData);

  aProcessor->RulesMatching(data);
  return PR_TRUE;
}

already_AddRefed<nsStyleContext>
nsStyleSet::ResolvePseudoStyleFor(nsIContent* aParentContent,
                                  nsIAtom* aPseudoTag,
                                  nsStyleContext* aParentContext,
                                  nsICSSPseudoComparator* aComparator)
{
  nsStyleContext*  result = nsnull;
  nsPresContext *presContext = PresContext();

  NS_ASSERTION(aPseudoTag, "must have pseudo tag");
  NS_ASSERTION(!aParentContent ||
               aParentContent->IsContentOfType(nsIContent::eELEMENT),
               "content (if non-null) must be element");

  if (aPseudoTag && presContext) {
    if (mRuleProcessors[eAgentSheet]        ||
        mRuleProcessors[ePresHintSheet]     ||
        mRuleProcessors[eUserSheet]         ||
        mRuleProcessors[eHTMLPresHintSheet] ||
        mRuleProcessors[eDocSheet]          ||
        mRuleProcessors[eStyleAttrSheet]    ||
        mRuleProcessors[eOverrideSheet]) {
      PseudoRuleProcessorData data(presContext, aParentContent, aPseudoTag,
                                   aComparator, mRuleWalker);
      FileRules(EnumPseudoRulesMatching, &data);

      result = GetContext(presContext, aParentContext, aPseudoTag).get();

      // Now reset the walker back to the root of the tree.
      mRuleWalker->Reset();
    }
  }

  return result;
}

already_AddRefed<nsStyleContext>
nsStyleSet::ProbePseudoStyleFor(nsIContent* aParentContent,
                                nsIAtom* aPseudoTag,
                                nsStyleContext* aParentContext)
{
  nsStyleContext*  result = nsnull;
  nsPresContext *presContext = PresContext();

  NS_ASSERTION(aPseudoTag, "must have pseudo tag");
  NS_ASSERTION(!aParentContent ||
               aParentContent->IsContentOfType(nsIContent::eELEMENT),
               "content (if non-null) must be element");

  if (aPseudoTag && presContext) {
    if (mRuleProcessors[eAgentSheet]        ||
        mRuleProcessors[ePresHintSheet]     ||
        mRuleProcessors[eUserSheet]         ||
        mRuleProcessors[eHTMLPresHintSheet] ||
        mRuleProcessors[eDocSheet]          ||
        mRuleProcessors[eStyleAttrSheet]    ||
        mRuleProcessors[eOverrideSheet]) {
      PseudoRuleProcessorData data(presContext, aParentContent, aPseudoTag,
                                   nsnull, mRuleWalker);
      FileRules(EnumPseudoRulesMatching, &data);

      if (!mRuleWalker->AtRoot())
        result = GetContext(presContext, aParentContext, aPseudoTag).get();

      // Now reset the walker back to the root of the tree.
      mRuleWalker->Reset();
    }
  }

  // For :before and :after pseudo-elements, having display: none or no
  // 'content' property is equivalent to not having the pseudo-element
  // at all.
  if (result &&
      (aPseudoTag == nsCSSPseudoElements::before ||
       aPseudoTag == nsCSSPseudoElements::after)) {
    const nsStyleDisplay *display = result->GetStyleDisplay();
    const nsStyleContent *content = result->GetStyleContent();
    // XXXldb What is contentCount for |content: ""|?
    if (display->mDisplay == NS_STYLE_DISPLAY_NONE ||
        content->ContentCount() == 0) {
      result->Release();
      result = nsnull;
    }
  }
  
  return result;
}

void
nsStyleSet::BeginShutdown(nsPresContext* aPresContext)
{
  mInShutdown = 1;
  mRoots.Clear(); // no longer valid, since we won't keep it up to date
}

void
nsStyleSet::Shutdown(nsPresContext* aPresContext)
{
  delete mRuleWalker;
  mRuleWalker = nsnull;

  mRuleTree->Destroy();
  mRuleTree = nsnull;

  mDefaultStyleData.Destroy(0, aPresContext);
}

static const PRInt32 kGCInterval = 1000;

void
nsStyleSet::NotifyStyleContextDestroyed(nsPresContext* aPresContext,
                                        nsStyleContext* aStyleContext)
{
  if (mInShutdown)
    return;

  if (!aStyleContext->GetParent()) {
    mRoots.RemoveElement(aStyleContext);
  }

  if (++mDestroyedCount == kGCInterval) {
    mDestroyedCount = 0;

    // Mark the style context tree by marking all roots, which will mark
    // all descendants.  This will reach style contexts in the
    // undisplayed map and "additional style contexts" since they are
    // descendants of the root.
    for (PRInt32 i = mRoots.Count() - 1; i >= 0; --i) {
      NS_STATIC_CAST(nsStyleContext*,mRoots[i])->Mark();
    }

    // Sweep the rule tree.
    NS_ASSERTION(mRuleWalker->AtRoot(), "Rule walker should be at root");
#ifdef DEBUG
    PRBool deleted =
#endif
      mRuleTree->Sweep();

    NS_ASSERTION(!deleted, "Root node must not be gc'd");
  }
}

void
nsStyleSet::ClearStyleData(nsPresContext* aPresContext)
{
  mRuleTree->ClearStyleData();

  for (PRInt32 i = mRoots.Count() - 1; i >= 0; --i) {
    NS_STATIC_CAST(nsStyleContext*,mRoots[i])->ClearStyleData(aPresContext);
  }
}

already_AddRefed<nsStyleContext>
nsStyleSet::ReParentStyleContext(nsPresContext* aPresContext,
                                 nsStyleContext* aStyleContext, 
                                 nsStyleContext* aNewParentContext)
{
  NS_ASSERTION(aPresContext, "must have pres context");
  NS_ASSERTION(aStyleContext, "must have style context");

  if (aPresContext && aStyleContext) {
    if (aStyleContext->GetParent() == aNewParentContext) {
      aStyleContext->AddRef();
      return aStyleContext;
    }
    else {  // really a new parent
      nsIAtom* pseudoTag = aStyleContext->GetPseudoType();

      nsRuleNode* ruleNode = aStyleContext->GetRuleNode();
      mRuleWalker->SetCurrentNode(ruleNode);

      already_AddRefed<nsStyleContext> result =
          GetContext(aPresContext, aNewParentContext, pseudoTag);
      mRuleWalker->Reset();
      return result;
    }
  }
  return nsnull;
}

struct StatefulData : public StateRuleProcessorData {
  StatefulData(nsPresContext* aPresContext,
               nsIContent* aContent, PRInt32 aStateMask)
    : StateRuleProcessorData(aPresContext, aContent, aStateMask),
      mHint(nsReStyleHint(0))
  {}
  nsReStyleHint   mHint;
}; 

static PRBool SheetHasStatefulStyle(nsIStyleRuleProcessor* aProcessor,
                                    void *aData)
{
  StatefulData* data = (StatefulData*)aData;
  nsReStyleHint hint;
  aProcessor->HasStateDependentStyle(data, &hint);
  data->mHint = nsReStyleHint(data->mHint | hint);
  return PR_TRUE; // continue
}

// Test if style is dependent on content state
nsReStyleHint
nsStyleSet::HasStateDependentStyle(nsPresContext* aPresContext,
                                   nsIContent*     aContent,
                                   PRInt32         aStateMask)
{
  nsReStyleHint result = nsReStyleHint(0);

  if (aContent->IsContentOfType(nsIContent::eELEMENT) &&
      (mRuleProcessors[eAgentSheet]        ||
       mRuleProcessors[ePresHintSheet]     ||
       mRuleProcessors[eUserSheet]         ||
       mRuleProcessors[eHTMLPresHintSheet] ||
       mRuleProcessors[eDocSheet]          ||
       mRuleProcessors[eStyleAttrSheet]    ||
       mRuleProcessors[eOverrideSheet])) {
    StatefulData data(aPresContext, aContent, aStateMask);
    WalkRuleProcessors(SheetHasStatefulStyle, &data);
    result = data.mHint;
  }

  return result;
}

struct AttributeData : public AttributeRuleProcessorData {
  AttributeData(nsPresContext* aPresContext,
                nsIContent* aContent, nsIAtom* aAttribute, PRInt32 aModType)
    : AttributeRuleProcessorData(aPresContext, aContent, aAttribute, aModType),
      mHint(nsReStyleHint(0))
  {}
  nsReStyleHint   mHint;
}; 

static PRBool
SheetHasAttributeStyle(nsIStyleRuleProcessor* aProcessor, void *aData)
{
  AttributeData* data = (AttributeData*)aData;
  nsReStyleHint hint;
  aProcessor->HasAttributeDependentStyle(data, &hint);
  data->mHint = nsReStyleHint(data->mHint | hint);
  return PR_TRUE; // continue
}

// Test if style is dependent on content state
nsReStyleHint
nsStyleSet::HasAttributeDependentStyle(nsPresContext* aPresContext,
                                       nsIContent*     aContent,
                                       nsIAtom*        aAttribute,
                                       PRInt32         aModType)
{
  nsReStyleHint result = nsReStyleHint(0);

  if (aContent->IsContentOfType(nsIContent::eELEMENT) &&
      (mRuleProcessors[eAgentSheet]        ||
       mRuleProcessors[ePresHintSheet]     ||
       mRuleProcessors[eUserSheet]         ||
       mRuleProcessors[eHTMLPresHintSheet] ||
       mRuleProcessors[eDocSheet]          ||
       mRuleProcessors[eStyleAttrSheet]    ||
       mRuleProcessors[eOverrideSheet])) {
    AttributeData data(aPresContext, aContent, aAttribute, aModType);
    WalkRuleProcessors(SheetHasAttributeStyle, &data);
    result = data.mHint;
  }

  return result;
}
