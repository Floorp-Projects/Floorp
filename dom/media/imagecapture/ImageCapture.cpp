/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageCapture.h"
#include "mozilla/dom/BlobEvent.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ImageCaptureError.h"
#include "mozilla/dom/ImageCaptureErrorEvent.h"
#include "mozilla/dom/ImageCaptureErrorEventBinding.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "nsIDocument.h"
#include "CaptureTask.h"
#include "MediaEngineSource.h"

namespace mozilla {

LogModule* GetICLog()
{
  static LazyLogModule log("ImageCapture");
  return log;
}

namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(ImageCapture, DOMEventTargetHelper,
                                   mVideoStreamTrack)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImageCapture)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(ImageCapture, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ImageCapture, DOMEventTargetHelper)

ImageCapture::ImageCapture(VideoStreamTrack* aVideoStreamTrack,
                           nsPIDOMWindowInner* aOwnerWindow)
  : DOMEventTargetHelper(aOwnerWindow)
{
  MOZ_ASSERT(aOwnerWindow);
  MOZ_ASSERT(aVideoStreamTrack);

  mVideoStreamTrack = aVideoStreamTrack;
}

ImageCapture::~ImageCapture()
{
  MOZ_ASSERT(NS_IsMainThread());
}

already_AddRefed<ImageCapture>
ImageCapture::Constructor(const GlobalObject& aGlobal,
                          VideoStreamTrack& aTrack,
                          ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(aGlobal.GetAsSupports());
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<ImageCapture> object = new ImageCapture(&aTrack, win);

  return object.forget();
}

VideoStreamTrack*
ImageCapture::GetVideoStreamTrack() const
{
  return mVideoStreamTrack;
}

nsresult
ImageCapture::TakePhotoByMediaEngine()
{
  // Callback for TakPhoto(), it also monitor the principal. If principal
  // changes, it returns PHOTO_ERROR with security error.
  class TakePhotoCallback : public MediaEnginePhotoCallback,
                            public PrincipalChangeObserver<MediaStreamTrack>
  {
  public:
    TakePhotoCallback(VideoStreamTrack* aVideoTrack, ImageCapture* aImageCapture)
      : mVideoTrack(aVideoTrack)
      , mImageCapture(aImageCapture)
      , mPrincipalChanged(false)
    {
      MOZ_ASSERT(NS_IsMainThread());
      mVideoTrack->AddPrincipalChangeObserver(this);
    }

    void PrincipalChanged(MediaStreamTrack* aMediaStream) override
    {
      mPrincipalChanged = true;
    }

    nsresult PhotoComplete(already_AddRefed<Blob> aBlob) override
    {
      RefPtr<Blob> blob = aBlob;

      if (mPrincipalChanged) {
        return PhotoError(NS_ERROR_DOM_SECURITY_ERR);
      }
      return mImageCapture->PostBlobEvent(blob);
    }

    nsresult PhotoError(nsresult aRv) override
    {
      return mImageCapture->PostErrorEvent(ImageCaptureError::PHOTO_ERROR, aRv);
    }

  protected:
    ~TakePhotoCallback()
    {
      MOZ_ASSERT(NS_IsMainThread());
      mVideoTrack->RemovePrincipalChangeObserver(this);
    }

    RefPtr<VideoStreamTrack> mVideoTrack;
    RefPtr<ImageCapture> mImageCapture;
    bool mPrincipalChanged;
  };

  RefPtr<MediaEnginePhotoCallback> callback =
    new TakePhotoCallback(mVideoStreamTrack, this);
  return mVideoStreamTrack->GetSource().TakePhoto(callback);
}

void
ImageCapture::TakePhoto(ErrorResult& aResult)
{
  // According to spec, VideoStreamTrack.readyState must be "live"; however
  // gecko doesn't implement it yet (bug 910249). Instead of readyState, we
  // check VideoStreamTrack.enable before bug 910249 is fixed.
  // The error code should be INVALID_TRACK, but spec doesn't define it in
  // ImageCaptureError. So it returns PHOTO_ERROR here before spec updates.
  if (!mVideoStreamTrack->Enabled()) {
    PostErrorEvent(ImageCaptureError::PHOTO_ERROR, NS_ERROR_FAILURE);
    return;
  }

  // Try if MediaEngine supports taking photo.
  nsresult rv = TakePhotoByMediaEngine();

  // It falls back to MediaStreamGraph image capture if MediaEngine doesn't
  // support TakePhoto().
  if (rv == NS_ERROR_NOT_IMPLEMENTED) {
    IC_LOG("MediaEngine doesn't support TakePhoto(), it falls back to MediaStreamGraph.");
    RefPtr<CaptureTask> task = new CaptureTask(this);

    // It adds itself into MediaStreamGraph, so ImageCapture doesn't need to hold
    // the reference.
    task->AttachTrack();
  }
}

nsresult
ImageCapture::PostBlobEvent(Blob* aBlob)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!CheckPrincipal()) {
    // Media is not same-origin, don't allow the data out.
    return PostErrorEvent(ImageCaptureError::PHOTO_ERROR, NS_ERROR_DOM_SECURITY_ERR);
  }

  BlobEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mData = aBlob;

  RefPtr<BlobEvent> blob_event =
    BlobEvent::Constructor(this, NS_LITERAL_STRING("photo"), init);

  return DispatchTrustedEvent(blob_event);
}

nsresult
ImageCapture::PostErrorEvent(uint16_t aErrorCode, nsresult aReason)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = CheckInnerWindowCorrectness();
  NS_ENSURE_SUCCESS(rv, rv);

  nsString errorMsg;
  if (NS_FAILED(aReason)) {
    nsCString name, message;
    rv = NS_GetNameAndMessageForDOMNSResult(aReason, name, message);
    if (NS_SUCCEEDED(rv)) {
      CopyASCIItoUTF16(message, errorMsg);
    }
  }

  RefPtr<ImageCaptureError> error =
    new ImageCaptureError(this, aErrorCode, errorMsg);

  ImageCaptureErrorEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mImageCaptureError = error;

  RefPtr<Event> event =
    ImageCaptureErrorEvent::Constructor(this, NS_LITERAL_STRING("error"), init);

  return DispatchTrustedEvent(event);
}

bool
ImageCapture::CheckPrincipal()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPrincipal> principal = mVideoStreamTrack->GetPrincipal();

  if (!GetOwner()) {
    return false;
  }
  nsCOMPtr<nsIDocument> doc = GetOwner()->GetExtantDoc();
  if (!doc || !principal) {
    return false;
  }

  bool subsumes;
  if (NS_FAILED(doc->NodePrincipal()->Subsumes(principal, &subsumes))) {
    return false;
  }

  return subsumes;
}

} // namespace dom
} // namespace mozilla
