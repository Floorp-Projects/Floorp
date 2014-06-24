/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsFontFaceList_h__
#define __nsFontFaceList_h__

#include "nsIDOMFontFaceList.h"
#include "nsIDOMFontFace.h"
#include "nsCOMPtr.h"
#include "nsInterfaceHashtable.h"
#include "nsHashKeys.h"

class gfxFontEntry;
class gfxTextRun;

class nsFontFaceList : public nsIDOMFontFaceList
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMFONTFACELIST

  nsFontFaceList();

  nsresult AddFontsFromTextRun(gfxTextRun* aTextRun,
                               uint32_t aOffset, uint32_t aLength);

protected:
  virtual ~nsFontFaceList();

  nsInterfaceHashtable<nsPtrHashKey<gfxFontEntry>,nsIDOMFontFace> mFontFaces;
};

#endif // __nsFontFaceList_h__
