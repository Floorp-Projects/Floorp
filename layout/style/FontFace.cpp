/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FontFace.h"

#include <algorithm>
#include "mozilla/dom/CSSFontFaceRule.h"
#include "mozilla/dom/FontFaceBinding.h"
#include "mozilla/dom/FontFaceImpl.h"
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

// -- FontFace ---------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(FontFace)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(FontFace)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoaded)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FontFace)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLoaded)
  tmp->Destroy();
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

FontFace::FontFace(nsISupports* aParent)
    : mParent(aParent), mLoadedRejection(NS_OK) {}

FontFace::~FontFace() {
  // Assert that we don't drop any FontFace objects during a Servo traversal,
  // since PostTraversalTask objects can hold raw pointers to FontFaces.
  MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal());
  Destroy();
}

void FontFace::Destroy() { mImpl->Destroy(); }

JSObject* FontFace::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return FontFace_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<FontFace> FontFace::CreateForRule(
    nsISupports* aGlobal, FontFaceSet* aFontFaceSet,
    RawServoFontFaceRule* aRule) {
  FontFaceSetImpl* setImpl = aFontFaceSet->GetImpl();
  MOZ_ASSERT(setImpl);

  RefPtr<FontFace> obj = new FontFace(aGlobal);
  obj->mImpl = FontFaceImpl::CreateForRule(obj, setImpl, aRule);
  return obj.forget();
}

already_AddRefed<FontFace> FontFace::Constructor(
    const GlobalObject& aGlobal, const nsACString& aFamily,
    const UTF8StringOrArrayBufferOrArrayBufferView& aSource,
    const FontFaceDescriptors& aDescriptors, ErrorResult& aRv) {
  nsISupports* global = aGlobal.GetAsSupports();
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global);
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  Document* doc = window->GetDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  FontFaceSetImpl* setImpl = doc->Fonts()->GetImpl();
  MOZ_ASSERT(setImpl);

  RefPtr<FontFace> obj = new FontFace(global);
  obj->mImpl = new FontFaceImpl(obj, setImpl);
  if (obj->mImpl->SetDescriptors(aFamily, aDescriptors)) {
    obj->mImpl->InitializeSource(aSource);
  }

  return obj.forget();
}

void FontFace::GetFamily(nsACString& aResult) { mImpl->GetFamily(aResult); }

void FontFace::SetFamily(const nsACString& aValue, ErrorResult& aRv) {
  mImpl->SetFamily(aValue, aRv);
}

void FontFace::GetStyle(nsACString& aResult) { mImpl->GetStyle(aResult); }

void FontFace::SetStyle(const nsACString& aValue, ErrorResult& aRv) {
  mImpl->SetStyle(aValue, aRv);
}

void FontFace::GetWeight(nsACString& aResult) { mImpl->GetWeight(aResult); }

void FontFace::SetWeight(const nsACString& aValue, ErrorResult& aRv) {
  mImpl->SetWeight(aValue, aRv);
}

void FontFace::GetStretch(nsACString& aResult) { mImpl->GetStretch(aResult); }

void FontFace::SetStretch(const nsACString& aValue, ErrorResult& aRv) {
  mImpl->SetStretch(aValue, aRv);
}

void FontFace::GetUnicodeRange(nsACString& aResult) {
  mImpl->GetUnicodeRange(aResult);
}

void FontFace::SetUnicodeRange(const nsACString& aValue, ErrorResult& aRv) {
  mImpl->SetUnicodeRange(aValue, aRv);
}

void FontFace::GetVariant(nsACString& aResult) { mImpl->GetVariant(aResult); }

void FontFace::SetVariant(const nsACString& aValue, ErrorResult& aRv) {
  mImpl->SetVariant(aValue, aRv);
}

void FontFace::GetFeatureSettings(nsACString& aResult) {
  mImpl->GetFeatureSettings(aResult);
}

void FontFace::SetFeatureSettings(const nsACString& aValue, ErrorResult& aRv) {
  mImpl->SetFeatureSettings(aValue, aRv);
}

void FontFace::GetVariationSettings(nsACString& aResult) {
  mImpl->GetVariationSettings(aResult);
}

void FontFace::SetVariationSettings(const nsACString& aValue,
                                    ErrorResult& aRv) {
  mImpl->SetVariationSettings(aValue, aRv);
}

void FontFace::GetDisplay(nsACString& aResult) { mImpl->GetDisplay(aResult); }

void FontFace::SetDisplay(const nsACString& aValue, ErrorResult& aRv) {
  mImpl->SetDisplay(aValue, aRv);
}

void FontFace::GetAscentOverride(nsACString& aResult) {
  mImpl->GetAscentOverride(aResult);
}

void FontFace::SetAscentOverride(const nsACString& aValue, ErrorResult& aRv) {
  mImpl->SetAscentOverride(aValue, aRv);
}

void FontFace::GetDescentOverride(nsACString& aResult) {
  mImpl->GetDescentOverride(aResult);
}

void FontFace::SetDescentOverride(const nsACString& aValue, ErrorResult& aRv) {
  mImpl->SetDescentOverride(aValue, aRv);
}

void FontFace::GetLineGapOverride(nsACString& aResult) {
  mImpl->GetLineGapOverride(aResult);
}

void FontFace::SetLineGapOverride(const nsACString& aValue, ErrorResult& aRv) {
  mImpl->SetLineGapOverride(aValue, aRv);
}

void FontFace::GetSizeAdjust(nsACString& aResult) {
  mImpl->GetSizeAdjust(aResult);
}

void FontFace::SetSizeAdjust(const nsACString& aValue, ErrorResult& aRv) {
  mImpl->SetSizeAdjust(aValue, aRv);
}

FontFaceLoadStatus FontFace::Status() { return mImpl->Status(); }

Promise* FontFace::Load(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  EnsurePromise();

  if (!mLoaded) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  mImpl->Load(aRv);
  return mLoaded;
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

void FontFace::MaybeResolve() {
  AssertIsMainThreadOrServoFontMetricsLocked();

  if (!mLoaded) {
    return;
  }

  if (ServoStyleSet* ss = ServoStyleSet::Current()) {
    // See comments in Gecko_GetFontMetrics.
    ss->AppendTask(PostTraversalTask::ResolveFontFaceLoadedPromise(this));
    return;
  }

  mLoaded->MaybeResolve(this);
}

void FontFace::MaybeReject(nsresult aResult) {
  AssertIsMainThreadOrServoFontMetricsLocked();

  if (ServoStyleSet* ss = ServoStyleSet::Current()) {
    // See comments in Gecko_GetFontMetrics.
    ss->AppendTask(
        PostTraversalTask::RejectFontFaceLoadedPromise(this, aResult));
    return;
  }

  if (mLoaded) {
    mLoaded->MaybeReject(aResult);
  } else if (mLoadedRejection == NS_OK) {
    mLoadedRejection = aResult;
  }
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

void FontFace::EnsurePromise() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mLoaded || !mImpl) {
    return;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);

  // If the pref is not set, don't create the Promise (which the page wouldn't
  // be able to get to anyway) as it causes the window.FontFace constructor
  // to be created.
  if (global && FontFaceSet::PrefEnabled()) {
    ErrorResult rv;
    mLoaded = Promise::Create(global, rv);

    if (mImpl->Status() == FontFaceLoadStatus::Loaded) {
      mLoaded->MaybeResolve(this);
    } else if (mLoadedRejection != NS_OK) {
      mLoaded->MaybeReject(mLoadedRejection);
    }
  }
}

}  // namespace dom
}  // namespace mozilla
