/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FontFaceSetIterator_h
#define mozilla_dom_FontFaceSetIterator_h

#include "mozilla/dom/FontFaceSet.h"
#include "mozilla/dom/FontFaceSetBinding.h"
#include "mozilla/dom/NonRefcountedDOMObject.h"

namespace mozilla {
namespace dom {

class FontFaceSetIterator final
{
public:
  FontFaceSetIterator(mozilla::dom::FontFaceSet* aFontFaceSet,
                      bool aIsKeyAndValue);

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(FontFaceSetIterator)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(FontFaceSetIterator)

  bool WrapObject(JSContext* aCx,
                  JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector);

  // WebIDL
  void Next(JSContext* aCx, FontFaceSetIteratorResult& aResult,
            mozilla::ErrorResult& aRv);

private:
  ~FontFaceSetIterator();

  RefPtr<FontFaceSet> mFontFaceSet;
  uint32_t mNextIndex;
  bool mIsKeyAndValue;
};

} // namespace dom
} // namespace mozilla

#endif // !defined(mozilla_dom_FontFaceSetIterator_h)
