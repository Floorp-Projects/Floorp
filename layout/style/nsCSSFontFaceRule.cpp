/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a Gecko @font-face rule */

#include "nsCSSFontFaceRule.h"

#include "mozilla/dom/CSSFontFaceRuleBinding.h"
#include "mozilla/dom/CSSStyleDeclarationBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

// -------------------------------------------
// nsCSSFontFaceStyleDecl and related routines
//

// Mapping from nsCSSFontDesc codes to CSSFontFaceDescriptors fields.
nsCSSValue CSSFontFaceDescriptors::* const
CSSFontFaceDescriptors::Fields[] = {
#define CSS_FONT_DESC(name_, method_) &CSSFontFaceDescriptors::m##method_,
#include "nsCSSFontDescList.h"
#undef CSS_FONT_DESC
};

const nsCSSValue&
CSSFontFaceDescriptors::Get(nsCSSFontDesc aFontDescID) const
{
  MOZ_ASSERT(aFontDescID > eCSSFontDesc_UNKNOWN &&
             aFontDescID < eCSSFontDesc_COUNT);
  return this->*CSSFontFaceDescriptors::Fields[aFontDescID];
}

nsCSSValue&
CSSFontFaceDescriptors::Get(nsCSSFontDesc aFontDescID)
{
  MOZ_ASSERT(aFontDescID > eCSSFontDesc_UNKNOWN &&
             aFontDescID < eCSSFontDesc_COUNT);
  return this->*CSSFontFaceDescriptors::Fields[aFontDescID];
}

// QueryInterface implementation for nsCSSFontFaceStyleDecl
NS_INTERFACE_MAP_BEGIN(nsCSSFontFaceStyleDecl)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsICSSDeclaration)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  // We forward the cycle collection interfaces to ContainingRule(), which is
  // never null (in fact, we're part of that object!)
  if (aIID.Equals(NS_GET_IID(nsCycleCollectionISupports)) ||
      aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {
    return ContainingRule()->QueryInterface(aIID, aInstancePtr);
  }
  else
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF_USING_AGGREGATOR(nsCSSFontFaceStyleDecl, ContainingRule())
NS_IMPL_RELEASE_USING_AGGREGATOR(nsCSSFontFaceStyleDecl, ContainingRule())

// helper for string GetPropertyValue and RemovePropertyValue
nsresult
nsCSSFontFaceStyleDecl::GetPropertyValue(nsCSSFontDesc aFontDescID,
                                         nsAString & aResult) const
{
  NS_ENSURE_ARG_RANGE(aFontDescID, eCSSFontDesc_UNKNOWN,
                      eCSSFontDesc_COUNT - 1);

  aResult.Truncate();
  if (aFontDescID == eCSSFontDesc_UNKNOWN)
    return NS_OK;

  const nsCSSValue& val = mDescriptors.Get(aFontDescID);

  if (val.GetUnit() == eCSSUnit_Null) {
    // Avoid having to check no-value in the Family and Src cases below.
    return NS_OK;
  }

  switch (aFontDescID) {
  case eCSSFontDesc_Family: {
      // we don't use nsCSSValue::AppendToString here because it doesn't
      // canonicalize the way we want, and anyway it's overkill when
      // we know we have eCSSUnit_String
      NS_ASSERTION(val.GetUnit() == eCSSUnit_String, "unexpected unit");
      nsDependentString family(val.GetStringBufferValue());
      nsStyleUtil::AppendEscapedCSSString(family, aResult);
      return NS_OK;
    }

  case eCSSFontDesc_Style:
    val.AppendToString(eCSSProperty_font_style, aResult);
    return NS_OK;

  case eCSSFontDesc_Weight:
    val.AppendToString(eCSSProperty_font_weight, aResult);
    return NS_OK;

  case eCSSFontDesc_Stretch:
    val.AppendToString(eCSSProperty_font_stretch, aResult);
    return NS_OK;

  case eCSSFontDesc_FontFeatureSettings:
    nsStyleUtil::AppendFontFeatureSettings(val, aResult);
    return NS_OK;

  case eCSSFontDesc_FontLanguageOverride:
    val.AppendToString(eCSSProperty_font_language_override, aResult);
    return NS_OK;

  case eCSSFontDesc_Display:
    NS_ASSERTION(val.GetUnit() == eCSSUnit_Enumerated,
                 "unknown unit for font-display descriptor");
    AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(val.GetIntValue(),
                                       nsCSSProps::kFontDisplayKTable), aResult);
    return NS_OK;

  case eCSSFontDesc_Src:
    nsStyleUtil::AppendSerializedFontSrc(val, aResult);
    return NS_OK;

  case eCSSFontDesc_UnicodeRange:
    nsStyleUtil::AppendUnicodeRange(val, aResult);
    return NS_OK;

  case eCSSFontDesc_UNKNOWN:
  case eCSSFontDesc_COUNT:
    ;
  }
  NS_NOTREACHED("nsCSSFontFaceStyleDecl::GetPropertyValue: "
                "out-of-range value got to the switch");
  return NS_ERROR_INVALID_ARG;
}


NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetCssText(nsAString & aCssText)
{
  GetCssTextImpl(aCssText);
  return NS_OK;
}

void
nsCSSFontFaceStyleDecl::GetCssTextImpl(nsAString& aCssText) const
{
  nsAutoString descStr;

  aCssText.Truncate();
  for (nsCSSFontDesc id = nsCSSFontDesc(eCSSFontDesc_UNKNOWN + 1);
       id < eCSSFontDesc_COUNT;
       id = nsCSSFontDesc(id + 1)) {
    if (mDescriptors.Get(id).GetUnit() != eCSSUnit_Null &&
        NS_SUCCEEDED(GetPropertyValue(id, descStr))) {
      NS_ASSERTION(descStr.Length() > 0,
                   "GetCssText: non-null unit, empty property value");
      aCssText.AppendLiteral("  ");
      aCssText.AppendASCII(nsCSSProps::GetStringValue(id).get());
      aCssText.AppendLiteral(": ");
      aCssText.Append(descStr);
      aCssText.AppendLiteral(";\n");
    }
  }
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::SetCssText(const nsAString& aCssText,
                                   nsIPrincipal* aSubjectPrincipal)
{
  return NS_ERROR_NOT_IMPLEMENTED; // bug 443978
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetPropertyValue(const nsAString & propertyName,
                                         nsAString & aResult)
{
  return GetPropertyValue(nsCSSProps::LookupFontDesc(propertyName), aResult);
}

already_AddRefed<dom::CSSValue>
nsCSSFontFaceStyleDecl::GetPropertyCSSValue(const nsAString & propertyName,
                                            ErrorResult& aRv)
{
  // ??? nsDOMCSSDeclaration returns null/NS_OK, but that seems wrong.
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::RemoveProperty(const nsAString & propertyName,
                                       nsAString & aResult)
{
  nsCSSFontDesc descID = nsCSSProps::LookupFontDesc(propertyName);
  NS_ASSERTION(descID >= eCSSFontDesc_UNKNOWN &&
               descID < eCSSFontDesc_COUNT,
               "LookupFontDesc returned value out of range");

  if (descID == eCSSFontDesc_UNKNOWN) {
    aResult.Truncate();
  } else {
    nsresult rv = GetPropertyValue(descID, aResult);
    NS_ENSURE_SUCCESS(rv, rv);
    mDescriptors.Get(descID).Reset();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetPropertyPriority(const nsAString & propertyName,
                                            nsAString & aResult)
{
  // font descriptors do not have priorities at present
  aResult.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::SetProperty(const nsAString& propertyName,
                                    const nsAString& value,
                                    const nsAString& priority,
                                    nsIPrincipal* aSubjectPrincipal)
{
  // FIXME(heycam): If we are changing unicode-range, then a FontFace object
  // representing this rule must have its mUnicodeRange value invalidated.

  return NS_ERROR_NOT_IMPLEMENTED; // bug 443978
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetLength(uint32_t *aLength)
{
  uint32_t len = 0;
  for (nsCSSFontDesc id = nsCSSFontDesc(eCSSFontDesc_UNKNOWN + 1);
       id < eCSSFontDesc_COUNT;
       id = nsCSSFontDesc(id + 1))
    if (mDescriptors.Get(id).GetUnit() != eCSSUnit_Null)
      len++;

  *aLength = len;
  return NS_OK;
}

void
nsCSSFontFaceStyleDecl::IndexedGetter(uint32_t index, bool& aFound, nsAString & aResult)
{
  int32_t nset = -1;
  for (nsCSSFontDesc id = nsCSSFontDesc(eCSSFontDesc_UNKNOWN + 1);
       id < eCSSFontDesc_COUNT;
       id = nsCSSFontDesc(id + 1)) {
    if (mDescriptors.Get(id).GetUnit() != eCSSUnit_Null) {
      nset++;
      if (nset == int32_t(index)) {
        aFound = true;
        aResult.AssignASCII(nsCSSProps::GetStringValue(id).get());
        return;
      }
    }
  }
  aFound = false;
}

css::Rule*
nsCSSFontFaceStyleDecl::GetParentRule()
{
  return ContainingRule();
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetPropertyValue(const nsCSSPropertyID aPropID,
                                         nsAString& aValue)
{
  return
    GetPropertyValue(NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(aPropID)),
                     aValue);
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::SetPropertyValue(const nsCSSPropertyID aPropID,
                                         const nsAString& aValue,
                                         nsIPrincipal* aSubjectPrincipal)
{
  return SetProperty(NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(aPropID)),
                     aValue, EmptyString(), aSubjectPrincipal);
}

nsINode*
nsCSSFontFaceStyleDecl::GetParentObject()
{
  return ContainingRule()->GetDocument();
}

DocGroup*
nsCSSFontFaceStyleDecl::GetDocGroup() const
{
  nsIDocument* document = ContainingRule()->GetDocument();
  return document ? document->GetDocGroup() : nullptr;
}

JSObject*
nsCSSFontFaceStyleDecl::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::CSSStyleDeclarationBinding::Wrap(cx, this, aGivenProto);
}

// -------------------------------------------
// nsCSSFontFaceRule
//

/* virtual */ already_AddRefed<css::Rule>
nsCSSFontFaceRule::Clone() const
{
  RefPtr<css::Rule> clone = new nsCSSFontFaceRule(*this);
  return clone.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsCSSFontFaceRule)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsCSSFontFaceRule,
                                               mozilla::css::Rule)
  // Keep this in sync with IsCCLeaf.

  // Trace the wrapper for our declaration.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecl.TraceWrapper(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsCSSFontFaceRule,
                                                mozilla::css::Rule)
  // Keep this in sync with IsCCLeaf.

  // Unlink the wrapper for our declaraton.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecl.ReleaseWrapper(static_cast<nsISupports*>(p));
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsCSSFontFaceRule,
                                                  mozilla::css::Rule)
  // Keep this in sync with IsCCLeaf.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool
nsCSSFontFaceRule::IsCCLeaf() const
{
  if (!Rule::IsCCLeaf()) {
    return false;
  }

  return !mDecl.PreservingWrapper();
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(nsCSSFontFaceRule, mozilla::css::Rule)

#ifdef DEBUG
void
nsCSSFontFaceRule::List(FILE* out, int32_t aIndent) const
{
  nsCString baseInd, descInd;
  for (int32_t indent = aIndent; --indent >= 0; ) {
    baseInd.AppendLiteral("  ");
    descInd.AppendLiteral("  ");
  }
  descInd.AppendLiteral("  ");

  nsString descStr;

  fprintf_stderr(out, "%s@font-face {\n", baseInd.get());
  for (nsCSSFontDesc id = nsCSSFontDesc(eCSSFontDesc_UNKNOWN + 1);
       id < eCSSFontDesc_COUNT;
       id = nsCSSFontDesc(id + 1))
    if (mDecl.mDescriptors.Get(id).GetUnit() != eCSSUnit_Null) {
      if (NS_FAILED(mDecl.GetPropertyValue(id, descStr)))
        descStr.AssignLiteral("#<serialization error>");
      else if (descStr.Length() == 0)
        descStr.AssignLiteral("#<serialization missing>");
      fprintf_stderr(out, "%s%s: %s\n",
                     descInd.get(), nsCSSProps::GetStringValue(id).get(),
                     NS_ConvertUTF16toUTF8(descStr).get());
    }
  fprintf_stderr(out, "%s}\n", baseInd.get());
}
#endif

/* virtual */ int32_t
nsCSSFontFaceRule::GetType() const
{
  return Rule::FONT_FACE_RULE;
}

uint16_t
nsCSSFontFaceRule::Type() const
{
  return CSSRuleBinding::FONT_FACE_RULE;
}

void
nsCSSFontFaceRule::GetCssTextImpl(nsAString& aCssText) const
{
  nsAutoString propText;
  mDecl.GetCssTextImpl(propText);

  aCssText.AssignLiteral("@font-face {\n");
  aCssText.Append(propText);
  aCssText.Append('}');
}

nsICSSDeclaration*
nsCSSFontFaceRule::Style()
{
  return &mDecl;
}

// Arguably these should forward to nsCSSFontFaceStyleDecl methods.
void
nsCSSFontFaceRule::SetDesc(nsCSSFontDesc aDescID, nsCSSValue const & aValue)
{
  NS_PRECONDITION(aDescID > eCSSFontDesc_UNKNOWN &&
                  aDescID < eCSSFontDesc_COUNT,
                  "aDescID out of range in nsCSSFontFaceRule::SetDesc");

  // FIXME: handle dynamic changes

  // FIXME(heycam): If we are changing unicode-range, then a FontFace object
  // representing this rule must have its mUnicodeRange value invalidated.

  mDecl.mDescriptors.Get(aDescID) = aValue;
}

void
nsCSSFontFaceRule::GetDesc(nsCSSFontDesc aDescID, nsCSSValue & aValue)
{
  NS_PRECONDITION(aDescID > eCSSFontDesc_UNKNOWN &&
                  aDescID < eCSSFontDesc_COUNT,
                  "aDescID out of range in nsCSSFontFaceRule::GetDesc");

  aValue = mDecl.mDescriptors.Get(aDescID);
}

/* virtual */ size_t
nsCSSFontFaceRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mDecl
}

/* virtual */ JSObject*
nsCSSFontFaceRule::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto)
{
  return CSSFontFaceRuleBinding::Wrap(aCx, this, aGivenProto);
}
