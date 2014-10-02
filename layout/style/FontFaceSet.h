/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FontFaceSet_h
#define mozilla_dom_FontFaceSet_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/FontFaceSetBinding.h"
#include "nsPIDOMWindow.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {
class FontFace;
class Promise;
}
}

namespace mozilla {
namespace dom {

class FontFaceSet MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FontFaceSet, DOMEventTargetHelper)

  FontFaceSet(nsPIDOMWindow* aWindow);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // Web IDL
  IMPL_EVENT_HANDLER(loading)
  IMPL_EVENT_HANDLER(loadingdone)
  IMPL_EVENT_HANDLER(loadingerror)
  already_AddRefed<mozilla::dom::Promise> Load(const nsAString& aFont,
                                               const nsAString& aText,
                                               mozilla::ErrorResult& aRv);
  bool Check(const nsAString& aFont,
             const nsAString& aText,
             mozilla::ErrorResult& aRv);
  mozilla::dom::Promise* GetReady(mozilla::ErrorResult& aRv);
  mozilla::dom::FontFaceSetLoadStatus Status();

  FontFaceSet* Add(FontFace& aFontFace, mozilla::ErrorResult& aRv);
  void Clear();
  bool Delete(FontFace& aFontFace, mozilla::ErrorResult& aRv);
  bool Has(FontFace& aFontFace);
  FontFace* IndexedGetter(uint32_t aIndex, bool& aFound);
  uint32_t Length();

private:
  ~FontFaceSet();

  nsRefPtr<mozilla::dom::Promise> mReady;
};

} // namespace dom
} // namespace mozilla

#endif // !defined(mozilla_dom_FontFaceSet_h)
