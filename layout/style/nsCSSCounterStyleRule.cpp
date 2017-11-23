/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a Gecko @counter-style rule */

#include "nsCSSCounterStyleRule.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/CSSCounterStyleRuleBinding.h"
#include "nsCSSParser.h"
#include "nsStyleUtil.h"

using namespace mozilla;
using namespace mozilla::dom;

nsCSSCounterStyleRule::nsCSSCounterStyleRule(const nsCSSCounterStyleRule& aCopy)
  : Rule(aCopy)
  , mName(aCopy.mName)
  , mGeneration(aCopy.mGeneration)
{
  for (size_t i = 0; i < ArrayLength(mValues); ++i) {
    mValues[i] = aCopy.mValues[i];
  }
}

nsCSSCounterStyleRule::~nsCSSCounterStyleRule()
{
}

/* virtual */ already_AddRefed<css::Rule>
nsCSSCounterStyleRule::Clone() const
{
  RefPtr<css::Rule> clone = new nsCSSCounterStyleRule(*this);
  return clone.forget();
}

nsCSSCounterStyleRule::Getter const
nsCSSCounterStyleRule::kGetters[] = {
#define CSS_COUNTER_DESC(name_, method_) &nsCSSCounterStyleRule::Get##method_,
#include "nsCSSCounterDescList.h"
#undef CSS_COUNTER_DESC
};

NS_IMPL_ADDREF_INHERITED(nsCSSCounterStyleRule, mozilla::css::Rule)
NS_IMPL_RELEASE_INHERITED(nsCSSCounterStyleRule, mozilla::css::Rule)

// QueryInterface implementation for nsCSSCounterStyleRule
// If this ever gets its own cycle-collection bits, reevaluate our IsCCLeaf
// implementation.
NS_INTERFACE_MAP_BEGIN(nsCSSCounterStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSCounterStyleRule)
NS_INTERFACE_MAP_END_INHERITING(mozilla::css::Rule)

bool
nsCSSCounterStyleRule::IsCCLeaf() const
{
  return Rule::IsCCLeaf();
}

#ifdef DEBUG
void
nsCSSCounterStyleRule::List(FILE* out, int32_t aIndent) const
{
  nsCString baseInd, descInd;
  for (int32_t indent = aIndent; --indent >= 0; ) {
    baseInd.AppendLiteral("  ");
  }
  descInd = baseInd;
  descInd.AppendLiteral("  ");

  nsDependentAtomString name(mName);
  fprintf_stderr(out, "%s@counter-style %s (rev.%u) {\n",
                 baseInd.get(), NS_ConvertUTF16toUTF8(name).get(),
                 mGeneration);
  // TODO
  fprintf_stderr(out, "%s}\n", baseInd.get());
}
#endif

/* virtual */ int32_t
nsCSSCounterStyleRule::GetType() const
{
  return Rule::COUNTER_STYLE_RULE;
}

uint16_t
nsCSSCounterStyleRule::Type() const
{
  return nsIDOMCSSRule::COUNTER_STYLE_RULE;
}

void
nsCSSCounterStyleRule::GetCssTextImpl(nsAString& aCssText) const
{
  aCssText.AssignLiteral(u"@counter-style ");
  nsDependentAtomString name(mName);
  nsStyleUtil::AppendEscapedCSSIdent(name, aCssText);
  aCssText.AppendLiteral(u" {\n");
  for (nsCSSCounterDesc id = nsCSSCounterDesc(0);
       id < eCSSCounterDesc_COUNT;
       id = nsCSSCounterDesc(id + 1)) {
    if (mValues[id].GetUnit() != eCSSUnit_Null) {
      nsAutoString tmp;
      // This is annoying.  We want to be a const method, but kGetters stores
      // XPCOM method pointers, which aren't const methods.  The thing is,
      // none of those mutate "this".  So it's OK to cast away const here.
      (const_cast<nsCSSCounterStyleRule*>(this)->*kGetters[id])(tmp);
      aCssText.AppendLiteral(u"  ");
      AppendASCIItoUTF16(nsCSSProps::GetStringValue(id), aCssText);
      aCssText.AppendLiteral(u": ");
      aCssText.Append(tmp);
      aCssText.AppendLiteral(u";\n");
    }
  }
  aCssText.AppendLiteral(u"}");
}

// nsIDOMCSSCounterStyleRule methods
NS_IMETHODIMP
nsCSSCounterStyleRule::GetName(nsAString& aName)
{
  aName.Truncate();
  nsDependentAtomString name(mName);
  nsStyleUtil::AppendEscapedCSSIdent(name, aName);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSCounterStyleRule::SetName(const nsAString& aName)
{
  nsCSSParser parser;
  if (RefPtr<nsAtom> name = parser.ParseCounterStyleName(aName, nullptr)) {
    nsIDocument* doc = GetDocument();
    MOZ_AUTO_DOC_UPDATE(doc, UPDATE_STYLE, true);

    mName = name;

    if (StyleSheet* sheet = GetStyleSheet()) {
      if (sheet->IsGecko()) {
        sheet->AsGecko()->SetModifiedByChildRule();
      }
      if (doc) {
        doc->StyleRuleChanged(sheet, this);
      }
    }
  }
  return NS_OK;
}

int32_t
nsCSSCounterStyleRule::GetSystem() const
{
  const nsCSSValue& system = GetDesc(eCSSCounterDesc_System);
  switch (system.GetUnit()) {
    case eCSSUnit_Enumerated:
      return system.GetIntValue();
    case eCSSUnit_Pair:
      return system.GetPairValue().mXValue.GetIntValue();
    default:
      return NS_STYLE_COUNTER_SYSTEM_SYMBOLIC;
  }
}

const nsCSSValue&
nsCSSCounterStyleRule::GetSystemArgument() const
{
  const nsCSSValue& system = GetDesc(eCSSCounterDesc_System);
  MOZ_ASSERT(system.GetUnit() == eCSSUnit_Pair,
             "Invalid system value");
  return system.GetPairValue().mYValue;
}

void
nsCSSCounterStyleRule::SetDesc(nsCSSCounterDesc aDescID, const nsCSSValue& aValue)
{
  MOZ_ASSERT(aDescID >= 0 && aDescID < eCSSCounterDesc_COUNT,
             "descriptor ID out of range");

  nsIDocument* doc = GetDocument();
  MOZ_AUTO_DOC_UPDATE(doc, UPDATE_STYLE, true);

  mValues[aDescID] = aValue;
  mGeneration++;

  if (StyleSheet* sheet = GetStyleSheet()) {
    if (sheet->IsGecko()) {
      sheet->AsGecko()->SetModifiedByChildRule();
    }
    if (doc) {
      doc->StyleRuleChanged(sheet, this);
    }
  }
}

NS_IMETHODIMP
nsCSSCounterStyleRule::GetSystem(nsAString& aSystem)
{
  const nsCSSValue& value = GetDesc(eCSSCounterDesc_System);
  if (value.GetUnit() == eCSSUnit_Null) {
    aSystem.Truncate();
    return NS_OK;
  }

  aSystem = NS_ConvertASCIItoUTF16(nsCSSProps::ValueToKeyword(
          GetSystem(), nsCSSProps::kCounterSystemKTable));
  if (value.GetUnit() == eCSSUnit_Pair) {
    aSystem.Append(' ');
    GetSystemArgument().AppendToString(eCSSProperty_UNKNOWN, aSystem);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSCounterStyleRule::GetSymbols(nsAString& aSymbols)
{
  const nsCSSValue& value = GetDesc(eCSSCounterDesc_Symbols);

  aSymbols.Truncate();
  if (value.GetUnit() == eCSSUnit_List) {
    for (const nsCSSValueList* item = value.GetListValue();
         item; item = item->mNext) {
      item->mValue.AppendToString(eCSSProperty_UNKNOWN, aSymbols);
      if (item->mNext) {
        aSymbols.Append(' ');
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSCounterStyleRule::GetAdditiveSymbols(nsAString& aSymbols)
{
  const nsCSSValue& value = GetDesc(eCSSCounterDesc_AdditiveSymbols);

  aSymbols.Truncate();
  if (value.GetUnit() == eCSSUnit_PairList) {
    for (const nsCSSValuePairList* item = value.GetPairListValue();
         item; item = item->mNext) {
      item->mXValue.AppendToString(eCSSProperty_UNKNOWN, aSymbols);
      aSymbols.Append(' ');
      item->mYValue.AppendToString(eCSSProperty_UNKNOWN, aSymbols);
      if (item->mNext) {
        aSymbols.AppendLiteral(", ");
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSCounterStyleRule::GetRange(nsAString& aRange)
{
  const nsCSSValue& value = GetDesc(eCSSCounterDesc_Range);

  switch (value.GetUnit()) {
    case eCSSUnit_Auto:
      aRange.AssignLiteral(u"auto");
      break;

    case eCSSUnit_PairList:
      aRange.Truncate();
      for (const nsCSSValuePairList* item = value.GetPairListValue();
          item; item = item->mNext) {
        const nsCSSValue& lower = item->mXValue;
        const nsCSSValue& upper = item->mYValue;
        if (lower.GetUnit() == eCSSUnit_Enumerated) {
          NS_ASSERTION(lower.GetIntValue() ==
                       NS_STYLE_COUNTER_RANGE_INFINITE,
                       "Unrecognized keyword");
          aRange.AppendLiteral("infinite");
        } else {
          aRange.AppendInt(lower.GetIntValue());
        }
        aRange.Append(' ');
        if (upper.GetUnit() == eCSSUnit_Enumerated) {
          NS_ASSERTION(upper.GetIntValue() ==
                       NS_STYLE_COUNTER_RANGE_INFINITE,
                       "Unrecognized keyword");
          aRange.AppendLiteral("infinite");
        } else {
          aRange.AppendInt(upper.GetIntValue());
        }
        if (item->mNext) {
          aRange.AppendLiteral(", ");
        }
      }
      break;

    default:
      aRange.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSCounterStyleRule::GetSpeakAs(nsAString& aSpeakAs)
{
  const nsCSSValue& value = GetDesc(eCSSCounterDesc_SpeakAs);

  switch (value.GetUnit()) {
    case eCSSUnit_Enumerated:
      switch (value.GetIntValue()) {
        case NS_STYLE_COUNTER_SPEAKAS_BULLETS:
          aSpeakAs.AssignLiteral(u"bullets");
          break;
        case NS_STYLE_COUNTER_SPEAKAS_NUMBERS:
          aSpeakAs.AssignLiteral(u"numbers");
          break;
        case NS_STYLE_COUNTER_SPEAKAS_WORDS:
          aSpeakAs.AssignLiteral(u"words");
          break;
        case NS_STYLE_COUNTER_SPEAKAS_SPELL_OUT:
          aSpeakAs.AssignLiteral(u"spell-out");
          break;
        default:
          NS_NOTREACHED("Unknown speech synthesis");
      }
      break;

    case eCSSUnit_Auto:
    case eCSSUnit_AtomIdent:
      aSpeakAs.Truncate();
      value.AppendToString(eCSSProperty_UNKNOWN, aSpeakAs);
      break;

    case eCSSUnit_Null:
      aSpeakAs.Truncate();
      break;

    default:
      NS_NOTREACHED("Unknown speech synthesis");
      aSpeakAs.Truncate();
  }
  return NS_OK;
}

nsresult
nsCSSCounterStyleRule::GetDescriptor(nsCSSCounterDesc aDescID,
                                     nsAString& aValue)
{
  NS_ASSERTION(aDescID == eCSSCounterDesc_Negative ||
               aDescID == eCSSCounterDesc_Prefix ||
               aDescID == eCSSCounterDesc_Suffix ||
               aDescID == eCSSCounterDesc_Pad ||
               aDescID == eCSSCounterDesc_Fallback,
               "Unexpected descriptor");
  const nsCSSValue& value = GetDesc(aDescID);
  aValue.Truncate();
  if (value.GetUnit() != eCSSUnit_Null) {
    value.AppendToString(eCSSProperty_UNKNOWN, aValue);
  }
  return NS_OK;
}

#define CSS_COUNTER_DESC_GETTER(name_)                    \
NS_IMETHODIMP                                             \
nsCSSCounterStyleRule::Get##name_(nsAString& a##name_)    \
{                                                         \
  return GetDescriptor(eCSSCounterDesc_##name_, a##name_);\
}
CSS_COUNTER_DESC_GETTER(Negative)
CSS_COUNTER_DESC_GETTER(Prefix)
CSS_COUNTER_DESC_GETTER(Suffix)
CSS_COUNTER_DESC_GETTER(Pad)
CSS_COUNTER_DESC_GETTER(Fallback)
#undef CSS_COUNTER_DESC_GETTER

/* static */ bool
nsCSSCounterStyleRule::CheckDescValue(int32_t aSystem,
                                      nsCSSCounterDesc aDescID,
                                      const nsCSSValue& aValue)
{
  switch (aDescID) {
    case eCSSCounterDesc_System:
      if (aValue.GetUnit() != eCSSUnit_Pair) {
        return aValue.GetIntValue() == aSystem;
      } else {
        return aValue.GetPairValue().mXValue.GetIntValue() == aSystem;
      }

    case eCSSCounterDesc_Symbols:
      switch (aSystem) {
        case NS_STYLE_COUNTER_SYSTEM_NUMERIC:
        case NS_STYLE_COUNTER_SYSTEM_ALPHABETIC:
          // for these two system, the list must contain at least 2 elements
          return aValue.GetListValue()->mNext;
        case NS_STYLE_COUNTER_SYSTEM_EXTENDS:
          // for extends system, no symbols should be set
          return false;
        default:
          return true;
      }

    case eCSSCounterDesc_AdditiveSymbols:
      switch (aSystem) {
        case NS_STYLE_COUNTER_SYSTEM_EXTENDS:
          return false;
        default:
          return true;
      }

    default:
      return true;
  }
}

nsresult
nsCSSCounterStyleRule::SetDescriptor(nsCSSCounterDesc aDescID,
                                     const nsAString& aValue)
{
  nsCSSParser parser;
  nsCSSValue value;
  nsIURI* baseURL = nullptr;
  nsIPrincipal* principal = nullptr;
  if (StyleSheet* sheet = GetStyleSheet()) {
    baseURL = sheet->GetBaseURI();
    principal = sheet->Principal();
  }
  if (parser.ParseCounterDescriptor(aDescID, aValue, nullptr,
                                    baseURL, principal, value)) {
    if (CheckDescValue(GetSystem(), aDescID, value)) {
      SetDesc(aDescID, value);
    }
  }
  return NS_OK;
}

#define CSS_COUNTER_DESC_SETTER(name_)                        \
NS_IMETHODIMP                                                 \
nsCSSCounterStyleRule::Set##name_(const nsAString& a##name_)  \
{                                                             \
  return SetDescriptor(eCSSCounterDesc_##name_, a##name_);    \
}
CSS_COUNTER_DESC_SETTER(System)
CSS_COUNTER_DESC_SETTER(Symbols)
CSS_COUNTER_DESC_SETTER(AdditiveSymbols)
CSS_COUNTER_DESC_SETTER(Negative)
CSS_COUNTER_DESC_SETTER(Prefix)
CSS_COUNTER_DESC_SETTER(Suffix)
CSS_COUNTER_DESC_SETTER(Range)
CSS_COUNTER_DESC_SETTER(Pad)
CSS_COUNTER_DESC_SETTER(Fallback)
CSS_COUNTER_DESC_SETTER(SpeakAs)
#undef CSS_COUNTER_DESC_SETTER

/* virtual */ size_t
nsCSSCounterStyleRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

/* virtual */ JSObject*
nsCSSCounterStyleRule::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto)
{
  return CSSCounterStyleRuleBinding::Wrap(aCx, this, aGivenProto);
}
