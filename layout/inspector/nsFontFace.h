/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsFontFace_h__
#define __nsFontFace_h__

#include "nsIDOMFontFace.h"
#include "nsAutoPtr.h"

class gfxFontEntry;
class gfxFontGroup;
class nsCSSFontFaceRule;

class nsFontFace : public nsIDOMFontFace
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMFONTFACE

  nsFontFace(gfxFontEntry*      aFontEntry,
             gfxFontGroup*      aFontGroup,
             uint8_t            aMatchInfo);

  gfxFontEntry* GetFontEntry() const { return mFontEntry.get(); }

  void AddMatchType(uint8_t aMatchType) {
    mMatchType |= aMatchType;
  }

protected:
  virtual ~nsFontFace();

  nsRefPtr<gfxFontEntry> mFontEntry;
  nsRefPtr<gfxFontGroup> mFontGroup;
  uint8_t mMatchType;
};

#endif // __nsFontFace_h__
