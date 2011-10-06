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
#include "nsCharSeparatedTokenizer.h"
#include "nsEscape.h"
#include "nsMediaFragmentURIParser.h"

nsMediaFragmentURIParser::nsMediaFragmentURIParser(const nsCString& aSpec)
{
  nsReadingIterator<char> start, end;
  aSpec.BeginReading(start);
  aSpec.EndReading(end);
  if (FindCharInReadable('#', start, end)) {
    mHash = Substring(++start, end);
  }
}

PRBool nsMediaFragmentURIParser::ParseNPT(nsDependentSubstring& aString, double& aStart, double& aEnd)
{
  nsDependentSubstring original(aString);
  if (aString.Length() > 4 &&
      aString[0] == 'n' && aString[1] == 'p' &&
      aString[2] == 't' && aString[3] == ':') {
    aString.Rebind(aString, 4);
  }

  if (aString.Length() == 0) {
    return PR_FALSE;
  }

  double start = -1.0;
  double end = -1.0;

  if (ParseNPTTime(aString, start)) {
    aStart = start;
  }

  if (aString.Length() == 0) {
    return PR_TRUE;
  }

  if (aString[0] != ',') {
    aString.Rebind(original, 0);
    return PR_FALSE;
  }

  aString.Rebind(aString, 1);

  if (aString.Length() == 0) {
    aString.Rebind(original, 0);
    return PR_FALSE;
  }

  if (ParseNPTTime(aString, end)) {
    aEnd = end;
  }

  if (aString.Length() != 0) {
    aString.Rebind(original, 0);
    return PR_FALSE;
  }

  return PR_TRUE;
}

PRBool nsMediaFragmentURIParser::ParseNPTTime(nsDependentSubstring& aString, double& aTime)
{
  if (aString.Length() == 0) {
    return PR_FALSE;
  }

  return
    ParseNPTHHMMSS(aString, aTime) ||
    ParseNPTMMSS(aString, aTime) ||
    ParseNPTSec(aString, aTime);
}

// Return PR_TRUE if the given character is a numeric character
static PRBool IsDigit(nsDependentSubstring::char_type aChar)
{
  return (aChar >= '0' && aChar <= '9');
}

// Return the index of the first character in the string that is not
// a numerical digit, starting from 'aStart'.
static PRUint32 FirstNonDigit(nsDependentSubstring& aString, PRUint32 aStart)
{
   while (aStart < aString.Length() && IsDigit(aString[aStart])) {
    ++aStart;
  }
  return aStart;
}
 
PRBool nsMediaFragmentURIParser::ParseNPTSec(nsDependentSubstring& aString, double& aSec)
{
  nsDependentSubstring original(aString);
  if (aString.Length() == 0) {
    return PR_FALSE;
  }

  PRUint32 index = FirstNonDigit(aString, 0);
  if (index == 0) {
    return PR_FALSE;
  }

  nsDependentSubstring n(aString, 0, index);
  PRInt32 ec;
  PRInt32 s = PromiseFlatString(n).ToInteger(&ec);
  if (NS_FAILED(ec)) {
    return PR_FALSE;
  }

  aString.Rebind(aString, index);
  double fraction = 0.0;
  if (!ParseNPTFraction(aString, fraction)) {
    aString.Rebind(original, 0);
    return PR_FALSE;
  }

  aSec = s + fraction;
  return PR_TRUE;
}

PRBool nsMediaFragmentURIParser::ParseNPTMMSS(nsDependentSubstring& aString, double& aTime)
{
  nsDependentSubstring original(aString);
  PRUint32 mm = 0;
  PRUint32 ss = 0;
  double fraction = 0.0;
  if (!ParseNPTMM(aString, mm)) {
    aString.Rebind(original, 0);
    return PR_FALSE;
  }

  if (aString.Length() < 2 || aString[0] != ':') {
    aString.Rebind(original, 0);
    return PR_FALSE;
  }

  aString.Rebind(aString, 1);
  if (!ParseNPTSS(aString, ss)) {
    aString.Rebind(original, 0);
    return PR_FALSE;
  }

  if (!ParseNPTFraction(aString, fraction)) {
    aString.Rebind(original, 0);
    return PR_FALSE;
  }
  aTime = mm * 60 + ss + fraction;
  return PR_TRUE;
}

PRBool nsMediaFragmentURIParser::ParseNPTFraction(nsDependentSubstring& aString, double& aFraction)
{
  double fraction = 0.0;

  if (aString.Length() > 0 && aString[0] == '.') {
    PRUint32 index = FirstNonDigit(aString, 1);

    if (index > 1) {
      nsDependentSubstring number(aString, 0, index);
      PRInt32 ec;
      fraction = PromiseFlatString(number).ToDouble(&ec);
      if (NS_FAILED(ec)) {
        return PR_FALSE;
      }
    }
    aString.Rebind(aString, index);
  }

  aFraction = fraction;
  return PR_TRUE;
}

PRBool nsMediaFragmentURIParser::ParseNPTHHMMSS(nsDependentSubstring& aString, double& aTime)
{
  nsDependentSubstring original(aString);
  PRUint32 hh = 0;
  double seconds = 0.0;
  if (!ParseNPTHH(aString, hh)) {
    return PR_FALSE;
  }

  if (aString.Length() < 2 || aString[0] != ':') {
    aString.Rebind(original, 0);
    return PR_FALSE;
  }

  aString.Rebind(aString, 1);
  if (!ParseNPTMMSS(aString, seconds)) {
    aString.Rebind(original, 0);
    return PR_FALSE;
  }

  aTime = hh * 3600 + seconds;
  return PR_TRUE;
}

PRBool nsMediaFragmentURIParser::ParseNPTHH(nsDependentSubstring& aString, PRUint32& aHour)
{
  if (aString.Length() == 0) {
    return PR_FALSE;
  }

  PRUint32 index = FirstNonDigit(aString, 0);
  if (index == 0) {
    return PR_FALSE;
  }

  nsDependentSubstring n(aString, 0, index);
  PRInt32 ec;
  PRInt32 u = PromiseFlatString(n).ToInteger(&ec);
  if (NS_FAILED(ec)) {
    return PR_FALSE;
  }

  aString.Rebind(aString, index);
  aHour = u;
  return PR_TRUE;
}

PRBool nsMediaFragmentURIParser::ParseNPTMM(nsDependentSubstring& aString, PRUint32& aMinute)
{
  return ParseNPTSS(aString, aMinute);
}

PRBool nsMediaFragmentURIParser::ParseNPTSS(nsDependentSubstring& aString, PRUint32& aSecond)
{
  if (aString.Length() < 2) {
    return PR_FALSE;
  }

  if (IsDigit(aString[0]) && IsDigit(aString[1])) {
    nsDependentSubstring n(aString, 0, 2);
    PRInt32 ec;

    PRInt32 u = PromiseFlatString(n).ToInteger(&ec);
    if (NS_FAILED(ec)) {
      return PR_FALSE;
    }

    aString.Rebind(aString, 2);
    if (u >= 60)
      return PR_FALSE;

    aSecond = u;
    return PR_TRUE;
  }

  return PR_FALSE;
}

void nsMediaFragmentURIParser::Parse()
{
  nsCCharSeparatedTokenizer tokenizer(mHash, '&');
  while (tokenizer.hasMoreTokens()) {
    const nsCSubstring& nv = tokenizer.nextToken();
    PRInt32 index = nv.FindChar('=');
    if (index >= 0) {
      nsCAutoString name;
      nsCAutoString value;
      NS_UnescapeURL(StringHead(nv, index), esc_Ref | esc_AlwaysCopy, name);
      NS_UnescapeURL(Substring(nv, index + 1, nv.Length()),
                     esc_Ref | esc_AlwaysCopy, value);
      nsAutoString a = NS_ConvertUTF8toUTF16(name);
      nsAutoString b = NS_ConvertUTF8toUTF16(value);
      mFragments.AppendElement(Pair(a, b));
    }
  }
}

double nsMediaFragmentURIParser::GetStartTime()
{
  for (PRUint32 i = 0; i < mFragments.Length(); ++i) {
    PRUint32 index = mFragments.Length() - i - 1;
    if (mFragments[index].mName.EqualsLiteral("t")) {
      double start = -1;
      double end = -1;
      nsDependentSubstring s(mFragments[index].mValue, 0);
      if (ParseNPT(s, start, end)) {
        return start;
      }
    }
  }
  return 0.0;
}

double nsMediaFragmentURIParser::GetEndTime()
{
  for (PRUint32 i = 0; i < mFragments.Length(); ++i) {
    PRUint32 index = mFragments.Length() - i - 1;
    if (mFragments[index].mName.EqualsLiteral("t")) {
      double start = -1;
      double end = -1;
      nsDependentSubstring s(mFragments[index].mValue, 0);
      if (ParseNPT(s, start, end)) {
        return end;
      }
    }
  }
  return -1;
}


