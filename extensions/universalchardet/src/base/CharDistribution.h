/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CharDistribution_h__
#define CharDistribution_h__

#include "nscore.h"

#define ENOUGH_DATA_THRESHOLD 1024

class CharDistributionAnalysis
{
public:
  CharDistributionAnalysis() {Reset();}

  //feed a block of data and do distribution analysis
  void HandleData(const char* aBuf, uint32_t aLen) {}

  //Feed a character with known length
  void HandleOneChar(const char* aStr, uint32_t aCharLen)
  {
    int32_t order;

    //we only care about 2-bytes character in our distribution analysis
    order = (aCharLen == 2) ? GetOrder(aStr) : -1;

    if (order >= 0)
    {
      mTotalChars++;
      //order is valid
      if ((uint32_t)order < mTableSize)
      {
        if (512 > mCharToFreqOrder[order])
          mFreqChars++;
      }
    }
  }

  //return confidence base on existing data
  float GetConfidence(void);

  //Reset analyser, clear any state
  void      Reset()
  {
    mDone = false;
    mTotalChars = 0;
    mFreqChars = 0;
    mDataThreshold = 0;
  }

  //It is not necessary to receive all data to draw conclusion. For charset detection,
  // certain amount of data is enough
  bool GotEnoughData() {return mTotalChars > ENOUGH_DATA_THRESHOLD;}

protected:
  //we do not handle character base on its original encoding string, but
  //convert this encoding string to a number, here called order.
  //This allow multiple encoding of a language to share one frequency table
  virtual int32_t GetOrder(const char* str) {return -1;}

  //If this flag is set to true, detection is done and conclusion has been made
  bool     mDone;

  //The number of characters whose frequency order is less than 512
  uint32_t mFreqChars;

  //Total character encounted.
  uint32_t mTotalChars;

  //Number of hi-byte characters needed to trigger detection
  uint32_t mDataThreshold;

  //Mapping table to get frequency order from char order (get from GetOrder())
  const int16_t  *mCharToFreqOrder;

  //Size of above table
  uint32_t mTableSize;

  //This is a constant value varies from language to language, it is used in
  //calculating confidence. See my paper for further detail.
  float    mTypicalDistributionRatio;
};


class EUCTWDistributionAnalysis: public CharDistributionAnalysis
{
public:
  EUCTWDistributionAnalysis();
protected:

  //for euc-TW encoding, we are interested
  //  first  byte range: 0xc4 -- 0xfe
  //  second byte range: 0xa1 -- 0xfe
  //no validation needed here. State machine has done that
  int32_t GetOrder(const char* str) override
  {
    if ((unsigned char)*str >= (unsigned char)0xc4)
      return 94*((unsigned char)str[0]-(unsigned char)0xc4) + (unsigned char)str[1] - (unsigned char)0xa1;
    else
      return -1;
  }
};


class EUCKRDistributionAnalysis : public CharDistributionAnalysis
{
public:
  EUCKRDistributionAnalysis();
protected:
  //for euc-KR encoding, we are interested
  //  first  byte range: 0xb0 -- 0xfe
  //  second byte range: 0xa1 -- 0xfe
  //no validation needed here. State machine has done that
  int32_t GetOrder(const char* str) override
  {
    if ((unsigned char)*str >= (unsigned char)0xb0)
      return 94*((unsigned char)str[0]-(unsigned char)0xb0) + (unsigned char)str[1] - (unsigned char)0xa1;
    else
      return -1;
  }
};

class GB2312DistributionAnalysis : public CharDistributionAnalysis
{
public:
  GB2312DistributionAnalysis();
protected:
  //for GB2312 encoding, we are interested
  //  first  byte range: 0xb0 -- 0xfe
  //  second byte range: 0xa1 -- 0xfe
  //no validation needed here. State machine has done that
  int32_t GetOrder(const char* str) override
  {
    if ((unsigned char)*str >= (unsigned char)0xb0 && (unsigned char)str[1] >= (unsigned char)0xa1)
      return 94*((unsigned char)str[0]-(unsigned char)0xb0) + (unsigned char)str[1] - (unsigned char)0xa1;
    else
      return -1;
  }
};


class Big5DistributionAnalysis : public CharDistributionAnalysis
{
public:
  Big5DistributionAnalysis();
protected:
  //for big5 encoding, we are interested
  //  first  byte range: 0xa4 -- 0xfe
  //  second byte range: 0x40 -- 0x7e , 0xa1 -- 0xfe
  //no validation needed here. State machine has done that
  int32_t GetOrder(const char* str) override
  {
    if ((unsigned char)*str >= (unsigned char)0xa4)
      if ((unsigned char)str[1] >= (unsigned char)0xa1)
        return 157*((unsigned char)str[0]-(unsigned char)0xa4) + (unsigned char)str[1] - (unsigned char)0xa1 +63;
      else
        return 157*((unsigned char)str[0]-(unsigned char)0xa4) + (unsigned char)str[1] - (unsigned char)0x40;
    else
      return -1;
  }
};

class SJISDistributionAnalysis : public CharDistributionAnalysis
{
public:
  SJISDistributionAnalysis();
protected:
  //for sjis encoding, we are interested
  //  first  byte range: 0x81 -- 0x9f , 0xe0 -- 0xfe
  //  second byte range: 0x40 -- 0x7e,  0x81 -- oxfe
  //no validation needed here. State machine has done that
  int32_t GetOrder(const char* str) override
  {
    int32_t order;
    if ((unsigned char)*str >= (unsigned char)0x81 && (unsigned char)*str <= (unsigned char)0x9f)
      order = 188 * ((unsigned char)str[0]-(unsigned char)0x81);
    else if ((unsigned char)*str >= (unsigned char)0xe0 && (unsigned char)*str <= (unsigned char)0xef)
      order = 188 * ((unsigned char)str[0]-(unsigned char)0xe0 + 31);
    else
      return -1;
    order += (unsigned char)*(str+1) - 0x40;
    if ((unsigned char)str[1] > (unsigned char)0x7f)
      order--;
    return order;
  }
};

class EUCJPDistributionAnalysis : public CharDistributionAnalysis
{
public:
  EUCJPDistributionAnalysis();
protected:
  //for euc-JP encoding, we are interested
  //  first  byte range: 0xa0 -- 0xfe
  //  second byte range: 0xa1 -- 0xfe
  //no validation needed here. State machine has done that
  int32_t GetOrder(const char* str) override
  {
    if ((unsigned char)*str >= (unsigned char)0xa0)
      return 94*((unsigned char)str[0]-(unsigned char)0xa1) + (unsigned char)str[1] - (unsigned char)0xa1;
    else
      return -1;
  }
};

#endif //CharDistribution_h__
