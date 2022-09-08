/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchImageHelper.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/Logging.h"
#include "mozilla/NullPrincipal.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"

mozilla::LazyLogModule gFetchImageLog("FetchImageHelper");

#undef LOG
#define LOG(msg, ...)                      \
  MOZ_LOG(gFetchImageLog, LogLevel::Debug, \
          ("FetchImageHelper=%p, " msg, this, ##__VA_ARGS__))

using namespace mozilla::gfx;

namespace mozilla::dom {

FetchImageHelper::FetchImageHelper(const MediaImage& aImage)
    : mSrc(aImage.mSrc) {}

FetchImageHelper::~FetchImageHelper() { AbortFetchingImage(); }

RefPtr<ImagePromise> FetchImageHelper::FetchImage() {
  MOZ_ASSERT(NS_IsMainThread());
  if (IsFetchingImage()) {
    return mPromise.Ensure(__func__);
  }

  LOG("Start fetching image from %s", NS_ConvertUTF16toUTF8(mSrc).get());
  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(uri), mSrc))) {
    LOG("Failed to create URI");
    return ImagePromise::CreateAndReject(false, __func__);
  }

  MOZ_ASSERT(!mListener);
  mListener = new ImageFetchListener();
  if (NS_FAILED(mListener->FetchDecodedImageFromURI(uri, this))) {
    LOG("Failed to decode image from async channel");
    return ImagePromise::CreateAndReject(false, __func__);
  }
  return mPromise.Ensure(__func__);
}

void FetchImageHelper::AbortFetchingImage() {
  MOZ_ASSERT(NS_IsMainThread());
  LOG("AbortFetchingImage");
  mPromise.RejectIfExists(false, __func__);
  ClearListenerIfNeeded();
}

void FetchImageHelper::ClearListenerIfNeeded() {
  if (mListener) {
    mListener->Clear();
    mListener = nullptr;
  }
}

bool FetchImageHelper::IsFetchingImage() const {
  MOZ_ASSERT(NS_IsMainThread());
  return !mPromise.IsEmpty() && mListener;
}

void FetchImageHelper::HandleFetchSuccess(imgIContainer* aImage) {
  MOZ_ASSERT(aImage);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsFetchingImage());
  LOG("Finished fetching image");
  mPromise.Resolve(aImage, __func__);
  ClearListenerIfNeeded();
}

void FetchImageHelper::HandleFetchFail() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsFetchingImage());
  LOG("Reject the promise because of fetching failed");
  mPromise.RejectIfExists(false, __func__);
  ClearListenerIfNeeded();
}

/**
 * Implementation for FetchImageHelper::ImageFetchListener
 */
NS_IMPL_ISUPPORTS(FetchImageHelper::ImageFetchListener, imgIContainerCallback)

FetchImageHelper::ImageFetchListener::~ImageFetchListener() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mHelper, "Cancel() should be called before desturction!");
}

nsresult FetchImageHelper::ImageFetchListener::FetchDecodedImageFromURI(
    nsIURI* aURI, FetchImageHelper* aHelper) {
  MOZ_ASSERT(!mHelper && !mChannel,
             "Should call Clear() berfore running another fetching process!");
  RefPtr<nsIPrincipal> nullPrincipal =
      NullPrincipal::CreateWithoutOriginAttributes();
  nsCOMPtr<nsIChannel> channel;
  nsresult rv =
      NS_NewChannel(getter_AddRefs(channel), aURI, nullPrincipal,
                    nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                    nsIContentPolicy::TYPE_INTERNAL_IMAGE, nullptr, nullptr,
                    nullptr, nullptr, nsIRequest::LOAD_ANONYMOUS);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<imgITools> imgTools = do_GetService("@mozilla.org/image/tools;1");
  if (!imgTools) {
    return NS_ERROR_FAILURE;
  }

  rv = imgTools->DecodeImageFromChannelAsync(aURI, channel, this, nullptr);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }
  MOZ_ASSERT(aHelper);
  mHelper = aHelper;
  mChannel = channel;
  return NS_OK;
}

void FetchImageHelper::ImageFetchListener::Clear() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mChannel) {
    mChannel->CancelWithReason(
        NS_BINDING_ABORTED, "FetchImageHelper::ImageFetchListener::Clear"_ns);
    mChannel = nullptr;
  }
  mHelper = nullptr;
}

bool FetchImageHelper::ImageFetchListener::IsFetchingImage() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mHelper ? mHelper->IsFetchingImage() : false;
}

NS_IMETHODIMP FetchImageHelper::ImageFetchListener::OnImageReady(
    imgIContainer* aImage, nsresult aStatus) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!IsFetchingImage()) {
    return NS_OK;
  }
  // We have received image, so we don't need the channel anymore.
  mChannel = nullptr;

  MOZ_ASSERT(mHelper);
  if (NS_FAILED(aStatus) || !aImage) {
    mHelper->HandleFetchFail();
    Clear();
    return aStatus;
  }

  mHelper->HandleFetchSuccess(aImage);

  return NS_OK;
}

}  // namespace mozilla::dom
