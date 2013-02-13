/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFontFaceList.h"
#include "nsFontFace.h"
#include "nsFontFaceLoader.h"
#include "nsIFrame.h"
#include "gfxFont.h"

nsFontFaceList::nsFontFaceList()
{
  mFontFaces.Init();
}

nsFontFaceList::~nsFontFaceList()
{
}

////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS1(nsFontFaceList, nsIDOMFontFaceList)

////////////////////////////////////////////////////////////////////////
// nsIDOMFontFaceList

/* nsIDOMFontFace item (in unsigned long index); */
struct FindByIndexData {
  uint32_t mTarget;
  uint32_t mCurrent;
  nsIDOMFontFace* mResult;
};

static PLDHashOperator
FindByIndex(gfxFontEntry* aKey, nsIDOMFontFace* aData, void* aUserData)
{
  FindByIndexData* data = static_cast<FindByIndexData*>(aUserData);
  if (data->mCurrent == data->mTarget) {
    data->mResult = aData;
    return PL_DHASH_STOP;
  }
  data->mCurrent++;
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsFontFaceList::Item(uint32_t index, nsIDOMFontFace **_retval)
{
  NS_ENSURE_TRUE(index < mFontFaces.Count(), NS_ERROR_INVALID_ARG);
  FindByIndexData userData;
  userData.mTarget = index;
  userData.mCurrent = 0;
  userData.mResult = nullptr;
  mFontFaces.EnumerateRead(FindByIndex, &userData);
  NS_ASSERTION(userData.mResult != nullptr, "null entry in nsFontFaceList?");
  NS_IF_ADDREF(*_retval = userData.mResult);
  return NS_OK;
}

/* readonly attribute unsigned long length; */
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
                                    uint32_t aOffset, uint32_t aLength,
                                    nsIFrame* aFrame)
{
  gfxTextRun::GlyphRunIterator iter(aTextRun, aOffset, aLength);
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
      nsCOMPtr<nsFontFace> ff =
        new nsFontFace(fe, aTextRun->GetFontGroup(),
                       iter.GetGlyphRun()->mMatchType);
      mFontFaces.Put(fe, ff);
    }
  }

  return NS_OK;
}
