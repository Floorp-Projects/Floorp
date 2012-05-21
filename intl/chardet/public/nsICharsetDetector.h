/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsICharsetDetector_h__
#define nsICharsetDetector_h__

#include "nsISupports.h"

class nsICharsetDetectionObserver;

// {12BB8F14-2389-11d3-B3BF-00805F8A6670}
#define NS_ICHARSETDETECTOR_IID \
{ 0x12bb8f14, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

#define NS_CHARSET_DETECTOR_CONTRACTID_BASE "@mozilla.org/intl/charsetdetect;1?type="
#define NS_CHARSET_DETECTOR_CATEGORY "charset-detectors"
 
class nsICharsetDetector : public nsISupports {
public:  
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICHARSETDETECTOR_IID)

  /* 
     Setup the observer so it know how to notify the answer
   */
  NS_IMETHOD Init(nsICharsetDetectionObserver* observer) = 0;

  /*
     Feed a block of bytes to the detector.
     It will call the Notify function of the nsICharsetObserver if it find out
     the answer. 
     aBytesArray - array of bytes
     aLen        - length of aBytesArray
     oDontFeedMe - return true if the detector do not need the following block
                          false it need more bytes. 
                   This is used to enhance performance
   */
  NS_IMETHOD DoIt(const char* aBytesArray, PRUint32 aLen, bool* oDontFeedMe) = 0;

  /*
     It also tell the detector the last chance the make a decision
   */
  NS_IMETHOD Done() = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICharsetDetector,
                              NS_ICHARSETDETECTOR_IID)

#endif /* nsICharsetDetector_h__ */
