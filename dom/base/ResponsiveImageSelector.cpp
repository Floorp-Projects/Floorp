/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ResponsiveImageSelector.h"
#include "nsIURI.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"
#include "nsPresContext.h"

#include "nsCSSParser.h"
#include "nsCSSProps.h"
#include "nsIMediaList.h"
#include "nsRuleNode.h"
#include "nsRuleData.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION(ResponsiveImageSelector, mOwnerNode)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ResponsiveImageSelector, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ResponsiveImageSelector, Release)

static bool
ParseInteger(const nsAString& aString, int32_t& aInt)
{
  nsContentUtils::ParseHTMLIntegerResultFlags parseResult;
  aInt = nsContentUtils::ParseHTMLInteger(aString, &parseResult);
  return !(parseResult &
           ( nsContentUtils::eParseHTMLInteger_Error |
             nsContentUtils::eParseHTMLInteger_DidNotConsumeAllInput |
             nsContentUtils::eParseHTMLInteger_IsPercent |
             nsContentUtils::eParseHTMLInteger_NonStandard ));
}

static bool
ParseFloat(const nsAString& aString, double& aDouble)
{
  // Check if it is a valid floating-point number first since the result of
  // nsString.ToDouble() is more lenient than the spec,
  // https://html.spec.whatwg.org/#valid-floating-point-number
  nsAString::const_iterator iter, end;
  aString.BeginReading(iter);
  aString.EndReading(end);

  if (iter == end) {
    return false;
  }

  if (*iter == char16_t('-') && ++iter == end) {
    return false;
  }

  if (nsCRT::IsAsciiDigit(*iter)) {
    for (; iter != end && nsCRT::IsAsciiDigit(*iter) ; ++iter);
  } else if (*iter == char16_t('.')) {
    // Do nothing, jumps to fraction part
  } else {
    return false;
  }

  // Fraction
  if (*iter == char16_t('.')) {
    ++iter;
    if (iter == end || !nsCRT::IsAsciiDigit(*iter)) {
      // U+002E FULL STOP character (.) must be followed by one or more ASCII digits
      return false;
    }

    for (; iter != end && nsCRT::IsAsciiDigit(*iter) ; ++iter);
  }

  if (iter != end && (*iter == char16_t('e') || *iter == char16_t('E'))) {
    ++iter;
    if (*iter == char16_t('-') || *iter == char16_t('+')) {
      ++iter;
    }

    if (iter == end || !nsCRT::IsAsciiDigit(*iter)) {
      // Should have one or more ASCII digits
      return false;
    }

    for (; iter != end && nsCRT::IsAsciiDigit(*iter) ; ++iter);
  }

  if (iter != end) {
    return false;
  }

  nsresult rv;
  aDouble = PromiseFlatString(aString).ToDouble(&rv);
  return NS_SUCCEEDED(rv);
}

ResponsiveImageSelector::ResponsiveImageSelector(nsIContent *aContent)
  : mOwnerNode(aContent),
    mSelectedCandidateIndex(-1)
{
}

ResponsiveImageSelector::ResponsiveImageSelector(nsIDocument *aDocument)
  : mOwnerNode(aDocument),
    mSelectedCandidateIndex(-1)
{
}

ResponsiveImageSelector::~ResponsiveImageSelector()
{}

// http://www.whatwg.org/specs/web-apps/current-work/#processing-the-image-candidates
bool
ResponsiveImageSelector::SetCandidatesFromSourceSet(const nsAString & aSrcSet)
{
  ClearSelectedCandidate();

  nsCOMPtr<nsIURI> docBaseURI = mOwnerNode ? mOwnerNode->GetBaseURI() : nullptr;

  if (!docBaseURI) {
    MOZ_ASSERT(false,
               "Should not be parsing SourceSet without a document");
    return false;
  }

  mCandidates.Clear();

  nsAString::const_iterator iter, end;
  aSrcSet.BeginReading(iter);
  aSrcSet.EndReading(end);

  // Read URL / descriptor pairs
  while (iter != end) {
    nsAString::const_iterator url, urlEnd, descriptor;

    // Skip whitespace and commas.
    // Extra commas at this point are a non-fatal syntax error.
    for (; iter != end && (nsContentUtils::IsHTMLWhitespace(*iter) ||
                           *iter == char16_t(',')); ++iter);

    if (iter == end) {
      break;
    }

    url = iter;

    // Find end of url
    for (;iter != end && !nsContentUtils::IsHTMLWhitespace(*iter); ++iter);

    // Omit trailing commas from URL.
    // Multiple commas are a non-fatal error.
    while (iter != url) {
      if (*(--iter) != char16_t(',')) {
        iter++;
        break;
      }
    }

    const nsDependentSubstring &urlStr = Substring(url, iter);

    MOZ_ASSERT(url != iter, "Shouldn't have empty URL at this point");

    ResponsiveImageCandidate candidate;
    if (candidate.ConsumeDescriptors(iter, end)) {
      candidate.SetURLSpec(urlStr);
      AppendCandidateIfUnique(candidate);
    }
  }

  bool parsedCandidates = mCandidates.Length() > 0;

  // Re-add default to end of list
  MaybeAppendDefaultCandidate();

  return parsedCandidates;
}

uint32_t
ResponsiveImageSelector::NumCandidates(bool aIncludeDefault)
{
  uint32_t candidates = mCandidates.Length();

  // If present, the default candidate is the last item
  if (!aIncludeDefault && candidates &&
      (mCandidates[candidates - 1].Type() ==
       ResponsiveImageCandidate::eCandidateType_Default)) {
    candidates--;
  }

  return candidates;
}

nsIContent*
ResponsiveImageSelector::Content()
{
  return mOwnerNode->IsContent() ? mOwnerNode->AsContent() : nullptr;
}

nsIDocument*
ResponsiveImageSelector::Document()
{
  return mOwnerNode->OwnerDoc();
}

void
ResponsiveImageSelector::SetDefaultSource(const nsAString& aURLString)
{
  ClearSelectedCandidate();

  // Check if the last element of our candidates is a default
  int32_t candidates = mCandidates.Length();
  if (candidates && (mCandidates[candidates - 1].Type() ==
                     ResponsiveImageCandidate::eCandidateType_Default)) {
    mCandidates.RemoveElementAt(candidates - 1);
  }

  mDefaultSourceURL = aURLString;

  // Add new default to end of list
  MaybeAppendDefaultCandidate();
}

void
ResponsiveImageSelector::ClearSelectedCandidate()
{
  mSelectedCandidateIndex = -1;
  mSelectedCandidateURL = nullptr;
}

bool
ResponsiveImageSelector::SetSizesFromDescriptor(const nsAString & aSizes)
{
  ClearSelectedCandidate();
  mSizeQueries.Clear();
  mSizeValues.Clear();

  nsCSSParser cssParser;

  return cssParser.ParseSourceSizeList(aSizes, nullptr, 0,
                                       mSizeQueries, mSizeValues);
}

void
ResponsiveImageSelector::AppendCandidateIfUnique(const ResponsiveImageCandidate & aCandidate)
{
  int numCandidates = mCandidates.Length();

  // With the exception of Default, which should not be added until we are done
  // building the list.
  if (aCandidate.Type() == ResponsiveImageCandidate::eCandidateType_Default) {
    return;
  }

  // Discard candidates with identical parameters, they will never match
  for (int i = 0; i < numCandidates; i++) {
    if (mCandidates[i].HasSameParameter(aCandidate)) {
      return;
    }
  }

  mCandidates.AppendElement(aCandidate);
}

void
ResponsiveImageSelector::MaybeAppendDefaultCandidate()
{
  if (mDefaultSourceURL.IsEmpty()) {
    return;
  }

  int numCandidates = mCandidates.Length();

  // https://html.spec.whatwg.org/multipage/embedded-content.html#update-the-source-set
  // step 4.1.3:
  // If child has a src attribute whose value is not the empty string and source
  // set does not contain an image source with a density descriptor value of 1,
  // and no image source with a width descriptor, append child's src attribute
  // value to source set.
  for (int i = 0; i < numCandidates; i++) {
    if (mCandidates[i].IsComputedFromWidth()) {
      return;
    } else if (mCandidates[i].Density(this) == 1.0) {
      return;
    }
  }

  ResponsiveImageCandidate defaultCandidate;
  defaultCandidate.SetParameterDefault();
  defaultCandidate.SetURLSpec(mDefaultSourceURL);
  // We don't use MaybeAppend since we want to keep this even if it can never
  // match, as it may if the source set changes.
  mCandidates.AppendElement(defaultCandidate);
}

already_AddRefed<nsIURI>
ResponsiveImageSelector::GetSelectedImageURL()
{
  SelectImage();

  nsCOMPtr<nsIURI> url = mSelectedCandidateURL;
  return url.forget();
}

bool
ResponsiveImageSelector::GetSelectedImageURLSpec(nsAString& aResult)
{
  SelectImage();

  if (mSelectedCandidateIndex == -1) {
    return false;
  }

  aResult.Assign(mCandidates[mSelectedCandidateIndex].URLString());
  return true;
}

double
ResponsiveImageSelector::GetSelectedImageDensity()
{
  int bestIndex = GetSelectedCandidateIndex();
  if (bestIndex < 0) {
    return 1.0;
  }

  return mCandidates[bestIndex].Density(this);
}

bool
ResponsiveImageSelector::SelectImage(bool aReselect)
{
  if (!aReselect && mSelectedCandidateIndex != -1) {
    // Already have selection
    return false;
  }

  int oldBest = mSelectedCandidateIndex;
  ClearSelectedCandidate();

  int numCandidates = mCandidates.Length();
  if (!numCandidates) {
    return oldBest != -1;
  }

  nsIDocument* doc = Document();
  nsIPresShell *shell = doc ? doc->GetShell() : nullptr;
  nsPresContext *pctx = shell ? shell->GetPresContext() : nullptr;
  nsCOMPtr<nsIURI> baseURI = mOwnerNode ? mOwnerNode->GetBaseURI() : nullptr;

  if (!pctx || !doc || !baseURI) {
    return oldBest != -1;
  }

  double displayDensity = pctx->CSSPixelsToDevPixels(1.0f);

  // Per spec, "In a UA-specific manner, choose one image source"
  // - For now, select the lowest density greater than displayDensity, otherwise
  //   the greatest density available

  // If the list contains computed width candidates, compute the current
  // effective image width.
  double computedWidth = -1;
  for (int i = 0; i < numCandidates; i++) {
    if (mCandidates[i].IsComputedFromWidth()) {
      DebugOnly<bool> computeResult = \
        ComputeFinalWidthForCurrentViewport(&computedWidth);
      MOZ_ASSERT(computeResult,
                 "Computed candidates not allowed without sizes data");
      break;
    }
  }

  int bestIndex = -1;
  double bestDensity = -1.0;
  for (int i = 0; i < numCandidates; i++) {
    double candidateDensity = \
      (computedWidth == -1) ? mCandidates[i].Density(this)
                            : mCandidates[i].Density(computedWidth);
    // - If bestIndex is below display density, pick anything larger.
    // - Otherwise, prefer if less dense than bestDensity but still above
    //   displayDensity.
    if (bestIndex == -1 ||
        (bestDensity < displayDensity && candidateDensity > bestDensity) ||
        (candidateDensity >= displayDensity && candidateDensity < bestDensity)) {
      bestIndex = i;
      bestDensity = candidateDensity;
    }
  }

  MOZ_ASSERT(bestIndex >= 0 && bestIndex < numCandidates);

  // Resolve URL
  nsresult rv;
  const nsAString& urlStr = mCandidates[bestIndex].URLString();
  nsCOMPtr<nsIURI> candidateURL;
  rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(candidateURL),
                                                 urlStr, doc, baseURI);

  mSelectedCandidateURL = NS_SUCCEEDED(rv) ? candidateURL : nullptr;
  mSelectedCandidateIndex = bestIndex;

  return mSelectedCandidateIndex != oldBest;
}

int
ResponsiveImageSelector::GetSelectedCandidateIndex()
{
  SelectImage();

  return mSelectedCandidateIndex;
}

bool
ResponsiveImageSelector::ComputeFinalWidthForCurrentViewport(double *aWidth)
{
  unsigned int numSizes = mSizeQueries.Length();
  nsIDocument* doc = Document();
  nsIPresShell *presShell = doc ? doc->GetShell() : nullptr;
  nsPresContext *pctx = presShell ? presShell->GetPresContext() : nullptr;

  if (!pctx) {
    return false;
  }

  MOZ_ASSERT(numSizes == mSizeValues.Length(),
             "mSizeValues length differs from mSizeQueries");

  unsigned int i;
  for (i = 0; i < numSizes; i++) {
    if (mSizeQueries[i]->Matches(pctx, nullptr)) {
      break;
    }
  }

  nscoord effectiveWidth;
  if (i == numSizes) {
    // No match defaults to 100% viewport
    nsCSSValue defaultWidth(100.0f, eCSSUnit_ViewportWidth);
    effectiveWidth = nsRuleNode::CalcLengthWithInitialFont(pctx,
                                                           defaultWidth);
  } else {
    effectiveWidth = nsRuleNode::CalcLengthWithInitialFont(pctx,
                                                           mSizeValues[i]);
  }

  *aWidth = nsPresContext::AppUnitsToDoubleCSSPixels(std::max(effectiveWidth, 0));
  return true;
}

ResponsiveImageCandidate::ResponsiveImageCandidate()
{
  mType = eCandidateType_Invalid;
  mValue.mDensity = 1.0;
}

ResponsiveImageCandidate::ResponsiveImageCandidate(const nsAString& aURLString,
                                                   double aDensity)
  : mURLString(aURLString)
{
  mType = eCandidateType_Density;
  mValue.mDensity = aDensity;
}


void
ResponsiveImageCandidate::SetURLSpec(const nsAString& aURLString)
{
  mURLString = aURLString;
}

void
ResponsiveImageCandidate::SetParameterAsComputedWidth(int32_t aWidth)
{
  mType = eCandidateType_ComputedFromWidth;
  mValue.mWidth = aWidth;
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
ResponsiveImageCandidate::SetParameterInvalid()
{
  mType = eCandidateType_Invalid;
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

// Represents all supported descriptors for a ResponsiveImageCandidate, though
// there is no candidate type that uses all of these. This should generally
// match the mValue union of ResponsiveImageCandidate.
struct ResponsiveImageDescriptors {
  ResponsiveImageDescriptors()
    : mInvalid(false) {};

  Maybe<double> mDensity;
  Maybe<int32_t> mWidth;
  // We don't support "h" descriptors yet and they are not spec'd, but the
  // current spec does specify that they can be silently ignored (whereas
  // entirely unknown descriptors cause us to invalidate the candidate)
  Maybe<int32_t> mFutureCompatHeight;
  // If this descriptor set is bogus, e.g. a value was added twice (and thus
  // dropped) or an unknown descriptor was added.
  bool mInvalid;

  void AddDescriptor(const nsAString& aDescriptor);
  bool Valid();
  // Use the current set of descriptors to configure a candidate
  void FillCandidate(ResponsiveImageCandidate &aCandidate);
};

// Try to parse a single descriptor from a string. If value already set or
// unknown, sets invalid flag.
// This corresponds to the descriptor "Descriptor parser" step in:
// https://html.spec.whatwg.org/#parse-a-srcset-attribute
void
ResponsiveImageDescriptors::AddDescriptor(const nsAString& aDescriptor)
{
  if (aDescriptor.IsEmpty()) {
    return;
  }

  // All currently supported descriptors end with an identifying character.
  nsAString::const_iterator descStart, descType;
  aDescriptor.BeginReading(descStart);
  aDescriptor.EndReading(descType);
  descType--;
  const nsDependentSubstring& valueStr = Substring(descStart, descType);
  if (*descType == char16_t('w')) {
    int32_t possibleWidth;
    // If the value is not a valid non-negative integer, it doesn't match this
    // descriptor, fall through.
    if (ParseInteger(valueStr, possibleWidth) && possibleWidth >= 0) {
      if (possibleWidth != 0 && mWidth.isNothing() && mDensity.isNothing()) {
        mWidth.emplace(possibleWidth);
      } else {
        // Valid width descriptor, but width or density were already seen, sizes
        // support isn't enabled, or it parsed to 0, which is an error per spec
        mInvalid = true;
      }

      return;
    }
  } else if (*descType == char16_t('h')) {
    int32_t possibleHeight;
    // If the value is not a valid non-negative integer, it doesn't match this
    // descriptor, fall through.
    if (ParseInteger(valueStr, possibleHeight) && possibleHeight >= 0) {
      if (possibleHeight != 0 && mFutureCompatHeight.isNothing() &&
          mDensity.isNothing()) {
        mFutureCompatHeight.emplace(possibleHeight);
      } else {
        // Valid height descriptor, but height or density were already seen, or
        // it parsed to zero, which is an error per spec
        mInvalid = true;
      }

      return;
    }
  } else if (*descType == char16_t('x')) {
    // If the value is not a valid floating point number, it doesn't match this
    // descriptor, fall through.
    double possibleDensity = 0.0;
    if (ParseFloat(valueStr, possibleDensity)) {
      if (possibleDensity >= 0.0 &&
          mWidth.isNothing() &&
          mDensity.isNothing() &&
          mFutureCompatHeight.isNothing()) {
        mDensity.emplace(possibleDensity);
      } else {
        // Valid density descriptor, but height or width or density were already
        // seen, or it parsed to less than zero, which is an error per spec
        mInvalid = true;
      }

      return;
    }
  }

  // Matched no known descriptor, mark this descriptor set invalid
  mInvalid = true;
}

bool
ResponsiveImageDescriptors::Valid()
{
  return !mInvalid && !(mFutureCompatHeight.isSome() && mWidth.isNothing());
}

void
ResponsiveImageDescriptors::FillCandidate(ResponsiveImageCandidate &aCandidate)
{
  if (!Valid()) {
    aCandidate.SetParameterInvalid();
  } else if (mWidth.isSome()) {
    MOZ_ASSERT(mDensity.isNothing()); // Shouldn't be valid

    aCandidate.SetParameterAsComputedWidth(*mWidth);
  } else if (mDensity.isSome()) {
    MOZ_ASSERT(mWidth.isNothing()); // Shouldn't be valid

    aCandidate.SetParameterAsDensity(*mDensity);
  } else {
    // A valid set of descriptors with no density nor width (e.g. an empty set)
    // becomes 1.0 density, per spec
    aCandidate.SetParameterAsDensity(1.0);
  }
}

bool
ResponsiveImageCandidate::ConsumeDescriptors(nsAString::const_iterator& aIter,
                                             const nsAString::const_iterator& aIterEnd)
{
  nsAString::const_iterator &iter = aIter;
  const nsAString::const_iterator &end  = aIterEnd;

  bool inParens = false;

  ResponsiveImageDescriptors descriptors;

  // Parse descriptor list.
  // This corresponds to the descriptor parsing loop from:
  // https://html.spec.whatwg.org/#parse-a-srcset-attribute

  // Skip initial whitespace
  for (; iter != end && nsContentUtils::IsHTMLWhitespace(*iter); ++iter);

  nsAString::const_iterator currentDescriptor = iter;

  for (;; iter++) {
    if (iter == end) {
      descriptors.AddDescriptor(Substring(currentDescriptor, iter));
      break;
    } else if (inParens) {
      if (*iter == char16_t(')')) {
        inParens = false;
      }
    } else {
      if (*iter == char16_t(',')) {
        // End of descriptors, flush current descriptor and advance past comma
        // before breaking
        descriptors.AddDescriptor(Substring(currentDescriptor, iter));
        iter++;
        break;
      } else if (nsContentUtils::IsHTMLWhitespace(*iter)) {
        // End of current descriptor, consume it, skip spaces
        // ("After descriptor" state in spec) before continuing
        descriptors.AddDescriptor(Substring(currentDescriptor, iter));
        for (; iter != end && nsContentUtils::IsHTMLWhitespace(*iter); ++iter);
        if (iter == end) {
          break;
        }
        currentDescriptor = iter;
        // Leave one whitespace so the loop advances to this position next iteration
        iter--;
      } else if (*iter == char16_t('(')) {
        inParens = true;
      }
    }
  }

  descriptors.FillCandidate(*this);

  return Type() != eCandidateType_Invalid;
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
  } else if (mType == eCandidateType_ComputedFromWidth) {
    return aOther.mValue.mWidth == mValue.mWidth;
  }

  MOZ_ASSERT(false, "Somebody forgot to check for all uses of this enum");
  return false;
}

const nsAString&
ResponsiveImageCandidate::URLString() const
{
  return mURLString;
}

double
ResponsiveImageCandidate::Density(ResponsiveImageSelector *aSelector) const
{
  if (mType == eCandidateType_ComputedFromWidth) {
    double width;
    if (!aSelector->ComputeFinalWidthForCurrentViewport(&width)) {
      return 1.0;
    }
    return Density(width);
  }

  // Other types don't need matching width
  MOZ_ASSERT(mType == eCandidateType_Default || mType == eCandidateType_Density,
             "unhandled candidate type");
  return Density(-1);
}

double
ResponsiveImageCandidate::Density(double aMatchingWidth) const
{
  if (mType == eCandidateType_Invalid) {
    MOZ_ASSERT(false, "Getting density for uninitialized candidate");
    return 1.0;
  }

  if (mType == eCandidateType_Default) {
    return 1.0;
  }

  if (mType == eCandidateType_Density) {
    return mValue.mDensity;
  } else if (mType == eCandidateType_ComputedFromWidth) {
    if (aMatchingWidth < 0) {
      MOZ_ASSERT(false, "Don't expect to have a negative matching width at this point");
      return 1.0;
    }
    double density = double(mValue.mWidth) / aMatchingWidth;
    MOZ_ASSERT(density > 0.0);
    return density;
  }

  MOZ_ASSERT(false, "Unknown candidate type");
  return 1.0;
}

bool
ResponsiveImageCandidate::IsComputedFromWidth() const
{
  if (mType == eCandidateType_ComputedFromWidth) {
    return true;
  }

  MOZ_ASSERT(mType == eCandidateType_Default || mType == eCandidateType_Density,
             "Unknown candidate type");
  return false;
}

} // namespace dom
} // namespace mozilla
