/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is nsStyleStructInlines.h.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *   Rob Arnold <robarnold@mozilla.com>
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

/*
 * Inline methods that belong in nsStyleStruct.h, except that they
 * require more headers.
 */

#ifndef nsStyleStructInlines_h_
#define nsStyleStructInlines_h_

#include "nsStyleStruct.h"
#include "imgIRequest.h"
#include "imgIContainer.h"

inline void
nsStyleBorder::SetBorderImage(imgIRequest* aImage)
{
  mBorderImage = aImage;
  mSubImages.Clear();
}

inline imgIRequest*
nsStyleBorder::GetBorderImage() const
{
  NS_ABORT_IF_FALSE(!mBorderImage || mImageTracked,
                    "Should be tracking any images we're going to use!");
  return mBorderImage;
}

inline bool nsStyleBorder::IsBorderImageLoaded() const
{
  PRUint32 status;
  return mBorderImage &&
         NS_SUCCEEDED(mBorderImage->GetImageStatus(&status)) &&
         (status & imgIRequest::STATUS_LOAD_COMPLETE) &&
         !(status & imgIRequest::STATUS_ERROR);
}

inline void
nsStyleBorder::SetSubImage(PRUint8 aIndex, imgIContainer* aSubImage) const
{
  const_cast<nsStyleBorder*>(this)->mSubImages.ReplaceObjectAt(aSubImage, aIndex);
}

inline imgIContainer*
nsStyleBorder::GetSubImage(PRUint8 aIndex) const
{
  imgIContainer* subImage = nsnull;
  if (aIndex < mSubImages.Count())
    subImage = mSubImages[aIndex];
  return subImage;
}

#endif /* !defined(nsStyleStructInlines_h_) */
