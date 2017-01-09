/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StyleSheet.h"

#include "mozilla/dom/CSSRuleList.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/ServoStyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/CSSStyleSheet.h"

#include "mozAutoDocUpdate.h"
#include "nsMediaList.h"
#include "nsNullPrincipal.h"

namespace mozilla {

StyleSheet::StyleSheet(StyleBackendType aType, css::SheetParsingMode aParsingMode)
  : mDocument(nullptr)
  , mOwningNode(nullptr)
  , mParsingMode(aParsingMode)
  , mType(aType)
  , mDisabled(false)
{
}

StyleSheet::StyleSheet(const StyleSheet& aCopy,
                       nsIDocument* aDocumentToUse,
                       nsINode* aOwningNodeToUse)
  : mTitle(aCopy.mTitle)
  , mDocument(aDocumentToUse)
  , mOwningNode(aOwningNodeToUse)
  , mParsingMode(aCopy.mParsingMode)
  , mType(aCopy.mType)
  , mDisabled(aCopy.mDisabled)
{
  if (aCopy.mMedia) {
    // XXX This is wrong; we should be keeping @import rules and
    // sheets in sync!
    mMedia = aCopy.mMedia->Clone();
  }
}

StyleSheet::~StyleSheet()
{
  DropMedia();
}

// QueryInterface implementation for StyleSheet
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StyleSheet)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMStyleSheet)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleSheet)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(StyleSheet)
NS_IMPL_CYCLE_COLLECTING_RELEASE(StyleSheet)

NS_IMPL_CYCLE_COLLECTION_CLASS(StyleSheet)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(StyleSheet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMedia)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(StyleSheet)
  tmp->DropMedia();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(StyleSheet)

mozilla::dom::CSSStyleSheetParsingMode
StyleSheet::ParsingModeDOM()
{
#define CHECK(X, Y) \
  static_assert(static_cast<int>(X) == static_cast<int>(Y),             \
                "mozilla::dom::CSSStyleSheetParsingMode and mozilla::css::SheetParsingMode should have identical values");

  CHECK(mozilla::dom::CSSStyleSheetParsingMode::Agent, css::eAgentSheetFeatures);
  CHECK(mozilla::dom::CSSStyleSheetParsingMode::User, css::eUserSheetFeatures);
  CHECK(mozilla::dom::CSSStyleSheetParsingMode::Author, css::eAuthorSheetFeatures);

#undef CHECK

  return static_cast<mozilla::dom::CSSStyleSheetParsingMode>(mParsingMode);
}

bool
StyleSheet::IsComplete() const
{
  return SheetInfo().mComplete;
}

void
StyleSheet::SetComplete()
{
  NS_ASSERTION(!IsGecko() || !AsGecko()->mDirty,
               "Can't set a dirty sheet complete!");
  SheetInfo().mComplete = true;
  if (mDocument && !mDisabled) {
    // Let the document know
    mDocument->BeginUpdate(UPDATE_STYLE);
    mDocument->SetStyleSheetApplicableState(this, true);
    mDocument->EndUpdate(UPDATE_STYLE);
  }

  if (mOwningNode && !mDisabled &&
      mOwningNode->HasFlag(NODE_IS_IN_SHADOW_TREE) &&
      mOwningNode->IsContent()) {
    dom::ShadowRoot* shadowRoot = mOwningNode->AsContent()->GetContainingShadow();
    shadowRoot->StyleSheetChanged();
  }
}

void
StyleSheet::SetEnabled(bool aEnabled)
{
  // Internal method, so callers must handle BeginUpdate/EndUpdate
  bool oldDisabled = mDisabled;
  mDisabled = !aEnabled;

  if (IsComplete() && oldDisabled != mDisabled) {
    EnabledStateChanged();

    if (mDocument) {
      mDocument->SetStyleSheetApplicableState(this, !mDisabled);
    }
  }
}

StyleSheetInfo::StyleSheetInfo(CORSMode aCORSMode,
                               ReferrerPolicy aReferrerPolicy,
                               const dom::SRIMetadata& aIntegrity)
  : mPrincipal(nsNullPrincipal::Create())
  , mCORSMode(aCORSMode)
  , mReferrerPolicy(aReferrerPolicy)
  , mIntegrity(aIntegrity)
  , mComplete(false)
#ifdef DEBUG
  , mPrincipalSet(false)
#endif
{
  if (!mPrincipal) {
    NS_RUNTIMEABORT("nsNullPrincipal::Init failed");
  }
}

// nsIDOMStyleSheet interface

NS_IMETHODIMP
StyleSheet::GetType(nsAString& aType)
{
  aType.AssignLiteral("text/css");
  return NS_OK;
}

NS_IMETHODIMP
StyleSheet::GetDisabled(bool* aDisabled)
{
  *aDisabled = Disabled();
  return NS_OK;
}

NS_IMETHODIMP
StyleSheet::SetDisabled(bool aDisabled)
{
  // DOM method, so handle BeginUpdate/EndUpdate
  MOZ_AUTO_DOC_UPDATE(mDocument, UPDATE_STYLE, true);
  SetEnabled(!aDisabled);
  return NS_OK;
}

NS_IMETHODIMP
StyleSheet::GetOwnerNode(nsIDOMNode** aOwnerNode)
{
  nsCOMPtr<nsIDOMNode> ownerNode = do_QueryInterface(GetOwnerNode());
  ownerNode.forget(aOwnerNode);
  return NS_OK;
}

NS_IMETHODIMP
StyleSheet::GetHref(nsAString& aHref)
{
  if (nsIURI* sheetURI = SheetInfo().mOriginalSheetURI) {
    nsAutoCString str;
    nsresult rv = sheetURI->GetSpec(str);
    NS_ENSURE_SUCCESS(rv, rv);
    CopyUTF8toUTF16(str, aHref);
  } else {
    SetDOMStringToNull(aHref);
  }
  return NS_OK;
}

NS_IMETHODIMP
StyleSheet::GetTitle(nsAString& aTitle)
{
  aTitle.Assign(mTitle);
  return NS_OK;
}

// nsIDOMStyleSheet interface

NS_IMETHODIMP
StyleSheet::GetParentStyleSheet(nsIDOMStyleSheet** aParentStyleSheet)
{
  NS_ENSURE_ARG_POINTER(aParentStyleSheet);
  NS_IF_ADDREF(*aParentStyleSheet = GetParentStyleSheet());
  return NS_OK;
}

NS_IMETHODIMP
StyleSheet::GetMedia(nsIDOMMediaList** aMedia)
{
  NS_ADDREF(*aMedia = Media());
  return NS_OK;
}

NS_IMETHODIMP
StyleSheet::GetOwnerRule(nsIDOMCSSRule** aOwnerRule)
{
  NS_IF_ADDREF(*aOwnerRule = GetDOMOwnerRule());
  return NS_OK;
}

NS_IMETHODIMP
StyleSheet::GetCssRules(nsIDOMCSSRuleList** aCssRules)
{
  ErrorResult rv;
  nsCOMPtr<nsIDOMCSSRuleList> rules =
    GetCssRules(*nsContentUtils::SubjectPrincipal(), rv);
  rules.forget(aCssRules);
  return rv.StealNSResult();
}

NS_IMETHODIMP
StyleSheet::InsertRule(const nsAString& aRule, uint32_t aIndex,
                       uint32_t* aReturn)
{
  ErrorResult rv;
  *aReturn =
    InsertRule(aRule, aIndex, *nsContentUtils::SubjectPrincipal(), rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
StyleSheet::DeleteRule(uint32_t aIndex)
{
  ErrorResult rv;
  DeleteRule(aIndex, *nsContentUtils::SubjectPrincipal(), rv);
  return rv.StealNSResult();
}

// WebIDL CSSStyleSheet API

#define FORWARD_INTERNAL(method_, args_) \
  if (IsServo()) { \
    return AsServo()->method_ args_; \
  } \
  return AsGecko()->method_ args_;

dom::CSSRuleList*
StyleSheet::GetCssRules(nsIPrincipal& aSubjectPrincipal,
                        ErrorResult& aRv)
{
  if (!AreRulesAvailable(aSubjectPrincipal, aRv)) {
    return nullptr;
  }
  FORWARD_INTERNAL(GetCssRulesInternal, (aRv))
}

uint32_t
StyleSheet::InsertRule(const nsAString& aRule, uint32_t aIndex,
                       nsIPrincipal& aSubjectPrincipal,
                       ErrorResult& aRv)
{
  if (!AreRulesAvailable(aSubjectPrincipal, aRv)) {
    return 0;
  }
  FORWARD_INTERNAL(InsertRuleInternal, (aRule, aIndex, aRv))
}

void
StyleSheet::DeleteRule(uint32_t aIndex,
                       nsIPrincipal& aSubjectPrincipal,
                       ErrorResult& aRv)
{
  if (!AreRulesAvailable(aSubjectPrincipal, aRv)) {
    return;
  }
  FORWARD_INTERNAL(DeleteRuleInternal, (aIndex, aRv))
}

void
StyleSheet::EnabledStateChanged()
{
  FORWARD_INTERNAL(EnabledStateChangedInternal, ())
}

#undef FORWARD_INTERNAL

void
StyleSheet::SubjectSubsumesInnerPrincipal(nsIPrincipal& aSubjectPrincipal,
                                          ErrorResult& aRv)
{
  StyleSheetInfo& info = SheetInfo();

  if (aSubjectPrincipal.Subsumes(info.mPrincipal)) {
    return;
  }

  // Allow access only if CORS mode is not NONE
  if (GetCORSMode() == CORS_NONE) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  // Now make sure we set the principal of our inner to the subjectPrincipal.
  // We do this because we're in a situation where the caller would not normally
  // be able to access the sheet, but the sheet has opted in to being read.
  // Unfortunately, that means it's also opted in to being _edited_, and if the
  // caller now makes edits to the sheet we want the resulting resource loads,
  // if any, to look as if they are coming from the caller's principal, not the
  // original sheet principal.
  //
  // That means we need a unique inner, of course.  But we don't want to do that
  // if we're not complete yet.  Luckily, all the callers of this method throw
  // anyway if not complete, so we can just do that here too.
  if (!info.mComplete) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  WillDirty();

  info.mPrincipal = &aSubjectPrincipal;

  DidDirty();
}

bool
StyleSheet::AreRulesAvailable(nsIPrincipal& aSubjectPrincipal,
                              ErrorResult& aRv)
{
  // Rules are not available on incomplete sheets.
  if (!SheetInfo().mComplete) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return false;
  }
  //-- Security check: Only scripts whose principal subsumes that of the
  //   style sheet can access rule collections.
  SubjectSubsumesInnerPrincipal(aSubjectPrincipal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return false;
  }
  return true;
}

void
StyleSheet::SetMedia(nsMediaList* aMedia)
{
  mMedia = aMedia;
}

void
StyleSheet::DropMedia()
{
  if (mMedia) {
    mMedia->SetStyleSheet(nullptr);
    mMedia = nullptr;
  }
}

nsMediaList*
StyleSheet::Media()
{
  if (!mMedia) {
    mMedia = new nsMediaList();
    mMedia->SetStyleSheet(this);
  }

  return mMedia;
}

// nsWrapperCache

JSObject*
StyleSheet::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::CSSStyleSheetBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace mozilla
