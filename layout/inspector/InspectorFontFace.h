/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_InspectorFontFace_h
#define mozilla_InspectorFontFace_h

#include "gfxTypes.h"
#include "mozilla/dom/CSSFontFaceRule.h"
#include "mozilla/dom/InspectorUtilsBinding.h"
#include "mozilla/dom/NonRefcountedDOMObject.h"
#include "nsRange.h"

class gfxFontEntry;
class gfxFontGroup;

namespace mozilla {
namespace dom {

/**
 * Information on font face usage by a given DOM Range, as returned by
 * InspectorUtils.getUsedFontFaces.
 */
class InspectorFontFace final : public NonRefcountedDOMObject {
 public:
  InspectorFontFace(gfxFontEntry* aFontEntry, gfxFontGroup* aFontGroup,
                    FontMatchType aMatchType);

  ~InspectorFontFace();

  gfxFontEntry* GetFontEntry() const { return mFontEntry; }
  void AddMatchType(FontMatchType aMatchType) { mMatchType |= aMatchType; }

  void AddRange(nsRange* aRange);
  size_t RangeCount() const { return mRanges.Length(); }

  // Web IDL
  bool FromFontGroup();
  bool FromLanguagePrefs();
  bool FromSystemFallback();
  void GetName(nsAString& aName);
  void GetCSSFamilyName(nsAString& aCSSFamilyName);
  void GetCSSGeneric(nsAString& aGeneric);
  CSSFontFaceRule* GetRule();
  int32_t SrcIndex();
  void GetURI(nsAString& aURI);
  void GetLocalName(nsAString& aLocalName);
  void GetFormat(nsAString& aFormat);
  void GetMetadata(nsAString& aMetadata);

  void GetVariationAxes(nsTArray<InspectorVariationAxis>& aResult,
                        ErrorResult& aRV);
  void GetVariationInstances(nsTArray<InspectorVariationInstance>& aResult,
                             ErrorResult& aRV);
  void GetFeatures(nsTArray<InspectorFontFeature>& aResult, ErrorResult& aRV);

  void GetRanges(nsTArray<RefPtr<nsRange>>& aResult);

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector) {
    return InspectorFontFace_Binding::Wrap(aCx, this, aGivenProto, aReflector);
  }

 protected:
  RefPtr<gfxFontEntry> mFontEntry;
  RefPtr<gfxFontGroup> mFontGroup;
  RefPtr<CSSFontFaceRule> mRule;
  FontMatchType mMatchType;

  nsTArray<RefPtr<nsRange>> mRanges;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_InspectorFontFace_h
