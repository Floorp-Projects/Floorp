/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsUdetXPCOMWrapper_h__
#define _nsUdetXPCOMWrapper_h__
#include "nsISupports.h"
#include "nsICharsetDetector.h"
#include "nsIStringCharsetDetector.h"
#include "nsICharsetDetectionObserver.h"
#include "nsCOMPtr.h"

#include "nsIFactory.h"

// {374E0CDE-F605-4259-8C92-E639C6C2EEEF}
#define NS_UNIVERSAL_DETECTOR_CID \
{ 0x374e0cde, 0xf605, 0x4259, { 0x8c, 0x92, 0xe6, 0x39, 0xc6, 0xc2, 0xee, 0xef } }

// {6EE5301A-3981-49bd-85F8-1A2CC228CF3E}
#define NS_UNIVERSAL_STRING_DETECTOR_CID \
{ 0x6ee5301a, 0x3981, 0x49bd, { 0x85, 0xf8, 0x1a, 0x2c, 0xc2, 0x28, 0xcf, 0x3e } }

// {12BB8F1B-2389-11d3-B3BF-00805F8A6670}
#define NS_JA_PSMDETECTOR_CID \
{ 0x12bb8f1b, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {12BB8F1C-2389-11d3-B3BF-00805F8A6670}
#define NS_JA_STRING_PSMDETECTOR_CID \
{ 0x12bb8f1c, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {EA06D4E1-2B3D-11d3-B3BF-00805F8A6670}
#define NS_KO_PSMDETECTOR_CID \
{ 0xea06d4e1, 0x2b3d, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {EA06D4E2-2B3D-11d3-B3BF-00805F8A6670}
#define NS_ZHCN_PSMDETECTOR_CID \
{ 0xea06d4e2, 0x2b3d, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {EA06D4E3-2B3D-11d3-B3BF-00805F8A6670}
#define NS_ZHTW_PSMDETECTOR_CID \
{ 0xea06d4e3, 0x2b3d, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


// {EA06D4E4-2B3D-11d3-B3BF-00805F8A6670}
#define NS_KO_STRING_PSMDETECTOR_CID \
{ 0xea06d4e4, 0x2b3d, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {EA06D4E5-2B3D-11d3-B3BF-00805F8A6670}
#define NS_ZHCN_STRING_PSMDETECTOR_CID \
{ 0xea06d4e5, 0x2b3d, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {EA06D4E6-2B3D-11d3-B3BF-00805F8A6670}
#define NS_ZHTW_STRING_PSMDETECTOR_CID \
{ 0xea06d4e6, 0x2b3d, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


// {FCACEF21-2B40-11d3-B3BF-00805F8A6670}
#define NS_ZH_STRING_PSMDETECTOR_CID \
{ 0xfcacef21, 0x2b40, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {FCACEF22-2B40-11d3-B3BF-00805F8A6670}
#define NS_CJK_STRING_PSMDETECTOR_CID \
{ 0xfcacef22, 0x2b40, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


// {FCACEF23-2B40-11d3-B3BF-00805F8A6670}
#define NS_ZH_PSMDETECTOR_CID \
{ 0xfcacef23, 0x2b40, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {FCACEF24-2B40-11d3-B3BF-00805F8A6670}
#define NS_CJK_PSMDETECTOR_CID \
{ 0xfcacef24, 0x2b40, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

//=====================================================================
class nsXPCOMDetector :  
      public nsUniversalDetector,
      public nsICharsetDetector
{
  NS_DECL_ISUPPORTS
  public:
    nsXPCOMDetector(PRUint32 aLanguageFilter);
    virtual ~nsXPCOMDetector();
    NS_IMETHOD Init(nsICharsetDetectionObserver* aObserver);
    NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, bool *oDontFeedMe);
    NS_IMETHOD Done();
  protected:
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
    nsXPCOMStringDetector(PRUint32 aLanguageFilter);
    virtual ~nsXPCOMStringDetector();
    NS_IMETHOD DoIt(const char* aBuf, PRUint32 aLen, 
                    const char** oCharset, nsDetectionConfident &oConf);
  protected:
    virtual void Report(const char* aCharset);
  private:
    nsCOMPtr<nsICharsetDetectionObserver> mObserver;
    const char* mResult;
};

//=====================================================================
class nsUniversalXPCOMDetector : public nsXPCOMDetector
{
public:
  nsUniversalXPCOMDetector() 
    : nsXPCOMDetector(NS_FILTER_ALL) {}
};

class nsUniversalXPCOMStringDetector : public nsXPCOMStringDetector
{
public:
  nsUniversalXPCOMStringDetector() 
    : nsXPCOMStringDetector(NS_FILTER_ALL) {}
};

class nsJAPSMDetector : public nsXPCOMDetector
{
public:
  nsJAPSMDetector() 
    : nsXPCOMDetector(NS_FILTER_JAPANESE) {}
};

class nsJAStringPSMDetector : public nsXPCOMStringDetector
{
public:
  nsJAStringPSMDetector() 
    : nsXPCOMStringDetector(NS_FILTER_JAPANESE) {}
};

class nsKOPSMDetector : public nsXPCOMDetector
{
public:
  nsKOPSMDetector() 
    : nsXPCOMDetector(NS_FILTER_KOREAN) {}
};

class nsKOStringPSMDetector : public nsXPCOMStringDetector
{
public:
  nsKOStringPSMDetector() 
    : nsXPCOMStringDetector(NS_FILTER_KOREAN) {}
};

class nsZHTWPSMDetector : public nsXPCOMDetector
{
public:
  nsZHTWPSMDetector() 
    : nsXPCOMDetector(NS_FILTER_CHINESE_TRADITIONAL) {}
};

class nsZHTWStringPSMDetector : public nsXPCOMStringDetector
{
public:
  nsZHTWStringPSMDetector() 
    : nsXPCOMStringDetector(NS_FILTER_CHINESE_TRADITIONAL) {}
};

class nsZHCNPSMDetector : public nsXPCOMDetector
{
public:
  nsZHCNPSMDetector() 
    : nsXPCOMDetector(NS_FILTER_CHINESE_SIMPLIFIED) {}
};

class nsZHCNStringPSMDetector : public nsXPCOMStringDetector
{
public:
  nsZHCNStringPSMDetector() 
    : nsXPCOMStringDetector(NS_FILTER_CHINESE_SIMPLIFIED) {}
};

class nsZHPSMDetector : public nsXPCOMDetector
{
public:
  nsZHPSMDetector() 
    : nsXPCOMDetector(NS_FILTER_CHINESE) {}
};

class nsZHStringPSMDetector : public nsXPCOMStringDetector
{
public:
  nsZHStringPSMDetector() 
    : nsXPCOMStringDetector(NS_FILTER_CHINESE) {}
};

class nsCJKPSMDetector : public nsXPCOMDetector
{
public:
  nsCJKPSMDetector() 
    : nsXPCOMDetector(NS_FILTER_CJK) {}
};

class nsCJKStringPSMDetector : public nsXPCOMStringDetector
{
public:
  nsCJKStringPSMDetector() 
    : nsXPCOMStringDetector(NS_FILTER_CJK) {}
};

#endif //_nsUdetXPCOMWrapper_h__
