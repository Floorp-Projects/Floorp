/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

/*
 * style sheet and style rule processor representing data from presentational
 * HTML attributes
 */

#include "nsHTMLStyleSheet.h"
#include "nsINameSpaceManager.h"
#include "nsIAtom.h"
#include "nsIURL.h"
#include "nsMappedAttributes.h"
#include "nsILink.h"
#include "nsIFrame.h"
#include "nsStyleContext.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsIEventStateManager.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsStyleConsts.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsCSSAnonBoxes.h"
#include "nsRuleWalker.h"
#include "nsRuleData.h"
#include "nsContentErrors.h"
#include "nsRuleProcessorData.h"

NS_IMPL_ISUPPORTS1(nsHTMLStyleSheet::HTMLColorRule, nsIStyleRule)

NS_IMETHODIMP
nsHTMLStyleSheet::HTMLColorRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Color)) {
    if (aRuleData->mColorData->mColor.GetUnit() == eCSSUnit_Null &&
        aRuleData->mPresContext->UseDocumentColors())
      aRuleData->mColorData->mColor.SetColorValue(mColor);
  }
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLStyleSheet::HTMLColorRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}
#endif

 
NS_IMPL_ISUPPORTS1(nsHTMLStyleSheet::GenericTableRule, nsIStyleRule)

NS_IMETHODIMP
nsHTMLStyleSheet::GenericTableRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  // Nothing to do.
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLStyleSheet::GenericTableRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsHTMLStyleSheet::TableTHRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Text)) {
    if (aRuleData->mTextData->mTextAlign.GetUnit() == eCSSUnit_Null) {
      aRuleData->mTextData->mTextAlign.
        SetIntValue(NS_STYLE_TEXT_ALIGN_MOZ_CENTER_OR_INHERIT,
                    eCSSUnit_Enumerated);
    }
  }
  return NS_OK;
}

// -----------------------------------------------------------

struct MappedAttrTableEntry : public PLDHashEntryHdr {
  nsMappedAttributes *mAttributes;
};

static PLDHashNumber
MappedAttrTable_HashKey(PLDHashTable *table, const void *key)
{
  nsMappedAttributes *attributes =
    static_cast<nsMappedAttributes*>(const_cast<void*>(key));

  return attributes->HashValue();
}

static void
MappedAttrTable_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  MappedAttrTableEntry *entry = static_cast<MappedAttrTableEntry*>(hdr);

  entry->mAttributes->DropStyleSheetReference();
  memset(entry, 0, sizeof(MappedAttrTableEntry));
}

static PRBool
MappedAttrTable_MatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                           const void *key)
{
  nsMappedAttributes *attributes =
    static_cast<nsMappedAttributes*>(const_cast<void*>(key));
  const MappedAttrTableEntry *entry =
    static_cast<const MappedAttrTableEntry*>(hdr);

  return attributes->Equals(entry->mAttributes);
}

static PLDHashTableOps MappedAttrTable_Ops = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  MappedAttrTable_HashKey,
  MappedAttrTable_MatchEntry,
  PL_DHashMoveEntryStub,
  MappedAttrTable_ClearEntry,
  PL_DHashFinalizeStub,
  NULL
};

// -----------------------------------------------------------

nsHTMLStyleSheet::nsHTMLStyleSheet(void)
  : mURL(nsnull),
    mDocument(nsnull),
    mLinkRule(nsnull),
    mVisitedRule(nsnull),
    mActiveRule(nsnull),
    mDocumentColorRule(nsnull)
{
  mMappedAttrTable.ops = nsnull;
}

nsresult
nsHTMLStyleSheet::Init()
{
  mTableTHRule = new TableTHRule();
  if (!mTableTHRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mTableTHRule);
  return NS_OK;
}

nsHTMLStyleSheet::~nsHTMLStyleSheet()
{
  NS_IF_RELEASE(mURL);

  NS_IF_RELEASE(mLinkRule);
  NS_IF_RELEASE(mVisitedRule);
  NS_IF_RELEASE(mActiveRule);
  NS_IF_RELEASE(mDocumentColorRule);
  NS_IF_RELEASE(mTableTHRule);

  if (mMappedAttrTable.ops)
    PL_DHashTableFinish(&mMappedAttrTable);
}

NS_IMPL_ISUPPORTS2(nsHTMLStyleSheet, nsIStyleSheet, nsIStyleRuleProcessor)

static nsresult GetBodyColor(nsPresContext* aPresContext, nscolor* aColor)
{
  nsIPresShell *shell = aPresContext->PresShell();
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(shell->GetDocument());
  if (!htmlDoc)
    return NS_ERROR_FAILURE;
  nsIContent* bodyContent = htmlDoc->GetBodyContentExternal();
  if (!bodyContent)
    return NS_ERROR_FAILURE;
  nsIFrame *bodyFrame = bodyContent->GetPrimaryFrame();
  if (!bodyFrame)
    return NS_ERROR_FAILURE;
  *aColor = bodyFrame->GetStyleColor()->mColor;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyleSheet::RulesMatching(ElementRuleProcessorData* aData)
{
  nsIContent *content = aData->mContent;

  if (content) {
    nsRuleWalker *ruleWalker = aData->mRuleWalker;
    if (aData->mIsHTMLContent) {
      nsIAtom* tag = aData->mContentTag;

      // if we have anchor colors, check if this is an anchor with an href
      if (tag == nsGkAtoms::a) {
        if (mLinkRule || mVisitedRule || mActiveRule) {
          PRUint32 state = aData->GetContentStateForVisitedHandling(
                                    ruleWalker->VisitedHandling(),
                                    // If the node being matched is a link,
                                    // it's the relevant link.
                                    aData->IsLink());
          if (mLinkRule && (state & NS_EVENT_STATE_UNVISITED)) {
            ruleWalker->Forward(mLinkRule);
            ruleWalker->SetHaveRelevantLink();
          }
          else if (mVisitedRule && (state & NS_EVENT_STATE_VISITED)) {
            ruleWalker->Forward(mVisitedRule);
            ruleWalker->SetHaveRelevantLink();
          }

          // No need to add to the active rule if it's not a link
          if (mActiveRule && aData->IsLink() &&
              (state & NS_EVENT_STATE_ACTIVE)) {
            ruleWalker->Forward(mActiveRule);
          }
        } // end link/visited/active rules
      } // end A tag
      // add the rule to handle text-align for a <th>
      else if (tag == nsGkAtoms::th) {
        ruleWalker->Forward(mTableTHRule);
      }
      else if (tag == nsGkAtoms::table) {
        if (aData->mCompatMode == eCompatibility_NavQuirks) {
          nscolor bodyColor;
          nsresult rv =
            GetBodyColor(ruleWalker->CurrentNode()->GetPresContext(),
                         &bodyColor);
          if (NS_SUCCEEDED(rv) &&
              (!mDocumentColorRule || bodyColor != mDocumentColorRule->mColor)) {
            NS_IF_RELEASE(mDocumentColorRule);
            mDocumentColorRule = new HTMLColorRule();
            if (mDocumentColorRule) {
              NS_ADDREF(mDocumentColorRule);
              mDocumentColorRule->mColor = bodyColor;
            }
          }
          if (mDocumentColorRule)
            ruleWalker->Forward(mDocumentColorRule);
        }
      }
    } // end html element

    // just get the style rules from the content
    content->WalkContentStyleRules(ruleWalker);
  }

  return NS_OK;
}

// Test if style is dependent on content state
nsRestyleHint
nsHTMLStyleSheet::HasStateDependentStyle(StateRuleProcessorData* aData)
{
  if (aData->mIsHTMLContent &&
      aData->mContentTag == nsGkAtoms::a &&
      aData->IsLink() &&
      ((mActiveRule && (aData->mStateMask & NS_EVENT_STATE_ACTIVE)) ||
       (mLinkRule && (aData->mStateMask & NS_EVENT_STATE_VISITED)) ||
       (mVisitedRule && (aData->mStateMask & NS_EVENT_STATE_VISITED)))) {
    return eRestyle_Self;
  }
  
  return nsRestyleHint(0);
}

PRBool
nsHTMLStyleSheet::HasDocumentStateDependentStyle(StateRuleProcessorData* aData)
{
  return PR_FALSE;
}

nsRestyleHint
nsHTMLStyleSheet::HasAttributeDependentStyle(AttributeRuleProcessorData* aData)
{
  // Do nothing on before-change checks
  if (!aData->mAttrHasChanged) {
    return nsRestyleHint(0);
  }

  // Note: no need to worry about whether some states changed with this
  // attribute here, because we handle that under HasStateDependentStyle() as
  // needed.

  // Result is true for |href| changes on HTML links if we have link rules.
  nsIContent *content = aData->mContent;
  if (aData->mAttribute == nsGkAtoms::href &&
      (mLinkRule || mVisitedRule || mActiveRule) &&
      content &&
      content->IsHTML() &&
      aData->mContentTag == nsGkAtoms::a) {
    return eRestyle_Self;
  }

  // Don't worry about the mDocumentColorRule since it only applies
  // to descendants of body, when we're already reresolving.

  // Handle the content style rules.
  if (content && content->IsAttributeMapped(aData->mAttribute)) {
    return eRestyle_Self;
  }

  return nsRestyleHint(0);
}

NS_IMETHODIMP
nsHTMLStyleSheet::MediumFeaturesChanged(nsPresContext* aPresContext,
                                        PRBool* aRulesChanged)
{
  *aRulesChanged = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLStyleSheet::RulesMatching(PseudoElementRuleProcessorData* aData)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyleSheet::RulesMatching(AnonBoxRuleProcessorData* aData)
{
  return NS_OK;
}

#ifdef MOZ_XUL
NS_IMETHODIMP
nsHTMLStyleSheet::RulesMatching(XULTreeRuleProcessorData* aData)
{
  return NS_OK;
}
#endif

  // nsIStyleSheet api
NS_IMETHODIMP
nsHTMLStyleSheet::GetSheetURI(nsIURI** aSheetURI) const
{
  *aSheetURI = mURL;
  NS_IF_ADDREF(*aSheetURI);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyleSheet::GetBaseURI(nsIURI** aBaseURI) const
{
  *aBaseURI = mURL;
  NS_IF_ADDREF(*aBaseURI);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyleSheet::GetTitle(nsString& aTitle) const
{
  aTitle.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyleSheet::GetType(nsString& aType) const
{
  aType.AssignLiteral("text/html");
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsHTMLStyleSheet::HasRules() const
{
  return PR_TRUE; // We have rules at all reasonable times
}

NS_IMETHODIMP
nsHTMLStyleSheet::GetApplicable(PRBool& aApplicable) const
{
  aApplicable = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyleSheet::SetEnabled(PRBool aEnabled)
{ // these can't be disabled
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyleSheet::GetComplete(PRBool& aComplete) const
{
  aComplete = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyleSheet::SetComplete()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyleSheet::GetParentSheet(nsIStyleSheet*& aParent) const
{
  aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyleSheet::GetOwningDocument(nsIDocument*& aDocument) const
{
  aDocument = mDocument;
  NS_IF_ADDREF(aDocument);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLStyleSheet::SetOwningDocument(nsIDocument* aDocument)
{
  mDocument = aDocument; // not refcounted
  return NS_OK;
}

nsresult
nsHTMLStyleSheet::Init(nsIURI* aURL, nsIDocument* aDocument)
{
  NS_PRECONDITION(aURL && aDocument, "null ptr");
  if (! aURL || ! aDocument)
    return NS_ERROR_NULL_POINTER;

  if (mURL || mDocument)
    return NS_ERROR_ALREADY_INITIALIZED;

  mDocument = aDocument; // not refcounted!
  mURL = aURL;
  NS_ADDREF(mURL);
  return NS_OK;
}

nsresult
nsHTMLStyleSheet::Reset(nsIURI* aURL)
{
  NS_IF_RELEASE(mURL);
  mURL = aURL;
  NS_ADDREF(mURL);

  NS_IF_RELEASE(mLinkRule);
  NS_IF_RELEASE(mVisitedRule);
  NS_IF_RELEASE(mActiveRule);
  NS_IF_RELEASE(mDocumentColorRule);

  if (mMappedAttrTable.ops) {
    PL_DHashTableFinish(&mMappedAttrTable);
    mMappedAttrTable.ops = nsnull;
  }

  return NS_OK;
}

nsresult
nsHTMLStyleSheet::SetLinkColor(nscolor aColor)
{
  if (mLinkRule) {
    if (mLinkRule->mColor == aColor)
      return NS_OK;
    NS_RELEASE(mLinkRule);
  }

  mLinkRule = new HTMLColorRule();
  if (!mLinkRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mLinkRule);

  mLinkRule->mColor = aColor;
  return NS_OK;
}


nsresult
nsHTMLStyleSheet::SetActiveLinkColor(nscolor aColor)
{
  if (mActiveRule) {
    if (mActiveRule->mColor == aColor)
      return NS_OK;
    NS_RELEASE(mActiveRule);
  }

  mActiveRule = new HTMLColorRule();
  if (!mActiveRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mActiveRule);

  mActiveRule->mColor = aColor;
  return NS_OK;
}

nsresult
nsHTMLStyleSheet::SetVisitedLinkColor(nscolor aColor)
{
  if (mVisitedRule) {
    if (mVisitedRule->mColor == aColor)
      return NS_OK;
    NS_RELEASE(mVisitedRule);
  }

  mVisitedRule = new HTMLColorRule();
  if (!mVisitedRule)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(mVisitedRule);

  mVisitedRule->mColor = aColor;
  return NS_OK;
}

already_AddRefed<nsMappedAttributes>
nsHTMLStyleSheet::UniqueMappedAttributes(nsMappedAttributes* aMapped)
{
  if (!mMappedAttrTable.ops) {
    PRBool res = PL_DHashTableInit(&mMappedAttrTable, &MappedAttrTable_Ops,
                                   nsnull, sizeof(MappedAttrTableEntry), 16);
    if (!res) {
      mMappedAttrTable.ops = nsnull;
      return nsnull;
    }
  }
  MappedAttrTableEntry *entry = static_cast<MappedAttrTableEntry*>
                                           (PL_DHashTableOperate(&mMappedAttrTable, aMapped, PL_DHASH_ADD));
  if (!entry)
    return nsnull;
  if (!entry->mAttributes) {
    // We added a new entry to the hashtable, so we have a new unique set.
    entry->mAttributes = aMapped;
  }
  NS_ADDREF(entry->mAttributes); // for caller
  return entry->mAttributes;
}

void
nsHTMLStyleSheet::DropMappedAttributes(nsMappedAttributes* aMapped)
{
  NS_ENSURE_TRUE(aMapped, /**/);

  NS_ASSERTION(mMappedAttrTable.ops, "table uninitialized");
#ifdef DEBUG
  PRUint32 entryCount = mMappedAttrTable.entryCount - 1;
#endif

  PL_DHashTableOperate(&mMappedAttrTable, aMapped, PL_DHASH_REMOVE);

  NS_ASSERTION(entryCount == mMappedAttrTable.entryCount, "not removed");
}

#ifdef DEBUG
void nsHTMLStyleSheet::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("HTML Style Sheet: ", out);
  nsCAutoString urlSpec;
  mURL->GetSpec(urlSpec);
  if (!urlSpec.IsEmpty()) {
    fputs(urlSpec.get(), out);
  }
  fputs("\n", out);
}
#endif

// XXX For convenience and backwards compatibility
nsresult
NS_NewHTMLStyleSheet(nsHTMLStyleSheet** aInstancePtrResult, nsIURI* aURL, 
                     nsIDocument* aDocument)
{
  nsresult rv;
  nsHTMLStyleSheet* sheet;
  if (NS_FAILED(rv = NS_NewHTMLStyleSheet(&sheet)))
    return rv;

  if (NS_FAILED(rv = sheet->Init(aURL, aDocument))) {
    NS_RELEASE(sheet);
    return rv;
  }

  *aInstancePtrResult = sheet;
  return NS_OK;
}


nsresult
NS_NewHTMLStyleSheet(nsHTMLStyleSheet** aInstancePtrResult)
{
  NS_ASSERTION(aInstancePtrResult, "null out param");

  nsHTMLStyleSheet *it = new nsHTMLStyleSheet();
  if (!it) {
    *aInstancePtrResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(it);
  nsresult rv = it->Init();
  if (NS_FAILED(rv))
    NS_RELEASE(it);

  *aInstancePtrResult = it; // NS_ADDREF above, or set to null by NS_RELEASE
  return rv;
}
