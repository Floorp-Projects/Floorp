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
#include "mozilla/StaticPrefs.h"
#include "nsIDocument.h"
#include "nsStyleUtil.h"

namespace mozilla {
namespace dom {

// -- FontFaceBufferSource ---------------------------------------------------

/**
 * An object that wraps a FontFace object and exposes its ArrayBuffer
 * or ArrayBufferView data in a form the user font set can consume.
 */
class FontFaceBufferSource : public gfxFontFaceBufferSource
{
public:
  explicit FontFaceBufferSource(FontFace* aFontFace)
    : mFontFace(aFontFace) {}
  virtual void TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength) override;

private:
  RefPtr<FontFace> mFontFace;
};

void
FontFaceBufferSource::TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength)
{
  MOZ_ASSERT(mFontFace, "only call TakeBuffer once on a given "
                        "FontFaceBufferSource object");
  mFontFace->TakeBuffer(aBuffer, aLength);
  mFontFace = nullptr;
}

// -- Utility functions ------------------------------------------------------

template<typename T>
static void
GetDataFrom(const T& aObject, uint8_t*& aBuffer, uint32_t& aLength)
{
  MOZ_ASSERT(!aBuffer);
  aObject.ComputeLengthAndData();
  // We use malloc here rather than a FallibleTArray or fallible
  // operator new[] since the gfxUserFontEntry will be calling free
  // on it.
  aBuffer = (uint8_t*) malloc(aObject.Length());
  if (!aBuffer) {
    return;
  }
  memcpy((void*) aBuffer, aObject.Data(), aObject.Length());
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
  : mParent(aParent)
  , mLoadedRejection(NS_OK)
  , mStatus(FontFaceLoadStatus::Unloaded)
  , mSourceType(SourceType(0))
  , mSourceBuffer(nullptr)
  , mSourceBufferLength(0)
  , mFontFaceSet(aFontFaceSet)
  , mUnicodeRangeDirty(true)
  , mInFontFaceSet(false)
{
}

FontFace::~FontFace()
{
  // Assert that we don't drop any FontFace objects during a Servo traversal,
  // since PostTraversalTask objects can hold raw pointers to FontFaces.
  MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal());

  SetUserFontEntry(nullptr);

  if (mSourceBuffer) {
    free(mSourceBuffer);
  }
}

JSObject*
FontFace::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FontFaceBinding::Wrap(aCx, this, aGivenProto);
}

static FontFaceLoadStatus
LoadStateToStatus(gfxUserFontEntry::UserFontLoadState aLoadState)
{
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

already_AddRefed<FontFace>
FontFace::CreateForRule(nsISupports* aGlobal,
                        FontFaceSet* aFontFaceSet,
                        RawServoFontFaceRule* aRule)
{
  RefPtr<FontFace> obj = new FontFace(aGlobal, aFontFaceSet);
  obj->mRule = aRule;
  obj->mSourceType = eSourceType_FontFaceRule;
  obj->mInFontFaceSet = true;
  return obj.forget();
}

already_AddRefed<FontFace>
FontFace::Constructor(const GlobalObject& aGlobal,
                      const nsAString& aFamily,
                      const StringOrArrayBufferOrArrayBufferView& aSource,
                      const FontFaceDescriptors& aDescriptors,
                      ErrorResult& aRv)
{
  nsISupports* global = aGlobal.GetAsSupports();
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global);
  nsIDocument* doc = window->GetDoc();
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

void
FontFace::InitializeSource(const StringOrArrayBufferOrArrayBufferView& aSource)
{
  if (aSource.IsString()) {
    IgnoredErrorResult rv;
    if (!SetDescriptor(eCSSFontDesc_Src, aSource.GetAsString(), rv)) {
      Reject(NS_ERROR_DOM_SYNTAX_ERR);

      SetStatus(FontFaceLoadStatus::Error);
      return;
    }

    mSourceType = eSourceType_URLs;
    return;
  }

  mSourceType = FontFace::eSourceType_Buffer;

  if (aSource.IsArrayBuffer()) {
    GetDataFrom(aSource.GetAsArrayBuffer(),
                mSourceBuffer, mSourceBufferLength);
  } else {
    MOZ_ASSERT(aSource.IsArrayBufferView());
    GetDataFrom(aSource.GetAsArrayBufferView(),
                mSourceBuffer, mSourceBufferLength);
  }

  SetStatus(FontFaceLoadStatus::Loading);
  DoLoad();
}

void
FontFace::GetFamily(nsString& aResult)
{
  mFontFaceSet->FlushUserFontSet();

  // Serialize the same way as in nsCSSFontFaceStyleDecl::GetPropertyValue.
  nsCSSValue value;
  GetDesc(eCSSFontDesc_Family, value);

  aResult.Truncate();

  if (value.GetUnit() == eCSSUnit_Null) {
    return;
  }

  nsDependentString family(value.GetStringBufferValue());
  if (!family.IsEmpty()) {
    // The string length can be zero when the author passed an invalid
    // family name or an invalid descriptor to the JS FontFace constructor.
    nsStyleUtil::AppendEscapedCSSString(family, aResult);
  }
}

void
FontFace::SetFamily(const nsAString& aValue, ErrorResult& aRv)
{
  mFontFaceSet->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_Family, aValue, aRv);
}

void
FontFace::GetStyle(nsString& aResult)
{
  mFontFaceSet->FlushUserFontSet();
  GetDesc(eCSSFontDesc_Style, aResult);
}

void
FontFace::SetStyle(const nsAString& aValue, ErrorResult& aRv)
{
  mFontFaceSet->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_Style, aValue, aRv);
}

void
FontFace::GetWeight(nsString& aResult)
{
  mFontFaceSet->FlushUserFontSet();
  GetDesc(eCSSFontDesc_Weight, aResult);
}

void
FontFace::SetWeight(const nsAString& aValue, ErrorResult& aRv)
{
  mFontFaceSet->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_Weight, aValue, aRv);
}

void
FontFace::GetStretch(nsString& aResult)
{
  mFontFaceSet->FlushUserFontSet();
  GetDesc(eCSSFontDesc_Stretch, aResult);
}

void
FontFace::SetStretch(const nsAString& aValue, ErrorResult& aRv)
{
  mFontFaceSet->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_Stretch, aValue, aRv);
}

void
FontFace::GetUnicodeRange(nsString& aResult)
{
  mFontFaceSet->FlushUserFontSet();

  // There is no eCSSProperty_unicode_range for us to pass in to GetDesc
  // to get a serialized (possibly defaulted) value, but that function
  // doesn't use the property ID for this descriptor anyway.
  GetDesc(eCSSFontDesc_UnicodeRange, aResult);
}

void
FontFace::SetUnicodeRange(const nsAString& aValue, ErrorResult& aRv)
{
  mFontFaceSet->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_UnicodeRange, aValue, aRv);
}

void
FontFace::GetVariant(nsString& aResult)
{
  mFontFaceSet->FlushUserFontSet();

  // XXX Just expose the font-variant descriptor as "normal" until we
  // support it properly (bug 1055385).
  aResult.AssignLiteral("normal");
}

void
FontFace::SetVariant(const nsAString& aValue, ErrorResult& aRv)
{
  mFontFaceSet->FlushUserFontSet();

  // XXX Ignore assignments to variant until we support font-variant
  // descriptors (bug 1055385).
}

void
FontFace::GetFeatureSettings(nsString& aResult)
{
  mFontFaceSet->FlushUserFontSet();
  GetDesc(eCSSFontDesc_FontFeatureSettings, aResult);
}

void
FontFace::SetFeatureSettings(const nsAString& aValue, ErrorResult& aRv)
{
  mFontFaceSet->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_FontFeatureSettings, aValue, aRv);
}

void
FontFace::GetVariationSettings(nsString& aResult)
{
  mFontFaceSet->FlushUserFontSet();
  GetDesc(eCSSFontDesc_FontVariationSettings, aResult);
}

void
FontFace::SetVariationSettings(const nsAString& aValue, ErrorResult& aRv)
{
  mFontFaceSet->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_FontVariationSettings, aValue, aRv);
}

void
FontFace::GetDisplay(nsString& aResult)
{
  mFontFaceSet->FlushUserFontSet();
  GetDesc(eCSSFontDesc_Display, aResult);
}

void
FontFace::SetDisplay(const nsAString& aValue, ErrorResult& aRv)
{
  mFontFaceSet->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_Display, aValue, aRv);
}

FontFaceLoadStatus
FontFace::Status()
{
  return mStatus;
}

Promise*
FontFace::Load(ErrorResult& aRv)
{
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

gfxUserFontEntry*
FontFace::CreateUserFontEntry()
{
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

void
FontFace::DoLoad()
{
  if (!CreateUserFontEntry()) {
    return;
  }
  mUserFontEntry->Load();
}

Promise*
FontFace::GetLoaded(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  mFontFaceSet->FlushUserFontSet();

  EnsurePromise();

  if (!mLoaded) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return mLoaded;
}

void
FontFace::SetStatus(FontFaceLoadStatus aStatus)
{
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

void
FontFace::DoResolve()
{
  AssertIsMainThreadOrServoFontMetricsLocked();

  if (ServoStyleSet* ss = ServoStyleSet::Current()) {
    // See comments in Gecko_GetFontMetrics.
    ss->AppendTask(PostTraversalTask::ResolveFontFaceLoadedPromise(this));
    return;
  }

  mLoaded->MaybeResolve(this);
}

void
FontFace::DoReject(nsresult aResult)
{
  AssertIsMainThreadOrServoFontMetricsLocked();

  if (ServoStyleSet* ss = ServoStyleSet::Current()) {
    // See comments in Gecko_GetFontMetrics.
    ss->AppendTask(PostTraversalTask::RejectFontFaceLoadedPromise(this, aResult));
    return;
  }

  mLoaded->MaybeReject(aResult);
}

already_AddRefed<URLExtraData>
FontFace::GetURLExtraData() const
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);
  nsCOMPtr<nsIPrincipal> principal = global->PrincipalOrNull();

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mParent);
  nsCOMPtr<nsIURI> docURI = window->GetDocumentURI();
  nsCOMPtr<nsIURI> base = window->GetDocBaseURI();

  RefPtr<URLExtraData> url = new URLExtraData(base, docURI, principal);
  return url.forget();
}

bool
FontFace::SetDescriptor(nsCSSFontDesc aFontDesc,
                        const nsAString& aValue,
                        ErrorResult& aRv)
{
  // FIXME We probably don't need to distinguish between this anymore
  // since we have common backend now.
  NS_ASSERTION(!HasRule(),
               "we don't handle rule backed FontFace objects yet");
  if (HasRule()) {
    return false;
  }

  NS_ConvertUTF16toUTF8 value(aValue);
  RefPtr<URLExtraData> url = GetURLExtraData();
  if (!Servo_FontFaceRule_SetDescriptor(GetData(), aFontDesc, &value, url)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return false;
  }

  if (aFontDesc == eCSSFontDesc_UnicodeRange) {
    mUnicodeRangeDirty = true;
  }

  // XXX Setting descriptors doesn't actually have any effect on FontFace
  // objects that have started loading or have already been loaded.
  return true;
}

bool
FontFace::SetDescriptors(const nsAString& aFamily,
                         const FontFaceDescriptors& aDescriptors)
{
  MOZ_ASSERT(!HasRule());
  MOZ_ASSERT(!mDescriptors);

  IgnoredErrorResult rv;
  mDescriptors = Servo_FontFaceRule_CreateEmpty().Consume();

  // Parse all of the mDescriptors in aInitializer, which are the values
  // we got from the JS constructor.
  if (!SetDescriptor(eCSSFontDesc_Family, aFamily, rv) ||
      !SetDescriptor(eCSSFontDesc_Style, aDescriptors.mStyle, rv) ||
      !SetDescriptor(eCSSFontDesc_Weight, aDescriptors.mWeight, rv) ||
      !SetDescriptor(eCSSFontDesc_Stretch, aDescriptors.mStretch, rv) ||
      !SetDescriptor(eCSSFontDesc_UnicodeRange,
                     aDescriptors.mUnicodeRange, rv) ||
      !SetDescriptor(eCSSFontDesc_FontFeatureSettings,
                     aDescriptors.mFeatureSettings, rv) ||
      (StaticPrefs::layout_css_font_variations_enabled() &&
       !SetDescriptor(eCSSFontDesc_FontVariationSettings,
                      aDescriptors.mVariationSettings, rv)) ||
      !SetDescriptor(eCSSFontDesc_Display, aDescriptors.mDisplay, rv)) {
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

void
FontFace::GetDesc(nsCSSFontDesc aDescID, nsCSSValue& aResult) const
{
  aResult.Reset();
  Servo_FontFaceRule_GetDescriptor(GetData(), aDescID, &aResult);
}

void
FontFace::GetDesc(nsCSSFontDesc aDescID, nsString& aResult) const
{
  aResult.Truncate();
  Servo_FontFaceRule_GetDescriptorCssText(GetData(), aDescID, &aResult);

  // Fill in a default value for missing descriptors.
  if (aResult.IsEmpty()) {
    if (aDescID == eCSSFontDesc_UnicodeRange) {
      aResult.AssignLiteral("U+0-10FFFF");
    } else if (aDescID == eCSSFontDesc_Display) {
      aResult.AssignLiteral("auto");
    } else if (aDescID != eCSSFontDesc_Family &&
               aDescID != eCSSFontDesc_Src) {
      aResult.AssignLiteral("normal");
    }
  }
}

void
FontFace::SetUserFontEntry(gfxUserFontEntry* aEntry)
{
  if (mUserFontEntry) {
    mUserFontEntry->mFontFaces.RemoveElement(this);
  }

  mUserFontEntry = static_cast<Entry*>(aEntry);
  if (mUserFontEntry) {
    mUserFontEntry->mFontFaces.AppendElement(this);

    MOZ_ASSERT(mUserFontEntry->GetUserFontSet() ==
                 mFontFaceSet->GetUserFontSet(),
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

bool
FontFace::GetFamilyName(nsString& aResult)
{
  nsCSSValue value;
  GetDesc(eCSSFontDesc_Family, value);

  if (value.GetUnit() == eCSSUnit_String) {
    nsString familyname;
    value.GetStringValue(familyname);
    aResult.Append(familyname);
  }

  return !aResult.IsEmpty();
}

void
FontFace::DisconnectFromRule()
{
  MOZ_ASSERT(HasRule());

  // Make a copy of the descriptors.
  mDescriptors = Servo_FontFaceRule_Clone(mRule).Consume();
  mRule = nullptr;
  mInFontFaceSet = false;
}

bool
FontFace::HasFontData() const
{
  return mSourceType == eSourceType_Buffer && mSourceBuffer;
}

void
FontFace::TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength)
{
  MOZ_ASSERT(HasFontData());

  aBuffer = mSourceBuffer;
  aLength = mSourceBufferLength;

  mSourceBuffer = nullptr;
  mSourceBufferLength = 0;
}

already_AddRefed<gfxFontFaceBufferSource>
FontFace::CreateBufferSource()
{
  RefPtr<FontFaceBufferSource> bufferSource = new FontFaceBufferSource(this);
  return bufferSource.forget();
}

bool
FontFace::IsInFontFaceSet(FontFaceSet* aFontFaceSet) const
{
  if (mFontFaceSet == aFontFaceSet) {
    return mInFontFaceSet;
  }
  return mOtherFontFaceSets.Contains(aFontFaceSet);
}

void
FontFace::AddFontFaceSet(FontFaceSet* aFontFaceSet)
{
  MOZ_ASSERT(!IsInFontFaceSet(aFontFaceSet));

  if (mFontFaceSet == aFontFaceSet) {
    mInFontFaceSet = true;
  } else {
    mOtherFontFaceSets.AppendElement(aFontFaceSet);
  }
}

void
FontFace::RemoveFontFaceSet(FontFaceSet* aFontFaceSet)
{
  MOZ_ASSERT(IsInFontFaceSet(aFontFaceSet));

  if (mFontFaceSet == aFontFaceSet) {
    mInFontFaceSet = false;
  } else {
    mOtherFontFaceSets.RemoveElement(aFontFaceSet);
  }
}

void
FontFace::Reject(nsresult aResult)
{
  AssertIsMainThreadOrServoFontMetricsLocked();

  if (mLoaded) {
    DoReject(aResult);
  } else if (mLoadedRejection == NS_OK) {
    mLoadedRejection = aResult;
  }
}

void
FontFace::EnsurePromise()
{
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

gfxCharacterMap*
FontFace::GetUnicodeRangeAsCharacterMap()
{
  if (!mUnicodeRangeDirty) {
    return mUnicodeRange;
  }

  nsCSSValue val;
  GetDesc(eCSSFontDesc_UnicodeRange, val);

  if (val.GetUnit() == eCSSUnit_Array) {
    mUnicodeRange = new gfxCharacterMap();
    const nsCSSValue::Array& sources = *val.GetArrayValue();
    MOZ_ASSERT(sources.Count() % 2 == 0,
               "odd number of entries in a unicode-range: array");

    for (uint32_t i = 0; i < sources.Count(); i += 2) {
      uint32_t min = sources[i].GetIntValue();
      uint32_t max = sources[i+1].GetIntValue();
      mUnicodeRange->SetRange(min, max);
    }
  } else {
    mUnicodeRange = nullptr;
  }

  mUnicodeRangeDirty = false;
  return mUnicodeRange;
}

// -- FontFace::Entry --------------------------------------------------------

/* virtual */ void
FontFace::Entry::SetLoadState(UserFontLoadState aLoadState)
{
  gfxUserFontEntry::SetLoadState(aLoadState);

  for (size_t i = 0; i < mFontFaces.Length(); i++) {
    mFontFaces[i]->SetStatus(LoadStateToStatus(aLoadState));
  }
}

/* virtual */ void
FontFace::Entry::GetUserFontSets(nsTArray<gfxUserFontSet*>& aResult)
{
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

} // namespace dom
} // namespace mozilla
