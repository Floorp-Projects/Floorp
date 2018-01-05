/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "mozilla/OperatorNewExtensions.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/RestyleManagerInlines.h"
#include "mozilla/ServoStyleSet.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsHTMLStyleSheet::HTMLColorRule, nsIStyleRule)

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

/* virtual */ bool
nsHTMLStyleSheet::HTMLColorRule::MightMapInheritedStyleData()
{
  return true;
}

/* virtual */ bool
nsHTMLStyleSheet::HTMLColorRule::
GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty, nsCSSValue* aValue)
{
  MOZ_ASSERT(false, "GetDiscretelyAnimatedCSSValue is not implemented yet");
  return false;
}

#ifdef DEBUG
/* virtual */ void
nsHTMLStyleSheet::HTMLColorRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString indentStr;
  for (int32_t index = aIndent; --index >= 0; ) {
    indentStr.AppendLiteral("  ");
  }
  fprintf_stderr(out, "%s[html color rule] {}\n", indentStr.get());
}
#endif


NS_IMPL_ISUPPORTS(nsHTMLStyleSheet::GenericTableRule, nsIStyleRule)

#ifdef DEBUG
/* virtual */ void
nsHTMLStyleSheet::GenericTableRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString indentStr;
  for (int32_t index = aIndent; --index >= 0; ) {
    indentStr.AppendLiteral("  ");
  }
  fprintf_stderr(out, "%s[generic table rule] {}\n", indentStr.get());
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

/* virtual */ bool
nsHTMLStyleSheet::TableTHRule::MightMapInheritedStyleData()
{
  return true;
}

/* virtual */ bool
nsHTMLStyleSheet::TableTHRule::
GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty, nsCSSValue* aValue)
{
  MOZ_ASSERT(false, "GetDiscretelyAnimatedCSSValue is not implemented yet");
  return false;
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

/* virtual */ bool
nsHTMLStyleSheet::TableQuirkColorRule::MightMapInheritedStyleData()
{
  return true;
}

/* virtual */ bool
nsHTMLStyleSheet::TableQuirkColorRule::
GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty, nsCSSValue* aValue)
{
  MOZ_ASSERT(false, "GetDiscretelyAnimatedCSSValue is not implemented yet");
  return false;
}

NS_IMPL_ISUPPORTS(nsHTMLStyleSheet::LangRule, nsIStyleRule)

/* virtual */ void
nsHTMLStyleSheet::LangRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  if (aRuleData->mSIDs & NS_STYLE_INHERIT_BIT(Font)) {
    nsCSSValue* lang = aRuleData->ValueForLang();
    if (lang->GetUnit() == eCSSUnit_Null) {
      RefPtr<nsAtom> langAtom = mLang;
      lang->SetAtomIdentValue(langAtom.forget());
    }
  }
}

/* virtual */ bool
nsHTMLStyleSheet::LangRule::MightMapInheritedStyleData()
{
  return true;
}

/* virtual */ bool
nsHTMLStyleSheet::LangRule::
GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty, nsCSSValue* aValue)
{
  MOZ_ASSERT(false, "GetDiscretelyAnimatedCSSValue is not implemented yet");
  return false;
}

#ifdef DEBUG
/* virtual */ void
nsHTMLStyleSheet::LangRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t index = aIndent; --index >= 0; ) {
    str.AppendLiteral("  ");
  }
  str.AppendLiteral("[lang rule] { language: \"");
  AppendUTF16toUTF8(nsDependentAtomString(mLang), str);
  str.AppendLiteral("\" }\n");
  fprintf_stderr(out, "%s", str.get());
}
#endif

// -----------------------------------------------------------

struct MappedAttrTableEntry : public PLDHashEntryHdr {
  nsMappedAttributes *mAttributes;
};

static PLDHashNumber
MappedAttrTable_HashKey(const void *key)
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
MappedAttrTable_MatchEntry(const PLDHashEntryHdr *hdr, const void *key)
{
  nsMappedAttributes *attributes =
    static_cast<nsMappedAttributes*>(const_cast<void*>(key));
  const MappedAttrTableEntry *entry =
    static_cast<const MappedAttrTableEntry*>(hdr);

  return attributes->Equals(entry->mAttributes);
}

static const PLDHashTableOps MappedAttrTable_Ops = {
  MappedAttrTable_HashKey,
  MappedAttrTable_MatchEntry,
  PLDHashTable::MoveEntryStub,
  MappedAttrTable_ClearEntry,
  nullptr
};

// -----------------------------------------------------------

struct LangRuleTableEntry : public PLDHashEntryHdr {
  RefPtr<nsHTMLStyleSheet::LangRule> mRule;
};

static PLDHashNumber
LangRuleTable_HashKey(const void *key)
{
  auto* lang = static_cast<const nsAtom*>(key);
  return lang->hash();
}

static void
LangRuleTable_ClearEntry(PLDHashTable *table, PLDHashEntryHdr *hdr)
{
  LangRuleTableEntry *entry = static_cast<LangRuleTableEntry*>(hdr);

  entry->~LangRuleTableEntry();
  memset(entry, 0, sizeof(LangRuleTableEntry));
}

static bool
LangRuleTable_MatchEntry(const PLDHashEntryHdr *hdr, const void *key)
{
  auto* lang = static_cast<const nsAtom*>(key);
  const LangRuleTableEntry *entry = static_cast<const LangRuleTableEntry*>(hdr);

  return entry->mRule->mLang == lang;
}

static void
LangRuleTable_InitEntry(PLDHashEntryHdr *hdr, const void *key)
{
  auto* lang = static_cast<const nsAtom*>(key);

  LangRuleTableEntry* entry = new (KnownNotNull, hdr) LangRuleTableEntry();

  // Create the unique rule for this language
  entry->mRule = new nsHTMLStyleSheet::LangRule(const_cast<nsAtom*>(lang));
}

static const PLDHashTableOps LangRuleTable_Ops = {
  LangRuleTable_HashKey,
  LangRuleTable_MatchEntry,
  PLDHashTable::MoveEntryStub,
  LangRuleTable_ClearEntry,
  LangRuleTable_InitEntry
};

// -----------------------------------------------------------

nsHTMLStyleSheet::nsHTMLStyleSheet(nsIDocument* aDocument)
  : mDocument(aDocument)
  , mTableQuirkColorRule(new TableQuirkColorRule())
  , mTableTHRule(new TableTHRule())
  , mMappedAttrTable(&MappedAttrTable_Ops, sizeof(MappedAttrTableEntry))
  , mMappedAttrsDirty(false)
  , mLangRuleTable(&LangRuleTable_Ops, sizeof(LangRuleTableEntry))
{
  MOZ_ASSERT(aDocument);
}

NS_IMPL_ISUPPORTS(nsHTMLStyleSheet, nsIStyleRuleProcessor)

/* virtual */ void
nsHTMLStyleSheet::RulesMatching(ElementRuleProcessorData* aData)
{
  nsRuleWalker *ruleWalker = aData->mRuleWalker;
  if (!ruleWalker->AuthorStyleDisabled()) {
    // if we have anchor colors, check if this is an anchor with an href
    if (aData->mElement->IsHTMLElement(nsGkAtoms::a)) {
      if (mLinkRule || mVisitedRule || mActiveRule) {
        EventStates state =
          nsCSSRuleProcessor::GetContentStateForVisitedHandling(
                                  aData->mElement,
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
    else if (aData->mElement->IsHTMLElement(nsGkAtoms::th)) {
      ruleWalker->Forward(mTableTHRule);
    }
    else if (aData->mElement->IsHTMLElement(nsGkAtoms::table)) {
      if (aData->mTreeMatchContext.mCompatMode == eCompatibility_NavQuirks) {
        ruleWalker->Forward(mTableQuirkColorRule);
      }
    }
  } // end html element

  // just get the style rules from the content.  For SVG we do this even if
  // author style is disabled, because SVG presentational hints aren't
  // considered style.
  if (!ruleWalker->AuthorStyleDisabled() || aData->mElement->IsSVGElement()) {
    aData->mElement->WalkContentStyleRules(ruleWalker);
  }

  // http://www.whatwg.org/specs/web-apps/current-work/multipage/elements.html#language
  // says that the xml:lang attribute overrides HTML's lang attribute,
  // so we need to do this after WalkContentStyleRules.
  const nsAttrValue* langAttr =
    aData->mElement->GetParsedAttr(nsGkAtoms::lang, kNameSpaceID_XML);
  if (langAttr) {
    MOZ_ASSERT(langAttr->Type() == nsAttrValue::eAtom);
    ruleWalker->Forward(LangRuleFor(langAttr->GetAtomValue()));
  }

  // Set the language to "x-math" on the <math> element, so that appropriate
  // font settings are used for MathML.
  if (aData->mElement->IsMathMLElement(nsGkAtoms::math)) {
    ruleWalker->Forward(LangRuleFor(nsGkAtoms::x_math));
  }
}

// Test if style is dependent on content state
/* virtual */ nsRestyleHint
nsHTMLStyleSheet::HasStateDependentStyle(StateRuleProcessorData* aData)
{
  if (aData->mElement->IsHTMLElement(nsGkAtoms::a) &&
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
nsHTMLStyleSheet::HasAttributeDependentStyle(
    AttributeRuleProcessorData* aData,
    RestyleHintData& aRestyleHintDataResult)
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
      element->IsHTMLElement(nsGkAtoms::a)) {
    return eRestyle_Self;
  }

  // Don't worry about the mDocumentColorRule since it only applies
  // to descendants of body, when we're already reresolving.

  // Handle the content style rules.
  if (element->IsAttributeMapped(aData->mAttribute)) {
    // cellpadding on tables is special and requires reresolving all
    // the cells in the table
    if (aData->mAttribute == nsGkAtoms::cellpadding &&
        element->IsHTMLElement(nsGkAtoms::table)) {
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

  mServoUnvisitedLinkDecl = nullptr;
  mServoVisitedLinkDecl = nullptr;
  mServoActiveLinkDecl = nullptr;

  mLangRuleTable.Clear();
  mMappedAttrTable.Clear();
  mMappedAttrsDirty = false;
}

nsresult
nsHTMLStyleSheet::ImplLinkColorSetter(
    RefPtr<HTMLColorRule>& aRule,
    RefPtr<RawServoDeclarationBlock>& aDecl,
    nscolor aColor)
{
  if (!mDocument || !mDocument->GetShell()) {
    return NS_OK;
  }

  RestyleManager* restyle =
    mDocument->GetShell()->GetPresContext()->RestyleManager();

  if (restyle->IsServo()) {
    MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal());
    aDecl = Servo_DeclarationBlock_CreateEmpty().Consume();
    Servo_DeclarationBlock_SetColorValue(aDecl.get(), eCSSProperty_color,
                                         aColor);
  } else {
    if (aRule && aRule->mColor == aColor) {
      return NS_OK;
    }

    aRule = new HTMLColorRule(aColor);
    if (!aRule) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Now make sure we restyle any links that might need it.  This
  // shouldn't happen often, so just rebuilding everything is ok.
  Element* root = mDocument->GetRootElement();
  if (root) {
    restyle->PostRestyleEvent(root, eRestyle_Subtree, nsChangeHint(0));
  }
  return NS_OK;
}

nsresult
nsHTMLStyleSheet::SetLinkColor(nscolor aColor)
{
  return ImplLinkColorSetter(mLinkRule, mServoUnvisitedLinkDecl, aColor);
}


nsresult
nsHTMLStyleSheet::SetActiveLinkColor(nscolor aColor)
{
  return ImplLinkColorSetter(mActiveRule, mServoActiveLinkDecl, aColor);
}

nsresult
nsHTMLStyleSheet::SetVisitedLinkColor(nscolor aColor)
{
  return ImplLinkColorSetter(mVisitedRule, mServoVisitedLinkDecl, aColor);
}

already_AddRefed<nsMappedAttributes>
nsHTMLStyleSheet::UniqueMappedAttributes(nsMappedAttributes* aMapped)
{
  mMappedAttrsDirty = true;
  auto entry = static_cast<MappedAttrTableEntry*>
                          (mMappedAttrTable.Add(aMapped, fallible));
  if (!entry)
    return nullptr;
  if (!entry->mAttributes) {
    // We added a new entry to the hashtable, so we have a new unique set.
    entry->mAttributes = aMapped;
  }
  RefPtr<nsMappedAttributes> ret = entry->mAttributes;
  return ret.forget();
}

void
nsHTMLStyleSheet::DropMappedAttributes(nsMappedAttributes* aMapped)
{
  NS_ENSURE_TRUE_VOID(aMapped);
#ifdef DEBUG
  uint32_t entryCount = mMappedAttrTable.EntryCount() - 1;
#endif

  mMappedAttrTable.Remove(aMapped);

  NS_ASSERTION(entryCount == mMappedAttrTable.EntryCount(), "not removed");
}

void
nsHTMLStyleSheet::CalculateMappedServoDeclarations()
{
  for (auto iter = mMappedAttrTable.Iter(); !iter.Done(); iter.Next()) {
    MappedAttrTableEntry* attr = static_cast<MappedAttrTableEntry*>(iter.Get());
    if (attr->mAttributes->GetServoStyle()) {
      // Only handle cases which haven't been filled in already
      continue;
    }
    attr->mAttributes->LazilyResolveServoDeclaration(mDocument);
  }
}

nsIStyleRule*
nsHTMLStyleSheet::LangRuleFor(const nsAtom* aLanguage)
{
  auto entry =
    static_cast<LangRuleTableEntry*>(mLangRuleTable.Add(aLanguage, fallible));
  if (!entry) {
    NS_ASSERTION(false, "out of memory");
    return nullptr;
  }
  return entry->mRule;
}

size_t
nsHTMLStyleSheet::DOMSizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  n += mMappedAttrTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mMappedAttrTable.ConstIter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<MappedAttrTableEntry*>(iter.Get());
    n += entry->mAttributes->SizeOfIncludingThis(aMallocSizeOf);
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
