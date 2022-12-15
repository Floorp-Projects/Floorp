/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FontFaceSetIterator.h"

#include "js/Array.h"  // JS::NewArrayObject

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION(FontFaceSetIterator, mFontFaceSet)

FontFaceSetIterator::FontFaceSetIterator(FontFaceSet* aFontFaceSet,
                                         bool aIsKeyAndValue)
    : mFontFaceSet(aFontFaceSet),
      mNextIndex(0),
      mIsKeyAndValue(aIsKeyAndValue) {
  MOZ_COUNT_CTOR(FontFaceSetIterator);
}

FontFaceSetIterator::~FontFaceSetIterator() {
  MOZ_COUNT_DTOR(FontFaceSetIterator);
}

bool FontFaceSetIterator::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto,
                                     JS::MutableHandle<JSObject*> aReflector) {
  return FontFaceSetIterator_Binding::Wrap(aCx, this, aGivenProto, aReflector);
}

void FontFaceSetIterator::Next(JSContext* aCx,
                               FontFaceSetIteratorResult& aResult,
                               ErrorResult& aRv) {
  if (!mFontFaceSet) {
    aResult.mDone = true;
    return;
  }

  // Skip over non-Author origin fonts (GetFontFaceAt returns nullptr
  // for those).
  FontFace* face;
  while (!(face = mFontFaceSet->GetFontFaceAt(mNextIndex++))) {
    if (mNextIndex >= mFontFaceSet->SizeIncludingNonAuthorOrigins()) {
      break;  // this iterator is done
    }
  }

  if (!face) {
    aResult.mValue.setUndefined();
    aResult.mDone = true;
    mFontFaceSet = nullptr;
    return;
  }

  JS::Rooted<JS::Value> value(aCx);
  if (!ToJSValue(aCx, face, &value)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (mIsKeyAndValue) {
    JS::RootedValueArray<2> values(aCx);
    values[0].set(value);
    values[1].set(value);

    JS::Rooted<JSObject*> array(aCx);
    array = JS::NewArrayObject(aCx, values);
    if (array) {
      aResult.mValue.setObject(*array);
    }
  } else {
    aResult.mValue = value;
  }

  aResult.mDone = false;
}

}  // namespace dom
}  // namespace mozilla
