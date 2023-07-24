/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_responsiveimageselector_h__
#define mozilla_dom_responsiveimageselector_h__

#include "mozilla/UniquePtr.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/FunctionRef.h"
#include "nsISupports.h"
#include "nsIContent.h"
#include "nsString.h"
#include "nsCycleCollectionParticipant.h"

class nsMediaQuery;
class nsCSSValue;

namespace mozilla::dom {

class ResponsiveImageCandidate;

class ResponsiveImageSelector {
  friend class ResponsiveImageCandidate;

 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ResponsiveImageSelector)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ResponsiveImageSelector)

  explicit ResponsiveImageSelector(nsIContent* aContent);
  explicit ResponsiveImageSelector(dom::Document* aDocument);

  // Parses the raw candidates and calls into the callback for each one of them.
  static void ParseSourceSet(const nsAString& aSrcSet,
                             FunctionRef<void(ResponsiveImageCandidate&&)>);

  // NOTE ABOUT CURRENT SELECTION
  //
  // The best candidate is selected lazily when GetSelectedImage*() is
  // called, or when SelectImage() is called explicitly. This result
  // is then cached until either invalidated by further Set*() calls,
  // or explicitly by replaced by SelectImage(aReselect = true).
  //
  // Because the selected image depends on external variants like
  // viewport size and device pixel ratio, the time at which image
  // selection occurs can affect the result.

  // Given a srcset string, parse and replace current candidates (does not
  // replace default source)
  bool SetCandidatesFromSourceSet(const nsAString& aSrcSet,
                                  nsIPrincipal* aTriggeringPrincipal = nullptr);

  // Fill the source sizes from a valid sizes descriptor. Returns false if
  // descriptor is invalid.
  bool SetSizesFromDescriptor(const nsAString& aSizesDescriptor);

  // Set the default source, treated as the least-precedence 1.0 density source.
  void SetDefaultSource(const nsAString& aURLString, nsIPrincipal* = nullptr);
  void SetDefaultSource(nsIURI* aURI, nsIPrincipal* = nullptr);
  void ClearDefaultSource();

  uint32_t NumCandidates(bool aIncludeDefault = true);

  // If this was created for a specific content. May be null if we were only
  // created for a document.
  nsIContent* Content();

  // The document we were created for, or the owner document of the content if
  // we were created for a specific nsIContent.
  dom::Document* Document();

  // Get the url and density for the selected best candidate. These
  // implicitly cause an image to be selected if necessary.
  already_AddRefed<nsIURI> GetSelectedImageURL();
  // Returns false if there is no selected image
  bool GetSelectedImageURLSpec(nsAString& aResult);
  double GetSelectedImageDensity();
  nsIPrincipal* GetSelectedImageTriggeringPrincipal();

  // Runs image selection now if necessary. If an image has already
  // been choosen, takes no action unless aReselect is true.
  //
  // aReselect - Always re-run selection, replacing the previously
  //             choosen image.
  // return - true if the selected image result changed.
  bool SelectImage(bool aReselect = false);

 protected:
  virtual ~ResponsiveImageSelector();

 private:
  // Append a candidate unless its selector is duplicated by a higher priority
  // candidate
  void AppendCandidateIfUnique(ResponsiveImageCandidate&& aCandidate);

  // Append a default candidate with this URL if necessary. Does not check if
  // the array already contains one, use SetDefaultSource instead.
  void MaybeAppendDefaultCandidate();

  // Get index of selected candidate, triggering selection if necessary.
  int GetSelectedCandidateIndex();

  // Forget currently selected candidate. (See "NOTE ABOUT CURRENT SELECTION"
  // above.)
  void ClearSelectedCandidate();

  // Compute a density from a Candidate width. Returns false if sizes were not
  // specified for this selector.
  //
  // aContext is the presContext to use for current viewport sizing, null will
  // use the associated content's context.
  bool ComputeFinalWidthForCurrentViewport(double* aWidth);

  nsCOMPtr<nsINode> mOwnerNode;
  // The cached URL for default candidate.
  nsString mDefaultSourceURL;
  nsCOMPtr<nsIPrincipal> mDefaultSourceTriggeringPrincipal;
  // If this array contains an eCandidateType_Default, it should be the last
  // element, such that the Setters can preserve/replace it respectively.
  nsTArray<ResponsiveImageCandidate> mCandidates;
  int mSelectedCandidateIndex;
  // The cached resolved URL for mSelectedCandidateIndex, such that we only
  // resolve the absolute URL at selection time
  nsCOMPtr<nsIURI> mSelectedCandidateURL;

  // Servo bits.
  UniquePtr<StyleSourceSizeList> mServoSourceSizeList;
};

class ResponsiveImageCandidate {
 public:
  ResponsiveImageCandidate();
  ResponsiveImageCandidate(const ResponsiveImageCandidate&) = delete;
  ResponsiveImageCandidate(ResponsiveImageCandidate&&) = default;

  void SetURLSpec(const nsAString& aURLString);
  void SetTriggeringPrincipal(nsIPrincipal* aPrincipal);
  // Set this as a default-candidate. This behaves the same as density 1.0, but
  // has a differing type such that it can be replaced by subsequent
  // SetDefaultSource calls.
  void SetParameterDefault();

  // Set this candidate as a by-density candidate with specified density.
  void SetParameterAsDensity(double aDensity);
  void SetParameterAsComputedWidth(int32_t aWidth);

  void SetParameterInvalid();

  // Consume descriptors from a string defined by aIter and aIterEnd, adjusts
  // aIter to the end of data consumed.
  // Returns false if descriptors string is invalid, but still parses to the end
  // of descriptors microsyntax.
  bool ConsumeDescriptors(nsAString::const_iterator& aIter,
                          const nsAString::const_iterator& aIterEnd);

  // Check if our parameter (which does not include the url) is identical
  bool HasSameParameter(const ResponsiveImageCandidate& aOther) const;

  const nsAString& URLString() const { return mURLString; }
  nsIPrincipal* TriggeringPrincipal() const { return mTriggeringPrincipal; }

  // Compute and return the density relative to a selector.
  double Density(ResponsiveImageSelector* aSelector) const;
  // If the width is already known. Useful when iterating over candidates to
  // avoid having each call re-compute the width.
  double Density(double aMatchingWidth) const;

  // Append the descriptors for this candidate serialized as a string.
  void AppendDescriptors(nsAString&) const;

  bool IsValid() const { return mType != CandidateType::Invalid; }

  // If this selector is computed from the selector's matching width.
  bool IsComputedFromWidth() const {
    return mType == CandidateType::ComputedFromWidth;
  }

  bool IsDefault() const { return mType == CandidateType::Default; }

  enum class CandidateType : uint8_t {
    Invalid,
    Density,
    // Treated as 1.0 density, but a separate type so we can update the
    // responsive candidates and default separately
    Default,
    ComputedFromWidth
  };

  CandidateType Type() const { return mType; }

 private:
  nsString mURLString;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  CandidateType mType;
  union {
    double mDensity;
    int32_t mWidth;
  } mValue;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_responsiveimageselector_h__
