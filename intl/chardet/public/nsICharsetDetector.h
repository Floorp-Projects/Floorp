/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

NS_DEFINE_STATIC_IID_ACCESSOR(nsICharsetDetector,
                              NS_ICHARSETDETECTOR_IID)

#endif /* nsICharsetDetector_h__ */
