/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FontFace.h"

#include <algorithm>
#include "gfxFontUtils.h"
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
#include "nsStyleUtil.h"

namespace mozilla {
namespace dom {

// -- Utility functions ------------------------------------------------------

template <typename T>
static void GetDataFrom(const T& aObject, uint8_t*& aBuffer,
                        uint32_t& aLength) {
  MOZ_ASSERT(!aBuffer);
  // We need to use malloc here because the gfxUserFontEntry will be calling
  // free on it, so the Vector's default AllocPolicy (MallocAllocPolicy) is
  // fine.
  Maybe<Vector<uint8_t>> buffer =
      aObject.template CreateFromData<Vector<uint8_t>>();
  if (buffer.isNothing()) {
    return;
  }
  aLength = buffer->length();
  aBuffer = buffer->extractOrCopyRawBuffer();
}

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

FontFace::FontFace(nsIGlobalObject* aParent)
    : mParent(aParent), mLoadedRejection(NS_OK) {}

FontFace::~FontFace() {
  // Assert that we don't drop any FontFace objects during a Servo traversal,
  // since PostTraversalTask objects can hold raw pointers to FontFaces.
  MOZ_ASSERT(!gfxFontUtils::IsInServoTraversal());
  Destroy();
}

void FontFace::Destroy() { mImpl->Destroy(); }

JSObject* FontFace::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return FontFace_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<FontFace> FontFace::CreateForRule(
    nsIGlobalObject* aGlobal, FontFaceSet* aFontFaceSet,
    StyleLockedFontFaceRule* aRule) {
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
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  FontFaceSet* set = global->GetFonts();
  if (NS_WARN_IF(!set)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  FontFaceSetImpl* setImpl = set->GetImpl();
  if (NS_WARN_IF(!setImpl)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<FontFace> obj = new FontFace(global);
  obj->mImpl = new FontFaceImpl(obj, setImpl);
  if (!obj->mImpl->SetDescriptors(aFamily, aDescriptors)) {
    return obj.forget();
  }

  if (aSource.IsUTF8String()) {
    obj->mImpl->InitializeSourceURL(aSource.GetAsUTF8String());
  } else {
    uint8_t* buffer = nullptr;
    uint32_t length = 0;
    if (aSource.IsArrayBuffer()) {
      GetDataFrom(aSource.GetAsArrayBuffer(), buffer, length);
    } else if (aSource.IsArrayBufferView()) {
      GetDataFrom(aSource.GetAsArrayBufferView(), buffer, length);
    } else {
      MOZ_ASSERT_UNREACHABLE("Unhandled source type!");
      return nullptr;
    }

    obj->mImpl->InitializeSourceBuffer(buffer, length);
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
  EnsurePromise();

  if (!mLoaded) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  mImpl->Load(aRv);
  return mLoaded;
}

Promise* FontFace::GetLoaded(ErrorResult& aRv) {
  EnsurePromise();

  if (!mLoaded) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return mLoaded;
}

void FontFace::MaybeResolve() {
  gfxFontUtils::AssertSafeThreadOrServoFontMetricsLocked();

  if (!mLoaded) {
    return;
  }

  if (ServoStyleSet* ss = gfxFontUtils::CurrentServoStyleSet()) {
    // See comments in Gecko_GetFontMetrics.
    ss->AppendTask(PostTraversalTask::ResolveFontFaceLoadedPromise(this));
    return;
  }

  mLoaded->MaybeResolve(this);
}

void FontFace::MaybeReject(nsresult aResult) {
  gfxFontUtils::AssertSafeThreadOrServoFontMetricsLocked();

  if (ServoStyleSet* ss = gfxFontUtils::CurrentServoStyleSet()) {
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

void FontFace::EnsurePromise() {
  if (mLoaded || !mImpl || !mParent) {
    return;
  }

  ErrorResult rv;
  mLoaded = Promise::Create(mParent, rv);

  if (mImpl->Status() == FontFaceLoadStatus::Loaded) {
    mLoaded->MaybeResolve(this);
  } else if (mLoadedRejection != NS_OK) {
    mLoaded->MaybeReject(mLoadedRejection);
  }
}

}  // namespace dom
}  // namespace mozilla
