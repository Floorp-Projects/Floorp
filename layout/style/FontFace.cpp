/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FontFace.h"

#include "mozilla/dom/FontFaceBinding.h"
#include "mozilla/dom/FontFaceSet.h"
#include "mozilla/dom/Promise.h"
#include "nsCSSRules.h"
#include "nsIDocument.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FontFace, mParent, mLoaded, mRule)

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
{
  MOZ_COUNT_CTOR(FontFace);

  MOZ_ASSERT(mPresContext);

  SetIsDOMBinding();
}

FontFace::~FontFace()
{
  MOZ_COUNT_DTOR(FontFace);
}

JSObject*
FontFace::WrapObject(JSContext* aCx)
{
  return FontFaceBinding::Wrap(aCx, this);
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
                        nsCSSFontFaceRule* aRule,
                        gfxUserFontEntry* aUserFontEntry)
{
  nsRefPtr<FontFace> obj = new FontFace(aGlobal, aPresContext);
  obj->mRule = aRule;
  obj->mStatus = LoadStateToStatus(aUserFontEntry->LoadState());
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
  return obj.forget();
}

void
FontFace::GetFamily(nsString& aResult)
{
}

void
FontFace::SetFamily(const nsAString& aValue, ErrorResult& aRv)
{
}

void
FontFace::GetStyle(nsString& aResult)
{
}

void
FontFace::SetStyle(const nsAString& aValue, ErrorResult& aRv)
{
}

void
FontFace::GetWeight(nsString& aResult)
{
}

void
FontFace::SetWeight(const nsAString& aValue, ErrorResult& aRv)
{
}

void
FontFace::GetStretch(nsString& aResult)
{
}

void
FontFace::SetStretch(const nsAString& aValue, ErrorResult& aRv)
{
}

void
FontFace::GetUnicodeRange(nsString& aResult)
{
}

void
FontFace::SetUnicodeRange(const nsAString& aValue, ErrorResult& aRv)
{
}

void
FontFace::GetVariant(nsString& aResult)
{
}

void
FontFace::SetVariant(const nsAString& aValue, ErrorResult& aRv)
{
}

void
FontFace::GetFeatureSettings(nsString& aResult)
{
}

void
FontFace::SetFeatureSettings(const nsAString& aValue, ErrorResult& aRv)
{
}

FontFaceLoadStatus
FontFace::Status()
{
  return mStatus;
}

Promise*
FontFace::Load()
{
  return Loaded();
}

Promise*
FontFace::Loaded()
{
  return nullptr;
}

void
FontFace::SetStatus(FontFaceLoadStatus aStatus)
{
  mStatus = aStatus;
}

// -- FontFace::Entry --------------------------------------------------------

/* virtual */ void
FontFace::Entry::SetLoadState(UserFontLoadState aLoadState)
{
  gfxUserFontEntry::SetLoadState(aLoadState);

  FontFace* face = GetFontFace();
  if (face) {
    face->SetStatus(LoadStateToStatus(aLoadState));
  }
}

FontFace*
FontFace::Entry::GetFontFace()
{
  FontFaceSet* fontFaceSet =
    static_cast<FontFaceSet::UserFontSet*>(mFontSet)->GetFontFaceSet();
  if (!fontFaceSet) {
    return nullptr;
  }

  return fontFaceSet->FindFontFaceForEntry(this);
}
