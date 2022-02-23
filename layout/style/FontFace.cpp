/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FontFace.h"

#include <algorithm>
#include "mozilla/dom/CSSFontFaceRule.h"
#include "mozilla/dom/FontFaceBinding.h"
#include "mozilla/dom/FontFaceSet.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/dom/Document.h"
#include "nsStyleUtil.h"

namespace mozilla {
namespace dom {

// -- FontFaceBufferSource ---------------------------------------------------

/**
 * An object that wraps a FontFace object and exposes its ArrayBuffer
 * or ArrayBufferView data in a form the user font set can consume.
 */
class FontFaceBufferSource : public gfxFontFaceBufferSource {
 public:
  explicit FontFaceBufferSource(FontFace* aFontFace) : mFontFace(aFontFace) {}
  virtual void TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength) override;

 private:
  RefPtr<FontFace> mFontFace;
};

void FontFaceBufferSource::TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength) {
  MOZ_ASSERT(mFontFace,
             "only call TakeBuffer once on a given "
             "FontFaceBufferSource object");
  mFontFace->TakeBuffer(aBuffer, aLength);
  mFontFace = nullptr;
}

// -- Utility functions ------------------------------------------------------

template <typename T>
static void GetDataFrom(const T& aObject, uint8_t*& aBuffer,
                        uint32_t& aLength) {
  MOZ_ASSERT(!aBuffer);
  aObject.ComputeState();
  // We use malloc here rather than a FallibleTArray or fallible
  // operator new[] since the gfxUserFontEntry will be calling free
  // on it.
  aBuffer = (uint8_t*)malloc(aObject.Length());
  if (!aBuffer) {
    return;
  }
  memcpy((void*)aBuffer, aObject.Data(), aObject.Length());
  aLength = aObject.Length();
}

// -- FontFace ---------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(FontFace)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(FontFace)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoaded)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFontFaceSet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOtherFontFaceSets)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FontFace)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLoaded)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFontFaceSet)
  tmp->mInFontFaceSet = false;
  tmp->SetUserFontEntry(nullptr);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOtherFontFaceSets)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(FontFace)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FontFace)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FontFace)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FontFace)

FontFace::FontFace(nsISupports* aParent, FontFaceSet* aFontFaceSet)
    : mParent(aParent),
      mLoadedRejection(NS_OK),
      mStatus(FontFaceLoadStatus::Unloaded),
      mSourceType(SourceType(0)),
      mSourceBuffer(nullptr),
      mSourceBufferLength(0),
      mFontFaceSet(aFontFaceSet),
      mUnicodeRangeDirty(true),
      mInFontFaceSet(false) {}

FontFace::~FontFace() {
  // Assert that we don't drop any FontFace objects during a Servo traversal,
  // since PostTraversalTask objects can hold raw pointers to FontFaces.
  MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal());

  SetUserFontEntry(nullptr);

  if (mSourceBuffer) {
    free(mSourceBuffer);
  }
}

JSObject* FontFace::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return FontFace_Binding::Wrap(aCx, this, aGivenProto);
}

static FontFaceLoadStatus LoadStateToStatus(
    gfxUserFontEntry::UserFontLoadState aLoadState) {
  switch (aLoadState) {
    case gfxUserFontEntry::UserFontLoadState::STATUS_NOT_LOADED:
      return FontFaceLoadStatus::Unloaded;
    case gfxUserFontEntry::UserFontLoadState::STATUS_LOAD_PENDING:
    case gfxUserFontEntry::UserFontLoadState::STATUS_LOADING:
      return FontFaceLoadStatus::Loading;
    case gfxUserFontEntry::UserFontLoadState::STATUS_LOADED:
      return FontFaceLoadStatus::Loaded;
    case gfxUserFontEntry::UserFontLoadState::STATUS_FAILED:
      return FontFaceLoadStatus::Error;
  }
  MOZ_ASSERT_UNREACHABLE("invalid aLoadState value");
  return FontFaceLoadStatus::Error;
}

already_AddRefed<FontFace> FontFace::CreateForRule(
    nsISupports* aGlobal, FontFaceSet* aFontFaceSet,
    RawServoFontFaceRule* aRule) {
  RefPtr<FontFace> obj = new FontFace(aGlobal, aFontFaceSet);
  obj->mRule = aRule;
  obj->mSourceType = eSourceType_FontFaceRule;
  obj->mInFontFaceSet = true;
  return obj.forget();
}

already_AddRefed<FontFace> FontFace::Constructor(
    const GlobalObject& aGlobal, const nsACString& aFamily,
    const UTF8StringOrArrayBufferOrArrayBufferView& aSource,
    const FontFaceDescriptors& aDescriptors, ErrorResult& aRv) {
  nsISupports* global = aGlobal.GetAsSupports();
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global);
  Document* doc = window->GetDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<FontFace> obj = new FontFace(global, doc->Fonts());
  if (!obj->SetDescriptors(aFamily, aDescriptors)) {
    return obj.forget();
  }

  obj->InitializeSource(aSource);
  return obj.forget();
}

void FontFace::InitializeSource(
    const UTF8StringOrArrayBufferOrArrayBufferView& aSource) {
  if (aSource.IsUTF8String()) {
    IgnoredErrorResult rv;
    SetDescriptor(eCSSFontDesc_Src, aSource.GetAsUTF8String(), rv);
    if (rv.Failed()) {
      Reject(NS_ERROR_DOM_SYNTAX_ERR);

      SetStatus(FontFaceLoadStatus::Error);
      return;
    }

    mSourceType = eSourceType_URLs;
    return;
  }

  mSourceType = FontFace::eSourceType_Buffer;

  if (aSource.IsArrayBuffer()) {
    GetDataFrom(aSource.GetAsArrayBuffer(), mSourceBuffer, mSourceBufferLength);
  } else {
    MOZ_ASSERT(aSource.IsArrayBufferView());
    GetDataFrom(aSource.GetAsArrayBufferView(), mSourceBuffer,
                mSourceBufferLength);
  }

  SetStatus(FontFaceLoadStatus::Loading);
  DoLoad();
}

void FontFace::GetFamily(nsACString& aResult) {
  GetDesc(eCSSFontDesc_Family, aResult);
}

void FontFace::SetFamily(const nsACString& aValue, ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();
  if (SetDescriptor(eCSSFontDesc_Family, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFace::GetStyle(nsACString& aResult) {
  GetDesc(eCSSFontDesc_Style, aResult);
}

void FontFace::SetStyle(const nsACString& aValue, ErrorResult& aRv) {
  if (SetDescriptor(eCSSFontDesc_Style, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFace::GetWeight(nsACString& aResult) {
  GetDesc(eCSSFontDesc_Weight, aResult);
}

void FontFace::SetWeight(const nsACString& aValue, ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();
  if (SetDescriptor(eCSSFontDesc_Weight, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFace::GetStretch(nsACString& aResult) {
  GetDesc(eCSSFontDesc_Stretch, aResult);
}

void FontFace::SetStretch(const nsACString& aValue, ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();
  if (SetDescriptor(eCSSFontDesc_Stretch, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFace::GetUnicodeRange(nsACString& aResult) {
  GetDesc(eCSSFontDesc_UnicodeRange, aResult);
}

void FontFace::SetUnicodeRange(const nsACString& aValue, ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();
  if (SetDescriptor(eCSSFontDesc_UnicodeRange, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFace::GetVariant(nsACString& aResult) {
  // XXX Just expose the font-variant descriptor as "normal" until we
  // support it properly (bug 1055385).
  aResult.AssignLiteral("normal");
}

void FontFace::SetVariant(const nsACString& aValue, ErrorResult& aRv) {
  // XXX Ignore assignments to variant until we support font-variant
  // descriptors (bug 1055385).
}

void FontFace::GetFeatureSettings(nsACString& aResult) {
  GetDesc(eCSSFontDesc_FontFeatureSettings, aResult);
}

void FontFace::SetFeatureSettings(const nsACString& aValue, ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();
  if (SetDescriptor(eCSSFontDesc_FontFeatureSettings, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFace::GetVariationSettings(nsACString& aResult) {
  GetDesc(eCSSFontDesc_FontVariationSettings, aResult);
}

void FontFace::SetVariationSettings(const nsACString& aValue,
                                    ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();
  if (SetDescriptor(eCSSFontDesc_FontVariationSettings, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFace::GetDisplay(nsACString& aResult) {
  GetDesc(eCSSFontDesc_Display, aResult);
}

void FontFace::SetDisplay(const nsACString& aValue, ErrorResult& aRv) {
  if (SetDescriptor(eCSSFontDesc_Display, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFace::GetAscentOverride(nsACString& aResult) {
  GetDesc(eCSSFontDesc_AscentOverride, aResult);
}

void FontFace::SetAscentOverride(const nsACString& aValue, ErrorResult& aRv) {
  if (SetDescriptor(eCSSFontDesc_AscentOverride, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFace::GetDescentOverride(nsACString& aResult) {
  GetDesc(eCSSFontDesc_DescentOverride, aResult);
}

void FontFace::SetDescentOverride(const nsACString& aValue, ErrorResult& aRv) {
  if (SetDescriptor(eCSSFontDesc_DescentOverride, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFace::GetLineGapOverride(nsACString& aResult) {
  GetDesc(eCSSFontDesc_LineGapOverride, aResult);
}

void FontFace::SetLineGapOverride(const nsACString& aValue, ErrorResult& aRv) {
  if (SetDescriptor(eCSSFontDesc_LineGapOverride, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFace::GetSizeAdjust(nsACString& aResult) {
  GetDesc(eCSSFontDesc_SizeAdjust, aResult);
}

void FontFace::SetSizeAdjust(const nsACString& aValue, ErrorResult& aRv) {
  if (SetDescriptor(eCSSFontDesc_SizeAdjust, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFace::DescriptorUpdated() {
  // If we haven't yet initialized mUserFontEntry, no need to do anything here;
  // we'll respect the updated descriptor when the time comes to create it.
  if (!mUserFontEntry) {
    return;
  }

  // Behind the scenes, this will actually update the existing entry and return
  // it, rather than create a new one.
  RefPtr<gfxUserFontEntry> newEntry =
      mFontFaceSet->FindOrCreateUserFontEntryFromFontFace(this);
  SetUserFontEntry(newEntry);

  if (mInFontFaceSet) {
    mFontFaceSet->MarkUserFontSetDirty();
  }
  for (auto& set : mOtherFontFaceSets) {
    set->MarkUserFontSetDirty();
  }
}

FontFaceLoadStatus FontFace::Status() { return mStatus; }

Promise* FontFace::Load(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  mFontFaceSet->FlushUserFontSet();

  EnsurePromise();

  if (!mLoaded) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Calling Load on a FontFace constructed with an ArrayBuffer data source,
  // or on one that is already loading (or has finished loading), has no
  // effect.
  if (mSourceType == eSourceType_Buffer ||
      mStatus != FontFaceLoadStatus::Unloaded) {
    return mLoaded;
  }

  // Calling the user font entry's Load method will end up setting our
  // status to Loading, but the spec requires us to set it to Loading
  // here.
  SetStatus(FontFaceLoadStatus::Loading);

  DoLoad();

  return mLoaded;
}

gfxUserFontEntry* FontFace::CreateUserFontEntry() {
  if (!mUserFontEntry) {
    MOZ_ASSERT(!HasRule(),
               "Rule backed FontFace objects should already have a user font "
               "entry by the time Load() can be called on them");

    RefPtr<gfxUserFontEntry> newEntry =
        mFontFaceSet->FindOrCreateUserFontEntryFromFontFace(this);
    if (newEntry) {
      SetUserFontEntry(newEntry);
    }
  }

  return mUserFontEntry;
}

void FontFace::DoLoad() {
  if (!CreateUserFontEntry()) {
    return;
  }
  mUserFontEntry->Load();
}

Promise* FontFace::GetLoaded(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  EnsurePromise();

  if (!mLoaded) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return mLoaded;
}

void FontFace::SetStatus(FontFaceLoadStatus aStatus) {
  AssertIsMainThreadOrServoFontMetricsLocked();

  if (mStatus == aStatus) {
    return;
  }

  if (aStatus < mStatus) {
    // We're being asked to go backwards in status!  Normally, this shouldn't
    // happen.  But it can if the FontFace had a user font entry that had
    // loaded, but then was given a new one by FontFaceSet::InsertRuleFontFace
    // if we used a local() rule.  For now, just ignore the request to
    // go backwards in status.
    return;
  }

  mStatus = aStatus;

  if (mInFontFaceSet) {
    mFontFaceSet->OnFontFaceStatusChanged(this);
  }

  for (FontFaceSet* otherSet : mOtherFontFaceSets) {
    otherSet->OnFontFaceStatusChanged(this);
  }

  if (mStatus == FontFaceLoadStatus::Loaded) {
    if (mLoaded) {
      DoResolve();
    }
  } else if (mStatus == FontFaceLoadStatus::Error) {
    if (mSourceType == eSourceType_Buffer) {
      Reject(NS_ERROR_DOM_SYNTAX_ERR);
    } else {
      Reject(NS_ERROR_DOM_NETWORK_ERR);
    }
  }
}

void FontFace::DoResolve() {
  AssertIsMainThreadOrServoFontMetricsLocked();

  if (ServoStyleSet* ss = ServoStyleSet::Current()) {
    // See comments in Gecko_GetFontMetrics.
    ss->AppendTask(PostTraversalTask::ResolveFontFaceLoadedPromise(this));
    return;
  }

  mLoaded->MaybeResolve(this);
}

void FontFace::DoReject(nsresult aResult) {
  AssertIsMainThreadOrServoFontMetricsLocked();

  if (ServoStyleSet* ss = ServoStyleSet::Current()) {
    // See comments in Gecko_GetFontMetrics.
    ss->AppendTask(
        PostTraversalTask::RejectFontFaceLoadedPromise(this, aResult));
    return;
  }

  mLoaded->MaybeReject(aResult);
}

already_AddRefed<URLExtraData> FontFace::GetURLExtraData() const {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);
  nsCOMPtr<nsIPrincipal> principal = global->PrincipalOrNull();

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mParent);
  nsCOMPtr<nsIURI> docURI = window->GetDocumentURI();
  nsCOMPtr<nsIURI> base = window->GetDocBaseURI();

  // We pass RP_Unset when creating ReferrerInfo object here because it's not
  // going to result to change referer policy in a resource request.
  nsCOMPtr<nsIReferrerInfo> referrerInfo =
      new ReferrerInfo(docURI, ReferrerPolicy::_empty);

  RefPtr<URLExtraData> url = new URLExtraData(base, referrerInfo, principal);
  return url.forget();
}

// Boolean result indicates whether the value of the descriptor was actually
// changed.
bool FontFace::SetDescriptor(nsCSSFontDesc aFontDesc, const nsACString& aValue,
                             ErrorResult& aRv) {
  // FIXME We probably don't need to distinguish between this anymore
  // since we have common backend now.
  NS_ASSERTION(!HasRule(), "we don't handle rule backed FontFace objects yet");
  if (HasRule()) {
    return false;
  }

  // FIXME(heycam): Should not allow modification of FontFaces that are
  // CSS-connected and whose rule is read only.
  RefPtr<URLExtraData> url = GetURLExtraData();
  bool changed;
  if (!Servo_FontFaceRule_SetDescriptor(GetData(), aFontDesc, &aValue, url,
                                        &changed)) {
    aRv.ThrowSyntaxError("Invalid font descriptor");
    return false;
  }

  if (!changed) {
    return false;
  }

  if (aFontDesc == eCSSFontDesc_UnicodeRange) {
    mUnicodeRangeDirty = true;
  }

  return true;
}

bool FontFace::SetDescriptors(const nsACString& aFamily,
                              const FontFaceDescriptors& aDescriptors) {
  MOZ_ASSERT(!HasRule());
  MOZ_ASSERT(!mDescriptors);

  mDescriptors = Servo_FontFaceRule_CreateEmpty().Consume();

  // Helper to call SetDescriptor and return true on success, false on failure.
  auto setDesc = [=](nsCSSFontDesc aDesc, const nsACString& aVal) -> bool {
    IgnoredErrorResult rv;
    SetDescriptor(aDesc, aVal, rv);
    return !rv.Failed();
  };

  // Parse all of the mDescriptors in aInitializer, which are the values
  // we got from the JS constructor.
  if (!setDesc(eCSSFontDesc_Family, aFamily) ||
      !setDesc(eCSSFontDesc_Style, aDescriptors.mStyle) ||
      !setDesc(eCSSFontDesc_Weight, aDescriptors.mWeight) ||
      !setDesc(eCSSFontDesc_Stretch, aDescriptors.mStretch) ||
      !setDesc(eCSSFontDesc_UnicodeRange, aDescriptors.mUnicodeRange) ||
      !setDesc(eCSSFontDesc_FontFeatureSettings,
               aDescriptors.mFeatureSettings) ||
      (StaticPrefs::layout_css_font_variations_enabled() &&
       !setDesc(eCSSFontDesc_FontVariationSettings,
                aDescriptors.mVariationSettings)) ||
      !setDesc(eCSSFontDesc_Display, aDescriptors.mDisplay) ||
      (StaticPrefs::layout_css_font_metrics_overrides_enabled() &&
       (!setDesc(eCSSFontDesc_AscentOverride, aDescriptors.mAscentOverride) ||
        !setDesc(eCSSFontDesc_DescentOverride, aDescriptors.mDescentOverride) ||
        !setDesc(eCSSFontDesc_LineGapOverride,
                 aDescriptors.mLineGapOverride))) ||
      (StaticPrefs::layout_css_size_adjust_enabled() &&
       !setDesc(eCSSFontDesc_SizeAdjust, aDescriptors.mSizeAdjust))) {
    // XXX Handle font-variant once we support it (bug 1055385).

    // If any of the descriptors failed to parse, none of them should be set
    // on the FontFace.
    mDescriptors = Servo_FontFaceRule_CreateEmpty().Consume();

    Reject(NS_ERROR_DOM_SYNTAX_ERR);

    SetStatus(FontFaceLoadStatus::Error);
    return false;
  }

  return true;
}

void FontFace::GetDesc(nsCSSFontDesc aDescID, nsACString& aResult) const {
  aResult.Truncate();
  Servo_FontFaceRule_GetDescriptorCssText(GetData(), aDescID, &aResult);

  // Fill in a default value for missing descriptors.
  if (aResult.IsEmpty()) {
    if (aDescID == eCSSFontDesc_UnicodeRange) {
      aResult.AssignLiteral("U+0-10FFFF");
    } else if (aDescID == eCSSFontDesc_Display) {
      aResult.AssignLiteral("auto");
    } else if (aDescID != eCSSFontDesc_Family && aDescID != eCSSFontDesc_Src) {
      aResult.AssignLiteral("normal");
    }
  }
}

void FontFace::SetUserFontEntry(gfxUserFontEntry* aEntry) {
  if (mUserFontEntry) {
    mUserFontEntry->mFontFaces.RemoveElement(this);
  }

  mUserFontEntry = static_cast<Entry*>(aEntry);
  if (mUserFontEntry) {
    mUserFontEntry->mFontFaces.AppendElement(this);

    MOZ_ASSERT(
        mUserFontEntry->GetUserFontSet() == mFontFaceSet->GetUserFontSet(),
        "user font entry must be associated with the same user font set "
        "as the FontFace");

    // Our newly assigned user font entry might be in the process of or
    // finished loading, so set our status accordingly.  But only do so
    // if we're not going "backwards" in status, which could otherwise
    // happen in this case:
    //
    //   new FontFace("ABC", "url(x)").load();
    //
    // where the SetUserFontEntry call (from the after-initialization
    // DoLoad call) comes after the author's call to load(), which set mStatus
    // to Loading.
    FontFaceLoadStatus newStatus =
        LoadStateToStatus(mUserFontEntry->LoadState());
    if (newStatus > mStatus) {
      SetStatus(newStatus);
    }
  }
}

Maybe<StyleComputedFontWeightRange> FontFace::GetFontWeight() const {
  StyleComputedFontWeightRange range;
  if (!Servo_FontFaceRule_GetFontWeight(GetData(), &range)) {
    return Nothing();
  }
  return Some(range);
}

Maybe<StyleComputedFontStretchRange> FontFace::GetFontStretch() const {
  StyleComputedFontStretchRange range;
  if (!Servo_FontFaceRule_GetFontStretch(GetData(), &range)) {
    return Nothing();
  }
  return Some(range);
}

Maybe<StyleComputedFontStyleDescriptor> FontFace::GetFontStyle() const {
  auto descriptor = StyleComputedFontStyleDescriptor::Normal();
  if (!Servo_FontFaceRule_GetFontStyle(GetData(), &descriptor)) {
    return Nothing();
  }
  return Some(descriptor);
}

Maybe<StyleFontDisplay> FontFace::GetFontDisplay() const {
  StyleFontDisplay display;
  if (!Servo_FontFaceRule_GetFontDisplay(GetData(), &display)) {
    return Nothing();
  }
  return Some(display);
}

Maybe<StyleFontLanguageOverride> FontFace::GetFontLanguageOverride() const {
  StyleFontLanguageOverride langOverride;
  if (!Servo_FontFaceRule_GetFontLanguageOverride(GetData(), &langOverride)) {
    return Nothing();
  }
  return Some(langOverride);
}

Maybe<StylePercentage> FontFace::GetAscentOverride() const {
  StylePercentage ascent{0};
  if (!Servo_FontFaceRule_GetAscentOverride(GetData(), &ascent)) {
    return Nothing();
  }
  return Some(ascent);
}

Maybe<StylePercentage> FontFace::GetDescentOverride() const {
  StylePercentage descent{0};
  if (!Servo_FontFaceRule_GetDescentOverride(GetData(), &descent)) {
    return Nothing();
  }
  return Some(descent);
}

Maybe<StylePercentage> FontFace::GetLineGapOverride() const {
  StylePercentage lineGap{0};
  if (!Servo_FontFaceRule_GetLineGapOverride(GetData(), &lineGap)) {
    return Nothing();
  }
  return Some(lineGap);
}

Maybe<StylePercentage> FontFace::GetSizeAdjust() const {
  StylePercentage sizeAdjust;
  if (!Servo_FontFaceRule_GetSizeAdjust(GetData(), &sizeAdjust)) {
    return Nothing();
  }
  return Some(sizeAdjust);
}

bool FontFace::HasLocalSrc() const {
  AutoTArray<StyleFontFaceSourceListComponent, 8> components;
  GetSources(components);
  for (auto& component : components) {
    if (component.tag == StyleFontFaceSourceListComponent::Tag::Local) {
      return true;
    }
  }
  return false;
}

void FontFace::GetFontFeatureSettings(
    nsTArray<gfxFontFeature>& aFeatures) const {
  Servo_FontFaceRule_GetFeatureSettings(GetData(), &aFeatures);
}

void FontFace::GetFontVariationSettings(
    nsTArray<gfxFontVariation>& aVariations) const {
  Servo_FontFaceRule_GetVariationSettings(GetData(), &aVariations);
}

void FontFace::GetSources(
    nsTArray<StyleFontFaceSourceListComponent>& aSources) const {
  Servo_FontFaceRule_GetSources(GetData(), &aSources);
}

nsAtom* FontFace::GetFamilyName() const {
  return Servo_FontFaceRule_GetFamilyName(GetData());
}

void FontFace::DisconnectFromRule() {
  MOZ_ASSERT(HasRule());

  // Make a copy of the descriptors.
  mDescriptors = Servo_FontFaceRule_Clone(mRule).Consume();
  mRule = nullptr;
  mInFontFaceSet = false;
}

bool FontFace::HasFontData() const {
  return mSourceType == eSourceType_Buffer && mSourceBuffer;
}

void FontFace::TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength) {
  MOZ_ASSERT(HasFontData());

  aBuffer = mSourceBuffer;
  aLength = mSourceBufferLength;

  mSourceBuffer = nullptr;
  mSourceBufferLength = 0;
}

already_AddRefed<gfxFontFaceBufferSource> FontFace::CreateBufferSource() {
  RefPtr<FontFaceBufferSource> bufferSource = new FontFaceBufferSource(this);
  return bufferSource.forget();
}

bool FontFace::IsInFontFaceSet(FontFaceSet* aFontFaceSet) const {
  if (mFontFaceSet == aFontFaceSet) {
    return mInFontFaceSet;
  }
  return mOtherFontFaceSets.Contains(aFontFaceSet);
}

void FontFace::AddFontFaceSet(FontFaceSet* aFontFaceSet) {
  MOZ_ASSERT(!IsInFontFaceSet(aFontFaceSet));

  if (mFontFaceSet == aFontFaceSet) {
    mInFontFaceSet = true;
  } else {
    mOtherFontFaceSets.AppendElement(aFontFaceSet);
  }
}

void FontFace::RemoveFontFaceSet(FontFaceSet* aFontFaceSet) {
  MOZ_ASSERT(IsInFontFaceSet(aFontFaceSet));

  if (mFontFaceSet == aFontFaceSet) {
    mInFontFaceSet = false;
  } else {
    mOtherFontFaceSets.RemoveElement(aFontFaceSet);
  }
}

void FontFace::Reject(nsresult aResult) {
  AssertIsMainThreadOrServoFontMetricsLocked();

  if (mLoaded) {
    DoReject(aResult);
  } else if (mLoadedRejection == NS_OK) {
    mLoadedRejection = aResult;
  }
}

void FontFace::EnsurePromise() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mLoaded) {
    return;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);

  // If the pref is not set, don't create the Promise (which the page wouldn't
  // be able to get to anyway) as it causes the window.FontFace constructor
  // to be created.
  if (global && FontFaceSet::PrefEnabled()) {
    ErrorResult rv;
    mLoaded = Promise::Create(global, rv);

    if (mStatus == FontFaceLoadStatus::Loaded) {
      mLoaded->MaybeResolve(this);
    } else if (mLoadedRejection != NS_OK) {
      mLoaded->MaybeReject(mLoadedRejection);
    }
  }
}

gfxCharacterMap* FontFace::GetUnicodeRangeAsCharacterMap() {
  if (!mUnicodeRangeDirty) {
    return mUnicodeRange;
  }

  size_t len;
  const StyleUnicodeRange* rangesPtr =
      Servo_FontFaceRule_GetUnicodeRanges(GetData(), &len);

  Span<const StyleUnicodeRange> ranges(rangesPtr, len);
  if (!ranges.IsEmpty()) {
    RefPtr<gfxCharacterMap> charMap = new gfxCharacterMap();
    for (auto& range : ranges) {
      charMap->SetRange(range.start, range.end);
    }
    charMap->Compact();
    // As it's common for multiple font resources to have the same
    // unicode-range list, look for an existing copy of this map to share,
    // or add this one to the sharing cache if not already present.
    mUnicodeRange =
        gfxPlatformFontList::PlatformFontList()->FindCharMap(charMap);
  } else {
    mUnicodeRange = nullptr;
  }

  mUnicodeRangeDirty = false;
  return mUnicodeRange;
}

// -- FontFace::Entry --------------------------------------------------------

/* virtual */
void FontFace::Entry::SetLoadState(UserFontLoadState aLoadState) {
  gfxUserFontEntry::SetLoadState(aLoadState);

  for (size_t i = 0; i < mFontFaces.Length(); i++) {
    mFontFaces[i]->SetStatus(LoadStateToStatus(aLoadState));
  }
}

/* virtual */
void FontFace::Entry::GetUserFontSets(nsTArray<gfxUserFontSet*>& aResult) {
  aResult.Clear();

  for (FontFace* f : mFontFaces) {
    if (f->mInFontFaceSet) {
      aResult.AppendElement(f->mFontFaceSet->GetUserFontSet());
    }
    for (FontFaceSet* s : f->mOtherFontFaceSets) {
      aResult.AppendElement(s->GetUserFontSet());
    }
  }

  // Remove duplicates.
  aResult.Sort();
  auto it = std::unique(aResult.begin(), aResult.end());
  aResult.TruncateLength(it - aResult.begin());
}

}  // namespace dom
}  // namespace mozilla
