#ifndef __JPCNTX_H__
#define __JPCNTX_H__

#define NUM_OF_CATEGORY 6

#include "nscore.h" 

#define ENOUGH_REL_THRESHOLD  100
#define MAX_REL_THRESHOLD     1000

extern char jp2CharContext[83][83];

class JapaneseContextAnalysis
{
public:
  JapaneseContextAnalysis() {Reset();};
  void HandleData(const char* aBuf, PRUint32 aLen);
  void HandleOneChar(const char* aStr, PRUint32 aCharLen)
  {
    PRInt32 order;

    if (mTotalRel > MAX_REL_THRESHOLD)   mDone = PR_TRUE;
    if (mDone)       return;
     
    order = (aCharLen == 2) ? GetOrder(aStr) : -1;
    if (order != -1 && mLastCharOrder != -1)
    {
      mTotalRel++;
      mRelSample[jp2CharContext[mLastCharOrder][order]]++;
    }
    mLastCharOrder = order;
  };

  float GetConfidence();
  void      Reset(void);
  void      SetOpion(){};
  PRBool GotEnoughData() {return mTotalRel > ENOUGH_REL_THRESHOLD;};

protected:
  virtual PRInt32 GetOrder(const char* str, PRUint32 *charLen) = 0;
  virtual PRInt32 GetOrder(const char* str) = 0;
  PRUint32 mRelSample[NUM_OF_CATEGORY];
  PRUint32 mTotalRel;
  PRInt32  mLastCharOrder;
  PRUint32 mNeedToSkipCharNum;
  PRBool   mDone;
};


class SJISContextAnalysis : public JapaneseContextAnalysis
{
  //SJISContextAnalysis(){};
protected:
  PRInt32 GetOrder(const char* str, PRUint32 *charLen);
  PRInt32 GetOrder(const char* str)
  {
    if (*str == '\202' && 
          (unsigned char)*(str+1) >= (unsigned char)0x9f && 
          (unsigned char)*(str+1) <= (unsigned char)0xf1)
      return (unsigned char)*(str+1) - (unsigned char)0x9f;
    else 
      return -1;
  };
};

class EUCJPContextAnalysis : public JapaneseContextAnalysis
{
protected:
  PRInt32 GetOrder(const char* str, PRUint32 *charLen);
  PRInt32 GetOrder(const char* str)
    {return *str == '\244' ? ((unsigned char)*(str+1) - (unsigned char)0xa1):-1;};
};

#endif /* __JPCNTX_H__ */

