/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsWrapperCache.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/ErrorResult.h"

class nsSVGElement;

// Alignment Types
static const unsigned short SVG_PRESERVEASPECTRATIO_UNKNOWN = 0;
static const unsigned short SVG_PRESERVEASPECTRATIO_NONE = 1;
static const unsigned short SVG_PRESERVEASPECTRATIO_XMINYMIN = 2;
static const unsigned short SVG_PRESERVEASPECTRATIO_XMIDYMIN = 3;
static const unsigned short SVG_PRESERVEASPECTRATIO_XMAXYMIN = 4;
static const unsigned short SVG_PRESERVEASPECTRATIO_XMINYMID = 5;
static const unsigned short SVG_PRESERVEASPECTRATIO_XMIDYMID = 6;
static const unsigned short SVG_PRESERVEASPECTRATIO_XMAXYMID = 7;
static const unsigned short SVG_PRESERVEASPECTRATIO_XMINYMAX = 8;
static const unsigned short SVG_PRESERVEASPECTRATIO_XMIDYMAX = 9;
static const unsigned short SVG_PRESERVEASPECTRATIO_XMAXYMAX = 10;

// Meet-or-slice Types
static const unsigned short SVG_MEETORSLICE_UNKNOWN = 0;
static const unsigned short SVG_MEETORSLICE_MEET = 1;
static const unsigned short SVG_MEETORSLICE_SLICE = 2;

namespace mozilla {
class SVGAnimatedPreserveAspectRatio;

class SVGPreserveAspectRatio
{
  friend class SVGAnimatedPreserveAspectRatio;
public:
  SVGPreserveAspectRatio(uint16_t aAlign, uint16_t aMeetOrSlice, bool aDefer = false)
    : mAlign(static_cast<uint8_t>(aAlign))
    , mMeetOrSlice(static_cast<uint8_t>(aMeetOrSlice))
    , mDefer(aDefer)
  {}

  bool operator==(const SVGPreserveAspectRatio& aOther) const;

  explicit SVGPreserveAspectRatio()
    : mAlign(0)
    , mMeetOrSlice(0)
    , mDefer(false)
  {}

  nsresult SetAlign(uint16_t aAlign) {
    if (aAlign < SVG_PRESERVEASPECTRATIO_NONE ||
        aAlign > SVG_PRESERVEASPECTRATIO_XMAXYMAX)
      return NS_ERROR_FAILURE;
    mAlign = static_cast<uint8_t>(aAlign);
    return NS_OK;
  }

  uint16_t GetAlign() const {
    return mAlign;
  }

  nsresult SetMeetOrSlice(uint16_t aMeetOrSlice) {
    if (aMeetOrSlice < SVG_MEETORSLICE_MEET ||
        aMeetOrSlice > SVG_MEETORSLICE_SLICE)
      return NS_ERROR_FAILURE;
    mMeetOrSlice = static_cast<uint8_t>(aMeetOrSlice);
    return NS_OK;
  }

  uint16_t GetMeetOrSlice() const {
    return mMeetOrSlice;
  }

  void SetDefer(bool aDefer) {
    mDefer = aDefer;
  }

  bool GetDefer() const {
    return mDefer;
  }

private:
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
    SetIsDOMBinding();
  }
  ~DOMSVGPreserveAspectRatio();

  // WebIDL
  nsSVGElement* GetParentObject() const { return mSVGElement; }
  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope, bool* aTriedToWrap);

  uint16_t Align();
  void SetAlign(uint16_t aAlign, ErrorResult& rv);
  uint16_t MeetOrSlice();
  void SetMeetOrSlice(uint16_t aMeetOrSlice, ErrorResult& rv);

protected:
  SVGAnimatedPreserveAspectRatio* mVal; // kept alive because it belongs to mSVGElement
  nsRefPtr<nsSVGElement> mSVGElement;
  const bool mIsBaseValue;
};

} //namespace dom
} //namespace mozilla
