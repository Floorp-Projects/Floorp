/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
