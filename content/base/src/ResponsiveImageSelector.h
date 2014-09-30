/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_responsiveimageselector_h__
#define mozilla_dom_responsiveimageselector_h__

#include "nsISupports.h"
#include "nsIContent.h"
#include "nsString.h"

class nsMediaQuery;
class nsCSSValue;

namespace mozilla {
namespace dom {

class ResponsiveImageCandidate;

class ResponsiveImageSelector : public nsISupports
{
  friend class ResponsiveImageCandidate;
public:
  NS_DECL_ISUPPORTS
  explicit ResponsiveImageSelector(nsIContent* aContent);

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
  bool SetCandidatesFromSourceSet(const nsAString & aSrcSet);

  // Fill the source sizes from a valid sizes descriptor. Returns false if
  // descriptor is invalid.
  bool SetSizesFromDescriptor(const nsAString & aSizesDescriptor);

  // Set the default source, treated as the least-precedence 1.0 density source.
  nsresult SetDefaultSource(const nsAString & aSpec);
  void SetDefaultSource(nsIURI *aURL);

  uint32_t NumCandidates(bool aIncludeDefault = true);

  nsIContent *Content() { return mContent; }

  // Get the url and density for the selected best candidate. These
  // implicitly cause an image to be selected if necessary.
  already_AddRefed<nsIURI> GetSelectedImageURL();
  double GetSelectedImageDensity();

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
  void AppendCandidateIfUnique(const ResponsiveImageCandidate &aCandidate);

  // Append a default candidate with this URL. Does not check if the array
  // already contains one, use SetDefaultSource instead.
  void AppendDefaultCandidate(nsIURI *aURL);

  // Get index of best candidate
  int GetBestCandidateIndex();

  // Compute a density from a Candidate width. Returns false if sizes were not
  // specified for this selector.
  //
  // aContext is the presContext to use for current viewport sizing, null will
  // use the associated content's context.
  bool ComputeFinalWidthForCurrentViewport(int32_t *aWidth);

  nsCOMPtr<nsIContent> mContent;
  // If this array contains an eCandidateType_Default, it should be the last
  // element, such that the Setters can preserve/replace it respectively.
  nsTArray<ResponsiveImageCandidate> mCandidates;
  int mBestCandidateIndex;

  nsTArray< nsAutoPtr<nsMediaQuery> > mSizeQueries;
  nsTArray<nsCSSValue> mSizeValues;
};

class ResponsiveImageCandidate {
public:
  ResponsiveImageCandidate();
  ResponsiveImageCandidate(nsIURI *aURL, double aDensity);

  void SetURL(nsIURI *aURL);
  // Set this as a default-candidate. This behaves the same as density 1.0, but
  // has a differing type such that it can be replaced by subsequent
  // SetDefaultSource calls.
  void SetParameterDefault();

  // Set this candidate as a by-density candidate with specified density.
  void SetParameterAsDensity(double aDensity);
  void SetParameterAsComputedWidth(int32_t aWidth);

  // Fill from a valid candidate descriptor. Returns false descriptor is
  // invalid.
  bool SetParamaterFromDescriptor(const nsAString & aDescriptor);

  // Check if our parameter (which does not include the url) is identical
  bool HasSameParameter(const ResponsiveImageCandidate & aOther) const;

  already_AddRefed<nsIURI> URL() const;

  // Compute and return the density relative to a selector.
  double Density(ResponsiveImageSelector *aSelector) const;
  // If the width is already known. Useful when iterating over candidates to
  // avoid having each call re-compute the width.
  double Density(int32_t aMatchingWidth) const;

  // If this selector is computed from the selector's matching width.
  bool IsComputedFromWidth() const;

  enum eCandidateType {
    eCandidateType_Invalid,
    eCandidateType_Density,
    // Treated as 1.0 density, but a separate type so we can update the
    // responsive candidates and default separately
    eCandidateType_Default,
    eCandidateType_ComputedFromWidth
  };

  eCandidateType Type() const { return mType; }

private:

  nsCOMPtr<nsIURI> mURL;
  eCandidateType mType;
  union {
    double mDensity;
    int32_t mWidth;
  } mValue;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_responsiveimageselector_h__
