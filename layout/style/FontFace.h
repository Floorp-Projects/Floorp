/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FontFace_h
#define mozilla_dom_FontFace_h

#include "mozilla/dom/FontFaceBinding.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/Maybe.h"
#include "mozilla/ServoStyleConsts.h"
#include "gfxUserFontSet.h"
#include "nsCSSPropertyID.h"
#include "nsCSSValue.h"
#include "nsWrapperCache.h"

class gfxFontFaceBufferSource;
struct RawServoFontFaceRule;

namespace mozilla {
struct CSSFontFaceDescriptors;
class PostTraversalTask;
namespace dom {
class CSSFontFaceRule;
class FontFaceBufferSource;
struct FontFaceDescriptors;
class FontFaceImpl;
class FontFaceSet;
class FontFaceSetImpl;
class Promise;
class UTF8StringOrArrayBufferOrArrayBufferView;
}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

class FontFace final : public nsISupports, public nsWrapperCache {
  friend class mozilla::PostTraversalTask;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FontFace)

  nsISupports* GetParentObject() const { return mParent; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<FontFace> CreateForRule(nsISupports* aGlobal,
                                                  FontFaceSet* aFontFaceSet,
                                                  RawServoFontFaceRule* aRule);

  // Web IDL
  static already_AddRefed<FontFace> Constructor(
      const GlobalObject& aGlobal, const nsACString& aFamily,
      const UTF8StringOrArrayBufferOrArrayBufferView& aSource,
      const FontFaceDescriptors& aDescriptors, ErrorResult& aRV);

  void GetFamily(nsACString& aResult);
  void SetFamily(const nsACString& aValue, ErrorResult& aRv);
  void GetStyle(nsACString& aResult);
  void SetStyle(const nsACString& aValue, ErrorResult& aRv);
  void GetWeight(nsACString& aResult);
  void SetWeight(const nsACString& aValue, ErrorResult& aRv);
  void GetStretch(nsACString& aResult);
  void SetStretch(const nsACString& aValue, ErrorResult& aRv);
  void GetUnicodeRange(nsACString& aResult);
  void SetUnicodeRange(const nsACString& aValue, ErrorResult& aRv);
  void GetVariant(nsACString& aResult);
  void SetVariant(const nsACString& aValue, ErrorResult& aRv);
  void GetFeatureSettings(nsACString& aResult);
  void SetFeatureSettings(const nsACString& aValue, ErrorResult& aRv);
  void GetVariationSettings(nsACString& aResult);
  void SetVariationSettings(const nsACString& aValue, ErrorResult& aRv);
  void GetDisplay(nsACString& aResult);
  void SetDisplay(const nsACString& aValue, ErrorResult& aRv);
  void GetAscentOverride(nsACString& aResult);
  void SetAscentOverride(const nsACString& aValue, ErrorResult& aRv);
  void GetDescentOverride(nsACString& aResult);
  void SetDescentOverride(const nsACString& aValue, ErrorResult& aRv);
  void GetLineGapOverride(nsACString& aResult);
  void SetLineGapOverride(const nsACString& aValue, ErrorResult& aRv);
  void GetSizeAdjust(nsACString& aResult);
  void SetSizeAdjust(const nsACString& aValue, ErrorResult& aRv);

  FontFaceLoadStatus Status();
  Promise* Load(ErrorResult& aRv);
  Promise* GetLoaded(ErrorResult& aRv);

  FontFaceImpl* GetImpl() const { return mImpl; }

  void Destroy();
  void MaybeResolve();
  void MaybeReject(nsresult aResult);

  already_AddRefed<URLExtraData> GetURLExtraData() const;

 private:
  explicit FontFace(nsISupports* aParent);
  ~FontFace();

  /**
   * Returns and takes ownership of the buffer storing the font data.
   */
  void TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength);

  // Creates mLoaded if it doesn't already exist. It may immediately resolve or
  // reject mLoaded based on mStatus and mLoadedRejection.
  void EnsurePromise();

  nsCOMPtr<nsISupports> mParent;

  RefPtr<FontFaceImpl> mImpl;

  // A Promise that is fulfilled once the font represented by this FontFace is
  // loaded, and is rejected if the load fails. This promise is created lazily
  // when JS asks for it.
  RefPtr<Promise> mLoaded;

  // Saves the rejection code for mLoaded if mLoaded hasn't been created yet.
  nsresult mLoadedRejection;
};

}  // namespace mozilla::dom

#endif  // !defined(mozilla_dom_FontFace_h)
