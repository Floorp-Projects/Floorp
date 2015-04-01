/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FontFace.h"

#include "mozilla/dom/FontFaceBinding.h"
#include "mozilla/dom/FontFaceSet.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "nsCSSParser.h"
#include "nsCSSRules.h"
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
  virtual void TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength);

private:
  nsRefPtr<FontFace> mFontFace;
};

void
FontFaceBufferSource::TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength)
{
  mFontFace->TakeBuffer(aBuffer, aLength);
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRule)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFontFaceSet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FontFace)
  if (!tmp->IsInFontFaceSet()) {
    tmp->mFontFaceSet->RemoveUnavailableFontFace(tmp);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLoaded)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRule)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFontFaceSet)
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

FontFace::FontFace(nsISupports* aParent, nsPresContext* aPresContext)
  : mParent(aParent)
  , mPresContext(aPresContext)
  , mStatus(FontFaceLoadStatus::Unloaded)
  , mSourceType(SourceType(0))
  , mSourceBuffer(nullptr)
  , mSourceBufferLength(0)
  , mFontFaceSet(aPresContext->Fonts())
  , mInFontFaceSet(false)
{
  MOZ_COUNT_CTOR(FontFace);

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aParent);

  // If the pref is not set, don't create the Promise (which the page wouldn't
  // be able to get to anyway) as it causes the window.FontFace constructor
  // to be created.
  if (global && FontFaceSet::PrefEnabled()) {
    ErrorResult rv;
    mLoaded = Promise::Create(global, rv);
  }
}

FontFace::~FontFace()
{
  MOZ_COUNT_DTOR(FontFace);

  SetUserFontEntry(nullptr);

  if (mFontFaceSet && !IsInFontFaceSet()) {
    mFontFaceSet->RemoveUnavailableFontFace(this);
  }

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
    case gfxUserFontEntry::UserFontLoadState::STATUS_LOADING:
      return FontFaceLoadStatus::Loading;
    case gfxUserFontEntry::UserFontLoadState::STATUS_LOADED:
      return FontFaceLoadStatus::Loaded;
    case gfxUserFontEntry::UserFontLoadState::STATUS_FAILED:
      return FontFaceLoadStatus::Error;
  }
  NS_NOTREACHED("invalid aLoadState value");
  return FontFaceLoadStatus::Error;
}

already_AddRefed<FontFace>
FontFace::CreateForRule(nsISupports* aGlobal,
                        nsPresContext* aPresContext,
                        nsCSSFontFaceRule* aRule)
{
  nsCOMPtr<nsIGlobalObject> globalObject = do_QueryInterface(aGlobal);

  nsRefPtr<FontFace> obj = new FontFace(aGlobal, aPresContext);
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
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(global);
  nsIDocument* doc = window->GetDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsIPresShell* shell = doc->GetShell();
  if (!shell) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsPresContext* presContext = shell->GetPresContext();
  if (!presContext) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<FontFace> obj = new FontFace(global, presContext);
  obj->mFontFaceSet->AddUnavailableFontFace(obj);
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
    if (!ParseDescriptor(eCSSFontDesc_Src,
                         aSource.GetAsString(),
                         mDescriptors->mSrc)) {
      if (mLoaded) {
        // The SetStatus call we are about to do assumes that for
        // FontFace objects with sources other than ArrayBuffer(View)s, that the
        // mLoaded Promise is rejected with a network error.  We get
        // in here beforehand to set it to the required syntax error.
        mLoaded->MaybeReject(NS_ERROR_DOM_SYNTAX_ERR);
      }

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
  mPresContext->FlushUserFontSet();

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
  mPresContext->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_Family, aValue, aRv);
}

void
FontFace::GetStyle(nsString& aResult)
{
  mPresContext->FlushUserFontSet();
  GetDesc(eCSSFontDesc_Style, eCSSProperty_font_style, aResult);
}

void
FontFace::SetStyle(const nsAString& aValue, ErrorResult& aRv)
{
  mPresContext->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_Style, aValue, aRv);
}

void
FontFace::GetWeight(nsString& aResult)
{
  mPresContext->FlushUserFontSet();
  GetDesc(eCSSFontDesc_Weight, eCSSProperty_font_weight, aResult);
}

void
FontFace::SetWeight(const nsAString& aValue, ErrorResult& aRv)
{
  mPresContext->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_Weight, aValue, aRv);
}

void
FontFace::GetStretch(nsString& aResult)
{
  mPresContext->FlushUserFontSet();
  GetDesc(eCSSFontDesc_Stretch, eCSSProperty_font_stretch, aResult);
}

void
FontFace::SetStretch(const nsAString& aValue, ErrorResult& aRv)
{
  mPresContext->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_Stretch, aValue, aRv);
}

void
FontFace::GetUnicodeRange(nsString& aResult)
{
  mPresContext->FlushUserFontSet();

  // There is no eCSSProperty_unicode_range for us to pass in to GetDesc
  // to get a serialized (possibly defaulted) value, but that function
  // doesn't use the property ID for this descriptor anyway.
  GetDesc(eCSSFontDesc_UnicodeRange, eCSSProperty_UNKNOWN, aResult);
}

void
FontFace::SetUnicodeRange(const nsAString& aValue, ErrorResult& aRv)
{
  mPresContext->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_UnicodeRange, aValue, aRv);
}

void
FontFace::GetVariant(nsString& aResult)
{
  mPresContext->FlushUserFontSet();

  // XXX Just expose the font-variant descriptor as "normal" until we
  // support it properly (bug 1055385).
  aResult.AssignLiteral("normal");
}

void
FontFace::SetVariant(const nsAString& aValue, ErrorResult& aRv)
{
  mPresContext->FlushUserFontSet();

  // XXX Ignore assignments to variant until we support font-variant
  // descriptors (bug 1055385).
}

void
FontFace::GetFeatureSettings(nsString& aResult)
{
  mPresContext->FlushUserFontSet();
  GetDesc(eCSSFontDesc_FontFeatureSettings, eCSSProperty_font_feature_settings,
          aResult);
}

void
FontFace::SetFeatureSettings(const nsAString& aValue, ErrorResult& aRv)
{
  mPresContext->FlushUserFontSet();
  SetDescriptor(eCSSFontDesc_FontFeatureSettings, aValue, aRv);
}

FontFaceLoadStatus
FontFace::Status()
{
  return mStatus;
}

Promise*
FontFace::Load(ErrorResult& aRv)
{
  mPresContext->FlushUserFontSet();

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

void
FontFace::DoLoad()
{
  if (!mUserFontEntry) {
    MOZ_ASSERT(!HasRule(),
               "Rule backed FontFace objects should already have a user font "
               "entry by the time Load() can be called on them");

    nsRefPtr<gfxUserFontEntry> newEntry =
      mFontFaceSet->FindOrCreateUserFontEntryFromFontFace(this);
    if (!newEntry) {
      return;
    }

    SetUserFontEntry(newEntry);
  }

  mUserFontEntry->Load();
}

Promise*
FontFace::GetLoaded(ErrorResult& aRv)
{
  mPresContext->FlushUserFontSet();

  if (!mLoaded) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return mLoaded;
}

void
FontFace::SetStatus(FontFaceLoadStatus aStatus)
{
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

  if (!mLoaded) {
    return;
  }

  if (mStatus == FontFaceLoadStatus::Loaded) {
    mLoaded->MaybeResolve(this);
  } else if (mStatus == FontFaceLoadStatus::Error) {
    if (mSourceType == eSourceType_Buffer) {
      mLoaded->MaybeReject(NS_ERROR_DOM_SYNTAX_ERR);
    } else {
      mLoaded->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
    }
  }
}

bool
FontFace::ParseDescriptor(nsCSSFontDesc aDescID,
                          const nsAString& aString,
                          nsCSSValue& aResult)
{
  nsCSSParser parser;

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);
  nsCOMPtr<nsIPrincipal> principal = global->PrincipalOrNull();

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mParent);
  nsCOMPtr<nsIURI> base = window->GetDocBaseURI();

  if (!parser.ParseFontFaceDescriptor(aDescID, aString,
                                      nullptr, // aSheetURL
                                      base,
                                      principal,
                                      aResult)) {
    aResult.Reset();
    return false;
  }

  return true;
}

void
FontFace::SetDescriptor(nsCSSFontDesc aFontDesc,
                        const nsAString& aValue,
                        ErrorResult& aRv)
{
  NS_ASSERTION(!HasRule(),
               "we don't handle rule backed FontFace objects yet");
  if (HasRule()) {
    return;
  }

  nsCSSValue parsedValue;
  if (!ParseDescriptor(aFontDesc, aValue, parsedValue)) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  mDescriptors->Get(aFontDesc) = parsedValue;

  // XXX Setting descriptors doesn't actually have any effect on FontFace
  // objects that have started loading or have already been loaded.
}

bool
FontFace::SetDescriptors(const nsAString& aFamily,
                         const FontFaceDescriptors& aDescriptors)
{
  MOZ_ASSERT(!HasRule());
  MOZ_ASSERT(!mDescriptors);

  mDescriptors = new CSSFontFaceDescriptors;

  // Parse all of the mDescriptors in aInitializer, which are the values
  // we got from the JS constructor.
  if (!ParseDescriptor(eCSSFontDesc_Family,
                       aFamily,
                       mDescriptors->mFamily) ||
      *mDescriptors->mFamily.GetStringBufferValue() == 0 ||
      !ParseDescriptor(eCSSFontDesc_Style,
                       aDescriptors.mStyle,
                       mDescriptors->mStyle) ||
      !ParseDescriptor(eCSSFontDesc_Weight,
                       aDescriptors.mWeight,
                       mDescriptors->mWeight) ||
      !ParseDescriptor(eCSSFontDesc_Stretch,
                       aDescriptors.mStretch,
                       mDescriptors->mStretch) ||
      !ParseDescriptor(eCSSFontDesc_UnicodeRange,
                       aDescriptors.mUnicodeRange,
                       mDescriptors->mUnicodeRange) ||
      !ParseDescriptor(eCSSFontDesc_FontFeatureSettings,
                       aDescriptors.mFeatureSettings,
                       mDescriptors->mFontFeatureSettings)) {
    // XXX Handle font-variant once we support it (bug 1055385).

    // If any of the descriptors failed to parse, none of them should be set
    // on the FontFace.
    mDescriptors = new CSSFontFaceDescriptors;

    if (mLoaded) {
      mLoaded->MaybeReject(NS_ERROR_DOM_SYNTAX_ERR);
    }

    SetStatus(FontFaceLoadStatus::Error);
    return false;
  }

  return true;
}

void
FontFace::GetDesc(nsCSSFontDesc aDescID, nsCSSValue& aResult) const
{
  if (HasRule()) {
    MOZ_ASSERT(mRule);
    MOZ_ASSERT(!mDescriptors);
    mRule->GetDesc(aDescID, aResult);
  } else {
    aResult = mDescriptors->Get(aDescID);
  }
}

void
FontFace::GetDesc(nsCSSFontDesc aDescID,
                  nsCSSProperty aPropID,
                  nsString& aResult) const
{
  MOZ_ASSERT(aDescID == eCSSFontDesc_UnicodeRange ||
             aPropID != eCSSProperty_UNKNOWN,
             "only pass eCSSProperty_UNKNOWN for eCSSFontDesc_UnicodeRange");

  nsCSSValue value;
  GetDesc(aDescID, value);

  aResult.Truncate();

  // Fill in a default value for missing descriptors.
  if (value.GetUnit() == eCSSUnit_Null) {
    if (aDescID == eCSSFontDesc_UnicodeRange) {
      aResult.AssignLiteral("U+0-10FFFF");
    } else if (aDescID != eCSSFontDesc_Family &&
               aDescID != eCSSFontDesc_Src) {
      aResult.AssignLiteral("normal");
    }
    return;
  }

  if (aDescID == eCSSFontDesc_UnicodeRange) {
    // Since there's no unicode-range property, we can't use
    // nsCSSValue::AppendToString to serialize this descriptor.
    nsStyleUtil::AppendUnicodeRange(value, aResult);
  } else {
    value.AppendToString(aPropID, aResult, nsCSSValue::eNormalized);
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
  mDescriptors = new CSSFontFaceDescriptors;
  mRule->GetDescriptors(*mDescriptors);
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
  nsRefPtr<FontFaceBufferSource> bufferSource = new FontFaceBufferSource(this);
  return bufferSource.forget();
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

} // namespace dom
} // namespace mozilla
