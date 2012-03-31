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
#include "nsEventStates.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsStyleConsts.h"
#include "nsCSSAnonBoxes.h"
#include "nsRuleWalker.h"
#include "nsRuleData.h"
#include "nsContentErrors.h"
#include "nsRuleProcessorData.h"
#include "nsCSSRuleProcessor.h"
#include "mozilla/dom/Element.h"
#include "nsCSSFrameConstructor.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS1(nsHTMLStyleSheet::HTMLColorRule, nsIStyleRule)

/* virtual */ void
nsHTMLStyleSheet::HTMLColorRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Color)) {
    nsCSSValue* color = aRuleData->ValueForColor();
    if (color->GetUnit() == eCSSUnit_Null &&
        aRuleData->mPresContext->UseDocumentColors())
      color->SetColorValue(mColor);
  }
}

#ifdef DEBUG
/* virtual */ void
nsHTMLStyleSheet::HTMLColorRule::List(FILE* out, PRInt32 aIndent) const
{
}
#endif

 
NS_IMPL_ISUPPORTS1(nsHTMLStyleSheet::GenericTableRule, nsIStyleRule)

#ifdef DEBUG
/* virtual */ void
nsHTMLStyleSheet::GenericTableRule::List(FILE* out, PRInt32 aIndent) const
{
}
#endif

/* virtual */ void
nsHTMLStyleSheet::TableTHRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Text)) {
    nsCSSValue* textAlign = aRuleData->ValueForTextAlign();
    if (textAlign->GetUnit() == eCSSUnit_Null) {
      textAlign->SetIntValue(NS_STYLE_TEXT_ALIGN_MOZ_CENTER_OR_INHERIT,
                             eCSSUnit_Enumerated);
    }
  }
}

/* virtual */ void
nsHTMLStyleSheet::TableQuirkColorRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Color)) {
    nsCSSValue* color = aRuleData->ValueForColor();
    // We do not check UseDocumentColors() here, because we want to
    // use the body color no matter what.
    if (color->GetUnit() == eCSSUnit_Null)
      color->SetIntValue(NS_STYLE_COLOR_INHERIT_FROM_BODY,
                         eCSSUnit_Enumerated);
  }
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

static bool
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
  : mDocument(nsnull)
{
  mMappedAttrTable.ops = nsnull;
}

nsresult
nsHTMLStyleSheet::Init()
{
  mTableTHRule = new TableTHRule();
  mTableQuirkColorRule = new TableQuirkColorRule();
  return NS_OK;
}

nsHTMLStyleSheet::~nsHTMLStyleSheet()
{
  if (mMappedAttrTable.ops)
    PL_DHashTableFinish(&mMappedAttrTable);
}

NS_IMPL_ISUPPORTS2(nsHTMLStyleSheet, nsIStyleSheet, nsIStyleRuleProcessor)

/* virtual */ void
nsHTMLStyleSheet::RulesMatching(ElementRuleProcessorData* aData)
{
  nsRuleWalker *ruleWalker = aData->mRuleWalker;
  if (aData->mElement->IsHTML()) {
    nsIAtom* tag = aData->mElement->Tag();

    // if we have anchor colors, check if this is an anchor with an href
    if (tag == nsGkAtoms::a) {
      if (mLinkRule || mVisitedRule || mActiveRule) {
        nsEventStates state = nsCSSRuleProcessor::GetContentStateForVisitedHandling(
                                  aData->mElement,
                                  aData->mTreeMatchContext,
                                  aData->mTreeMatchContext.VisitedHandling(),
                                  // If the node being matched is a link,
                                  // it's the relevant link.
                                  nsCSSRuleProcessor::IsLink(aData->mElement));
        if (mLinkRule && state.HasState(NS_EVENT_STATE_UNVISITED)) {
          ruleWalker->Forward(mLinkRule);
          aData->mTreeMatchContext.SetHaveRelevantLink();
        }
        else if (mVisitedRule && state.HasState(NS_EVENT_STATE_VISITED)) {
          ruleWalker->Forward(mVisitedRule);
          aData->mTreeMatchContext.SetHaveRelevantLink();
        }

        // No need to add to the active rule if it's not a link
        if (mActiveRule && nsCSSRuleProcessor::IsLink(aData->mElement) &&
            state.HasState(NS_EVENT_STATE_ACTIVE)) {
          ruleWalker->Forward(mActiveRule);
        }
      } // end link/visited/active rules
    } // end A tag
    // add the rule to handle text-align for a <th>
    else if (tag == nsGkAtoms::th) {
      ruleWalker->Forward(mTableTHRule);
    }
    else if (tag == nsGkAtoms::table) {
      if (aData->mTreeMatchContext.mCompatMode == eCompatibility_NavQuirks) {
        ruleWalker->Forward(mTableQuirkColorRule);
      }
    }
  } // end html element

    // just get the style rules from the content
  aData->mElement->WalkContentStyleRules(ruleWalker);
}

// Test if style is dependent on content state
/* virtual */ nsRestyleHint
nsHTMLStyleSheet::HasStateDependentStyle(StateRuleProcessorData* aData)
{
  if (aData->mElement->IsHTML(nsGkAtoms::a) &&
      nsCSSRuleProcessor::IsLink(aData->mElement) &&
      ((mActiveRule && aData->mStateMask.HasState(NS_EVENT_STATE_ACTIVE)) ||
       (mLinkRule && aData->mStateMask.HasState(NS_EVENT_STATE_VISITED)) ||
       (mVisitedRule && aData->mStateMask.HasState(NS_EVENT_STATE_VISITED)))) {
    return eRestyle_Self;
  }
  
  return nsRestyleHint(0);
}

/* virtual */ bool
nsHTMLStyleSheet::HasDocumentStateDependentStyle(StateRuleProcessorData* aData)
{
  return false;
}

/* virtual */ nsRestyleHint
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
  Element *element = aData->mElement;
  if (aData->mAttribute == nsGkAtoms::href &&
      (mLinkRule || mVisitedRule || mActiveRule) &&
      element->IsHTML(nsGkAtoms::a)) {
    return eRestyle_Self;
  }

  // Don't worry about the mDocumentColorRule since it only applies
  // to descendants of body, when we're already reresolving.

  // Handle the content style rules.
  if (element->IsAttributeMapped(aData->mAttribute)) {
    // cellpadding on tables is special and requires reresolving all
    // the cells in the table
    if (aData->mAttribute == nsGkAtoms::cellpadding &&
        element->IsHTML(nsGkAtoms::table)) {
      return eRestyle_Subtree;
    }
    return eRestyle_Self;
  }

  return nsRestyleHint(0);
}

/* virtual */ bool
nsHTMLStyleSheet::MediumFeaturesChanged(nsPresContext* aPresContext)
{
  return false;
}

/* virtual */ size_t
nsHTMLStyleSheet::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return 0; // nsHTMLStyleSheets are charged to the DOM, not layout
}

/* virtual */ size_t
nsHTMLStyleSheet::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return 0; // nsHTMLStyleSheets are charged to the DOM, not layout
}

/* virtual */ void
nsHTMLStyleSheet::RulesMatching(PseudoElementRuleProcessorData* aData)
{
}

/* virtual */ void
nsHTMLStyleSheet::RulesMatching(AnonBoxRuleProcessorData* aData)
{
}

#ifdef MOZ_XUL
/* virtual */ void
nsHTMLStyleSheet::RulesMatching(XULTreeRuleProcessorData* aData)
{
}
#endif

  // nsIStyleSheet api
/* virtual */ nsIURI*
nsHTMLStyleSheet::GetSheetURI() const
{
  return mURL;
}

/* virtual */ nsIURI*
nsHTMLStyleSheet::GetBaseURI() const
{
  return mURL;
}

/* virtual */ void
nsHTMLStyleSheet::GetTitle(nsString& aTitle) const
{
  aTitle.Truncate();
}

/* virtual */ void
nsHTMLStyleSheet::GetType(nsString& aType) const
{
  aType.AssignLiteral("text/html");
}

/* virtual */ bool
nsHTMLStyleSheet::HasRules() const
{
  return true; // We have rules at all reasonable times
}

/* virtual */ bool
nsHTMLStyleSheet::IsApplicable() const
{
  return true;
}

/* virtual */ void
nsHTMLStyleSheet::SetEnabled(bool aEnabled)
{ // these can't be disabled
}

/* virtual */ bool
nsHTMLStyleSheet::IsComplete() const
{
  return true;
}

/* virtual */ void
nsHTMLStyleSheet::SetComplete()
{
}

/* virtual */ nsIStyleSheet*
nsHTMLStyleSheet::GetParentSheet() const
{
  return nsnull;
}

/* virtual */ nsIDocument*
nsHTMLStyleSheet::GetOwningDocument() const
{
  return mDocument;
}

/* virtual */ void
nsHTMLStyleSheet::SetOwningDocument(nsIDocument* aDocument)
{
  mDocument = aDocument; // not refcounted
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
  return NS_OK;
}

void
nsHTMLStyleSheet::Reset(nsIURI* aURL)
{
  mURL = aURL;

  mLinkRule          = nsnull;
  mVisitedRule       = nsnull;
  mActiveRule        = nsnull;

  if (mMappedAttrTable.ops) {
    PL_DHashTableFinish(&mMappedAttrTable);
    mMappedAttrTable.ops = nsnull;
  }
}

nsresult
nsHTMLStyleSheet::ImplLinkColorSetter(nsRefPtr<HTMLColorRule>& aRule, nscolor aColor)
{
  if (aRule && aRule->mColor == aColor) {
    return NS_OK;
  }

  aRule = new HTMLColorRule();
  if (!aRule)
    return NS_ERROR_OUT_OF_MEMORY;

  aRule->mColor = aColor;
  // Now make sure we restyle any links that might need it.  This
  // shouldn't happen often, so just rebuilding everything is ok.
  if (mDocument && mDocument->GetShell()) {
    Element* root = mDocument->GetRootElement();
    if (root) {
      mDocument->GetShell()->FrameConstructor()->
        PostRestyleEvent(root, eRestyle_Subtree, NS_STYLE_HINT_NONE);
    }
  }
  return NS_OK;
}

nsresult
nsHTMLStyleSheet::SetLinkColor(nscolor aColor)
{
  return ImplLinkColorSetter(mLinkRule, aColor);
}


nsresult
nsHTMLStyleSheet::SetActiveLinkColor(nscolor aColor)
{
  return ImplLinkColorSetter(mActiveRule, aColor);
}

nsresult
nsHTMLStyleSheet::SetVisitedLinkColor(nscolor aColor)
{
  return ImplLinkColorSetter(mVisitedRule, aColor);
}

already_AddRefed<nsMappedAttributes>
nsHTMLStyleSheet::UniqueMappedAttributes(nsMappedAttributes* aMapped)
{
  if (!mMappedAttrTable.ops) {
    bool res = PL_DHashTableInit(&mMappedAttrTable, &MappedAttrTable_Ops,
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
/* virtual */ void
nsHTMLStyleSheet::List(FILE* out, PRInt32 aIndent) const
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

static size_t
SizeOfAttributesEntryExcludingThis(PLDHashEntryHdr* aEntry,
                                   nsMallocSizeOfFun aMallocSizeOf,
                                   void* aArg)
{
  NS_PRECONDITION(aEntry, "The entry should not be null!");

  MappedAttrTableEntry* entry = static_cast<MappedAttrTableEntry*>(aEntry);
  NS_ASSERTION(entry->mAttributes, "entry->mAttributes should not be null!");
  return entry->mAttributes->SizeOfIncludingThis(aMallocSizeOf);
}

size_t
nsHTMLStyleSheet::DOMSizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  if (mMappedAttrTable.ops) {
    n += PL_DHashTableSizeOfExcludingThis(&mMappedAttrTable,
                                          SizeOfAttributesEntryExcludingThis,
                                          aMallocSizeOf);
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mURL
  // - mLinkRule
  // - mVisitedRule
  // - mActiveRule
  // - mTableQuirkColorRule
  // - mTableTHRule
  //
  // The following members are not measured:
  // - mDocument, because it's non-owning

  return n;
}

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

