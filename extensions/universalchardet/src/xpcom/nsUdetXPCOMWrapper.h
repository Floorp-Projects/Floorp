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
#include "nsUniversalDetector.h"

//=====================================================================
class nsXPCOMDetector :
      public nsUniversalDetector,
      public nsICharsetDetector
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMDetector();
    NS_IMETHOD Init(nsICharsetDetectionObserver* aObserver) override;
    NS_IMETHOD DoIt(const char* aBuf, uint32_t aLen, bool *oDontFeedMe) override;
    NS_IMETHOD Done() override;
  protected:
    virtual ~nsXPCOMDetector();
    virtual void Report(const char* aCharset) override;
  private:
    nsCOMPtr<nsICharsetDetectionObserver> mObserver;
};

//=====================================================================

class nsJAPSMDetector final : public nsXPCOMDetector
{
public:
  nsJAPSMDetector()
    : nsXPCOMDetector() {}
};

#endif //_nsUdetXPCOMWrapper_h__
