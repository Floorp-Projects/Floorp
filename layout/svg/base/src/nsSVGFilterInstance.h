/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __NS_SVGFILTERINSTANCE_H__
#define __NS_SVGFILTERINSTANCE_H__

#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGRect.h"
#include "nsInterfaceHashtable.h"
#include "nsClassHashtable.h"
#include "nsIDOMSVGFilters.h"
#include "nsRect.h"
#include "nsIContent.h"
#include "nsAutoPtr.h"

#include "cairo.h"

class nsSVGLength2;
class nsSVGElement;

class nsSVGFilterInstance
{
public:
  class ColorModel {
  public:
    enum ColorSpace { SRGB, LINEAR_RGB };
    enum AlphaChannel { UNPREMULTIPLIED, PREMULTIPLIED };

    ColorModel(ColorSpace aColorSpace, AlphaChannel aAlphaChannel) :
      mColorSpace(aColorSpace), mAlphaChannel(aAlphaChannel) {}
    PRBool operator==(const ColorModel& aOther) const {
      return mColorSpace == aOther.mColorSpace &&
             mAlphaChannel == aOther.mAlphaChannel;
    }
    ColorSpace   mColorSpace;
    PRPackedBool mAlphaChannel;
  };

  float GetPrimitiveLength(nsSVGLength2 *aLength);

  void GetFilterSubregion(nsIContent *aFilter,
                          nsRect defaultRegion,
                          nsRect *result);

  cairo_surface_t *GetImage();
  void LookupImage(const nsAString &aName,
                   cairo_surface_t **aImage,
                   nsRect *aRegion,
                   const ColorModel &aColorModel);
  void DefineImage(const nsAString &aName,
                   cairo_surface_t *aImage,
                   const nsRect &aRegion,
                   const ColorModel &aColorModel);
  void GetFilterBox(float *x, float *y, float *width, float *height) {
    *x = mFilterX;
    *y = mFilterY;
    *width = mFilterWidth;
    *height = mFilterHeight;
  }

  nsSVGFilterInstance(nsSVGElement *aTarget,
                      nsIDOMSVGRect *aTargetBBox,
                      float aFilterX, float aFilterY,
                      float aFilterWidth, float aFilterHeight,
                      PRUint32 aFilterResX, PRUint32 aFilterResY,
                      PRUint16 aPrimitiveUnits) :
    mTarget(aTarget),
    mTargetBBox(aTargetBBox),
    mLastImage(nsnull),
    mFilterX(aFilterX), mFilterY(aFilterY),
    mFilterWidth(aFilterWidth), mFilterHeight(aFilterHeight),
    mFilterResX(aFilterResX), mFilterResY(aFilterResY),
    mPrimitiveUnits(aPrimitiveUnits) {
    mImageDictionary.Init();
  }

private:
  class ImageEntry {
  public:
    ImageEntry(cairo_surface_t *aImage,
               const nsRect &aRegion,
               const ColorModel &aColorModel) :
      mImage(aImage), mRegion(aRegion), mColorModel(aColorModel) {
      cairo_surface_reference(aImage); 
    }
    ~ImageEntry() {
      cairo_surface_destroy(mImage);
    }

    cairo_surface_t *mImage;
    nsRect mRegion;
    ColorModel mColorModel;
  };

  nsClassHashtable<nsStringHashKey,ImageEntry> mImageDictionary;
  nsRefPtr<nsSVGElement> mTarget;
  nsCOMPtr<nsIDOMSVGRect> mTargetBBox;
  ImageEntry *mLastImage;

  float mFilterX, mFilterY, mFilterWidth, mFilterHeight;
  PRUint32 mFilterResX, mFilterResY;
  PRUint16 mPrimitiveUnits;
};

#endif
