/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFontFaceList.h"
#include "nsFontFace.h"
#include "nsFontFaceLoader.h"
#include "nsIFrame.h"
#include "gfxTextRun.h"
#include "mozilla/gfx/2D.h"

nsFontFaceList::nsFontFaceList()
{
}

nsFontFaceList::~nsFontFaceList()
{
}

////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS(nsFontFaceList, nsIDOMFontFaceList)

////////////////////////////////////////////////////////////////////////
// nsIDOMFontFaceList

NS_IMETHODIMP
nsFontFaceList::Item(uint32_t index, nsIDOMFontFace **_retval)
{
  NS_ENSURE_TRUE(index < mFontFaces.Count(), NS_ERROR_INVALID_ARG);

  uint32_t current = 0;
  nsIDOMFontFace* result = nullptr;
  for (auto iter = mFontFaces.Iter(); !iter.Done(); iter.Next()) {
    if (current == index) {
      result = iter.UserData();
      break;
    }
    current++;
  }
  NS_ASSERTION(result != nullptr, "null entry in nsFontFaceList?");
  NS_IF_ADDREF(*_retval = result);
  return NS_OK;
}

NS_IMETHODIMP
nsFontFaceList::GetLength(uint32_t *aLength)
{
  *aLength = mFontFaces.Count();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsFontFaceList

nsresult
nsFontFaceList::AddFontsFromTextRun(gfxTextRun* aTextRun,
                                    uint32_t aOffset, uint32_t aLength)
{
  gfxTextRun::Range range(aOffset, aOffset + aLength);
  gfxTextRun::GlyphRunIterator iter(aTextRun, range);
  while (iter.NextRun()) {
    gfxFontEntry *fe = iter.GetGlyphRun()->mFont->GetFontEntry();
    // if we have already listed this face, just make sure the match type is
    // recorded
    nsFontFace* existingFace =
      static_cast<nsFontFace*>(mFontFaces.GetWeak(fe));
    if (existingFace) {
      existingFace->AddMatchType(iter.GetGlyphRun()->mMatchType);
    } else {
      // A new font entry we haven't seen before
      RefPtr<nsFontFace> ff =
        new nsFontFace(fe, aTextRun->GetFontGroup(),
                       iter.GetGlyphRun()->mMatchType);
      mFontFaces.Put(fe, ff);
    }
  }

  return NS_OK;
}
