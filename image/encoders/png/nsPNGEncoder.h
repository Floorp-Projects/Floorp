/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#include "imgIEncoder.h"

#include "mozilla/ReentrantMonitor.h"

#include "nsCOMPtr.h"

#include <png.h>

#define NS_PNGENCODER_CID \
{ /* 38d1592e-b81e-432b-86f8-471878bbfe07 */         \
     0x38d1592e,                                     \
     0xb81e,                                         \
     0x432b,                                         \
    {0x86, 0xf8, 0x47, 0x18, 0x78, 0xbb, 0xfe, 0x07} \
}

// Provides PNG encoding functionality. Use InitFromData() to do the
// encoding. See that function definition for encoding options.

class nsPNGEncoder : public imgIEncoder
{
  typedef mozilla::ReentrantMonitor ReentrantMonitor;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIENCODER
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  nsPNGEncoder();
  ~nsPNGEncoder();

protected:
  nsresult ParseOptions(const nsAString& aOptions,
                        bool* useTransparency,
                        bool* skipFirstFrame,
                        PRUint32* numAnimatedFrames,
                        PRUint32* numIterations,
                        PRUint32* frameDispose,
                        PRUint32* frameBlend,
                        PRUint32* frameDelay,
                        PRUint32* offsetX,
                        PRUint32* offsetY);
  void ConvertHostARGBRow(const PRUint8* aSrc, PRUint8* aDest,
                          PRUint32 aPixelWidth, bool aUseTransparency);
  void StripAlpha(const PRUint8* aSrc, PRUint8* aDest,
                  PRUint32 aPixelWidth);
  static void ErrorCallback(png_structp png_ptr, png_const_charp warning_msg);
  static void WriteCallback(png_structp png, png_bytep data, png_size_t size);
  void NotifyListener();

  png_struct* mPNG;
  png_info* mPNGinfo;

  bool mIsAnimation;
  bool mFinished;

  // image buffer
  PRUint8* mImageBuffer;
  PRUint32 mImageBufferSize;
  PRUint32 mImageBufferUsed;

  PRUint32 mImageBufferReadPoint;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
  PRUint32 mNotifyThreshold;

  /*
    nsPNGEncoder is designed to allow one thread to pump data into it while another
    reads from it.  We lock to ensure that the buffer remains append-only while
    we read from it (that it is not realloced) and to ensure that only one thread
    dispatches a callback for each call to AsyncWait.
   */
  ReentrantMonitor mReentrantMonitor;
};
