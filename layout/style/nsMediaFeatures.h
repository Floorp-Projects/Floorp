/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* the features that media queries can test */

#ifndef nsMediaFeatures_h_
#define nsMediaFeatures_h_

class nsAtom;
class nsIDocument;
struct nsCSSKTableEntry;
class nsCSSValue;
class nsStaticAtom;

struct nsMediaFeature;
typedef void (*nsMediaFeatureValueGetter)(nsIDocument* aDocument,
                                          const nsMediaFeature* aFeature,
                                          nsCSSValue& aResult);

struct nsMediaFeature
{
  nsStaticAtom** mName; // extra indirection to point to nsGkAtoms members

  enum RangeType { eMinMaxAllowed, eMinMaxNotAllowed };
  RangeType mRangeType;

  enum ValueType {
    // All value types allow eCSSUnit_Null to indicate that no value
    // was given (in addition to the types listed below).
    eLength,     // values are eCSSUnit_Pixel
    eInteger,    // values are eCSSUnit_Integer
    eFloat,      // values are eCSSUnit_Number
    eBoolInteger,// values are eCSSUnit_Integer (0, -0, or 1 only)
    eIntRatio,   // values are eCSSUnit_Array of two eCSSUnit_Integer
    eResolution, // values are in eCSSUnit_Inch (for dpi),
                 //   eCSSUnit_Pixel (for dppx), or
                 //   eCSSUnit_Centimeter (for dpcm)
    eEnumerated, // values are eCSSUnit_Enumerated (uses keyword table)
    eIdent       // values are eCSSUnit_Ident
    // Note that a number of pieces of code (both for parsing and
    // for matching of valueless expressions) assume that all numeric
    // value types cannot be negative.  The parsing code also does
    // not allow zeros in eIntRatio types.
  };
  ValueType mValueType;

  enum RequirementFlags : uint8_t {
    // Bitfield of requirements that must be satisfied in order for this
    // media feature to be active.
    eNoRequirements = 0,
    eHasWebkitPrefix = 1 << 0, // Feature name must start w/ "-webkit-", even
                               // before any "min-"/"max-" qualifier.

    // Feature is only supported if the pref
    // "layout.css.prefixes.device-pixel-ratio-webkit" is enabled.
    // (Should only be used for -webkit-device-pixel-ratio.)
    eWebkitDevicePixelRatioPrefEnabled = 1 << 1,
    // Feature is only usable from UA sheets and chrome:// urls.
    eUserAgentAndChromeOnly = 1 << 2,
  };
  uint8_t mReqFlags;

  union {
    // In static arrays, it's the first member that's initialized.  We
    // need that to be void* so we can initialize both other types.
    // This member should never be accessed by name.
    const void* mInitializer_;
    // If mValueType == eEnumerated:  const int32_t*: keyword table in
    //   the same format as the keyword tables in nsCSSProps.
    const nsCSSKTableEntry* mKeywordTable;
    // If mGetter == GetSystemMetric (which implies mValueType ==
    //   eBoolInteger): nsAtom * const *, for the system metric.
    nsAtom * const * mMetric;
  } mData;

  // A function that returns the current value for this feature for a
  // given presentation.  If it returns eCSSUnit_Null, the feature is
  // not present.
  nsMediaFeatureValueGetter mGetter;
};

class nsMediaFeatures
{
public:
  static void InitSystemMetrics();
  static void FreeSystemMetrics();
  static void Shutdown();

  // Terminated with an entry whose mName is null.
  static const nsMediaFeature features[];
};

#endif /* !defined(nsMediaFeatures_h_) */
