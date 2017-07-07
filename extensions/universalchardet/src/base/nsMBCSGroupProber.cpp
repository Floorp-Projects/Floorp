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
};

#endif

nsMBCSGroupProber::nsMBCSGroupProber()
{
  mProbers[0] = new nsUTF8Prober();
  mProbers[1] = new nsSJISProber();
  mProbers[2] = new nsEUCJPProber();
  Reset();
}

nsMBCSGroupProber::~nsMBCSGroupProber()
{
  for (uint32_t i = 0; i < NUM_OF_PROBERS; i++)
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
  for (uint32_t i = 0; i < NUM_OF_PROBERS; i++)
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

nsProbingState nsMBCSGroupProber::HandleData(const char* aBuf, uint32_t aLen)
{
  nsProbingState st;
  uint32_t start = 0;
  uint32_t keepNext = mKeepNext;

  //do filtering to reduce load to probers
  for (uint32_t pos = 0; pos < aLen; ++pos)
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
        for (uint32_t i = 0; i < NUM_OF_PROBERS; i++)
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
    for (uint32_t i = 0; i < NUM_OF_PROBERS; i++)
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
  uint32_t i;
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
  uint32_t i;
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
void nsMBCSGroupProber::GetDetectorState(nsUniversalDetector::DetectorState (&states)[nsUniversalDetector::NumDetectors], uint32_t &offset)
{
  for (uint32_t i = 0; i < NUM_OF_PROBERS; ++i) {
    states[offset].name = ProberName[i];
    states[offset].isActive = mIsActive[i];
    states[offset].confidence = mIsActive[i] ? mProbers[i]->GetConfidence() : 0.0;
    ++offset;
  }
}
#endif /* DEBUG_jgmyers */
