/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(nsMediaFragmentURIParser_h__)
#define nsMediaFragmentURIParser_h__

#include "nsString.h"
#include "nsTArray.h"

// Class to handle parsing of a W3C media fragment URI as per
// spec at: http://www.w3.org/TR/media-frags/
// Only the temporaral URI portion of the spec is implemented.
// To use:
// a) Construct an instance with the URI containing the fragment
// b) Call Parse() method to parse the fragment
// c) Use GetStartTime() and GetEndTime() to get the start/end
//    times from any temporal fragment included in the URI.
class nsMediaFragmentURIParser
{
  struct Pair
  {
    Pair(const nsAString& aName, const nsAString& aValue) :
      mName(aName), mValue(aValue) { }

    nsString mName;
    nsString mValue;
  };

public:
  // Create a parser, with the URL including fragment identifier
  // in 'aSpec'.
  nsMediaFragmentURIParser(const nsCString& aSpec);

  // Parse the URI fragment included in the URI that was passed
  // on construction.
  void Parse();

  // Return the start time in seconds obtained from the URI
  // fragment. If no start time or no valid temporal fragment
  // exists then 0 is returned.
  double GetStartTime();

  // Return the end time in seconds obtained from the URI
  // fragment. If no end time or no valid temporal fragment
  // exists then -1 is returned.
  double GetEndTime();

private:
  // The following methods parse the fragment as per the media
  // fragments specification. 'aString' contains the remaining
  // fragment data to be parsed. The method returns true
  // if the parse was successful and leaves the remaining unparsed
  // data in 'aString'. If the parse fails then false is returned
  // and 'aString' is left as it was when called.
  bool ParseNPT(nsDependentSubstring& aString, double& aStart, double& aEnd);
  bool ParseNPTTime(nsDependentSubstring& aString, double& aTime);
  bool ParseNPTSec(nsDependentSubstring& aString, double& aSec);
  bool ParseNPTFraction(nsDependentSubstring& aString, double& aFraction);
  bool ParseNPTMMSS(nsDependentSubstring& aString, double& aTime);
  bool ParseNPTHHMMSS(nsDependentSubstring& aString, double& aTime);
  bool ParseNPTHH(nsDependentSubstring& aString, PRUint32& aHour);
  bool ParseNPTMM(nsDependentSubstring& aString, PRUint32& aMinute);
  bool ParseNPTSS(nsDependentSubstring& aString, PRUint32& aSecond);

  // Fragment portion of the URI given on construction
  nsCAutoString mHash;

  // An array of name/value pairs containing the media fragments
  // parsed from the URI.
  nsTArray<Pair> mFragments;
};

#endif
