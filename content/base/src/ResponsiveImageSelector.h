/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_responsiveimageselector_h__
#define mozilla_dom_responsiveimageselector_h__

#include "nsISupports.h"
#include "nsIContent.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class ResponsiveImageCandidate;

class ResponsiveImageSelector : public nsISupports
{
public:
  NS_DECL_ISUPPORTS
  ResponsiveImageSelector(nsIContent *aContent);
  virtual ~ResponsiveImageSelector();

  // Given a srcset string, parse and replace current candidates (does not
  // replace default source)
  bool SetCandidatesFromSourceSet(const nsAString & aSrcSet);

  // Set the default source, treated as the least-precedence 1.0 density source.
  nsresult SetDefaultSource(const nsAString & aSpec);
  void SetDefaultSource(nsIURI *aURL);

  // Get the URL for the selected best candidate
  already_AddRefed<nsIURI> GetSelectedImageURL();
  double GetSelectedImageDensity();

private:
  // Append a candidate unless its selector is duplicated by a higher priority
  // candidate
  void AppendCandidateIfUnique(const ResponsiveImageCandidate &aCandidate);

  // Append a default candidate with this URL. Does not check if the array
  // already contains one, use SetDefaultSource instead.
  void AppendDefaultCandidate(nsIURI *aURL);

  // Get index of best candidate
  int GetBestCandidateIndex();

  nsCOMPtr<nsIContent> mContent;
  // If this array contains an eCandidateType_Default, it should be the last
  // element, such that the Setters can preserve/replace it respectively.
  nsTArray<ResponsiveImageCandidate> mCandidates;
  int mBestCandidateIndex;
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

  // Fill from a valid candidate descriptor. Returns false descriptor is
  // invalid.
  bool SetParamaterFromDescriptor(const nsAString & aDescriptor);

  // Check if our parameter (which does not include the url) is identical
  bool HasSameParameter(const ResponsiveImageCandidate & aOther) const;

  already_AddRefed<nsIURI> URL() const;
  double Density() const;

  enum eCandidateType {
    eCandidateType_Invalid,
    eCandidateType_Density,
    // Treated as 1.0 density, but a separate type so we can update the
    // responsive candidates and default separately
    eCandidateType_Default,
  };

  eCandidateType Type() const { return mType; }

private:

  nsCOMPtr<nsIURI> mURL;
  eCandidateType mType;
  union {
    double mDensity;
  } mValue;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_responsiveimageselector_h__
