/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsCyrillicDetector_h__
#define nsCyrillicDetector_h__

#include "nsCyrillicClass.h"




// {2002F781-3960-11d3-B3C3-00805F8A6670}
#define NS_RU_PROBDETECTOR_CID \
{ 0x2002f781, 0x3960, 0x11d3, { 0xb3, 0xc3, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


// {2002F782-3960-11d3-B3C3-00805F8A6670}
#define NS_UK_PROBDETECTOR_CID \
{ 0x2002f782, 0x3960, 0x11d3, { 0xb3, 0xc3, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {2002F783-3960-11d3-B3C3-00805F8A6670}
#define NS_RU_STRING_PROBDETECTOR_CID \
{ 0x2002f783, 0x3960, 0x11d3, { 0xb3, 0xc3, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {2002F784-3960-11d3-B3C3-00805F8A6670}
#define NS_UK_STRING_PROBDETECTOR_CID \
{ 0x2002f784, 0x3960, 0x11d3, { 0xb3, 0xc3, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

static const PRUint8 *gCyrillicCls[5] =
{
   CP1251Map,
   KOI8Map,
   ISO88595Map,
   MacCyrillicMap,
   IBM866Map
};

static const char * gRussian[5] = {
  "windows-1251", 
  "KOI8-R", 
  "ISO-8859-5", 
  "x-mac-cyrillic",
  "IBM866"
};

static const char * gUkrainian[5] = {
  "windows-1251", 
  "KOI8-U", 
  "ISO-8859-5", 
  "x-mac-cyrillic",
  "IBM866"
};

#define NUM_CYR_CHARSET 5

class nsCyrillicDetector 
{
  public:
    nsCyrillicDetector(PRUint8 aItems, 
                      const PRUint8 ** aCyrillicClass, 
                      const char **aCharsets) {
      mItems = aItems;
      mCyrillicClass = aCyrillicClass;
      mCharsets = aCharsets;
      for(PRUintn i=0;i<mItems;i++)
        mProb[i] = mLastCls[i] =0;
      mDone = false;
    }
    virtual ~nsCyrillicDetector() {}
    virtual void HandleData(const char* aBuf, PRUint32 aLen);
    virtual void   DataEnd();
  protected:
    virtual void Report(const char* aCharset) = 0;
    bool    mDone;

  private:
    PRUint8  mItems;
    const PRUint8 ** mCyrillicClass;
    const char** mCharsets;
    PRUint32 mProb[NUM_CYR_CHARSET];
    PRUint8 mLastCls[NUM_CYR_CHARSET];
};

class nsCyrXPCOMDetector :  
      public nsCyrillicDetector,
      public nsICharsetDetector
{
  public:
    // nsISupports interface
    NS_DECL_ISUPPORTS
    nsCyrXPCOMDetector(PRUint8 aItems, 
                      const PRUint8 ** aCyrillicClass, 
                      const char **aCharsets);
    virtual ~nsCyrXPCOMDetector();
    NS_IMETHOD Init(nsICharsetDetectionObserver* aObserver);
    NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, bool *oDontFeedMe);
    NS_IMETHOD Done();
  protected:
    virtual void Report(const char* aCharset);
  private:
    nsCOMPtr<nsICharsetDetectionObserver> mObserver;
};

class nsCyrXPCOMStringDetector :  
      public nsCyrillicDetector,
      public nsIStringCharsetDetector
{
  public:
    // nsISupports interface
    NS_DECL_ISUPPORTS
    nsCyrXPCOMStringDetector(PRUint8 aItems, 
                      const PRUint8 ** aCyrillicClass, 
                      const char **aCharsets);
    virtual ~nsCyrXPCOMStringDetector();
    NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, 
                     const char** oCharset, nsDetectionConfident &oConf);
  protected:
    virtual void Report(const char* aCharset);
  private:
    nsCOMPtr<nsICharsetDetectionObserver> mObserver;
    const char* mResult;
};

class nsRUProbDetector : public nsCyrXPCOMDetector
{
  public:
    nsRUProbDetector() 
      : nsCyrXPCOMDetector(5, gCyrillicCls, gRussian) {}
};

class nsRUStringProbDetector : public nsCyrXPCOMStringDetector
{
  public:
    nsRUStringProbDetector() 
      : nsCyrXPCOMStringDetector(5, gCyrillicCls, gRussian) {}
};

class nsUKProbDetector : public nsCyrXPCOMDetector
{
  public:
    nsUKProbDetector() 
      : nsCyrXPCOMDetector(5, gCyrillicCls, gUkrainian) {}
};

class nsUKStringProbDetector : public nsCyrXPCOMStringDetector
{
  public:
    nsUKStringProbDetector() 
      : nsCyrXPCOMStringDetector(5, gCyrillicCls, gUkrainian) {}
};

#endif
