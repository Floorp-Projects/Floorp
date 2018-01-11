/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_InspectorFontFace_h
#define mozilla_InspectorFontFace_h

#include "mozilla/dom/InspectorUtilsBinding.h"
#include "mozilla/dom/NonRefcountedDOMObject.h"

class gfxFontEntry;
class gfxFontGroup;
class nsCSSFontFaceRule;

namespace mozilla {
namespace dom {

/**
 * Information on font face usage by a given DOM Range, as returned by
 * InspectorUtils.getUsedFontFaces.
 */
class InspectorFontFace final : public NonRefcountedDOMObject
{
public:
  InspectorFontFace(gfxFontEntry* aFontEntry,
                    gfxFontGroup* aFontGroup,
                    uint8_t aMatchType)
    : mFontEntry(aFontEntry)
    , mFontGroup(aFontGroup)
    , mMatchType(aMatchType)
  {
    MOZ_COUNT_CTOR(InspectorFontFace);
  }

  ~InspectorFontFace()
  {
    MOZ_COUNT_DTOR(InspectorFontFace);
  }

  gfxFontEntry* GetFontEntry() const { return mFontEntry; }
  void AddMatchType(uint8_t aMatchType) { mMatchType |= aMatchType; }

  // Web IDL
  bool FromFontGroup();
  bool FromLanguagePrefs();
  bool FromSystemFallback();
  void GetName(nsAString& aName);
  void GetCSSFamilyName(nsAString& aCSSFamilyName);
  nsCSSFontFaceRule* GetRule();
  int32_t SrcIndex();
  void GetURI(nsAString& aURI);
  void GetLocalName(nsAString& aLocalName);
  void GetFormat(nsAString& aFormat);
  void GetMetadata(nsAString& aMetadata);

  bool WrapObject(JSContext* aCx,
                  JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector)
  {
    return InspectorFontFaceBinding::Wrap(aCx, this, aGivenProto, aReflector);
  }

protected:
  RefPtr<gfxFontEntry> mFontEntry;
  RefPtr<gfxFontGroup> mFontGroup;
  uint8_t mMatchType;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_InspectorFontFace_h
