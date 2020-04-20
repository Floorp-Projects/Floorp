/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIACONTROL_FETCHIMAGEHELPER_H_
#define DOM_MEDIA_MEDIACONTROL_FETCHIMAGEHELPER_H_

#include "imgIContainer.h"
#include "imgITools.h"
#include "imgINotificationObserver.h"
#include "mozilla/dom/MediaSessionBinding.h"
#include "mozilla/MozPromise.h"

namespace mozilla {
namespace dom {
/**
 * FetchImageHelper is used to fetch image data from MediaImage, and the fetched
 * image data would be used to show on the virtual control inferface. The URL of
 * MediaImage is defined by websites by using MediaSession API [1].
 *
 * By using `FetchImage()`, it would return a promise that would resolve with a
 * `DataSourceSurface`, then we can get the decoded image data from the surface.
 *
 * [1] https://w3c.github.io/mediasession/#dictdef-mediaimage
 */
using ImagePromise = MozPromise<RefPtr<mozilla::gfx::DataSourceSurface>, bool,
                                /* IsExclusive = */ true>;
class FetchImageHelper final {
 public:
  NS_INLINE_DECL_REFCOUNTING(FetchImageHelper)

  explicit FetchImageHelper(const MediaImage& aImage);

  // Return a promise which would be resolved with the decoded image surface
  // when we finish fetching and decoding image data, and it would be rejected
  // when we fail to fecth the image.
  RefPtr<ImagePromise> FetchImage();

  // Stop fetching and decoding image and reject the image promise. If we have
  // not started yet fetching image, then nothing would happen.
  void AbortFetchingImage();

  // Return true if we're fecthing image.
  bool IsFetchingImage() const;

 private:
  /**
   * ImageFetchListener is used to listen the notification of finishing fetching
   * image data (via `OnImageReady()`) and finishing decoding image data (via
   * `Notify()`).
   */
  class ImageFetchListener final : public imgIContainerCallback,
                                   public imgINotificationObserver {
   public:
    NS_DECL_ISUPPORTS
    ImageFetchListener() = default;

    // Start an async channel to load the image, and return error if the channel
    // opens failed. It would use `aHelper::HandleFetchSuccess/Fail()` to notify
    // the result asynchronously.
    nsresult FetchDecodedImageFromURI(nsIURI* aURI, FetchImageHelper* aHelper);
    void Clear();
    bool IsFetchingImage() const;

    // Methods of imgIContainerCallback and imgINotificationObserver
    NS_IMETHOD OnImageReady(imgIContainer* aImage, nsresult aStatus) override;
    void Notify(imgIRequest* aRequest, int32_t aType,
                const nsIntRect* aData) override;

   private:
    ~ImageFetchListener();

    FetchImageHelper* MOZ_NON_OWNING_REF mHelper = nullptr;
    nsCOMPtr<nsIChannel> mChannel;
    nsCOMPtr<imgIContainer> mImage;
  };

  ~FetchImageHelper();

  void ClearListenerIfNeeded();
  void HandleFetchSuccess(imgIContainer* aImage);
  void HandleFetchFail();

  nsString mSrc;
  MozPromiseHolder<ImagePromise> mPromise;
  RefPtr<ImageFetchListener> mListener;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_MEDIA_MEDIACONTROL_FETCHIMAGEHELPER_H_
