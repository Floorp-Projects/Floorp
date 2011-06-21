/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define _IMPL_NS_LAYOUT

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
  PRUint32 mTarget;
  PRUint32 mCurrent;
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
nsFontFaceList::Item(PRUint32 index, nsIDOMFontFace **_retval NS_OUTPARAM)
{
  NS_ENSURE_TRUE(index < mFontFaces.Count(), NS_ERROR_INVALID_ARG);
  FindByIndexData userData;
  userData.mTarget = index;
  userData.mCurrent = 0;
  userData.mResult = nsnull;
  mFontFaces.EnumerateRead(FindByIndex, &userData);
  NS_ASSERTION(userData.mResult != nsnull, "null entry in nsFontFaceList?");
  NS_IF_ADDREF(*_retval = userData.mResult);
  return NS_OK;
}

/* readonly attribute unsigned long length; */
NS_IMETHODIMP
nsFontFaceList::GetLength(PRUint32 *aLength)
{
  *aLength = mFontFaces.Count();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsFontFaceList

nsresult
nsFontFaceList::AddFontsFromTextRun(gfxTextRun* aTextRun,
                                    PRUint32 aOffset, PRUint32 aLength,
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
      // A new font entry we haven't seen before;
      // check whether this font entry is associated with an @font-face rule
      nsRefPtr<nsCSSFontFaceRule> rule;
      nsUserFontSet* fontSet =
        static_cast<nsUserFontSet*>(aFrame->PresContext()->GetUserFontSet());
      if (fontSet) {
        rule = fontSet->FindRuleForEntry(fe);
      }
      nsCOMPtr<nsFontFace> ff =
        new nsFontFace(fe, iter.GetGlyphRun()->mMatchType, rule);
      if (!mFontFaces.Put(fe, ff)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return NS_OK;
}
