/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfxMacPlatformFontList_H_
#define gfxMacPlatformFontList_H_

#include <CoreFoundation/CoreFoundation.h>

#include "CoreTextFontList.h"

class gfxMacPlatformFontList final : public CoreTextFontList {
 public:
  static gfxMacPlatformFontList* PlatformFontList() {
    return static_cast<gfxMacPlatformFontList*>(
        gfxPlatformFontList::PlatformFontList());
  }

  static void LookupSystemFont(mozilla::LookAndFeel::FontID aSystemFontID,
                               nsACString& aSystemFontName,
                               gfxFontStyle& aFontStyle);

 protected:
  bool DeprecatedFamilyIsAvailable(const nsACString& aName) override;
  FontVisibility GetVisibilityForFamily(const nsACString& aName) const override;

  FontFamily GetDefaultFontForPlatform(nsPresContext* aPresContext,
                                       const gfxFontStyle* aStyle,
                                       nsAtom* aLanguage = nullptr)
      MOZ_REQUIRES(mLock) override;

 private:
  friend class gfxPlatformMac;

  gfxMacPlatformFontList();
  virtual ~gfxMacPlatformFontList() = default;

  // Special-case font faces treated as font families (set via prefs)
  void InitSingleFaceList() MOZ_REQUIRES(mLock) override;
  void InitAliasesForSingleFaceList() MOZ_REQUIRES(mLock) override;

  // initialize system fonts
  void InitSystemFontNames() override MOZ_REQUIRES(mLock);

  nsTArray<nsCString> mSingleFaceFonts;
};

#endif /* gfxMacPlatformFontList_H_ */
