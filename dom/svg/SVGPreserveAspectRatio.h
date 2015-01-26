/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_CONTENT_SVGPRESERVEASPECTRATIO_H_
#define MOZILLA_CONTENT_SVGPRESERVEASPECTRATIO_H_

#include "mozilla/HashFunctions.h"  // for HashGeneric

#include "nsWrapperCache.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/ErrorResult.h"
#include "nsSVGElement.h"

namespace mozilla {
// Alignment Types
enum SVGAlign : uint8_t {
  SVG_PRESERVEASPECTRATIO_UNKNOWN = 0,
  SVG_PRESERVEASPECTRATIO_NONE = 1,
  SVG_PRESERVEASPECTRATIO_XMINYMIN = 2,
  SVG_PRESERVEASPECTRATIO_XMIDYMIN = 3,
  SVG_PRESERVEASPECTRATIO_XMAXYMIN = 4,
  SVG_PRESERVEASPECTRATIO_XMINYMID = 5,
  SVG_PRESERVEASPECTRATIO_XMIDYMID = 6,
  SVG_PRESERVEASPECTRATIO_XMAXYMID = 7,
  SVG_PRESERVEASPECTRATIO_XMINYMAX = 8,
  SVG_PRESERVEASPECTRATIO_XMIDYMAX = 9,
  SVG_PRESERVEASPECTRATIO_XMAXYMAX = 10
};

// These constants represent the range of valid enum values for the <align>
// parameter. They exclude the sentinel _UNKNOWN value.
const uint16_t SVG_ALIGN_MIN_VALID = SVG_PRESERVEASPECTRATIO_NONE;
const uint16_t SVG_ALIGN_MAX_VALID = SVG_PRESERVEASPECTRATIO_XMAXYMAX;

// Meet-or-slice Types
enum SVGMeetOrSlice : uint8_t {
  SVG_MEETORSLICE_UNKNOWN = 0,
  SVG_MEETORSLICE_MEET = 1,
  SVG_MEETORSLICE_SLICE = 2
};

// These constants represent the range of valid enum values for the
// <meetOrSlice> parameter. They exclude the sentinel _UNKNOWN value.
const uint16_t SVG_MEETORSLICE_MIN_VALID = SVG_MEETORSLICE_MEET;
const uint16_t SVG_MEETORSLICE_MAX_VALID = SVG_MEETORSLICE_SLICE;

class SVGAnimatedPreserveAspectRatio;

class SVGPreserveAspectRatio MOZ_FINAL
{
  friend class SVGAnimatedPreserveAspectRatio;
public:
  SVGPreserveAspectRatio(SVGAlign aAlign, SVGMeetOrSlice aMeetOrSlice,
                         bool aDefer = false)
    : mAlign(aAlign)
    , mMeetOrSlice(aMeetOrSlice)
    , mDefer(aDefer)
  {}

  bool operator==(const SVGPreserveAspectRatio& aOther) const;

  explicit SVGPreserveAspectRatio()
    : mAlign(SVG_PRESERVEASPECTRATIO_UNKNOWN)
    , mMeetOrSlice(SVG_MEETORSLICE_UNKNOWN)
    , mDefer(false)
  {}

  nsresult SetAlign(uint16_t aAlign) {
    if (aAlign < SVG_ALIGN_MIN_VALID || aAlign > SVG_ALIGN_MAX_VALID)
      return NS_ERROR_FAILURE;
    mAlign = static_cast<uint8_t>(aAlign);
    return NS_OK;
  }

  SVGAlign GetAlign() const {
    return static_cast<SVGAlign>(mAlign);
  }

  nsresult SetMeetOrSlice(uint16_t aMeetOrSlice) {
    if (aMeetOrSlice < SVG_MEETORSLICE_MIN_VALID ||
        aMeetOrSlice > SVG_MEETORSLICE_MAX_VALID)
      return NS_ERROR_FAILURE;
    mMeetOrSlice = static_cast<uint8_t>(aMeetOrSlice);
    return NS_OK;
  }

  SVGMeetOrSlice GetMeetOrSlice() const {
    return static_cast<SVGMeetOrSlice>(mMeetOrSlice);
  }

  void SetDefer(bool aDefer) {
    mDefer = aDefer;
  }

  bool GetDefer() const {
    return mDefer;
  }

  uint32_t Hash() const {
    return HashGeneric(mAlign, mMeetOrSlice, mDefer);
  }

private:
  // We can't use enum types here because some compilers fail to pack them.
  uint8_t mAlign;
  uint8_t mMeetOrSlice;
  bool mDefer;
};

namespace dom {

class DOMSVGPreserveAspectRatio MOZ_FINAL : public nsISupports,
                                            public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMSVGPreserveAspectRatio)

  DOMSVGPreserveAspectRatio(SVGAnimatedPreserveAspectRatio* aVal,
                            nsSVGElement *aSVGElement,
                            bool aIsBaseValue)
    : mVal(aVal), mSVGElement(aSVGElement), mIsBaseValue(aIsBaseValue)
  {
  }

  // WebIDL
  nsSVGElement* GetParentObject() const { return mSVGElement; }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  uint16_t Align();
  void SetAlign(uint16_t aAlign, ErrorResult& rv);
  uint16_t MeetOrSlice();
  void SetMeetOrSlice(uint16_t aMeetOrSlice, ErrorResult& rv);

protected:
  ~DOMSVGPreserveAspectRatio();

  SVGAnimatedPreserveAspectRatio* mVal; // kept alive because it belongs to mSVGElement
  nsRefPtr<nsSVGElement> mSVGElement;
  const bool mIsBaseValue;
};

} //namespace dom
} //namespace mozilla

#endif // MOZILLA_CONTENT_SVGPRESERVEASPECTRATIO_H_
