/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StyleSheet.h"

#include "mozilla/dom/CSSRuleList.h"
#include "mozilla/dom/MediaList.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/ServoStyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/CSSStyleSheet.h"

#include "mozAutoDocUpdate.h"
#include "nsMediaList.h"
#include "nsNullPrincipal.h"

namespace mozilla {

StyleSheet::StyleSheet(StyleBackendType aType, css::SheetParsingMode aParsingMode)
  : mParent(nullptr)
  , mDocument(nullptr)
  , mOwningNode(nullptr)
  , mParsingMode(aParsingMode)
  , mType(aType)
  , mDisabled(false)
  , mDocumentAssociationMode(NotOwnedByDocument)
  , mInner(nullptr)
{
}

StyleSheet::StyleSheet(const StyleSheet& aCopy,
                       nsIDocument* aDocumentToUse,
                       nsINode* aOwningNodeToUse)
  : mParent(nullptr)
  , mTitle(aCopy.mTitle)
  , mDocument(aDocumentToUse)
  , mOwningNode(aOwningNodeToUse)
  , mParsingMode(aCopy.mParsingMode)
  , mType(aCopy.mType)
  , mDisabled(aCopy.mDisabled)
    // We only use this constructor during cloning.  It's the cloner's
    // responsibility to notify us if we end up being owned by a document.
  , mDocumentAssociationMode(NotOwnedByDocument)
  , mInner(aCopy.mInner) // Shallow copy, but concrete subclasses will fix up.
{
  MOZ_ASSERT(mInner, "Should only copy StyleSheets with an mInner.");
  mInner->AddSheet(this);

  if (aCopy.mMedia) {
    // XXX This is wrong; we should be keeping @import rules and
    // sheets in sync!
    mMedia = aCopy.mMedia->Clone();
  }
}

StyleSheet::~StyleSheet()
{
  MOZ_ASSERT(mInner, "Should have an mInner at time of destruction.");
  MOZ_ASSERT(mInner->mSheets.Contains(this), "Our mInner should include us.");
  mInner->RemoveSheet(this);
  mInner = nullptr;

  DropMedia();
}

void
StyleSheet::UnlinkInner()
{
  // We can only have a cycle through our inner if we have a unique inner,
  // because otherwise there are no JS wrappers for anything in the inner.
  if (mInner->mSheets.Length() != 1) {
    return;
  }

  // Have to be a bit careful with child sheets, because we want to
  // drop their mNext pointers and null out their mParent and
  // mDocument, but don't want to work with deleted objects.  And we
  // don't want to do any addrefing in the process, just to make sure
  // we don't confuse the cycle collector (though on the face of it,
  // addref/release pairs during unlink should probably be ok).
  RefPtr<StyleSheet> child;
  child.swap(SheetInfo().mFirstChild);
  while (child) {
    MOZ_ASSERT(child->mParent == this, "We have a unique inner!");
    child->mParent = nullptr;
    child->mDocument = nullptr;

    RefPtr<StyleSheet> next;
    // Null out child->mNext, but don't let it die yet
    next.swap(child->mNext);
    // Switch to looking at the old value of child->mNext next iteration
    child.swap(next);
    // "next" is now our previous value of child; it'll get released
    // as we loop around.
  }
}

void
StyleSheet::TraverseInner(nsCycleCollectionTraversalCallback &cb)
{
  // We can only have a cycle through our inner if we have a unique inner,
  // because otherwise there are no JS wrappers for anything in the inner.
  if (mInner->mSheets.Length() != 1) {
    return;
  }

  StyleSheet* childSheet = GetFirstChild();
  while (childSheet) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "child sheet");
    cb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIDOMCSSStyleSheet*, childSheet));
    childSheet = childSheet->mNext;
  }
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
  tmp->TraverseInner(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(StyleSheet)
  tmp->DropMedia();
  tmp->UnlinkInner();
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

StyleSheetInfo::StyleSheetInfo(StyleSheetInfo& aCopy,
                               StyleSheet* aPrimarySheet)
  : mSheetURI(aCopy.mSheetURI)
  , mOriginalSheetURI(aCopy.mOriginalSheetURI)
  , mBaseURI(aCopy.mBaseURI)
  , mPrincipal(aCopy.mPrincipal)
  , mCORSMode(aCopy.mCORSMode)
  , mReferrerPolicy(aCopy.mReferrerPolicy)
  , mIntegrity(aCopy.mIntegrity)
  , mComplete(aCopy.mComplete)
  , mFirstChild()  // We don't rebuild the child because we're making a copy
                   // without children.
#ifdef DEBUG
  , mPrincipalSet(aCopy.mPrincipalSet)
#endif
{
  AddSheet(aPrimarySheet);
}

void
StyleSheetInfo::AddSheet(StyleSheet* aSheet)
{
  mSheets.AppendElement(aSheet);
}

void
StyleSheetInfo::RemoveSheet(StyleSheet* aSheet)
{
  if ((aSheet == mSheets.ElementAt(0)) && (mSheets.Length() > 1)) {
    StyleSheet::ChildSheetListBuilder::ReparentChildList(mSheets[1], mFirstChild);
  }

  if (1 == mSheets.Length()) {
    NS_ASSERTION(aSheet == mSheets.ElementAt(0), "bad parent");
    delete this;
    return;
  }

  mSheets.RemoveElement(aSheet);
}

void
StyleSheet::ChildSheetListBuilder::SetParentLinks(StyleSheet* aSheet)
{
  aSheet->mParent = parent;
  aSheet->SetAssociatedDocument(parent->mDocument,
                                parent->mDocumentAssociationMode);
}

void
StyleSheet::ChildSheetListBuilder::ReparentChildList(StyleSheet* aPrimarySheet,
                                                     StyleSheet* aFirstChild)
{
  for (StyleSheet *child = aFirstChild; child; child = child->mNext) {
    child->mParent = aPrimarySheet;
    child->SetAssociatedDocument(aPrimarySheet->mDocument,
                                 aPrimarySheet->mDocumentAssociationMode);
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
StyleSheet::UnparentChildren()
{
  // XXXbz this is a little bogus; see the XXX comment where we
  // declare mFirstChild in StyleSheetInfo.
  for (StyleSheet* child = GetFirstChild();
       child;
       child = child->mNext) {
    if (child->mParent == this) {
      child->mParent = nullptr;
      child->mDocument = nullptr;
    }
  }
}

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

StyleSheet*
StyleSheet::GetFirstChild() const
{
  return SheetInfo().mFirstChild;
}

void
StyleSheet::SetAssociatedDocument(nsIDocument* aDocument,
                                  DocumentAssociationMode aAssociationMode)
{
  MOZ_ASSERT_IF(!aDocument, aAssociationMode == NotOwnedByDocument);

  // not ref counted
  mDocument = aDocument;
  mDocumentAssociationMode = aAssociationMode;

  // Now set the same document on all our child sheets....
  // XXXbz this is a little bogus; see the XXX comment where we
  // declare mFirstChild.
  for (StyleSheet* child = GetFirstChild();
       child; child = child->mNext) {
    if (child->mParent == this) {
      child->SetAssociatedDocument(aDocument, aAssociationMode);
    }
  }
}

void
StyleSheet::ClearAssociatedDocument()
{
  SetAssociatedDocument(nullptr, NotOwnedByDocument);
}

void
StyleSheet::AppendStyleSheet(StyleSheet* aSheet)
{
  NS_PRECONDITION(nullptr != aSheet, "null arg");

  WillDirty();
  RefPtr<StyleSheet>* tail = &SheetInfo().mFirstChild;
  while (*tail) {
    tail = &(*tail)->mNext;
  }
  *tail = aSheet;

  // This is not reference counted. Our parent tells us when
  // it's going away.
  aSheet->mParent = this;
  aSheet->mDocument = mDocument;
  DidDirty();
}

size_t
StyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  const StyleSheet* s = this;
  while (s) {
    n += aMallocSizeOf(s);

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - s->mTitle
    // - s->mMedia

    s = s->mNext;
  }
  return n;
}

#ifdef DEBUG
void
StyleSheet::List(FILE* out, int32_t aIndent) const
{
  int32_t index;

  // Indent
  nsAutoCString str;
  for (index = aIndent; --index >= 0; ) {
    str.AppendLiteral("  ");
  }

  str.AppendLiteral("CSS Style Sheet: ");
  nsAutoCString urlSpec;
  nsresult rv = GetSheetURI()->GetSpec(urlSpec);
  if (NS_SUCCEEDED(rv) && !urlSpec.IsEmpty()) {
    str.Append(urlSpec);
  }

  if (mMedia) {
    str.AppendLiteral(" media: ");
    nsAutoString  buffer;
    mMedia->GetText(buffer);
    AppendUTF16toUTF8(buffer, str);
  }
  str.Append('\n');
  fprintf_stderr(out, "%s", str.get());

  for (const StyleSheet* child = GetFirstChild();
       child;
       child = child->mNext) {
    child->List(out, aIndent + 1);
  }
}
#endif

void
StyleSheet::SetMedia(dom::MediaList* aMedia)
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

dom::MediaList*
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
