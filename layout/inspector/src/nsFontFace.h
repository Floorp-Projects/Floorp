/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsFontFace_h__
#define __nsFontFace_h__

#include "nsIDOMFontFace.h"

#include "gfxFont.h"

class nsCSSFontFaceRule;

class nsFontFace : public nsIDOMFontFace
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMFONTFACE

  nsFontFace(gfxFontEntry*      aFontEntry,
             PRUint8            aMatchInfo,
             nsCSSFontFaceRule* aRule);
  virtual ~nsFontFace();

  gfxFontEntry* GetFontEntry() const { return mFontEntry.get(); }

  void AddMatchType(PRUint8 aMatchType) {
    mMatchType |= aMatchType;
  }

protected:
  nsRefPtr<gfxFontEntry> mFontEntry;
  nsRefPtr<nsCSSFontFaceRule> mRule;
  PRUint8 mMatchType;
};

#endif // __nsFontFace_h__
