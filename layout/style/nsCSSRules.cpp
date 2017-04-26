/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rules in a CSS stylesheet other than style rules (e.g., @import rules) */

#include "nsCSSRules.h"
#include "nsCSSFontFaceRule.h"

#include "mozilla/Attributes.h"

#include "nsCSSValue.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/css/ImportRule.h"
#include "mozilla/css/NameSpaceRule.h"

#include "nsString.h"
#include "nsIAtom.h"

#include "nsCSSProps.h"

#include "nsCOMPtr.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsMediaList.h"
#include "mozilla/dom/CSSRuleList.h"
#include "nsIDocument.h"
#include "nsPresContext.h"

#include "nsContentUtils.h"
#include "nsError.h"
#include "nsStyleUtil.h"
#include "mozilla/DeclarationBlockInlines.h"
#include "nsCSSParser.h"
#include "nsDOMClassInfoID.h"
#include "mozilla/dom/CSSStyleDeclarationBinding.h"
#include "mozilla/dom/CSSImportRuleBinding.h"
#include "mozilla/dom/CSSMozDocumentRuleBinding.h"
#include "mozilla/dom/CSSFontFaceRuleBinding.h"
#include "mozilla/dom/CSSFontFeatureValuesRuleBinding.h"
#include "mozilla/dom/CSSKeyframeRuleBinding.h"
#include "mozilla/dom/CSSKeyframesRuleBinding.h"
#include "mozilla/dom/CSSCounterStyleRuleBinding.h"
#include "StyleRule.h"
#include "nsFont.h"
#include "nsIURI.h"
#include "mozAutoDocUpdate.h"
#include "nsCCUncollectableMarker.h"
#include "nsWrapperCacheInlines.h"

using namespace mozilla;
using namespace mozilla::dom;

// base class for all rule types in a CSS style sheet

namespace mozilla {
namespace css {

NS_IMPL_CYCLE_COLLECTING_ADDREF(Rule)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Rule)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Rule)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(Rule)

bool
Rule::IsCCLeaf() const
{
  return !PreservingWrapper();
}

bool
Rule::IsKnownLive() const
{
  if (HasKnownLiveWrapper()) {
    return true;
  }

  StyleSheet* sheet = GetStyleSheet();
  if (!sheet) {
    return false;
  }

  if (!sheet->IsOwnedByDocument()) {
    return false;
  }

  return nsCCUncollectableMarker::InGeneration(
    sheet->GetAssociatedDocument()->GetMarkedCCGeneration());
}

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(Rule)
  return tmp->IsCCLeaf() || tmp->IsKnownLive();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(Rule)
  // Please see documentation for nsCycleCollectionParticipant::CanSkip* for why
  // we need to check HasNothingToTrace here but not in the other two CanSkip
  // methods.
  return tmp->IsCCLeaf() ||
    (tmp->IsKnownLive() && tmp->HasNothingToTrace(tmp));
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(Rule)
  return tmp->IsCCLeaf() || tmp->IsKnownLive();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

/* virtual */ void
Rule::SetStyleSheet(StyleSheet* aSheet)
{
  // We don't reference count this up reference. The style sheet
  // will tell us when it's going away or when we're detached from
  // it.
  mSheet = aSheet;
}

NS_IMETHODIMP
Rule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  NS_IF_ADDREF(*aParentRule = mParentRule);
  return NS_OK;
}

NS_IMETHODIMP
Rule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  NS_ENSURE_ARG_POINTER(aSheet);

  NS_IF_ADDREF(*aSheet = GetStyleSheet());
  return NS_OK;
}

/* virtual */ css::Rule*
Rule::GetCSSRule()
{
  return this;
}

NS_IMETHODIMP
Rule::GetType(uint16_t* aType)
{
  *aType = Type();
  return NS_OK;
}

NS_IMETHODIMP
Rule::SetCssText(const nsAString& aCssText)
{
  // We used to throw for some rule types, but not all.  Specifically, we did
  // not throw for StyleRule.  Let's just always not throw.
  return NS_OK;
}

NS_IMETHODIMP
Rule::GetCssText(nsAString& aCssText)
{
  GetCssTextImpl(aCssText);
  return NS_OK;
}

Rule*
Rule::GetParentRule() const
{
  return mParentRule;
}

// -------------------------------------------
// ImportRule
//

ImportRule::ImportRule(nsMediaList* aMedia, const nsString& aURLSpec,
                       uint32_t aLineNumber, uint32_t aColumnNumber)
  : Rule(aLineNumber, aColumnNumber)
  , mURLSpec(aURLSpec)
  , mMedia(aMedia)
{
  MOZ_ASSERT(aMedia);
  // XXXbz This is really silly.... the mMedia here will be replaced
  // with itself if we manage to load a sheet.  Which should really
  // never fail nowadays, in sane cases.
}

ImportRule::ImportRule(const ImportRule& aCopy)
  : Rule(aCopy),
    mURLSpec(aCopy.mURLSpec)
{
  // Whether or not an @import rule has a null sheet is a permanent
  // property of that @import rule, since it is null only if the target
  // sheet failed security checks.
  if (aCopy.mChildSheet) {
    RefPtr<StyleSheet> sheet =
      aCopy.mChildSheet->Clone(nullptr, this, nullptr, nullptr);
    SetSheet(static_cast<CSSStyleSheet*>(sheet.get()));
    // SetSheet sets mMedia appropriately
  } else {
    // We better just copy mMedia from aCopy, since we have nowhere else to get
    // one.
    mMedia = aCopy.mMedia;
  }
}

ImportRule::~ImportRule()
{
  if (mChildSheet) {
    mChildSheet->SetOwnerRule(nullptr);
  }
}

NS_IMPL_ADDREF_INHERITED(ImportRule, Rule)
NS_IMPL_RELEASE_INHERITED(ImportRule, Rule)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ImportRule, Rule, mMedia, mChildSheet)

bool
ImportRule::IsCCLeaf() const
{
  // We're not a leaf.
  return false;
}

// QueryInterface implementation for ImportRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ImportRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSImportRule)
NS_INTERFACE_MAP_END_INHERITING(Rule)

#ifdef DEBUG
/* virtual */ void
ImportRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  // Indent
  for (int32_t indent = aIndent; --indent >= 0; ) {
    str.AppendLiteral("  ");
  }

  str.AppendLiteral("@import \"");
  AppendUTF16toUTF8(mURLSpec, str);
  str.AppendLiteral("\" ");

  nsAutoString mediaText;
  mMedia->GetText(mediaText);
  AppendUTF16toUTF8(mediaText, str);
  str.AppendLiteral("\n");
  fprintf_stderr(out, "%s", str.get());
}
#endif

/* virtual */ int32_t
ImportRule::GetType() const
{
  return Rule::IMPORT_RULE;
}

/* virtual */ already_AddRefed<Rule>
ImportRule::Clone() const
{
  RefPtr<Rule> clone = new ImportRule(*this);
  return clone.forget();
}

void
ImportRule::SetSheet(CSSStyleSheet* aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");

  // set the new sheet
  mChildSheet = aSheet;
  aSheet->SetOwnerRule(this);

  // set our medialist to be the same as the sheet's medialist
  mMedia = static_cast<nsMediaList*>(mChildSheet->Media());
}

uint16_t
ImportRule::Type() const
{
  return nsIDOMCSSRule::IMPORT_RULE;
}

void
ImportRule::GetCssTextImpl(nsAString& aCssText) const
{
  aCssText.AssignLiteral("@import url(");
  nsStyleUtil::AppendEscapedCSSString(mURLSpec, aCssText);
  aCssText.Append(')');
  if (mMedia) {
    nsAutoString mediaText;
    mMedia->GetText(mediaText);
    if (!mediaText.IsEmpty()) {
      aCssText.Append(' ');
      aCssText.Append(mediaText);
    }
  }
  aCssText.Append(';');
}

MediaList*
ImportRule::Media() const
{
  return mMedia;
}

StyleSheet*
ImportRule::GetStyleSheet() const
{
  return mChildSheet;
}

NS_IMETHODIMP
ImportRule::GetHref(nsAString & aHref)
{
  aHref = mURLSpec;
  return NS_OK;
}

NS_IMETHODIMP
ImportRule::GetMedia(nsIDOMMediaList * *aMedia)
{
  NS_ENSURE_ARG_POINTER(aMedia);

  NS_ADDREF(*aMedia = mMedia);
  return NS_OK;
}

NS_IMETHODIMP
ImportRule::GetStyleSheet(nsIDOMCSSStyleSheet * *aStyleSheet)
{
  NS_ENSURE_ARG_POINTER(aStyleSheet);

  NS_IF_ADDREF(*aStyleSheet = mChildSheet);
  return NS_OK;
}

/* virtual */ size_t
ImportRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mURLSpec
  //
  // The following members are not measured:
  // - mMedia, because it is measured via CSSStyleSheet::mMedia
  // - mChildSheet, because it is measured via CSSStyleSheetInner::mSheets
}

/* virtual */ JSObject*
ImportRule::WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto)
{
  return CSSImportRuleBinding::Wrap(aCx, this, aGivenProto);
}

// -------------------------------------------
// nsICSSMediaRule
//
MediaRule::MediaRule(uint32_t aLineNumber, uint32_t aColumnNumber)
  : CSSMediaRule(aLineNumber, aColumnNumber)
{
}

MediaRule::MediaRule(const MediaRule& aCopy)
  : CSSMediaRule(aCopy)
{
  if (aCopy.mMedia) {
    mMedia = aCopy.mMedia->Clone().downcast<nsMediaList>();
    // XXXldb This doesn't really make sense.
    mMedia->SetStyleSheet(aCopy.GetStyleSheet());
  }
}

MediaRule::~MediaRule()
{
  if (mMedia) {
    mMedia->SetStyleSheet(nullptr);
  }
}

NS_IMPL_ADDREF_INHERITED(MediaRule, CSSMediaRule)
NS_IMPL_RELEASE_INHERITED(MediaRule, CSSMediaRule)

// QueryInterface implementation for MediaRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaRule)
NS_INTERFACE_MAP_END_INHERITING(CSSMediaRule)

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaRule, CSSMediaRule,
                                   mMedia)

/* virtual */ void
MediaRule::SetStyleSheet(StyleSheet* aSheet)
{
  if (mMedia) {
    // Set to null so it knows it's leaving one sheet and joining another.
    mMedia->SetStyleSheet(nullptr);
    if (aSheet) {
      mMedia->SetStyleSheet(aSheet->AsGecko());
    }
  }

  GroupRule::SetStyleSheet(aSheet);
}

#ifdef DEBUG
/* virtual */ void
MediaRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString indentStr;
  for (int32_t indent = aIndent; --indent >= 0; ) {
    indentStr.AppendLiteral("  ");
  }

  nsAutoCString str(indentStr);
  str.AppendLiteral("@media ");

  if (mMedia) {
    nsAutoString mediaText;
    mMedia->GetText(mediaText);
    AppendUTF16toUTF8(mediaText, str);
  }

  str.AppendLiteral(" {\n");
  fprintf_stderr(out, "%s", str.get());

  GroupRule::List(out, aIndent);

  fprintf_stderr(out, "%s}\n", indentStr.get());
}
#endif

/* virtual */ already_AddRefed<Rule>
MediaRule::Clone() const
{
  RefPtr<Rule> clone = new MediaRule(*this);
  return clone.forget();
}

nsresult
MediaRule::SetMedia(nsMediaList* aMedia)
{
  mMedia = aMedia;
  if (aMedia)
    mMedia->SetStyleSheet(GetStyleSheet());
  return NS_OK;
}

MediaList*
MediaRule::Media()
{
  // In practice, if we end up being parsed at all, we have non-null mMedia.  So
  // it's OK to claim we don't return null here.
  return mMedia;
}

void
MediaRule::GetCssTextImpl(nsAString& aCssText) const
{
  aCssText.AssignLiteral("@media ");
  AppendConditionText(aCssText);
  GroupRule::AppendRulesToCssText(aCssText);
}

// nsIDOMCSSConditionRule methods
NS_IMETHODIMP
MediaRule::GetConditionText(nsAString& aConditionText)
{
  aConditionText.Truncate(0);
  AppendConditionText(aConditionText);
  return NS_OK;
}

NS_IMETHODIMP
MediaRule::SetConditionText(const nsAString& aConditionText)
{
  if (!mMedia) {
    RefPtr<nsMediaList> media = new nsMediaList();
    media->SetStyleSheet(GetStyleSheet());
    nsresult rv = media->SetMediaText(aConditionText);
    if (NS_SUCCEEDED(rv)) {
      mMedia = media;
    }
    return rv;
  }

  return mMedia->SetMediaText(aConditionText);
}

// GroupRule interface
/* virtual */ bool
MediaRule::UseForPresentation(nsPresContext* aPresContext,
                              nsMediaQueryResultCacheKey& aKey)
{
  if (mMedia) {
    MOZ_ASSERT(aPresContext);
    return mMedia->Matches(aPresContext, &aKey);
  }
  return true;
}

/* virtual */ size_t
MediaRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += GroupRule::SizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mMedia

  return n;
}

void
MediaRule::AppendConditionText(nsAString& aOutput) const
{
  if (mMedia) {
    nsAutoString mediaText;
    mMedia->GetText(mediaText);
    aOutput.Append(mediaText);
  }
}

DocumentRule::DocumentRule(uint32_t aLineNumber, uint32_t aColumnNumber)
  : ConditionRule(aLineNumber, aColumnNumber)
{
}

DocumentRule::DocumentRule(const DocumentRule& aCopy)
  : ConditionRule(aCopy)
  , mURLs(new URL(*aCopy.mURLs))
{
}

DocumentRule::~DocumentRule()
{
}

NS_IMPL_ADDREF_INHERITED(DocumentRule, ConditionRule)
NS_IMPL_RELEASE_INHERITED(DocumentRule, ConditionRule)

// QueryInterface implementation for DocumentRule
NS_INTERFACE_MAP_BEGIN(DocumentRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSGroupingRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSConditionRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSMozDocumentRule)
NS_INTERFACE_MAP_END_INHERITING(ConditionRule)

#ifdef DEBUG
/* virtual */ void
DocumentRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString indentStr;
  for (int32_t indent = aIndent; --indent >= 0; ) {
    indentStr.AppendLiteral("  ");
  }

  nsAutoCString str;
  str.AppendLiteral("@-moz-document ");
  for (URL *url = mURLs; url; url = url->next) {
    switch (url->func) {
      case eURL:
        str.AppendLiteral("url(\"");
        break;
      case eURLPrefix:
        str.AppendLiteral("url-prefix(\"");
        break;
      case eDomain:
        str.AppendLiteral("domain(\"");
        break;
      case eRegExp:
        str.AppendLiteral("regexp(\"");
        break;
    }
    nsAutoCString escapedURL(url->url);
    escapedURL.ReplaceSubstring("\"", "\\\""); // escape quotes
    str.Append(escapedURL);
    str.AppendLiteral("\"), ");
  }
  str.Cut(str.Length() - 2, 1); // remove last ,
  fprintf_stderr(out, "%s%s {\n", indentStr.get(), str.get());

  GroupRule::List(out, aIndent);

  fprintf_stderr(out, "%s}\n", indentStr.get());
}
#endif

/* virtual */ int32_t
DocumentRule::GetType() const
{
  return Rule::DOCUMENT_RULE;
}

/* virtual */ already_AddRefed<Rule>
DocumentRule::Clone() const
{
  RefPtr<Rule> clone = new DocumentRule(*this);
  return clone.forget();
}

uint16_t
DocumentRule::Type() const
{
  // XXX What should really happen here?
  return nsIDOMCSSRule::UNKNOWN_RULE;
}

void
DocumentRule::GetCssTextImpl(nsAString& aCssText) const
{
  aCssText.AssignLiteral("@-moz-document ");
  AppendConditionText(aCssText);
  GroupRule::AppendRulesToCssText(aCssText);
}

// nsIDOMCSSGroupingRule methods
NS_IMETHODIMP
DocumentRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  return GroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
DocumentRule::InsertRule(const nsAString & aRule, uint32_t aIndex, uint32_t* _retval)
{
  return GroupRule::InsertRule(aRule, aIndex, _retval);
}

NS_IMETHODIMP
DocumentRule::DeleteRule(uint32_t aIndex)
{
  return GroupRule::DeleteRule(aIndex);
}

// nsIDOMCSSConditionRule methods
NS_IMETHODIMP
DocumentRule::GetConditionText(nsAString& aConditionText)
{
  aConditionText.Truncate(0);
  AppendConditionText(aConditionText);
  return NS_OK;
}

NS_IMETHODIMP
DocumentRule::SetConditionText(const nsAString& aConditionText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
DocumentRule::SetConditionText(const nsAString& aConditionText,
                               ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

// GroupRule interface
/* virtual */ bool
DocumentRule::UseForPresentation(nsPresContext* aPresContext,
                                 nsMediaQueryResultCacheKey& aKey)
{
  return UseForPresentation(aPresContext);
}

bool
DocumentRule::UseForPresentation(nsPresContext* aPresContext)
{
  nsIDocument *doc = aPresContext->Document();
  nsIURI *docURI = doc->GetDocumentURI();
  nsAutoCString docURISpec;
  if (docURI) {
    // If GetSpec fails (due to OOM) just skip these URI-specific CSS rules.
    nsresult rv = docURI->GetSpec(docURISpec);
    NS_ENSURE_SUCCESS(rv, false);
  }

  for (URL *url = mURLs; url; url = url->next) {
    switch (url->func) {
      case eURL: {
        if (docURISpec == url->url)
          return true;
      } break;
      case eURLPrefix: {
        if (StringBeginsWith(docURISpec, url->url))
          return true;
      } break;
      case eDomain: {
        nsAutoCString host;
        if (docURI)
          docURI->GetHost(host);
        int32_t lenDiff = host.Length() - url->url.Length();
        if (lenDiff == 0) {
          if (host == url->url)
            return true;
        } else {
          if (StringEndsWith(host, url->url) &&
              host.CharAt(lenDiff - 1) == '.')
            return true;
        }
      } break;
      case eRegExp: {
        NS_ConvertUTF8toUTF16 spec(docURISpec);
        NS_ConvertUTF8toUTF16 regex(url->url);
        if (nsContentUtils::IsPatternMatching(spec, regex, doc)) {
          return true;
        }
      } break;
    }
  }

  return false;
}

DocumentRule::URL::~URL()
{
  NS_CSS_DELETE_LIST_MEMBER(DocumentRule::URL, this, next);
}

/* virtual */ size_t
DocumentRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += GroupRule::SizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mURLs

  return n;
}

/* virtual */ JSObject*
DocumentRule::WrapObject(JSContext* aCx,
                         JS::Handle<JSObject*> aGivenProto)
{
  return CSSMozDocumentRuleBinding::Wrap(aCx, this, aGivenProto);
}

void
DocumentRule::AppendConditionText(nsAString& aCssText) const
{
  for (URL *url = mURLs; url; url = url->next) {
    switch (url->func) {
      case eURL:
        aCssText.AppendLiteral("url(");
        break;
      case eURLPrefix:
        aCssText.AppendLiteral("url-prefix(");
        break;
      case eDomain:
        aCssText.AppendLiteral("domain(");
        break;
      case eRegExp:
        aCssText.AppendLiteral("regexp(");
        break;
    }
    nsStyleUtil::AppendEscapedCSSString(NS_ConvertUTF8toUTF16(url->url),
                                        aCssText);
    aCssText.AppendLiteral("), ");
  }
  aCssText.Truncate(aCssText.Length() - 2); // remove last ", "
}

// -------------------------------------------
// NameSpaceRule
//

NameSpaceRule::NameSpaceRule(nsIAtom* aPrefix, const nsString& aURLSpec,
                             uint32_t aLineNumber, uint32_t aColumnNumber)
  : CSSNamespaceRule(aLineNumber, aColumnNumber),
    mPrefix(aPrefix),
    mURLSpec(aURLSpec)
{
}

NameSpaceRule::NameSpaceRule(const NameSpaceRule& aCopy)
  : CSSNamespaceRule(aCopy),
    mPrefix(aCopy.mPrefix),
    mURLSpec(aCopy.mURLSpec)
{
}

NameSpaceRule::~NameSpaceRule()
{
}

NS_IMPL_ADDREF_INHERITED(NameSpaceRule, CSSNamespaceRule)
NS_IMPL_RELEASE_INHERITED(NameSpaceRule, CSSNamespaceRule)

// QueryInterface implementation for NameSpaceRule
// If this ever gets its own cycle-collection bits, reevaluate our IsCCLeaf
// implementation.
NS_INTERFACE_MAP_BEGIN(NameSpaceRule)
  if (aIID.Equals(NS_GET_IID(css::NameSpaceRule))) {
    *aInstancePtr = this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else
NS_INTERFACE_MAP_END_INHERITING(CSSNamespaceRule)

#ifdef DEBUG
/* virtual */ void
NameSpaceRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t indent = aIndent; --indent >= 0; ) {
    str.AppendLiteral("  ");
  }

  nsAutoString  buffer;

  str.AppendLiteral("@namespace ");

  if (mPrefix) {
    mPrefix->ToString(buffer);
    AppendUTF16toUTF8(buffer, str);
    str.Append(' ');
  }

  str.AppendLiteral("url(\"");
  AppendUTF16toUTF8(mURLSpec, str);
  str.AppendLiteral("\")\n");
  fprintf_stderr(out, "%s", str.get());
}
#endif

/* virtual */ already_AddRefed<Rule>
NameSpaceRule::Clone() const
{
  RefPtr<Rule> clone = new NameSpaceRule(*this);
  return clone.forget();
}

void
NameSpaceRule::GetCssTextImpl(nsAString& aCssText) const
{
  aCssText.AssignLiteral("@namespace ");
  if (mPrefix) {
    aCssText.Append(nsDependentAtomString(mPrefix) + NS_LITERAL_STRING(" "));
  }
  aCssText.AppendLiteral("url(");
  nsStyleUtil::AppendEscapedCSSString(mURLSpec, aCssText);
  aCssText.AppendLiteral(");");
}

/* virtual */ size_t
NameSpaceRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mPrefix
  // - mURLSpec
}

} // namespace css
} // namespace mozilla

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
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleDeclaration)
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
    val.AppendToString(eCSSProperty_font_style, aResult,
                        nsCSSValue::eNormalized);
    return NS_OK;

  case eCSSFontDesc_Weight:
    val.AppendToString(eCSSProperty_font_weight, aResult,
                       nsCSSValue::eNormalized);
    return NS_OK;

  case eCSSFontDesc_Stretch:
    val.AppendToString(eCSSProperty_font_stretch, aResult,
                       nsCSSValue::eNormalized);
    return NS_OK;

  case eCSSFontDesc_FontFeatureSettings:
    nsStyleUtil::AppendFontFeatureSettings(val, aResult);
    return NS_OK;

  case eCSSFontDesc_FontLanguageOverride:
    val.AppendToString(eCSSProperty_font_language_override, aResult,
                       nsCSSValue::eNormalized);
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
nsCSSFontFaceStyleDecl::SetCssText(const nsAString & aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED; // bug 443978
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetPropertyValue(const nsAString & propertyName,
                                         nsAString & aResult)
{
  return GetPropertyValue(nsCSSProps::LookupFontDesc(propertyName), aResult);
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetAuthoredPropertyValue(const nsAString& propertyName,
                                                 nsAString& aResult)
{
  // We don't return any authored property values different from
  // GetPropertyValue, currently.
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
nsCSSFontFaceStyleDecl::SetProperty(const nsAString & propertyName,
                                    const nsAString & value,
                                    const nsAString & priority)
{
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

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::Item(uint32_t aIndex, nsAString& aReturn)
{
  bool found;
  IndexedGetter(aIndex, found, aReturn);
  if (!found) {
    aReturn.Truncate();
  }
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

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  NS_IF_ADDREF(*aParentRule = ContainingRule());
  return NS_OK;
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
                                         const nsAString& aValue)
{
  return SetProperty(NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(aPropID)),
                     aValue, EmptyString());
}

nsINode*
nsCSSFontFaceStyleDecl::GetParentObject()
{
  return ContainingRule()->GetDocument();
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

NS_IMPL_ADDREF_INHERITED(nsCSSFontFaceRule, mozilla::css::Rule)
NS_IMPL_RELEASE_INHERITED(nsCSSFontFaceRule, mozilla::css::Rule)

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

// QueryInterface implementation for nsCSSFontFaceRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsCSSFontFaceRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSFontFaceRule)
NS_INTERFACE_MAP_END_INHERITING(Rule)

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
  return nsIDOMCSSRule::FONT_FACE_RULE;
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

NS_IMETHODIMP
nsCSSFontFaceRule::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  NS_IF_ADDREF(*aStyle = &mDecl);
  return NS_OK;
}

// Arguably these should forward to nsCSSFontFaceStyleDecl methods.
void
nsCSSFontFaceRule::SetDesc(nsCSSFontDesc aDescID, nsCSSValue const & aValue)
{
  NS_PRECONDITION(aDescID > eCSSFontDesc_UNKNOWN &&
                  aDescID < eCSSFontDesc_COUNT,
                  "aDescID out of range in nsCSSFontFaceRule::SetDesc");

  // FIXME: handle dynamic changes

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

// -----------------------------------
// nsCSSFontFeatureValuesRule
//

/* virtual */ already_AddRefed<css::Rule>
nsCSSFontFeatureValuesRule::Clone() const
{
  RefPtr<css::Rule> clone = new nsCSSFontFeatureValuesRule(*this);
  return clone.forget();
}

NS_IMPL_ADDREF_INHERITED(nsCSSFontFeatureValuesRule, mozilla::css::Rule)
NS_IMPL_RELEASE_INHERITED(nsCSSFontFeatureValuesRule, mozilla::css::Rule)

// QueryInterface implementation for nsCSSFontFeatureValuesRule
// If this ever gets its own cycle-collection bits, reevaluate our IsCCLeaf
// implementation.
NS_INTERFACE_MAP_BEGIN(nsCSSFontFeatureValuesRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSFontFeatureValuesRule)
NS_INTERFACE_MAP_END_INHERITING(mozilla::css::Rule)

bool
nsCSSFontFeatureValuesRule::IsCCLeaf() const
{
  return Rule::IsCCLeaf();
}

static void
FeatureValuesToString(
  const nsTArray<gfxFontFeatureValueSet::FeatureValues>& aFeatureValues,
  nsAString& aOutStr)
{
  uint32_t i, n;

  // append values
  n = aFeatureValues.Length();
  for (i = 0; i < n; i++) {
    const gfxFontFeatureValueSet::FeatureValues& fv = aFeatureValues[i];

    // @alternate
    aOutStr.AppendLiteral("  @");
    nsAutoString functAlt;
    nsStyleUtil::GetFunctionalAlternatesName(fv.alternate, functAlt);
    aOutStr.Append(functAlt);
    aOutStr.AppendLiteral(" {");

    // for each ident-values tuple
    uint32_t j, numValues = fv.valuelist.Length();
    for (j = 0; j < numValues; j++) {
      aOutStr.Append(' ');
      const gfxFontFeatureValueSet::ValueList& vlist = fv.valuelist[j];
      nsStyleUtil::AppendEscapedCSSIdent(vlist.name, aOutStr);
      aOutStr.Append(':');

      uint32_t k, numSelectors = vlist.featureSelectors.Length();
      for (k = 0; k < numSelectors; k++) {
        aOutStr.Append(' ');
        aOutStr.AppendInt(vlist.featureSelectors[k]);
      }

      aOutStr.Append(';');
    }
    aOutStr.AppendLiteral(" }\n");
  }
}

static void
FontFeatureValuesRuleToString(
  const mozilla::FontFamilyList& aFamilyList,
  const nsTArray<gfxFontFeatureValueSet::FeatureValues>& aFeatureValues,
  nsAString& aOutStr)
{
  aOutStr.AssignLiteral("@font-feature-values ");
  nsAutoString familyListStr, valueTextStr;
  nsStyleUtil::AppendEscapedCSSFontFamilyList(aFamilyList, familyListStr);
  aOutStr.Append(familyListStr);
  aOutStr.AppendLiteral(" {\n");
  FeatureValuesToString(aFeatureValues, valueTextStr);
  aOutStr.Append(valueTextStr);
  aOutStr.Append('}');
}

#ifdef DEBUG
void
nsCSSFontFeatureValuesRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoString text;
  FontFeatureValuesRuleToString(mFamilyList, mFeatureValues, text);
  NS_ConvertUTF16toUTF8 utf8(text);

  // replace newlines with newlines plus indent spaces
  char* indent = new char[(aIndent + 1) * 2];
  int32_t i;
  for (i = 1; i < (aIndent + 1) * 2 - 1; i++) {
    indent[i] = 0x20;
  }
  indent[0] = 0xa;
  indent[aIndent * 2 + 1] = 0;
  utf8.ReplaceSubstring("\n", indent);
  delete [] indent;

  nsAutoCString indentStr;
  for (i = aIndent; --i >= 0; ) {
    indentStr.AppendLiteral("  ");
  }
  fprintf_stderr(out, "%s%s\n", indentStr.get(), utf8.get());
}
#endif

/* virtual */ int32_t
nsCSSFontFeatureValuesRule::GetType() const
{
  return Rule::FONT_FEATURE_VALUES_RULE;
}

uint16_t
nsCSSFontFeatureValuesRule::Type() const
{
  return nsIDOMCSSRule::FONT_FEATURE_VALUES_RULE;
}

void
nsCSSFontFeatureValuesRule::GetCssTextImpl(nsAString& aCssText) const
{
  FontFeatureValuesRuleToString(mFamilyList, mFeatureValues, aCssText);
}

void
nsCSSFontFeatureValuesRule::SetFontFamily(const nsAString& aFamily,
                                          ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void
nsCSSFontFeatureValuesRule::SetValueText(const nsAString& aFamily,
                                         ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::GetFontFamily(nsAString& aFamilyListStr)
{
  nsStyleUtil::AppendEscapedCSSFontFamilyList(mFamilyList, aFamilyListStr);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::SetFontFamily(const nsAString& aFontFamily)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::GetValueText(nsAString& aValueText)
{
  FeatureValuesToString(mFeatureValues, aValueText);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::SetValueText(const nsAString& aValueText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

struct MakeFamilyArray {
  explicit MakeFamilyArray(nsTArray<nsString>& aFamilyArray)
    : familyArray(aFamilyArray), hasGeneric(false)
  {}

  static bool
  AddFamily(const nsString& aFamily, bool aGeneric, void* aData)
  {
    MakeFamilyArray *familyArr = reinterpret_cast<MakeFamilyArray*> (aData);
    if (!aGeneric && !aFamily.IsEmpty()) {
      familyArr->familyArray.AppendElement(aFamily);
    }
    if (aGeneric) {
      familyArr->hasGeneric = true;
    }
    return true;
  }

  nsTArray<nsString>& familyArray;
  bool hasGeneric;
};

void
nsCSSFontFeatureValuesRule::SetFamilyList(
  const mozilla::FontFamilyList& aFamilyList)
{
  mFamilyList = aFamilyList;
}

void
nsCSSFontFeatureValuesRule::AddValueList(int32_t aVariantAlternate,
                     nsTArray<gfxFontFeatureValueSet::ValueList>& aValueList)
{
  uint32_t i, len = mFeatureValues.Length();
  bool foundAlternate = false;

  // add to an existing list for a given property value
  for (i = 0; i < len; i++) {
    gfxFontFeatureValueSet::FeatureValues& f = mFeatureValues.ElementAt(i);

    if (f.alternate == uint32_t(aVariantAlternate)) {
      f.valuelist.AppendElements(aValueList);
      foundAlternate = true;
      break;
    }
  }

  // create a new list for a given property value
  if (!foundAlternate) {
    gfxFontFeatureValueSet::FeatureValues &f = *mFeatureValues.AppendElement();
    f.alternate = aVariantAlternate;
    f.valuelist.AppendElements(aValueList);
  }
}

size_t
nsCSSFontFeatureValuesRule::SizeOfIncludingThis(
  MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

/* virtual */ JSObject*
nsCSSFontFeatureValuesRule::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto)
{
  return CSSFontFeatureValuesRuleBinding::Wrap(aCx, this, aGivenProto);
}

// -------------------------------------------
// nsCSSKeyframeStyleDeclaration
//

nsCSSKeyframeStyleDeclaration::nsCSSKeyframeStyleDeclaration(nsCSSKeyframeRule *aRule)
  : mRule(aRule)
{
}

nsCSSKeyframeStyleDeclaration::~nsCSSKeyframeStyleDeclaration()
{
  NS_ASSERTION(!mRule, "DropReference not called.");
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsCSSKeyframeStyleDeclaration)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsCSSKeyframeStyleDeclaration)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsCSSKeyframeStyleDeclaration)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsCSSKeyframeStyleDeclaration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)

DeclarationBlock*
nsCSSKeyframeStyleDeclaration::GetCSSDeclaration(Operation aOperation)
{
  if (mRule) {
    return mRule->Declaration();
  } else {
    return nullptr;
  }
}

void
nsCSSKeyframeStyleDeclaration::GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv)
{
  GetCSSParsingEnvironmentForRule(mRule, aCSSParseEnv);
}

URLExtraData*
nsCSSKeyframeStyleDeclaration::GetURLData() const
{
  MOZ_ASSERT_UNREACHABLE("GetURLData shouldn't be calling on a Gecko rule");
  return GetURLDataForRule(mRule);
}

NS_IMETHODIMP
nsCSSKeyframeStyleDeclaration::GetParentRule(nsIDOMCSSRule **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  NS_IF_ADDREF(*aParent = mRule);
  return NS_OK;
}

nsresult
nsCSSKeyframeStyleDeclaration::SetCSSDeclaration(DeclarationBlock* aDecl)
{
  MOZ_ASSERT(aDecl, "must be non-null");
  mRule->ChangeDeclaration(aDecl->AsGecko());
  return NS_OK;
}

nsIDocument*
nsCSSKeyframeStyleDeclaration::DocToUpdate()
{
  return nullptr;
}

nsINode*
nsCSSKeyframeStyleDeclaration::GetParentObject()
{
  return mRule ? mRule->GetDocument() : nullptr;
}

// -------------------------------------------
// nsCSSKeyframeRule
//

nsCSSKeyframeRule::nsCSSKeyframeRule(const nsCSSKeyframeRule& aCopy)
  // copy everything except our reference count and mDOMDeclaration
  : Rule(aCopy)
  , mKeys(aCopy.mKeys)
  , mDeclaration(new css::Declaration(*aCopy.mDeclaration))
{
  mDeclaration->SetOwningRule(this);
}

nsCSSKeyframeRule::~nsCSSKeyframeRule()
{
  mDeclaration->SetOwningRule(nullptr);
  if (mDOMDeclaration) {
    mDOMDeclaration->DropReference();
  }
}

/* virtual */ already_AddRefed<css::Rule>
nsCSSKeyframeRule::Clone() const
{
  RefPtr<css::Rule> clone = new nsCSSKeyframeRule(*this);
  return clone.forget();
}

NS_IMPL_ADDREF_INHERITED(nsCSSKeyframeRule, mozilla::css::Rule)
NS_IMPL_RELEASE_INHERITED(nsCSSKeyframeRule, mozilla::css::Rule)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsCSSKeyframeRule)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsCSSKeyframeRule,
                                                mozilla::css::Rule)
  if (tmp->mDOMDeclaration) {
    tmp->mDOMDeclaration->DropReference();
    tmp->mDOMDeclaration = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsCSSKeyframeRule,
                                                  mozilla::css::Rule)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDOMDeclaration)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool
nsCSSKeyframeRule::IsCCLeaf() const
{
  // Let's not worry about figuring out whether we're a leaf or not.
  return false;
}

// QueryInterface implementation for nsCSSKeyframeRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsCSSKeyframeRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSKeyframeRule)
NS_INTERFACE_MAP_END_INHERITING(mozilla::css::Rule)

#ifdef DEBUG
void
nsCSSKeyframeRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t index = aIndent; --index >= 0; ) {
    str.AppendLiteral("  ");
  }

  nsAutoString tmp;
  DoGetKeyText(tmp);
  AppendUTF16toUTF8(tmp, str);
  str.AppendLiteral(" { ");
  mDeclaration->ToString(tmp);
  AppendUTF16toUTF8(tmp, str);
  str.AppendLiteral("}\n");
  fprintf_stderr(out, "%s", str.get());
}
#endif

/* virtual */ int32_t
nsCSSKeyframeRule::GetType() const
{
  return Rule::KEYFRAME_RULE;
}

uint16_t
nsCSSKeyframeRule::Type() const
{
  return nsIDOMCSSRule::KEYFRAME_RULE;
}

void
nsCSSKeyframeRule::GetCssTextImpl(nsAString& aCssText) const
{
  DoGetKeyText(aCssText);
  aCssText.AppendLiteral(" { ");
  nsAutoString tmp;
  mDeclaration->ToString(tmp);
  aCssText.Append(tmp);
  aCssText.AppendLiteral(" }");
}

NS_IMETHODIMP
nsCSSKeyframeRule::GetKeyText(nsAString& aKeyText)
{
  DoGetKeyText(aKeyText);
  return NS_OK;
}

void
nsCSSKeyframeRule::DoGetKeyText(nsAString& aKeyText) const
{
  aKeyText.Truncate();
  uint32_t i = 0, i_end = mKeys.Length();
  MOZ_ASSERT(i_end != 0, "must have some keys");
  for (;;) {
    aKeyText.AppendFloat(mKeys[i] * 100.0f);
    aKeyText.Append(char16_t('%'));
    if (++i == i_end) {
      break;
    }
    aKeyText.AppendLiteral(", ");
  }
}

NS_IMETHODIMP
nsCSSKeyframeRule::SetKeyText(const nsAString& aKeyText)
{
  nsCSSParser parser;

  InfallibleTArray<float> newSelectors;
  // FIXME: pass filename and line number
  if (!parser.ParseKeyframeSelectorString(aKeyText, nullptr, 0, newSelectors)) {
    // for now, we don't do anything if the parse fails
    return NS_OK;
  }

  nsIDocument* doc = GetDocument();
  MOZ_AUTO_DOC_UPDATE(doc, UPDATE_STYLE, true);

  newSelectors.SwapElements(mKeys);

  if (StyleSheet* sheet = GetStyleSheet()) {
    sheet->AsGecko()->SetModifiedByChildRule();
    if (doc) {
      doc->StyleRuleChanged(sheet, this);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframeRule::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  NS_ADDREF(*aStyle = Style());
  return NS_OK;
}

nsICSSDeclaration*
nsCSSKeyframeRule::Style()
{
  if (!mDOMDeclaration) {
    mDOMDeclaration = new nsCSSKeyframeStyleDeclaration(this);
  }
  return mDOMDeclaration;
}

void
nsCSSKeyframeRule::ChangeDeclaration(css::Declaration* aDeclaration)
{
  // Our caller already did a BeginUpdate/EndUpdate, but with
  // UPDATE_CONTENT, and we need UPDATE_STYLE to trigger work in
  // PresShell::EndUpdate.
  nsIDocument* doc = GetDocument();
  MOZ_AUTO_DOC_UPDATE(doc, UPDATE_STYLE, true);

  if (aDeclaration != mDeclaration) {
    mDeclaration->SetOwningRule(nullptr);
    mDeclaration = aDeclaration;
    mDeclaration->SetOwningRule(this);
  }

  if (StyleSheet* sheet = GetStyleSheet()) {
    sheet->AsGecko()->SetModifiedByChildRule();
    if (doc) {
      doc->StyleRuleChanged(sheet, this);
    }
  }
}

/* virtual */ size_t
nsCSSKeyframeRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mKeys
  // - mDeclaration
  // - mDOMDeclaration
}

/* virtual */ JSObject*
nsCSSKeyframeRule::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto)
{
  return CSSKeyframeRuleBinding::Wrap(aCx, this, aGivenProto);
}

// -------------------------------------------
// nsCSSKeyframesRule
//

nsCSSKeyframesRule::nsCSSKeyframesRule(const nsCSSKeyframesRule& aCopy)
  // copy everything except our reference count.  GroupRule's copy
  // constructor also doesn't copy the lazily-constructed
  // mRuleCollection.
  : GroupRule(aCopy),
    mName(aCopy.mName)
{
}

nsCSSKeyframesRule::~nsCSSKeyframesRule()
{
}

/* virtual */ already_AddRefed<css::Rule>
nsCSSKeyframesRule::Clone() const
{
  RefPtr<css::Rule> clone = new nsCSSKeyframesRule(*this);
  return clone.forget();
}

NS_IMPL_ADDREF_INHERITED(nsCSSKeyframesRule, css::GroupRule)
NS_IMPL_RELEASE_INHERITED(nsCSSKeyframesRule, css::GroupRule)

// QueryInterface implementation for nsCSSKeyframesRule
NS_INTERFACE_MAP_BEGIN(nsCSSKeyframesRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSKeyframesRule)
NS_INTERFACE_MAP_END_INHERITING(GroupRule)

#ifdef DEBUG
void
nsCSSKeyframesRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString indentStr;
  for (int32_t indent = aIndent; --indent >= 0; ) {
    indentStr.AppendLiteral("  ");
  }

  fprintf_stderr(out, "%s@keyframes %s {\n",
                 indentStr.get(), NS_ConvertUTF16toUTF8(mName).get());

  GroupRule::List(out, aIndent);

  fprintf_stderr(out, "%s}\n", indentStr.get());
}
#endif

/* virtual */ int32_t
nsCSSKeyframesRule::GetType() const
{
  return Rule::KEYFRAMES_RULE;
}

uint16_t
nsCSSKeyframesRule::Type() const
{
  return nsIDOMCSSRule::KEYFRAMES_RULE;
}

void
nsCSSKeyframesRule::GetCssTextImpl(nsAString& aCssText) const
{
  aCssText.AssignLiteral("@keyframes ");
  aCssText.Append(mName);
  aCssText.AppendLiteral(" {\n");
  nsAutoString tmp;
  for (const Rule* rule : GeckoRules()) {
    static_cast<const nsCSSKeyframeRule*>(rule)->GetCssText(tmp);
    aCssText.Append(tmp);
    aCssText.Append('\n');
  }
  aCssText.Append('}');
}

NS_IMETHODIMP
nsCSSKeyframesRule::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframesRule::SetName(const nsAString& aName)
{
  if (mName == aName) {
    return NS_OK;
  }

  nsIDocument* doc = GetDocument();
  MOZ_AUTO_DOC_UPDATE(doc, UPDATE_STYLE, true);

  mName = aName;

  if (StyleSheet* sheet = GetStyleSheet()) {
    sheet->AsGecko()->SetModifiedByChildRule();
    if (doc) {
      doc->StyleRuleChanged(sheet, this);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframesRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  return GroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
nsCSSKeyframesRule::AppendRule(const nsAString& aRule)
{
  // The spec is confusing, and I think we should just append the rule,
  // which also turns out to match WebKit:
  // http://lists.w3.org/Archives/Public/www-style/2011Apr/0034.html
  nsCSSParser parser;

  // FIXME: pass filename and line number
  RefPtr<nsCSSKeyframeRule> rule =
    parser.ParseKeyframeRule(aRule, nullptr, 0);
  if (rule) {
    nsIDocument* doc = GetDocument();
    MOZ_AUTO_DOC_UPDATE(doc, UPDATE_STYLE, true);

    AppendStyleRule(rule);

    if (StyleSheet* sheet = GetStyleSheet()) {
      sheet->AsGecko()->SetModifiedByChildRule();
      if (doc) {
        doc->StyleRuleChanged(sheet, this);
      }
    }
  }

  return NS_OK;
}

static const uint32_t RULE_NOT_FOUND = uint32_t(-1);

uint32_t
nsCSSKeyframesRule::FindRuleIndexForKey(const nsAString& aKey)
{
  nsCSSParser parser;

  InfallibleTArray<float> keys;
  // FIXME: pass filename and line number
  if (parser.ParseKeyframeSelectorString(aKey, nullptr, 0, keys)) {
    IncrementalClearCOMRuleArray& rules = GeckoRules();
    // The spec isn't clear, but we'll match on the key list, which
    // mostly matches what WebKit does, except we'll do last-match
    // instead of first-match, and handling parsing differences better.
    // http://lists.w3.org/Archives/Public/www-style/2011Apr/0036.html
    // http://lists.w3.org/Archives/Public/www-style/2011Apr/0037.html
    for (uint32_t i = rules.Count(); i-- != 0; ) {
      if (static_cast<nsCSSKeyframeRule*>(rules[i])->GetKeys() == keys) {
        return i;
      }
    }
  }

  return RULE_NOT_FOUND;
}

NS_IMETHODIMP
nsCSSKeyframesRule::DeleteRule(const nsAString& aKey)
{
  uint32_t index = FindRuleIndexForKey(aKey);
  if (index != RULE_NOT_FOUND) {
    nsIDocument* doc = GetDocument();
    MOZ_AUTO_DOC_UPDATE(doc, UPDATE_STYLE, true);

    DeleteStyleRuleAt(index);

    if (StyleSheet* sheet = GetStyleSheet()) {
      sheet->AsGecko()->SetModifiedByChildRule();

      if (doc) {
        doc->StyleRuleChanged(sheet, this);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframesRule::FindRule(const nsAString& aKey,
                             nsIDOMCSSKeyframeRule** aResult)
{
  NS_IF_ADDREF(*aResult = FindRule(aKey));
  return NS_OK;
}

nsCSSKeyframeRule*
nsCSSKeyframesRule::FindRule(const nsAString& aKey)
{
  uint32_t index = FindRuleIndexForKey(aKey);
  if (index == RULE_NOT_FOUND) {
    return nullptr;
  }
  return static_cast<nsCSSKeyframeRule*>(GeckoRules()[index]);
}

// GroupRule interface
/* virtual */ bool
nsCSSKeyframesRule::UseForPresentation(nsPresContext* aPresContext,
                                       nsMediaQueryResultCacheKey& aKey)
{
  MOZ_ASSERT(false, "should not be called");
  return false;
}

/* virtual */ size_t
nsCSSKeyframesRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += GroupRule::SizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mName

  return n;
}

/* virtual */ JSObject*
nsCSSKeyframesRule::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto)
{
  return CSSKeyframesRuleBinding::Wrap(aCx, this, aGivenProto);
}

// -------------------------------------------
// nsCSSPageStyleDeclaration
//

nsCSSPageStyleDeclaration::nsCSSPageStyleDeclaration(nsCSSPageRule* aRule)
  : mRule(aRule)
{
}

nsCSSPageStyleDeclaration::~nsCSSPageStyleDeclaration()
{
  NS_ASSERTION(!mRule, "DropReference not called.");
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsCSSPageStyleDeclaration)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsCSSPageStyleDeclaration)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsCSSPageStyleDeclaration)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsCSSPageStyleDeclaration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)

DeclarationBlock*
nsCSSPageStyleDeclaration::GetCSSDeclaration(Operation aOperation)
{
  if (mRule) {
    return mRule->Declaration();
  } else {
    return nullptr;
  }
}

void
nsCSSPageStyleDeclaration::GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv)
{
  GetCSSParsingEnvironmentForRule(mRule, aCSSParseEnv);
}

URLExtraData*
nsCSSPageStyleDeclaration::GetURLData() const
{
  MOZ_ASSERT_UNREACHABLE("GetURLData shouldn't be calling on a Gecko rule");
  return GetURLDataForRule(mRule);
}

NS_IMETHODIMP
nsCSSPageStyleDeclaration::GetParentRule(nsIDOMCSSRule** aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  NS_IF_ADDREF(*aParent = mRule);
  return NS_OK;
}

nsresult
nsCSSPageStyleDeclaration::SetCSSDeclaration(DeclarationBlock* aDecl)
{
  MOZ_ASSERT(aDecl, "must be non-null");
  mRule->ChangeDeclaration(aDecl->AsGecko());
  return NS_OK;
}

nsIDocument*
nsCSSPageStyleDeclaration::DocToUpdate()
{
  return nullptr;
}

nsINode*
nsCSSPageStyleDeclaration::GetParentObject()
{
  return mRule ? mRule->GetDocument() : nullptr;
}

// -------------------------------------------
// nsCSSPageRule
//

nsCSSPageRule::nsCSSPageRule(const nsCSSPageRule& aCopy)
  // copy everything except our reference count and mDOMDeclaration
  : dom::CSSPageRule(aCopy)
  , mDeclaration(new css::Declaration(*aCopy.mDeclaration))
{
  mDeclaration->SetOwningRule(this);
}

nsCSSPageRule::~nsCSSPageRule()
{
  mDeclaration->SetOwningRule(nullptr);
  if (mDOMDeclaration) {
    mDOMDeclaration->DropReference();
  }
}

/* virtual */ already_AddRefed<css::Rule>
nsCSSPageRule::Clone() const
{
  RefPtr<css::Rule> clone = new nsCSSPageRule(*this);
  return clone.forget();
}

NS_IMPL_ADDREF_INHERITED(nsCSSPageRule, dom::CSSPageRule)
NS_IMPL_RELEASE_INHERITED(nsCSSPageRule, dom::CSSPageRule)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsCSSPageRule)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsCSSPageRule,
                                                dom::CSSPageRule)
  if (tmp->mDOMDeclaration) {
    tmp->mDOMDeclaration->DropReference();
    tmp->mDOMDeclaration = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsCSSPageRule,
                                                  dom::CSSPageRule)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDOMDeclaration)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool
nsCSSPageRule::IsCCLeaf() const
{
  // Let's not worry about figuring out whether we're a leaf or not.
  return false;
}

// QueryInterface implementation for nsCSSPageRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsCSSPageRule)
NS_INTERFACE_MAP_END_INHERITING(dom::CSSPageRule)

#ifdef DEBUG
void
nsCSSPageRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString str;
  for (int32_t indent = aIndent; --indent >= 0; ) {
    str.AppendLiteral("  ");
  }

  str.AppendLiteral("@page { ");
  nsAutoString tmp;
  mDeclaration->ToString(tmp);
  AppendUTF16toUTF8(tmp, str);
  str.AppendLiteral("}\n");
  fprintf_stderr(out, "%s", str.get());
}
#endif

void
nsCSSPageRule::GetCssTextImpl(nsAString& aCssText) const
{
  aCssText.AppendLiteral("@page { ");
  nsAutoString tmp;
  mDeclaration->ToString(tmp);
  aCssText.Append(tmp);
  aCssText.AppendLiteral(" }");
}

nsICSSDeclaration*
nsCSSPageRule::Style()
{
  if (!mDOMDeclaration) {
    mDOMDeclaration = new nsCSSPageStyleDeclaration(this);
  }
  return mDOMDeclaration;
}

void
nsCSSPageRule::ChangeDeclaration(css::Declaration* aDeclaration)
{
  if (aDeclaration != mDeclaration) {
    mDeclaration->SetOwningRule(nullptr);
    mDeclaration = aDeclaration;
    mDeclaration->SetOwningRule(this);
  }

  if (StyleSheet* sheet = GetStyleSheet()) {
    sheet->AsGecko()->SetModifiedByChildRule();
  }
}

/* virtual */ size_t
nsCSSPageRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

namespace mozilla {

CSSSupportsRule::CSSSupportsRule(bool aConditionMet,
                                 const nsString& aCondition,
                                 uint32_t aLineNumber, uint32_t aColumnNumber)
  : dom::CSSSupportsRule(aLineNumber, aColumnNumber)
  , mUseGroup(aConditionMet)
  , mCondition(aCondition)
{
}

CSSSupportsRule::~CSSSupportsRule()
{
}

CSSSupportsRule::CSSSupportsRule(const CSSSupportsRule& aCopy)
  : dom::CSSSupportsRule(aCopy),
    mUseGroup(aCopy.mUseGroup),
    mCondition(aCopy.mCondition)
{
}

#ifdef DEBUG
/* virtual */ void
CSSSupportsRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoCString indentStr;
  for (int32_t indent = aIndent; --indent >= 0; ) {
    indentStr.AppendLiteral("  ");
  }

  fprintf_stderr(out, "%s@supports %s {\n",
                 indentStr.get(), NS_ConvertUTF16toUTF8(mCondition).get());

  css::GroupRule::List(out, aIndent);

  fprintf_stderr(out, "%s}\n", indentStr.get());
}
#endif

/* virtual */ already_AddRefed<mozilla::css::Rule>
CSSSupportsRule::Clone() const
{
  RefPtr<css::Rule> clone = new CSSSupportsRule(*this);
  return clone.forget();
}

/* virtual */ bool
CSSSupportsRule::UseForPresentation(nsPresContext* aPresContext,
                                   nsMediaQueryResultCacheKey& aKey)
{
  return mUseGroup;
}

NS_IMPL_ADDREF_INHERITED(CSSSupportsRule, css::ConditionRule)
NS_IMPL_RELEASE_INHERITED(CSSSupportsRule, css::ConditionRule)

// QueryInterface implementation for CSSSupportsRule
NS_INTERFACE_MAP_BEGIN(CSSSupportsRule)
NS_INTERFACE_MAP_END_INHERITING(dom::CSSSupportsRule)

void
CSSSupportsRule::GetCssTextImpl(nsAString& aCssText) const
{
  aCssText.AssignLiteral("@supports ");
  aCssText.Append(mCondition);
  css::GroupRule::AppendRulesToCssText(aCssText);
}

// nsIDOMCSSConditionRule methods
NS_IMETHODIMP
CSSSupportsRule::GetConditionText(nsAString& aConditionText)
{
  aConditionText.Assign(mCondition);
  return NS_OK;
}

NS_IMETHODIMP
CSSSupportsRule::SetConditionText(const nsAString& aConditionText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* virtual */ size_t
CSSSupportsRule::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += css::GroupRule::SizeOfExcludingThis(aMallocSizeOf);
  n += mCondition.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  return n;
}

} // namespace mozilla

// -------------------------------------------
// nsCSSCounterStyleRule
//

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

  fprintf_stderr(out, "%s@counter-style %s (rev.%u) {\n",
                 baseInd.get(), NS_ConvertUTF16toUTF8(mName).get(),
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
  nsStyleUtil::AppendEscapedCSSIdent(mName, aCssText);
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
  nsStyleUtil::AppendEscapedCSSIdent(mName, aName);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSCounterStyleRule::SetName(const nsAString& aName)
{
  nsCSSParser parser;
  nsAutoString name;
  if (parser.ParseCounterStyleName(aName, nullptr, name)) {
    nsIDocument* doc = GetDocument();
    MOZ_AUTO_DOC_UPDATE(doc, UPDATE_STYLE, true);

    mName = name;

    if (StyleSheet* sheet = GetStyleSheet()) {
      sheet->AsGecko()->SetModifiedByChildRule();
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
    sheet->AsGecko()->SetModifiedByChildRule();
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
    GetSystemArgument().AppendToString(
        eCSSProperty_UNKNOWN, aSystem, nsCSSValue::eNormalized);
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
      item->mValue.AppendToString(eCSSProperty_UNKNOWN,
                                  aSymbols,
                                  nsCSSValue::eNormalized);
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
      item->mXValue.AppendToString(eCSSProperty_UNKNOWN,
                                   aSymbols, nsCSSValue::eNormalized);
      aSymbols.Append(' ');
      item->mYValue.AppendToString(eCSSProperty_UNKNOWN,
                                   aSymbols, nsCSSValue::eNormalized);
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
    case eCSSUnit_Ident:
      aSpeakAs.Truncate();
      value.AppendToString(eCSSProperty_UNKNOWN,
                           aSpeakAs, nsCSSValue::eNormalized);
      break;

    default:
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
    value.AppendToString(
        eCSSProperty_UNKNOWN, aValue, nsCSSValue::eNormalized);
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
