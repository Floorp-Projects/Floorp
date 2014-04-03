/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
#include "nsMappedAttributes.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "mozilla/EventStates.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsStyleConsts.h"
#include "nsRuleWalker.h"
#include "nsRuleData.h"
#include "nsError.h"
#include "nsRuleProcessorData.h"
#include "nsCSSRuleProcessor.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"
#include "nsHashKeys.h"
#include "RestyleManager.h"

using namespace mozilla;
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
nsHTMLStyleSheet::HTMLColorRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t index = aIndent; --index >= 0; ) fputs("  ", out);
  fputs("[html color rule] {}\n", out);
}
#endif

 
NS_IMPL_ISUPPORTS1(nsHTMLStyleSheet::GenericTableRule, nsIStyleRule)

#ifdef DEBUG
/* virtual */ void
nsHTMLStyleSheet::GenericTableRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t index = aIndent; --index >= 0; ) fputs("  ", out);
  fputs("[generic table rule] {}\n", out);
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


NS_IMPL_ISUPPORTS1(nsHTMLStyleSheet::LangRule, nsIStyleRule)

/* virtual */ void
nsHTMLStyleSheet::LangRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Font)) {
    nsCSSValue* lang = aRuleData->ValueForLang();
    if (lang->GetUnit() == eCSSUnit_Null) {
      lang->SetStringValue(mLang, eCSSUnit_Ident);
    }
  }
}

#ifdef DEBUG
/* virtual */ void
nsHTMLStyleSheet::LangRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t index = aIndent; --index >= 0; ) fputs("  ", out);
  fputs("[lang rule] { language: \"", out);
  fputs(NS_ConvertUTF16toUTF8(mLang).get(), out);
  fputs("\" }\n", out);
}
#endif

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

static const PLDHashTableOps MappedAttrTable_Ops = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  MappedAttrTable_HashKey,
  MappedAttrTable_MatchEntry,
  PL_DHashMoveEntryStub,
  MappedAttrTable_ClearEntry,
  PL_DHashFinalizeStub,
  nullptr
};

// -----------------------------------------------------------

struct LangRuleTableEntry : public PLDHashEntryHdr {
  nsRefPtr<nsHTMLStyleSheet::LangRule> mRule;
};

static PLDHashNumber
LangRuleTable_HashKey(PLDHashTable *table, const void *key)
{
  const nsString *lang = static_cast<const nsString*>(key);
  return HashString(*lang);
}

static void
LangRuleTable_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  LangRuleTableEntry *entry = static_cast<LangRuleTableEntry*>(hdr);

  entry->~LangRuleTableEntry();
  memset(entry, 0, sizeof(LangRuleTableEntry));
}

static bool
LangRuleTable_MatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                         const void *key)
{
  const nsString *lang = static_cast<const nsString*>(key);
  const LangRuleTableEntry *entry = static_cast<const LangRuleTableEntry*>(hdr);

  return entry->mRule->mLang == *lang;
}

static bool
LangRuleTable_InitEntry(PLDHashTable *table, PLDHashEntryHdr *hdr,
                        const void *key)
{
  const nsString *lang = static_cast<const nsString*>(key);

  LangRuleTableEntry *entry = new (hdr) LangRuleTableEntry();

  // Create the unique rule for this language
  entry->mRule = new nsHTMLStyleSheet::LangRule(*lang);

  return true;
}

static const PLDHashTableOps LangRuleTable_Ops = {
  PL_DHashAllocTable,
  PL_DHashFreeTable,
  LangRuleTable_HashKey,
  LangRuleTable_MatchEntry,
  PL_DHashMoveEntryStub,
  LangRuleTable_ClearEntry,
  PL_DHashFinalizeStub,
  LangRuleTable_InitEntry
};

// -----------------------------------------------------------

nsHTMLStyleSheet::nsHTMLStyleSheet(nsIDocument* aDocument)
  : mDocument(aDocument)
  , mTableQuirkColorRule(new TableQuirkColorRule())
  , mTableTHRule(new TableTHRule())
{
  MOZ_ASSERT(aDocument);
  mMappedAttrTable.ops = nullptr;
  mLangRuleTable.ops = nullptr;
}

nsHTMLStyleSheet::~nsHTMLStyleSheet()
{
  if (mLangRuleTable.ops)
    PL_DHashTableFinish(&mLangRuleTable);
  if (mMappedAttrTable.ops)
    PL_DHashTableFinish(&mMappedAttrTable);
}

NS_IMPL_ISUPPORTS1(nsHTMLStyleSheet, nsIStyleRuleProcessor)

/* virtual */ void
nsHTMLStyleSheet::RulesMatching(ElementRuleProcessorData* aData)
{
  nsRuleWalker *ruleWalker = aData->mRuleWalker;
  if (aData->mElement->IsHTML() && !ruleWalker->AuthorStyleDisabled()) {
    nsIAtom* tag = aData->mElement->Tag();

    // if we have anchor colors, check if this is an anchor with an href
    if (tag == nsGkAtoms::a) {
      if (mLinkRule || mVisitedRule || mActiveRule) {
        EventStates state =
          nsCSSRuleProcessor::GetContentStateForVisitedHandling(
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

  // just get the style rules from the content.  For SVG we do this even if
  // author style is disabled, because SVG presentational hints aren't
  // considered style.
  if (!ruleWalker->AuthorStyleDisabled() || aData->mElement->IsSVG()) {
    aData->mElement->WalkContentStyleRules(ruleWalker);
  }

  // http://www.whatwg.org/specs/web-apps/current-work/multipage/elements.html#language
  // says that the xml:lang attribute overrides HTML's lang attribute,
  // so we need to do this after WalkContentStyleRules.
  nsString lang;
  if (aData->mElement->GetAttr(kNameSpaceID_XML, nsGkAtoms::lang, lang)) {
    ruleWalker->Forward(LangRuleFor(lang));
  }
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

/* virtual */ nsRestyleHint
nsHTMLStyleSheet::HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData)
{
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
nsHTMLStyleSheet::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return 0; // nsHTMLStyleSheets are charged to the DOM, not layout
}

/* virtual */ size_t
nsHTMLStyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
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

void
nsHTMLStyleSheet::SetOwningDocument(nsIDocument* aDocument)
{
  mDocument = aDocument; // not refcounted
}

void
nsHTMLStyleSheet::Reset()
{
  mLinkRule          = nullptr;
  mVisitedRule       = nullptr;
  mActiveRule        = nullptr;

  if (mLangRuleTable.ops) {
    PL_DHashTableFinish(&mLangRuleTable);
    mLangRuleTable.ops = nullptr;
  }
  if (mMappedAttrTable.ops) {
    PL_DHashTableFinish(&mMappedAttrTable);
    mMappedAttrTable.ops = nullptr;
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
      mDocument->GetShell()->GetPresContext()->RestyleManager()->
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
    PL_DHashTableInit(&mMappedAttrTable, &MappedAttrTable_Ops,
                      nullptr, sizeof(MappedAttrTableEntry), 16);
  }
  MappedAttrTableEntry *entry = static_cast<MappedAttrTableEntry*>
                                           (PL_DHashTableOperate(&mMappedAttrTable, aMapped, PL_DHASH_ADD));
  if (!entry)
    return nullptr;
  if (!entry->mAttributes) {
    // We added a new entry to the hashtable, so we have a new unique set.
    entry->mAttributes = aMapped;
  }
  nsRefPtr<nsMappedAttributes> ret = entry->mAttributes;
  return ret.forget();
}

void
nsHTMLStyleSheet::DropMappedAttributes(nsMappedAttributes* aMapped)
{
  NS_ENSURE_TRUE_VOID(aMapped);

  NS_ASSERTION(mMappedAttrTable.ops, "table uninitialized");
#ifdef DEBUG
  uint32_t entryCount = mMappedAttrTable.entryCount - 1;
#endif

  PL_DHashTableOperate(&mMappedAttrTable, aMapped, PL_DHASH_REMOVE);

  NS_ASSERTION(entryCount == mMappedAttrTable.entryCount, "not removed");
}

nsIStyleRule*
nsHTMLStyleSheet::LangRuleFor(const nsString& aLanguage)
{
  if (!mLangRuleTable.ops) {
    PL_DHashTableInit(&mLangRuleTable, &LangRuleTable_Ops,
                      nullptr, sizeof(LangRuleTableEntry), 16);
  }
  LangRuleTableEntry *entry = static_cast<LangRuleTableEntry*>
    (PL_DHashTableOperate(&mLangRuleTable, &aLanguage, PL_DHASH_ADD));
  if (!entry) {
    NS_ASSERTION(false, "out of memory");
    return nullptr;
  }
  return entry->mRule;
}

static size_t
SizeOfAttributesEntryExcludingThis(PLDHashEntryHdr* aEntry,
                                   MallocSizeOf aMallocSizeOf,
                                   void* aArg)
{
  NS_PRECONDITION(aEntry, "The entry should not be null!");

  MappedAttrTableEntry* entry = static_cast<MappedAttrTableEntry*>(aEntry);
  NS_ASSERTION(entry->mAttributes, "entry->mAttributes should not be null!");
  return entry->mAttributes->SizeOfIncludingThis(aMallocSizeOf);
}

size_t
nsHTMLStyleSheet::DOMSizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
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
  // - mLangRuleTable
  //
  // The following members are not measured:
  // - mDocument, because it's non-owning

  return n;
}
