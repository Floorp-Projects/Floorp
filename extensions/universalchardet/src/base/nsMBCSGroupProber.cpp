/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdio.h>

#include "nsMBCSGroupProber.h"
#include "nsUniversalDetector.h"

#if defined(DEBUG_chardet) || defined(DEBUG_jgmyers)
const char *ProberName[] = 
{
  "UTF8",
  "SJIS",
  "EUCJP",
  "GB18030",
  "EUCKR",
  "Big5",
  "EUCTW",
};

#endif

nsMBCSGroupProber::nsMBCSGroupProber(PRUint32 aLanguageFilter)
{
  for (PRUint32 i = 0; i < NUM_OF_PROBERS; i++)
    mProbers[i] = nullptr;

  mProbers[0] = new nsUTF8Prober();
  if (aLanguageFilter & NS_FILTER_JAPANESE) 
  {
    mProbers[1] = new nsSJISProber(aLanguageFilter == NS_FILTER_JAPANESE);
    mProbers[2] = new nsEUCJPProber(aLanguageFilter == NS_FILTER_JAPANESE);
  }
  if (aLanguageFilter & NS_FILTER_CHINESE_SIMPLIFIED)
    mProbers[3] = new nsGB18030Prober(aLanguageFilter == NS_FILTER_CHINESE_SIMPLIFIED);
  if (aLanguageFilter & NS_FILTER_KOREAN)
    mProbers[4] = new nsEUCKRProber(aLanguageFilter == NS_FILTER_KOREAN);
  if (aLanguageFilter & NS_FILTER_CHINESE_TRADITIONAL) 
  {
    mProbers[5] = new nsBig5Prober(aLanguageFilter == NS_FILTER_CHINESE_TRADITIONAL);
    mProbers[6] = new nsEUCTWProber(aLanguageFilter == NS_FILTER_CHINESE_TRADITIONAL);
  }
  Reset();
}

nsMBCSGroupProber::~nsMBCSGroupProber()
{
  for (PRUint32 i = 0; i < NUM_OF_PROBERS; i++)
  {
    delete mProbers[i];
  }
}

const char* nsMBCSGroupProber::GetCharSetName()
{
  if (mBestGuess == -1)
  {
    GetConfidence();
    if (mBestGuess == -1)
      mBestGuess = 0;
  }
  return mProbers[mBestGuess]->GetCharSetName();
}

void  nsMBCSGroupProber::Reset(void)
{
  mActiveNum = 0;
  for (PRUint32 i = 0; i < NUM_OF_PROBERS; i++)
  {
    if (mProbers[i])
    {
      mProbers[i]->Reset();
      mIsActive[i] = true;
      ++mActiveNum;
    }
    else
      mIsActive[i] = false;
  }
  mBestGuess = -1;
  mState = eDetecting;
  mKeepNext = 0;
}

nsProbingState nsMBCSGroupProber::HandleData(const char* aBuf, PRUint32 aLen)
{
  nsProbingState st;
  PRUint32 start = 0;
  PRUint32 keepNext = mKeepNext;

  //do filtering to reduce load to probers
  for (PRUint32 pos = 0; pos < aLen; ++pos)
  {
    if (aBuf[pos] & 0x80)
    {
      if (!keepNext)
        start = pos;
      keepNext = 2;
    }
    else if (keepNext)
    {
      if (--keepNext == 0)
      {
        for (PRUint32 i = 0; i < NUM_OF_PROBERS; i++)
        {
          if (!mIsActive[i])
            continue;
          st = mProbers[i]->HandleData(aBuf + start, pos + 1 - start);
          if (st == eFoundIt)
          {
            mBestGuess = i;
            mState = eFoundIt;
            return mState;
          }
        }
      }
    }
  }

  if (keepNext) {
    for (PRUint32 i = 0; i < NUM_OF_PROBERS; i++)
    {
      if (!mIsActive[i])
        continue;
      st = mProbers[i]->HandleData(aBuf + start, aLen - start);
      if (st == eFoundIt)
      {
        mBestGuess = i;
        mState = eFoundIt;
        return mState;
      }
    }
  }
  mKeepNext = keepNext;

  return mState;
}

float nsMBCSGroupProber::GetConfidence(void)
{
  PRUint32 i;
  float bestConf = 0.0, cf;

  switch (mState)
  {
  case eFoundIt:
    return (float)0.99;
  case eNotMe:
    return (float)0.01;
  default:
    for (i = 0; i < NUM_OF_PROBERS; i++)
    {
      if (!mIsActive[i])
        continue;
      cf = mProbers[i]->GetConfidence();
      if (bestConf < cf)
      {
        bestConf = cf;
        mBestGuess = i;
      }
    }
  }
  return bestConf;
}

#ifdef DEBUG_chardet
void nsMBCSGroupProber::DumpStatus()
{
  PRUint32 i;
  float cf;
  
  GetConfidence();
  for (i = 0; i < NUM_OF_PROBERS; i++)
  {
    if (!mIsActive[i])
      printf("  MBCS inactive: [%s] (confidence is too low).\r\n", ProberName[i]);
    else
    {
      cf = mProbers[i]->GetConfidence();
      printf("  MBCS %1.3f: [%s]\r\n", cf, ProberName[i]);
    }
  }
}
#endif

#ifdef DEBUG_jgmyers
void nsMBCSGroupProber::GetDetectorState(nsUniversalDetector::DetectorState (&states)[nsUniversalDetector::NumDetectors], PRUint32 &offset)
{
  for (PRUint32 i = 0; i < NUM_OF_PROBERS; ++i) {
    states[offset].name = ProberName[i];
    states[offset].isActive = mIsActive[i];
    states[offset].confidence = mIsActive[i] ? mProbers[i]->GetConfidence() : 0.0;
    ++offset;
  }
}
#endif /* DEBUG_jgmyers */
