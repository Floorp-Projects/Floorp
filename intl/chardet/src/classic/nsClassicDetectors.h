/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsClassicDetectors_h__
#define nsClassicDetectors_h__

#include "nsCOMPtr.h"
#include "nsIFactory.h"

// {1D2394A0-542A-11d3-914D-006008A6EDF6}
#define NS_JA_CLASSIC_DETECTOR_CID \
{ 0x1d2394a0, 0x542a, 0x11d3, { 0x91, 0x4d, 0x0, 0x60, 0x8, 0xa6, 0xed, 0xf6 } }

// {1D2394A1-542A-11d3-914D-006008A6EDF6}
#define NS_JA_CLASSIC_STRING_DETECTOR_CID \
{ 0x1d2394a1, 0x542a, 0x11d3, { 0x91, 0x4d, 0x0, 0x60, 0x8, 0xa6, 0xed, 0xf6 } }

// {1D2394A2-542A-11d3-914D-006008A6EDF6}
#define NS_KO_CLASSIC_DETECTOR_CID \
{ 0x1d2394a2, 0x542a, 0x11d3, { 0x91, 0x4d, 0x0, 0x60, 0x8, 0xa6, 0xed, 0xf6 } }

// {1D2394A3-542A-11d3-914D-006008A6EDF6}
#define NS_KO_CLASSIC_STRING_DETECTOR_CID \
{ 0x1d2394a3, 0x542a, 0x11d3, { 0x91, 0x4d, 0x0, 0x60, 0x8, 0xa6, 0xed, 0xf6 } }

class nsClassicDetector : 
      public nsICharsetDetector // Implement the interface 
{
public:
  NS_DECL_ISUPPORTS

  nsClassicDetector(const char* language);
  virtual ~nsClassicDetector();
  NS_IMETHOD Init(nsICharsetDetectionObserver* aObserver);
  NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, PRBool* oDontFeedMe);
  NS_IMETHOD Done();
 
private:
  nsCOMPtr<nsICharsetDetectionObserver> mObserver;
  char mCharset[65];
  char mLanguage[32];
};


class nsClassicStringDetector : 
      public nsIStringCharsetDetector // Implement the interface 
{
public:
  NS_DECL_ISUPPORTS

  nsClassicStringDetector(const char* language);
  virtual ~nsClassicStringDetector();
  NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, 
                  const char** oCharset, 
                  nsDetectionConfident &oConfident);
protected:
  char mCharset[65];
  char mLanguage[32];
};

class nsJACharsetClassicDetector : public nsClassicDetector
{
public:
  nsJACharsetClassicDetector() 
    : nsClassicDetector("ja") {};
};

class nsJAStringCharsetClassicDetector : public nsClassicStringDetector
{
public:
  nsJAStringCharsetClassicDetector() 
    : nsClassicStringDetector("ja") {};
};

class nsKOCharsetClassicDetector : public nsClassicDetector
{
public:
  nsKOCharsetClassicDetector() 
    : nsClassicDetector("ko") {};
};

class nsKOStringCharsetClassicDetector : public nsClassicStringDetector
{
public:
  nsKOStringCharsetClassicDetector() 
    : nsClassicStringDetector("ko") {};
};

#endif /* nsClassicDetectors_h__ */
