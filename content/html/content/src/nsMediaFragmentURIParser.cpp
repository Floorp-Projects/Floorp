/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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

bool nsMediaFragmentURIParser::ParseNPT(nsDependentSubstring& aString, double& aStart, double& aEnd)
{
  nsDependentSubstring original(aString);
  if (aString.Length() > 4 &&
      aString[0] == 'n' && aString[1] == 'p' &&
      aString[2] == 't' && aString[3] == ':') {
    aString.Rebind(aString, 4);
  }

  if (aString.Length() == 0) {
    return false;
  }

  double start = -1.0;
  double end = -1.0;

  if (ParseNPTTime(aString, start)) {
    aStart = start;
  }

  if (aString.Length() == 0) {
    return true;
  }

  if (aString[0] != ',') {
    aString.Rebind(original, 0);
    return false;
  }

  aString.Rebind(aString, 1);

  if (aString.Length() == 0) {
    aString.Rebind(original, 0);
    return false;
  }

  if (ParseNPTTime(aString, end)) {
    aEnd = end;
  }

  if (aString.Length() != 0) {
    aString.Rebind(original, 0);
    return false;
  }

  return true;
}

bool nsMediaFragmentURIParser::ParseNPTTime(nsDependentSubstring& aString, double& aTime)
{
  if (aString.Length() == 0) {
    return false;
  }

  return
    ParseNPTHHMMSS(aString, aTime) ||
    ParseNPTMMSS(aString, aTime) ||
    ParseNPTSec(aString, aTime);
}

// Return true if the given character is a numeric character
static bool IsDigit(nsDependentSubstring::char_type aChar)
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
 
bool nsMediaFragmentURIParser::ParseNPTSec(nsDependentSubstring& aString, double& aSec)
{
  nsDependentSubstring original(aString);
  if (aString.Length() == 0) {
    return false;
  }

  PRUint32 index = FirstNonDigit(aString, 0);
  if (index == 0) {
    return false;
  }

  nsDependentSubstring n(aString, 0, index);
  PRInt32 ec;
  PRInt32 s = PromiseFlatString(n).ToInteger(&ec);
  if (NS_FAILED(ec)) {
    return false;
  }

  aString.Rebind(aString, index);
  double fraction = 0.0;
  if (!ParseNPTFraction(aString, fraction)) {
    aString.Rebind(original, 0);
    return false;
  }

  aSec = s + fraction;
  return true;
}

bool nsMediaFragmentURIParser::ParseNPTMMSS(nsDependentSubstring& aString, double& aTime)
{
  nsDependentSubstring original(aString);
  PRUint32 mm = 0;
  PRUint32 ss = 0;
  double fraction = 0.0;
  if (!ParseNPTMM(aString, mm)) {
    aString.Rebind(original, 0);
    return false;
  }

  if (aString.Length() < 2 || aString[0] != ':') {
    aString.Rebind(original, 0);
    return false;
  }

  aString.Rebind(aString, 1);
  if (!ParseNPTSS(aString, ss)) {
    aString.Rebind(original, 0);
    return false;
  }

  if (!ParseNPTFraction(aString, fraction)) {
    aString.Rebind(original, 0);
    return false;
  }
  aTime = mm * 60 + ss + fraction;
  return true;
}

bool nsMediaFragmentURIParser::ParseNPTFraction(nsDependentSubstring& aString, double& aFraction)
{
  double fraction = 0.0;

  if (aString.Length() > 0 && aString[0] == '.') {
    PRUint32 index = FirstNonDigit(aString, 1);

    if (index > 1) {
      nsDependentSubstring number(aString, 0, index);
      PRInt32 ec;
      fraction = PromiseFlatString(number).ToDouble(&ec);
      if (NS_FAILED(ec)) {
        return false;
      }
    }
    aString.Rebind(aString, index);
  }

  aFraction = fraction;
  return true;
}

bool nsMediaFragmentURIParser::ParseNPTHHMMSS(nsDependentSubstring& aString, double& aTime)
{
  nsDependentSubstring original(aString);
  PRUint32 hh = 0;
  double seconds = 0.0;
  if (!ParseNPTHH(aString, hh)) {
    return false;
  }

  if (aString.Length() < 2 || aString[0] != ':') {
    aString.Rebind(original, 0);
    return false;
  }

  aString.Rebind(aString, 1);
  if (!ParseNPTMMSS(aString, seconds)) {
    aString.Rebind(original, 0);
    return false;
  }

  aTime = hh * 3600 + seconds;
  return true;
}

bool nsMediaFragmentURIParser::ParseNPTHH(nsDependentSubstring& aString, PRUint32& aHour)
{
  if (aString.Length() == 0) {
    return false;
  }

  PRUint32 index = FirstNonDigit(aString, 0);
  if (index == 0) {
    return false;
  }

  nsDependentSubstring n(aString, 0, index);
  PRInt32 ec;
  PRInt32 u = PromiseFlatString(n).ToInteger(&ec);
  if (NS_FAILED(ec)) {
    return false;
  }

  aString.Rebind(aString, index);
  aHour = u;
  return true;
}

bool nsMediaFragmentURIParser::ParseNPTMM(nsDependentSubstring& aString, PRUint32& aMinute)
{
  return ParseNPTSS(aString, aMinute);
}

bool nsMediaFragmentURIParser::ParseNPTSS(nsDependentSubstring& aString, PRUint32& aSecond)
{
  if (aString.Length() < 2) {
    return false;
  }

  if (IsDigit(aString[0]) && IsDigit(aString[1])) {
    nsDependentSubstring n(aString, 0, 2);
    PRInt32 ec;

    PRInt32 u = PromiseFlatString(n).ToInteger(&ec);
    if (NS_FAILED(ec)) {
      return false;
    }

    aString.Rebind(aString, 2);
    if (u >= 60)
      return false;

    aSecond = u;
    return true;
  }

  return false;
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


