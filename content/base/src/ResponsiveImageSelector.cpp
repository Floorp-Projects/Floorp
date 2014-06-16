/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ResponsiveImageSelector.h"
#include "nsIURI.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"
#include "nsPresContext.h"
#include "nsNetUtil.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS0(ResponsiveImageSelector);

ResponsiveImageSelector::ResponsiveImageSelector(nsIContent *aContent)
  : mContent(aContent),
    mBestCandidateIndex(-1)
{
}

ResponsiveImageSelector::~ResponsiveImageSelector()
{}

// http://www.whatwg.org/specs/web-apps/current-work/#processing-the-image-candidates
bool
ResponsiveImageSelector::SetCandidatesFromSourceSet(const nsAString & aSrcSet)
{
  nsIDocument* doc = mContent ? mContent->OwnerDoc() : nullptr;
  nsCOMPtr<nsIURI> docBaseURI = mContent ? mContent->GetBaseURI() : nullptr;

  if (!mContent || !doc || !docBaseURI) {
    MOZ_ASSERT(false,
               "Should not be parsing SourceSet without a content and document");
    return false;
  }

  // Preserve the default source if we have one, it has a separate setter.
  uint32_t prevNumCandidates = mCandidates.Length();
  nsCOMPtr<nsIURI> defaultURL;
  if (prevNumCandidates && (mCandidates[prevNumCandidates - 1].Type() ==
                            ResponsiveImageCandidate::eCandidateType_Default)) {
    defaultURL = mCandidates[prevNumCandidates - 1].URL();
  }

  mCandidates.Clear();

  nsAString::const_iterator iter, end;
  aSrcSet.BeginReading(iter);
  aSrcSet.EndReading(end);

  // Read URL / descriptor pairs
  while (iter != end) {
    nsAString::const_iterator url, desc;

    // Skip whitespace
    for (; iter != end && nsContentUtils::IsHTMLWhitespace(*iter); ++iter);

    if (iter == end) {
      break;
    }

    url = iter;

    // Find end of url
    for (;iter != end && !nsContentUtils::IsHTMLWhitespace(*iter); ++iter);

    desc = iter;

    // Find end of descriptor
    for (; iter != end && *iter != char16_t(','); ++iter);
    const nsDependentSubstring &descriptor = Substring(desc, iter);

    nsresult rv;
    nsCOMPtr<nsIURI> candidateURL;
    rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(candidateURL),
                                                   Substring(url, desc),
                                                   doc,
                                                   docBaseURI);
    if (NS_SUCCEEDED(rv) && candidateURL) {
      NS_TryToSetImmutable(candidateURL);
      ResponsiveImageCandidate candidate;
      if (candidate.SetParamaterFromDescriptor(descriptor)) {
        candidate.SetURL(candidateURL);
        AppendCandidateIfUnique(candidate);
      }
    }

    // Advance past comma
    if (iter != end) {
      ++iter;
    }
  }

  bool parsedCandidates = mCandidates.Length() > 0;

  // Re-add default to end of list
  if (defaultURL) {
    AppendDefaultCandidate(defaultURL);
  }

  return parsedCandidates;
}

nsresult
ResponsiveImageSelector::SetDefaultSource(const nsAString & aSpec)
{
  nsIDocument* doc = mContent ? mContent->OwnerDoc() : nullptr;
  nsCOMPtr<nsIURI> docBaseURI = mContent ? mContent->GetBaseURI() : nullptr;

  if (!mContent || !doc || !docBaseURI) {
    MOZ_ASSERT(false,
               "Should not be calling this without a content and document");
    return NS_ERROR_UNEXPECTED;
  }

  if (aSpec.IsEmpty()) {
    SetDefaultSource(nullptr);
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIURI> candidateURL;
  rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(candidateURL),
                                                 aSpec, doc, docBaseURI);
  NS_ENSURE_SUCCESS(rv, rv);

  SetDefaultSource(candidateURL);
  return NS_OK;
}

void
ResponsiveImageSelector::SetDefaultSource(nsIURI *aURL)
{
  // Check if the last element of our candidates is a default
  int32_t candidates = mCandidates.Length();
  if (candidates && (mCandidates[candidates - 1].Type() ==
                     ResponsiveImageCandidate::eCandidateType_Default)) {
    mCandidates.RemoveElementAt(candidates - 1);
    if (mBestCandidateIndex == candidates - 1) {
      mBestCandidateIndex = -1;
    }
  }

  // Add new default if set
  if (aURL) {
    AppendDefaultCandidate(aURL);
  }
}

void
ResponsiveImageSelector::AppendCandidateIfUnique(const ResponsiveImageCandidate & aCandidate)
{
  // Discard candidates with identical parameters, they will never match
  int numCandidates = mCandidates.Length();
  for (int i = 0; i < numCandidates; i++) {
    if (mCandidates[i].HasSameParameter(aCandidate)) {
      return;
    }
  }

  mBestCandidateIndex = -1;
  mCandidates.AppendElement(aCandidate);
}

void
ResponsiveImageSelector::AppendDefaultCandidate(nsIURI *aURL)
{
  NS_ENSURE_TRUE(aURL, /* void */);

  ResponsiveImageCandidate defaultCandidate;
  defaultCandidate.SetParameterDefault();
  defaultCandidate.SetURL(aURL);
  // We don't use MaybeAppend since we want to keep this even if it can never
  // match, as it may if the source set changes.
  mBestCandidateIndex = -1;
  mCandidates.AppendElement(defaultCandidate);
}

already_AddRefed<nsIURI>
ResponsiveImageSelector::GetSelectedImageURL()
{
  int bestIndex = GetBestCandidateIndex();
  if (bestIndex < 0) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> bestURL = mCandidates[bestIndex].URL();
  MOZ_ASSERT(bestURL, "Shouldn't have candidates with no URL in the array");
  return bestURL.forget();
}

double
ResponsiveImageSelector::GetSelectedImageDensity()
{
  int bestIndex = GetBestCandidateIndex();
  if (bestIndex < 0) {
    return 1.0;
  }

  return mCandidates[bestIndex].Density();
}

int
ResponsiveImageSelector::GetBestCandidateIndex()
{
  if (mBestCandidateIndex != -1) {
    return mBestCandidateIndex;
  }

  int numCandidates = mCandidates.Length();
  if (!numCandidates) {
    return -1;
  }

  nsIDocument* doc = mContent ? mContent->OwnerDoc() : nullptr;
  nsIPresShell *shell = doc ? doc->GetShell() : nullptr;
  nsPresContext *pctx = shell ? shell->GetPresContext() : nullptr;

  if (!pctx) {
    MOZ_ASSERT(false, "Unable to find document prescontext");
    return -1;
  }

  double displayDensity = pctx->CSSPixelsToDevPixels(1.0f);

  // Per spec, "In a UA-specific manner, choose one image source"
  // - For now, select the lowest density greater than displayDensity, otherwise
  //   the greatest density available

  int bestIndex = 0; // First index will always be the best so far
  double bestDensity = mCandidates[bestIndex].Density();
  for (int i = 1; i < numCandidates; i++) {
    double candidateDensity = mCandidates[i].Density();
    // - If bestIndex is below display density, pick anything larger.
    // - Otherwise, prefer if less dense than bestDensity but still above
    //   displayDensity.
    if ((bestDensity < displayDensity && candidateDensity > bestDensity) ||
        (candidateDensity > displayDensity && candidateDensity < bestDensity)) {
      bestIndex = i;
      bestDensity = candidateDensity;
    }
  }

  mBestCandidateIndex = bestIndex;
  return bestIndex;
}

ResponsiveImageCandidate::ResponsiveImageCandidate()
{
  mType = eCandidateType_Invalid;
  mValue.mDensity = 1.0;
}

ResponsiveImageCandidate::ResponsiveImageCandidate(nsIURI *aURL,
                                                   double aDensity)
  : mURL(aURL)
{
  mType = eCandidateType_Density;
  mValue.mDensity = aDensity;
}


void
ResponsiveImageCandidate::SetURL(nsIURI *aURL)
{
  mURL = aURL;
}

void
ResponsiveImageCandidate::SetParameterDefault()
{
  MOZ_ASSERT(mType == eCandidateType_Invalid, "double setting candidate type");

  mType = eCandidateType_Default;
  // mValue shouldn't actually be used for this type, but set it to default
  // anyway
  mValue.mDensity = 1.0;
}

void
ResponsiveImageCandidate::SetParameterAsDensity(double aDensity)
{
  MOZ_ASSERT(mType == eCandidateType_Invalid, "double setting candidate type");

  mType = eCandidateType_Density;
  mValue.mDensity = aDensity;
}

bool
ResponsiveImageCandidate::SetParamaterFromDescriptor(const nsAString & aDescriptor)
{
  // Valid input values must be positive, using -1 for not-set
  double density = -1.0;

  nsAString::const_iterator iter, end;
  aDescriptor.BeginReading(iter);
  aDescriptor.EndReading(end);

  // Parse descriptor list
  // We currently only support a single density descriptor of the form:
  //   <floating-point number>x
  // Silently ignore other descriptors in the list for forward-compat
  while (iter != end) {
    // Skip initial whitespace
    for (; iter != end && nsContentUtils::IsHTMLWhitespace(*iter); ++iter);
    if (iter == end) {
      break;
    }

    // Find end of type
    nsAString::const_iterator start = iter;
    for (; iter != end && !nsContentUtils::IsHTMLWhitespace(*iter); ++iter);

    if (start == iter) {
      // Empty descriptor
      break;
    }

    // Iter is at end of descriptor, type is single character previous to that.
    // Safe because we just verified that iter > start
    --iter;
    nsAString::const_iterator type(iter);
    ++iter;

    const nsDependentSubstring& valStr = Substring(start, type);
    if (*type == char16_t('x')) {
      nsresult rv;
      double possibleDensity = PromiseFlatString(valStr).ToDouble(&rv);
      if (density == -1.0 && NS_SUCCEEDED(rv) && possibleDensity > 0.0) {
        density = possibleDensity;
      } else {
        return false;
      }
    }
  }

  // Not explicitly set -> 1.0
  if (density == -1.0) {
    density = 1.0;
  }

  SetParameterAsDensity(density);
  return true;
}

bool
ResponsiveImageCandidate::HasSameParameter(const ResponsiveImageCandidate & aOther) const
{
  if (aOther.mType != mType) {
    return false;
  }

  if (mType == eCandidateType_Default) {
    return true;
  }

  if (mType == eCandidateType_Density) {
    return aOther.mValue.mDensity == mValue.mDensity;
  }

  if (mType == eCandidateType_Invalid) {
    MOZ_ASSERT(false, "Comparing invalid candidates?");
    return true;
  }

  MOZ_ASSERT(false, "Somebody forgot to check for all uses of this enum");
  return false;
}

double
ResponsiveImageCandidate::Density() const
{
  // When we support 'sizes' this will get more interesting
  if (mType == eCandidateType_Invalid) {
    MOZ_ASSERT(false, "Getting density for uninitialized candidate");
    return 1.0;
  }

  if (mType == eCandidateType_Default) {
    return 1.0;
  }

  if (mType == eCandidateType_Density) {
    return mValue.mDensity;
  }

  MOZ_ASSERT(false, "Bad candidate type in Density()");
  return 1.0;
}

already_AddRefed<nsIURI>
ResponsiveImageCandidate::URL() const
{
  MOZ_ASSERT(mType != eCandidateType_Invalid,
             "Getting URL of incomplete candidate");
  nsCOMPtr<nsIURI> url = mURL;
  return url.forget();
}

} // namespace dom
} // namespace mozilla
