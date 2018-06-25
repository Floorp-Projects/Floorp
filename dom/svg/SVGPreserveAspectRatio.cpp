/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGPreserveAspectRatio.h"

#include "mozilla/dom/SVGPreserveAspectRatioBinding.h"
#include "nsContentUtils.h"
#include "nsWhitespaceTokenizer.h"
#include "SVGAnimatedPreserveAspectRatio.h"

using namespace mozilla;
using namespace dom;
using namespace SVGPreserveAspectRatio_Binding;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMSVGPreserveAspectRatio, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGPreserveAspectRatio)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGPreserveAspectRatio)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGPreserveAspectRatio)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

static const char *sAlignStrings[] =
  { "none", "xMinYMin", "xMidYMin", "xMaxYMin", "xMinYMid", "xMidYMid",
    "xMaxYMid", "xMinYMax", "xMidYMax", "xMaxYMax" };

static const char *sMeetOrSliceStrings[] = { "meet", "slice" };

static uint16_t
GetAlignForString(const nsAString &aAlignString)
{
  for (uint32_t i = 0 ; i < ArrayLength(sAlignStrings) ; i++) {
    if (aAlignString.EqualsASCII(sAlignStrings[i])) {
      return (i + SVG_ALIGN_MIN_VALID);
    }
  }

  return SVG_PRESERVEASPECTRATIO_UNKNOWN;
}

static uint16_t
GetMeetOrSliceForString(const nsAString &aMeetOrSlice)
{
  for (uint32_t i = 0 ; i < ArrayLength(sMeetOrSliceStrings) ; i++) {
    if (aMeetOrSlice.EqualsASCII(sMeetOrSliceStrings[i])) {
      return (i + SVG_MEETORSLICE_MIN_VALID);
    }
  }

  return SVG_MEETORSLICE_UNKNOWN;
}

/* static */ nsresult
SVGPreserveAspectRatio::FromString(const nsAString& aString,
                                   SVGPreserveAspectRatio* aValue)
{
  nsWhitespaceTokenizerTemplate<nsContentUtils::IsHTMLWhitespace>
    tokenizer(aString);
  if (tokenizer.whitespaceBeforeFirstToken() ||
      !tokenizer.hasMoreTokens()) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  const nsAString &token = tokenizer.nextToken();

  nsresult rv;
  SVGPreserveAspectRatio val;

  rv = val.SetAlign(GetAlignForString(token));

  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  if (tokenizer.hasMoreTokens()) {
    rv = val.SetMeetOrSlice(GetMeetOrSliceForString(tokenizer.nextToken()));
    if (NS_FAILED(rv)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
  } else {
    val.SetMeetOrSlice(SVG_MEETORSLICE_MEET);
  }

  if (tokenizer.whitespaceAfterCurrentToken()) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  *aValue = val;
  return NS_OK;
}

void
SVGPreserveAspectRatio::ToString(nsAString& aValueAsString) const
{
  MOZ_ASSERT(mAlign >= SVG_ALIGN_MIN_VALID && mAlign <= SVG_ALIGN_MAX_VALID,
             "Unknown align");
  aValueAsString.AssignASCII(sAlignStrings[mAlign - SVG_ALIGN_MIN_VALID]);

  if (mAlign != uint8_t(SVG_PRESERVEASPECTRATIO_NONE)) {
    MOZ_ASSERT(mMeetOrSlice >= SVG_MEETORSLICE_MIN_VALID &&
               mMeetOrSlice <= SVG_MEETORSLICE_MAX_VALID,
               "Unknown meetOrSlice");
    aValueAsString.Append(' ');
    aValueAsString.AppendASCII(sMeetOrSliceStrings[mMeetOrSlice - SVG_MEETORSLICE_MIN_VALID]);
  }
}

bool
SVGPreserveAspectRatio::operator==(const SVGPreserveAspectRatio& aOther) const
{
  return mAlign == aOther.mAlign &&
    mMeetOrSlice == aOther.mMeetOrSlice;
}

JSObject*
DOMSVGPreserveAspectRatio::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::SVGPreserveAspectRatio_Binding::Wrap(aCx, this, aGivenProto);
}

uint16_t
DOMSVGPreserveAspectRatio::Align()
{
  if (mIsBaseValue) {
    return mVal->GetBaseValue().GetAlign();
  }

  mSVGElement->FlushAnimations();
  return mVal->GetAnimValue().GetAlign();
}

void
DOMSVGPreserveAspectRatio::SetAlign(uint16_t aAlign, ErrorResult& rv)
{
  if (!mIsBaseValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  rv = mVal->SetBaseAlign(aAlign, mSVGElement);
}

uint16_t
DOMSVGPreserveAspectRatio::MeetOrSlice()
{
  if (mIsBaseValue) {
    return mVal->GetBaseValue().GetMeetOrSlice();
  }

  mSVGElement->FlushAnimations();
  return mVal->GetAnimValue().GetMeetOrSlice();
}

void
DOMSVGPreserveAspectRatio::SetMeetOrSlice(uint16_t aMeetOrSlice, ErrorResult& rv)
{
  if (!mIsBaseValue) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  rv = mVal->SetBaseMeetOrSlice(aMeetOrSlice, mSVGElement);
}

