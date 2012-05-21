/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIStringCharsetDetector_h__
#define nsIStringCharsetDetector_h__

#include "nsISupports.h"
#include "nsDetectionConfident.h"

// {12BB8F15-2389-11d3-B3BF-00805F8A6670}
#define NS_ISTRINGCHARSETDETECTOR_IID \
{ 0x12bb8f15, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


#define NS_STRCDETECTOR_CONTRACTID_BASE "@mozilla.org/intl/stringcharsetdetect;1?type="

/*
  This interface is similar to nsICharsetDetector
  The difference is it is for line base detection instead of block based 
  detectection. 
 */


class nsIStringCharsetDetector : public nsISupports {
public:  

   NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISTRINGCHARSETDETECTOR_IID)
  /*
     Perform the charset detection
    
     aBytesArray- the bytes
     aLen- the length of the bytes
     oCharset- the charset answer
     oConfident - the confidence of the answer
   */
  NS_IMETHOD DoIt(const char* aBytesArray, PRUint32 aLen, 
                    const char** oCharset, nsDetectionConfident &oConfident) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIStringCharsetDetector,
                              NS_ISTRINGCHARSETDETECTOR_IID)

#endif /* nsIStringCharsetDetector_h__ */
