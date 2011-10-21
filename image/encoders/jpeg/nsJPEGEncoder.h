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
 * The Original Code is JPEG Encoding code
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
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

// needed for JPEG library
#include <stdio.h>

extern "C" {
#include "jpeglib.h"
}

#define NS_JPEGENCODER_CID \
{ /* ac2bb8fe-eeeb-4572-b40f-be03932b56e0 */         \
     0xac2bb8fe,                                     \
     0xeeeb,                                         \
     0x4572,                                         \
    {0xb4, 0x0f, 0xbe, 0x03, 0x93, 0x2b, 0x56, 0xe0} \
}

// Provides JPEG encoding functionality. Use InitFromData() to do the
// encoding. See that function definition for encoding options.

class nsJPEGEncoder : public imgIEncoder
{
  typedef mozilla::ReentrantMonitor ReentrantMonitor;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIENCODER
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  nsJPEGEncoder();

private:
  ~nsJPEGEncoder();

protected:

  void ConvertHostARGBRow(const PRUint8* aSrc, PRUint8* aDest,
                          PRUint32 aPixelWidth);
  void StripAlpha(const PRUint8* aSrc, PRUint8* aDest, PRUint32 aPixelWidth);

  static void initDestination(jpeg_compress_struct* cinfo);
  static boolean emptyOutputBuffer(jpeg_compress_struct* cinfo);
  static void termDestination(jpeg_compress_struct* cinfo);

  static void errorExit(jpeg_common_struct* cinfo);

  void NotifyListener();

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
    nsJPEGEncoder is designed to allow one thread to pump data into it while another
    reads from it.  We lock to ensure that the buffer remains append-only while
    we read from it (that it is not realloced) and to ensure that only one thread
    dispatches a callback for each call to AsyncWait.
   */
  ReentrantMonitor mReentrantMonitor;
};
