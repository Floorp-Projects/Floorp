/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_CONTENT_SVGPRESERVEASPECTRATIO_H_
#define MOZILLA_CONTENT_SVGPRESERVEASPECTRATIO_H_

#include "mozilla/dom/SVGPreserveAspectRatioBinding.h"
#include "mozilla/HashFunctions.h"  // for HashGeneric

#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/ErrorResult.h"
#include "nsSVGElement.h"

namespace mozilla {

// These constants represent the range of valid enum values for the <align>
// parameter. They exclude the sentinel _UNKNOWN value.
const uint16_t SVG_ALIGN_MIN_VALID =
  dom::SVGPreserveAspectRatio_Binding::SVG_PRESERVEASPECTRATIO_NONE;
const uint16_t SVG_ALIGN_MAX_VALID =
  dom::SVGPreserveAspectRatio_Binding::SVG_PRESERVEASPECTRATIO_XMAXYMAX;

// These constants represent the range of valid enum values for the
// <meetOrSlice> parameter. They exclude the sentinel _UNKNOWN value.
const uint16_t SVG_MEETORSLICE_MIN_VALID =
  dom::SVGPreserveAspectRatio_Binding::SVG_MEETORSLICE_MEET;
const uint16_t SVG_MEETORSLICE_MAX_VALID =
  dom::SVGPreserveAspectRatio_Binding::SVG_MEETORSLICE_SLICE;

class SVGAnimatedPreserveAspectRatio;

class SVGPreserveAspectRatio final
{
  friend class SVGAnimatedPreserveAspectRatio;
public:
  explicit SVGPreserveAspectRatio()
    : mAlign(dom::SVGPreserveAspectRatio_Binding::SVG_PRESERVEASPECTRATIO_UNKNOWN)
    , mMeetOrSlice(dom::SVGPreserveAspectRatio_Binding::SVG_MEETORSLICE_UNKNOWN)
  {}

  SVGPreserveAspectRatio(uint16_t aAlign, uint16_t aMeetOrSlice)
    : mAlign(aAlign)
    , mMeetOrSlice(aMeetOrSlice)
  {}

  static nsresult FromString(const nsAString& aString,
                             SVGPreserveAspectRatio* aValue);
  void ToString(nsAString& aValueAsString) const;

  bool operator==(const SVGPreserveAspectRatio& aOther) const;

  nsresult SetAlign(uint16_t aAlign) {
    if (aAlign < SVG_ALIGN_MIN_VALID || aAlign > SVG_ALIGN_MAX_VALID)
      return NS_ERROR_FAILURE;
    mAlign = static_cast<uint8_t>(aAlign);
    return NS_OK;
  }

  uint16_t GetAlign() const {
    return mAlign;
  }

  nsresult SetMeetOrSlice(uint16_t aMeetOrSlice) {
    if (aMeetOrSlice < SVG_MEETORSLICE_MIN_VALID ||
        aMeetOrSlice > SVG_MEETORSLICE_MAX_VALID)
      return NS_ERROR_FAILURE;
    mMeetOrSlice = static_cast<uint8_t>(aMeetOrSlice);
    return NS_OK;
  }

  uint16_t GetMeetOrSlice() const {
    return mMeetOrSlice;
  }

  PLDHashNumber Hash() const {
    return HashGeneric(mAlign, mMeetOrSlice);
  }

private:
  // We can't use enum types here because some compilers fail to pack them.
  uint8_t mAlign;
  uint8_t mMeetOrSlice;
};

namespace dom {

class DOMSVGPreserveAspectRatio final : public nsISupports,
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
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint16_t Align();
  void SetAlign(uint16_t aAlign, ErrorResult& rv);
  uint16_t MeetOrSlice();
  void SetMeetOrSlice(uint16_t aMeetOrSlice, ErrorResult& rv);

protected:
  ~DOMSVGPreserveAspectRatio();

  SVGAnimatedPreserveAspectRatio* mVal; // kept alive because it belongs to mSVGElement
  RefPtr<nsSVGElement> mSVGElement;
  const bool mIsBaseValue;
};

} //namespace dom
} //namespace mozilla

#endif // MOZILLA_CONTENT_SVGPRESERVEASPECTRATIO_H_
