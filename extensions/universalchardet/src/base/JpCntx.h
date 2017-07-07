/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __JPCNTX_H__
#define __JPCNTX_H__

#define NUM_OF_CATEGORY 6

#include "nscore.h"

#define ENOUGH_REL_THRESHOLD  100
#define MAX_REL_THRESHOLD     1000

//hiragana frequency category table
extern const uint8_t jp2CharContext[83][83];

class JapaneseContextAnalysis
{
public:
  JapaneseContextAnalysis() {Reset();}

  void HandleData(const char* aBuf, uint32_t aLen);

  void HandleOneChar(const char* aStr, uint32_t aCharLen)
  {
    int32_t order;

    //if we received enough data, stop here
    if (mTotalRel > MAX_REL_THRESHOLD)   mDone = true;
    if (mDone)       return;

    //Only 2-bytes characters are of our interest
    order = (aCharLen == 2) ? GetOrder(aStr) : -1;
    if (order != -1 && mLastCharOrder != -1)
    {
      mTotalRel++;
      //count this sequence to its category counter
      mRelSample[jp2CharContext[mLastCharOrder][order]]++;
    }
    mLastCharOrder = order;
  }

  float GetConfidence(void);
  void      Reset();
  bool GotEnoughData() {return mTotalRel > ENOUGH_REL_THRESHOLD;}

protected:
  virtual int32_t GetOrder(const char* str, uint32_t *charLen) = 0;
  virtual int32_t GetOrder(const char* str) = 0;

  //category counters, each integer counts sequences in its category
  uint32_t mRelSample[NUM_OF_CATEGORY];

  //total sequence received
  uint32_t mTotalRel;

  //Number of sequences needed to trigger detection
  uint32_t mDataThreshold;

  //The order of previous char
  int32_t  mLastCharOrder;

  //if last byte in current buffer is not the last byte of a character, we
  //need to know how many byte to skip in next buffer.
  uint32_t mNeedToSkipCharNum;

  //If this flag is set to true, detection is done and conclusion has been made
  bool     mDone;
};


class SJISContextAnalysis : public JapaneseContextAnalysis
{
  //SJISContextAnalysis(){};
protected:
  int32_t GetOrder(const char* str, uint32_t *charLen);

  int32_t GetOrder(const char* str)
  {
    //We only interested in Hiragana, so first byte is '\202'
    if (*str == '\202' &&
          (unsigned char)*(str+1) >= (unsigned char)0x9f &&
          (unsigned char)*(str+1) <= (unsigned char)0xf1)
      return (unsigned char)*(str+1) - (unsigned char)0x9f;
    return -1;
  }
};

class EUCJPContextAnalysis : public JapaneseContextAnalysis
{
protected:
  int32_t GetOrder(const char* str, uint32_t *charLen);
  int32_t GetOrder(const char* str)
    //We only interested in Hiragana, so first byte is '\244'
  {
    if (*str == '\244' &&
          (unsigned char)*(str+1) >= (unsigned char)0xa1 &&
          (unsigned char)*(str+1) <= (unsigned char)0xf3)
      return (unsigned char)*(str+1) - (unsigned char)0xa1;
    return -1;
  }
};

#endif /* __JPCNTX_H__ */

