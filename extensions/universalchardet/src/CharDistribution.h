//CharDistribution.h
#ifndef CharDistribution_h__
#define CharDistribution_h__

#include "nscore.h"

#define ENOUGH_DATA_THRESHOLD 1024
 
class CharDistributionAnalysis
{
public:
  CharDistributionAnalysis() {Reset();};
  void HandleData(const char* aBuf, PRUint32 aLen) {};
  void HandleOneChar(const char* aStr, PRUint32 aCharLen)
  {
    PRInt32 order;

    order = (aCharLen == 2) ? GetOrder(aStr) : -1;
    if (order >= 0 && (PRUint32)order < mTableSize)
    {
      mTotalChars++;
      if (512 > mCharToFreqOrder[order])
        mFreqChars++;
    }
  };

  float GetConfidence();
  void      Reset(void) 
  {
    mDone = PR_FALSE;
    mTotalChars = 0;
    mFreqChars = 0;
  };
  void      SetOpion(){};
  PRBool GotEnoughData() {return mTotalChars > ENOUGH_DATA_THRESHOLD;};

protected:
  virtual PRInt32 GetOrder(const char* str) {return -1;};
  
  PRBool   mDone;
  PRUint32 mFreqChars;
  PRUint32 mTotalChars;
  PRInt16  *mCharToFreqOrder;
  PRUint32 mTableSize;
  float    mTypicalDistributionRatio;
};

class EUCTWDistributionAnalysis: public CharDistributionAnalysis
{
public:
  EUCTWDistributionAnalysis();
protected:
  PRInt32 GetOrder(const char* str) 
  { if ((unsigned char)*str >= (unsigned char)0xc4)  
      return 94*((unsigned char)str[0]-(unsigned char)0xc4) + (unsigned char)str[1] - (unsigned char)0xa1;
    else
      return -1;
  };
};


class EUCKRDistributionAnalysis : public CharDistributionAnalysis
{
public:
  EUCKRDistributionAnalysis();
protected:
  PRInt32 GetOrder(const char* str) 
  { if ((unsigned char)*str >= (unsigned char)0xb0)  
      return 94*((unsigned char)str[0]-(unsigned char)0xb0) + (unsigned char)str[1] - (unsigned char)0xa1;
    else
      return -1;
  };
};

class GB2312DistributionAnalysis : public CharDistributionAnalysis
{
public:
  GB2312DistributionAnalysis();
protected:
  PRInt32 GetOrder(const char* str) 
  { if ((unsigned char)*str >= (unsigned char)0xb0)  
      return 94*((unsigned char)str[0]-(unsigned char)0xb0) + (unsigned char)str[1] - (unsigned char)0xa1;
    else
      return -1;
  };
};


class Big5DistributionAnalysis : public CharDistributionAnalysis
{
public:
  Big5DistributionAnalysis();
protected:
  PRInt32 GetOrder(const char* str) 
  { if ((unsigned char)*str >= (unsigned char)0xa4)  
      if ((unsigned char)str[1] >= (unsigned char)0xa1)
        return 157*((unsigned char)str[0]-(unsigned char)0xa4) + (unsigned char)str[1] - (unsigned char)0xa1 +63;
      else
        return 157*((unsigned char)str[0]-(unsigned char)0xa4) + (unsigned char)str[1] - (unsigned char)0x40;
    else
      return -1;
  };
};

class SJISDistributionAnalysis : public CharDistributionAnalysis
{
public:
  SJISDistributionAnalysis();
protected:
  PRInt32 GetOrder(const char* str) 
  { 
    PRInt32 order;
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
  };
};

class EUCJPDistributionAnalysis : public CharDistributionAnalysis
{
public:
  EUCJPDistributionAnalysis();
protected:
  PRInt32 GetOrder(const char* str) 
  { if ((unsigned char)*str >= (unsigned char)0xa0)  
      return 94*((unsigned char)str[0]-(unsigned char)0xa1) + (unsigned char)str[1] - (unsigned char)0xa1;
    else
      return -1;
  };
};

#endif //CharDistribution_h__

