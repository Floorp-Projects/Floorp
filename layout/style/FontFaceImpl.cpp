/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FontFaceImpl.h"

#include <algorithm>
#include "gfxFontUtils.h"
#include "gfxPlatformFontList.h"
#include "mozilla/dom/CSSFontFaceRule.h"
#include "mozilla/dom/FontFaceBinding.h"
#include "mozilla/dom/FontFaceSetImpl.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoCSSParser.h"
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
  FontFaceBufferSource(uint8_t* aBuffer, uint32_t aLength)
      : mBuffer(aBuffer), mLength(aLength) {}

  void TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength) override {
    MOZ_ASSERT(mBuffer,
               "only call TakeBuffer once on a given "
               "FontFaceBufferSource object");
    aBuffer = mBuffer;
    aLength = mLength;
    mBuffer = nullptr;
    mLength = 0;
  }

 private:
  ~FontFaceBufferSource() override {
    if (mBuffer) {
      free(mBuffer);
    }
  }

  uint8_t* mBuffer;
  uint32_t mLength;
};

// -- FontFaceImpl -----------------------------------------------------------

FontFaceImpl::FontFaceImpl(FontFace* aOwner, FontFaceSetImpl* aFontFaceSet)
    : mOwner(aOwner),
      mStatus(FontFaceLoadStatus::Unloaded),
      mSourceType(SourceType(0)),
      mFontFaceSet(aFontFaceSet),
      mUnicodeRangeDirty(true),
      mInFontFaceSet(false) {}

FontFaceImpl::~FontFaceImpl() {
  // Assert that we don't drop any FontFace objects during a Servo traversal,
  // since PostTraversalTask objects can hold raw pointers to FontFaces.
  MOZ_ASSERT(!gfxFontUtils::IsInServoTraversal());

  SetUserFontEntry(nullptr);
}

void FontFaceImpl::Destroy() {
  mInFontFaceSet = false;
  SetUserFontEntry(nullptr);
  mOwner = nullptr;
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

already_AddRefed<FontFaceImpl> FontFaceImpl::CreateForRule(
    FontFace* aOwner, FontFaceSetImpl* aFontFaceSet,
    RawServoFontFaceRule* aRule) {
  RefPtr<FontFaceImpl> obj = new FontFaceImpl(aOwner, aFontFaceSet);
  obj->mRule = aRule;
  obj->mSourceType = eSourceType_FontFaceRule;
  obj->mInFontFaceSet = true;
  return obj.forget();
}

void FontFaceImpl::InitializeSourceURL(const nsACString& aURL) {
  MOZ_ASSERT(mOwner);
  mSourceType = eSourceType_URLs;

  IgnoredErrorResult rv;
  SetDescriptor(eCSSFontDesc_Src, aURL, rv);
  if (rv.Failed()) {
    mOwner->MaybeReject(NS_ERROR_DOM_SYNTAX_ERR);
    SetStatus(FontFaceLoadStatus::Error);
  }
}

void FontFaceImpl::InitializeSourceBuffer(uint8_t* aBuffer, uint32_t aLength) {
  MOZ_ASSERT(mOwner);
  MOZ_ASSERT(!mBufferSource);
  mSourceType = FontFaceImpl::eSourceType_Buffer;

  if (aBuffer) {
    mBufferSource = new FontFaceBufferSource(aBuffer, aLength);
  }

  SetStatus(FontFaceLoadStatus::Loading);
  DoLoad();
}

void FontFaceImpl::GetFamily(nsACString& aResult) {
  GetDesc(eCSSFontDesc_Family, aResult);
}

void FontFaceImpl::SetFamily(const nsACString& aValue, ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();
  if (SetDescriptor(eCSSFontDesc_Family, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFaceImpl::GetStyle(nsACString& aResult) {
  GetDesc(eCSSFontDesc_Style, aResult);
}

void FontFaceImpl::SetStyle(const nsACString& aValue, ErrorResult& aRv) {
  if (SetDescriptor(eCSSFontDesc_Style, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFaceImpl::GetWeight(nsACString& aResult) {
  GetDesc(eCSSFontDesc_Weight, aResult);
}

void FontFaceImpl::SetWeight(const nsACString& aValue, ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();
  if (SetDescriptor(eCSSFontDesc_Weight, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFaceImpl::GetStretch(nsACString& aResult) {
  GetDesc(eCSSFontDesc_Stretch, aResult);
}

void FontFaceImpl::SetStretch(const nsACString& aValue, ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();
  if (SetDescriptor(eCSSFontDesc_Stretch, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFaceImpl::GetUnicodeRange(nsACString& aResult) {
  GetDesc(eCSSFontDesc_UnicodeRange, aResult);
}

void FontFaceImpl::SetUnicodeRange(const nsACString& aValue, ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();
  if (SetDescriptor(eCSSFontDesc_UnicodeRange, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFaceImpl::GetVariant(nsACString& aResult) {
  // XXX Just expose the font-variant descriptor as "normal" until we
  // support it properly (bug 1055385).
  aResult.AssignLiteral("normal");
}

void FontFaceImpl::SetVariant(const nsACString& aValue, ErrorResult& aRv) {
  // XXX Ignore assignments to variant until we support font-variant
  // descriptors (bug 1055385).
}

void FontFaceImpl::GetFeatureSettings(nsACString& aResult) {
  GetDesc(eCSSFontDesc_FontFeatureSettings, aResult);
}

void FontFaceImpl::SetFeatureSettings(const nsACString& aValue,
                                      ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();
  if (SetDescriptor(eCSSFontDesc_FontFeatureSettings, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFaceImpl::GetVariationSettings(nsACString& aResult) {
  GetDesc(eCSSFontDesc_FontVariationSettings, aResult);
}

void FontFaceImpl::SetVariationSettings(const nsACString& aValue,
                                        ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();
  if (SetDescriptor(eCSSFontDesc_FontVariationSettings, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFaceImpl::GetDisplay(nsACString& aResult) {
  GetDesc(eCSSFontDesc_Display, aResult);
}

void FontFaceImpl::SetDisplay(const nsACString& aValue, ErrorResult& aRv) {
  if (SetDescriptor(eCSSFontDesc_Display, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFaceImpl::GetAscentOverride(nsACString& aResult) {
  GetDesc(eCSSFontDesc_AscentOverride, aResult);
}

void FontFaceImpl::SetAscentOverride(const nsACString& aValue,
                                     ErrorResult& aRv) {
  if (SetDescriptor(eCSSFontDesc_AscentOverride, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFaceImpl::GetDescentOverride(nsACString& aResult) {
  GetDesc(eCSSFontDesc_DescentOverride, aResult);
}

void FontFaceImpl::SetDescentOverride(const nsACString& aValue,
                                      ErrorResult& aRv) {
  if (SetDescriptor(eCSSFontDesc_DescentOverride, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFaceImpl::GetLineGapOverride(nsACString& aResult) {
  GetDesc(eCSSFontDesc_LineGapOverride, aResult);
}

void FontFaceImpl::SetLineGapOverride(const nsACString& aValue,
                                      ErrorResult& aRv) {
  if (SetDescriptor(eCSSFontDesc_LineGapOverride, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFaceImpl::GetSizeAdjust(nsACString& aResult) {
  GetDesc(eCSSFontDesc_SizeAdjust, aResult);
}

void FontFaceImpl::SetSizeAdjust(const nsACString& aValue, ErrorResult& aRv) {
  if (SetDescriptor(eCSSFontDesc_SizeAdjust, aValue, aRv)) {
    DescriptorUpdated();
  }
}

void FontFaceImpl::DescriptorUpdated() {
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

FontFaceLoadStatus FontFaceImpl::Status() { return mStatus; }

void FontFaceImpl::Load(ErrorResult& aRv) {
  mFontFaceSet->FlushUserFontSet();

  // Calling Load on a FontFace constructed with an ArrayBuffer data source,
  // or on one that is already loading (or has finished loading), has no
  // effect.
  if (mSourceType == eSourceType_Buffer ||
      mStatus != FontFaceLoadStatus::Unloaded) {
    return;
  }

  // Calling the user font entry's Load method will end up setting our
  // status to Loading, but the spec requires us to set it to Loading
  // here.
  SetStatus(FontFaceLoadStatus::Loading);

  DoLoad();
}

gfxUserFontEntry* FontFaceImpl::CreateUserFontEntry() {
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

void FontFaceImpl::DoLoad() {
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "FontFaceImpl::DoLoad", [self = RefPtr{this}]() { self->DoLoad(); }));
    return;
  }

  if (!CreateUserFontEntry()) {
    return;
  }
  mUserFontEntry->Load();
}

void FontFaceImpl::SetStatus(FontFaceLoadStatus aStatus) {
  gfxFontUtils::AssertSafeThreadOrServoFontMetricsLocked();

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

  for (FontFaceSetImpl* otherSet : mOtherFontFaceSets) {
    otherSet->OnFontFaceStatusChanged(this);
  }

  UpdateOwnerPromise();
}

void FontFaceImpl::UpdateOwnerPromise() {
  if (!mFontFaceSet->IsOnOwningThread()) {
    mFontFaceSet->DispatchToOwningThread(
        "FontFaceImpl::UpdateOwnerPromise",
        [self = RefPtr{this}] { self->UpdateOwnerPromise(); });
    return;
  }

  if (NS_WARN_IF(!mOwner)) {
    return;
  }

  if (mStatus == FontFaceLoadStatus::Loaded) {
    mOwner->MaybeResolve();
  } else if (mStatus == FontFaceLoadStatus::Error) {
    if (mSourceType == eSourceType_Buffer) {
      mOwner->MaybeReject(NS_ERROR_DOM_SYNTAX_ERR);
    } else {
      mOwner->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
    }
  }
}

// Boolean result indicates whether the value of the descriptor was actually
// changed.
bool FontFaceImpl::SetDescriptor(nsCSSFontDesc aFontDesc,
                                 const nsACString& aValue, ErrorResult& aRv) {
  // FIXME We probably don't need to distinguish between this anymore
  // since we have common backend now.
  NS_ASSERTION(!HasRule(), "we don't handle rule backed FontFace objects yet");
  if (HasRule()) {
    return false;
  }

  // FIXME(heycam): Should not allow modification of FontFaces that are
  // CSS-connected and whose rule is read only.
  RefPtr<URLExtraData> url = mFontFaceSet->GetURLExtraData();
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

bool FontFaceImpl::SetDescriptors(const nsACString& aFamily,
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

    if (mOwner) {
      mOwner->MaybeReject(NS_ERROR_DOM_SYNTAX_ERR);
    }

    SetStatus(FontFaceLoadStatus::Error);
    return false;
  }

  return true;
}

void FontFaceImpl::GetDesc(nsCSSFontDesc aDescID, nsACString& aResult) const {
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

void FontFaceImpl::SetUserFontEntry(gfxUserFontEntry* aEntry) {
  if (mUserFontEntry) {
    mUserFontEntry->mFontFaces.RemoveElement(this);
  }

  mUserFontEntry = static_cast<Entry*>(aEntry);
  if (mUserFontEntry) {
    mUserFontEntry->mFontFaces.AppendElement(this);

    MOZ_ASSERT(mUserFontEntry->GetUserFontSet() == mFontFaceSet,
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

Maybe<StyleComputedFontWeightRange> FontFaceImpl::GetFontWeight() const {
  StyleComputedFontWeightRange range;
  if (!Servo_FontFaceRule_GetFontWeight(GetData(), &range)) {
    return Nothing();
  }
  return Some(range);
}

Maybe<StyleComputedFontStretchRange> FontFaceImpl::GetFontStretch() const {
  StyleComputedFontStretchRange range;
  if (!Servo_FontFaceRule_GetFontStretch(GetData(), &range)) {
    return Nothing();
  }
  return Some(range);
}

Maybe<StyleComputedFontStyleDescriptor> FontFaceImpl::GetFontStyle() const {
  auto descriptor = StyleComputedFontStyleDescriptor::Normal();
  if (!Servo_FontFaceRule_GetFontStyle(GetData(), &descriptor)) {
    return Nothing();
  }
  return Some(descriptor);
}

Maybe<StyleFontDisplay> FontFaceImpl::GetFontDisplay() const {
  StyleFontDisplay display;
  if (!Servo_FontFaceRule_GetFontDisplay(GetData(), &display)) {
    return Nothing();
  }
  return Some(display);
}

Maybe<StyleFontLanguageOverride> FontFaceImpl::GetFontLanguageOverride() const {
  StyleFontLanguageOverride langOverride;
  if (!Servo_FontFaceRule_GetFontLanguageOverride(GetData(), &langOverride)) {
    return Nothing();
  }
  return Some(langOverride);
}

Maybe<StylePercentage> FontFaceImpl::GetAscentOverride() const {
  StylePercentage ascent{0};
  if (!Servo_FontFaceRule_GetAscentOverride(GetData(), &ascent)) {
    return Nothing();
  }
  return Some(ascent);
}

Maybe<StylePercentage> FontFaceImpl::GetDescentOverride() const {
  StylePercentage descent{0};
  if (!Servo_FontFaceRule_GetDescentOverride(GetData(), &descent)) {
    return Nothing();
  }
  return Some(descent);
}

Maybe<StylePercentage> FontFaceImpl::GetLineGapOverride() const {
  StylePercentage lineGap{0};
  if (!Servo_FontFaceRule_GetLineGapOverride(GetData(), &lineGap)) {
    return Nothing();
  }
  return Some(lineGap);
}

Maybe<StylePercentage> FontFaceImpl::GetSizeAdjust() const {
  StylePercentage sizeAdjust;
  if (!Servo_FontFaceRule_GetSizeAdjust(GetData(), &sizeAdjust)) {
    return Nothing();
  }
  return Some(sizeAdjust);
}

bool FontFaceImpl::HasLocalSrc() const {
  AutoTArray<StyleFontFaceSourceListComponent, 8> components;
  GetSources(components);
  for (auto& component : components) {
    if (component.tag == StyleFontFaceSourceListComponent::Tag::Local) {
      return true;
    }
  }
  return false;
}

void FontFaceImpl::GetFontFeatureSettings(
    nsTArray<gfxFontFeature>& aFeatures) const {
  Servo_FontFaceRule_GetFeatureSettings(GetData(), &aFeatures);
}

void FontFaceImpl::GetFontVariationSettings(
    nsTArray<gfxFontVariation>& aVariations) const {
  Servo_FontFaceRule_GetVariationSettings(GetData(), &aVariations);
}

void FontFaceImpl::GetSources(
    nsTArray<StyleFontFaceSourceListComponent>& aSources) const {
  Servo_FontFaceRule_GetSources(GetData(), &aSources);
}

nsAtom* FontFaceImpl::GetFamilyName() const {
  return Servo_FontFaceRule_GetFamilyName(GetData());
}

void FontFaceImpl::DisconnectFromRule() {
  MOZ_ASSERT(HasRule());

  // Make a copy of the descriptors.
  mDescriptors = Servo_FontFaceRule_Clone(mRule).Consume();
  mRule = nullptr;
  mInFontFaceSet = false;
}

bool FontFaceImpl::HasFontData() const {
  return mSourceType == eSourceType_Buffer && mBufferSource;
}

already_AddRefed<gfxFontFaceBufferSource> FontFaceImpl::TakeBufferSource() {
  MOZ_ASSERT(mBufferSource);
  return mBufferSource.forget();
}

bool FontFaceImpl::IsInFontFaceSet(FontFaceSetImpl* aFontFaceSet) const {
  if (mFontFaceSet == aFontFaceSet) {
    return mInFontFaceSet;
  }
  return mOtherFontFaceSets.Contains(aFontFaceSet);
}

void FontFaceImpl::AddFontFaceSet(FontFaceSetImpl* aFontFaceSet) {
  MOZ_ASSERT(!IsInFontFaceSet(aFontFaceSet));

  if (mFontFaceSet == aFontFaceSet) {
    mInFontFaceSet = true;
  } else {
    mOtherFontFaceSets.AppendElement(aFontFaceSet);
  }
}

void FontFaceImpl::RemoveFontFaceSet(FontFaceSetImpl* aFontFaceSet) {
  MOZ_ASSERT(IsInFontFaceSet(aFontFaceSet));

  if (mFontFaceSet == aFontFaceSet) {
    mInFontFaceSet = false;
  } else {
    mOtherFontFaceSets.RemoveElement(aFontFaceSet);
  }
}

gfxCharacterMap* FontFaceImpl::GetUnicodeRangeAsCharacterMap() {
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

// -- FontFaceImpl::Entry
// --------------------------------------------------------

/* virtual */
void FontFaceImpl::Entry::SetLoadState(UserFontLoadState aLoadState) {
  gfxUserFontEntry::SetLoadState(aLoadState);

  FontFaceLoadStatus status = LoadStateToStatus(aLoadState);
  for (size_t i = 0; i < mFontFaces.Length(); i++) {
    auto* impl = mFontFaces[i];
    auto* setImpl = impl->GetPrimaryFontFaceSet();
    if (setImpl->IsOnOwningThread()) {
      mFontFaces[i]->SetStatus(status);
    } else {
      setImpl->DispatchToOwningThread(
          "FontFaceImpl::Entry::SetLoadState",
          [self = RefPtr{impl}, status] { self->SetStatus(status); });
    }
  }
}

/* virtual */
void FontFaceImpl::Entry::GetUserFontSets(nsTArray<gfxUserFontSet*>& aResult) {
  aResult.Clear();

  for (FontFaceImpl* f : mFontFaces) {
    if (f->mInFontFaceSet) {
      aResult.AppendElement(f->mFontFaceSet);
    }
    for (FontFaceSetImpl* s : f->mOtherFontFaceSets) {
      aResult.AppendElement(s);
    }
  }

  // Remove duplicates.
  aResult.Sort();
  auto it = std::unique(aResult.begin(), aResult.end());
  aResult.TruncateLength(it - aResult.begin());
}

}  // namespace dom
}  // namespace mozilla
