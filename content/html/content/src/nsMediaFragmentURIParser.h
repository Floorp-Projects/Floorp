/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the media fragment URI parser.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Double <chris.double@double.co.nz>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
  // fragment data to be parsed. The method returns PR_TRUE
  // if the parse was successful and leaves the remaining unparsed
  // data in 'aString'. If the parse fails then PR_FALSE is returned
  // and 'aString' is left as it was when called.
  PRBool ParseNPT(nsDependentSubstring& aString, double& aStart, double& aEnd);
  PRBool ParseNPTTime(nsDependentSubstring& aString, double& aTime);
  PRBool ParseNPTSec(nsDependentSubstring& aString, double& aSec);
  PRBool ParseNPTFraction(nsDependentSubstring& aString, double& aFraction);
  PRBool ParseNPTMMSS(nsDependentSubstring& aString, double& aTime);
  PRBool ParseNPTHHMMSS(nsDependentSubstring& aString, double& aTime);
  PRBool ParseNPTHH(nsDependentSubstring& aString, PRUint32& aHour);
  PRBool ParseNPTMM(nsDependentSubstring& aString, PRUint32& aMinute);
  PRBool ParseNPTSS(nsDependentSubstring& aString, PRUint32& aSecond);

  // Fragment portion of the URI given on construction
  nsCAutoString mHash;

  // An array of name/value pairs containing the media fragments
  // parsed from the URI.
  nsTArray<Pair> mFragments;
};

#endif
