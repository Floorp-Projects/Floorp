/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsICharsetDetector_h__
#define nsICharsetDetector_h__

#include "nsISupports.h"

class nsICharsetDetectionObserver;

// {12BB8F14-2389-11d3-B3BF-00805F8A6670}
#define NS_ICHARSETDETECTOR_IID \
{ 0x12bb8f14, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

#define NS_CHARSET_DETECTOR_PROGID_BASE "component://netscape/intl/charsetdetect?type="
 
class nsICharsetDetector : public nsISupports {
public:  
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICHARSETDETECTOR_IID)

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
     oDontFeedMe - return PR_TRUE if the detector do not need the following block
                          PR_FALSE it need more bytes. 
                   This is used to enhance performance
   */
  NS_IMETHOD DoIt(const char* aBytesArray, PRUint32 aLen, PRBool* oDontFeedMe) = 0;

  /*
     It also tell the detector the last chance the make a decision
   */
  NS_IMETHOD Done() = 0;

};
#endif /* nsICharsetDetector_h__ */
