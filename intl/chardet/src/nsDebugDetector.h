/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
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
#ifndef nsDebugDetector_h__
#define nsDebugDetector_h__

#include "nsIFactory.h"
#include "nsICharsetDetector.h"


// {12BB8F18-2389-11d3-B3BF-00805F8A6670}
#define NS_1STBLKDBG_DETECTOR_CID \
{ 0x12bb8f18, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {12BB8F19-2389-11d3-B3BF-00805F8A6670}
#define NS_2NDBLKDBG_DETECTOR_CID \
{ 0x12bb8f19, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

// {12BB8F1A-2389-11d3-B3BF-00805F8A6670}
#define NS_LASTBLKDBG_DETECTOR_CID \
{ 0x12bb8f1a, 0x2389, 0x11d3, { 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }



typedef enum {
  k1stBlk,
  k2ndBlk,
  klastBlk
} nsDebugDetectorSel;

class nsDebugDetector : public nsICharsetDetector 
{
  NS_DECL_ISUPPORTS

public:  
  nsDebugDetector(nsDebugDetectorSel aSel);
  virtual ~nsDebugDetector();

  NS_IMETHOD Init(nsICharsetDetectionObserver* aObserver);

  NS_IMETHOD DoIt(const char* aBytesArray, PRUint32 aLen, PRBool* oDontFeedMe);

  NS_IMETHOD Done();

protected:

  virtual void Report();

private:
  PRInt32 mBlks;
  nsDebugDetectorSel mSel;
  nsICharsetDetectionObserver* mObserver;
  PRBool mStop;
};

class ns1stBlkDbgDetector : public nsDebugDetector
{
public:
    ns1stBlkDbgDetector () : nsDebugDetector(k1stBlk) {} ;
};

class ns2ndBlkDbgDetector : public nsDebugDetector
{
public:
  ns2ndBlkDbgDetector () : nsDebugDetector(k2ndBlk) {} ;
};

class nsLastBlkDbgDetector : public nsDebugDetector
{
public:
  nsLastBlkDbgDetector () : nsDebugDetector(klastBlk) {} ;
};

#endif // nsDebugDetector_h__
