/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_CAMERAPREVIEW_H
#define DOM_CAMERA_CAMERAPREVIEW_H

#include "MediaStreamGraph.h"
#include "StreamBuffer.h"
#include "nsDOMMediaStream.h"

#define DOM_CAMERA_LOG_LEVEL  3
#include "CameraCommon.h"

using namespace mozilla;
using namespace mozilla::layers;

namespace mozilla {

class CameraPreview : public nsDOMMediaStream
                    , public MediaStreamListener
{
public:
  NS_DECL_ISUPPORTS

  CameraPreview(nsIThread* aCameraThread, PRUint32 aWidth, PRUint32 aHeight);

  void SetFrameRate(PRUint32 aFramesPerSecond);

  NS_IMETHODIMP
  GetCurrentTime(double* aCurrentTime) {
    return nsDOMMediaStream::GetCurrentTime(aCurrentTime);
  }

  void Start();
  void Stop();

  virtual nsresult StartImpl() = 0;
  virtual nsresult StopImpl() = 0;

protected:
  virtual ~CameraPreview();

  PRUint32 mWidth;
  PRUint32 mHeight;
  PRUint32 mFramesPerSecond;
  SourceMediaStream* mInput;
  nsRefPtr<mozilla::layers::ImageContainer> mImageContainer;
  VideoSegment mVideoSegment;
  PRUint32 mFrameCount;
  nsCOMPtr<nsIThread> mCameraThread;

  enum { TRACK_VIDEO = 1 };

private:
  CameraPreview(const CameraPreview&) MOZ_DELETE;
  CameraPreview& operator=(const CameraPreview&) MOZ_DELETE;
};

} // namespace mozilla

#endif // DOM_CAMERA_CAMERAPREVIEW_H
