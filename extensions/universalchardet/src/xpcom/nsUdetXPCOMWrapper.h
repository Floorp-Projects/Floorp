/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsUdetXPCOMWrapper_h__
#define _nsUdetXPCOMWrapper_h__
#include "nsISupports.h"
#include "nsICharsetDetector.h"
#include "nsIStringCharsetDetector.h"
#include "nsICharsetDetectionObserver.h"
#include "nsCOMPtr.h"

#include "nsIFactory.h"

// {12BB8F1B-2389-11d3-B3BF-00805F8A6670}
#define NS_JA_PSMDETECTOR_CID \
{ 0x12bb8f1b, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {12BB8F1C-2389-11d3-B3BF-00805F8A6670}
#define NS_JA_STRING_PSMDETECTOR_CID \
{ 0x12bb8f1c, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

//=====================================================================
class nsXPCOMDetector :  
      public nsUniversalDetector,
      public nsICharsetDetector
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMDetector();
    NS_IMETHOD Init(nsICharsetDetectionObserver* aObserver);
    NS_IMETHOD DoIt(const char* aBuf, uint32_t aLen, bool *oDontFeedMe);
    NS_IMETHOD Done();
  protected:
    virtual ~nsXPCOMDetector();
    virtual void Report(const char* aCharset);
  private:
    nsCOMPtr<nsICharsetDetectionObserver> mObserver;
};


//=====================================================================
class nsXPCOMStringDetector :  
      public nsUniversalDetector,
      public nsIStringCharsetDetector
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMStringDetector();
    NS_IMETHOD DoIt(const char* aBuf, uint32_t aLen, 
                    const char** oCharset, nsDetectionConfident &oConf);
  protected:
    virtual ~nsXPCOMStringDetector();
    virtual void Report(const char* aCharset);
  private:
    nsCOMPtr<nsICharsetDetectionObserver> mObserver;
    const char* mResult;
};

//=====================================================================

class nsJAPSMDetector : public nsXPCOMDetector
{
public:
  nsJAPSMDetector() 
    : nsXPCOMDetector() {}
};

class nsJAStringPSMDetector : public nsXPCOMStringDetector
{
public:
  nsJAStringPSMDetector() 
    : nsXPCOMStringDetector() {}
};

#endif //_nsUdetXPCOMWrapper_h__
