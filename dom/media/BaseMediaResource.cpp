#include "BaseMediaResource.h"

#include "ChannelMediaResource.h"
#include "CloneableWithRangeMediaResource.h"
#include "FileMediaResource.h"
#include "MediaContainerType.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsICloneableInputStream.h"
#include "nsIFile.h"
#include "nsIFileChannel.h"
#include "nsIInputStream.h"
#include "nsISeekableStream.h"
#include "nsNetUtil.h"

namespace mozilla {

already_AddRefed<BaseMediaResource>
BaseMediaResource::Create(MediaResourceCallback* aCallback,
                          nsIChannel* aChannel,
                          bool aIsPrivateBrowsing)
{
  NS_ASSERTION(NS_IsMainThread(),
               "MediaResource::Open called on non-main thread");

  // If the channel was redirected, we want the post-redirect URI;
  // but if the URI scheme was expanded, say from chrome: to jar:file:,
  // we want the original URI.
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsAutoCString contentTypeString;
  aChannel->GetContentType(contentTypeString);
  Maybe<MediaContainerType> containerType =
    MakeMediaContainerType(contentTypeString);
  if (!containerType) {
    return nullptr;
  }

  // Let's try to create a FileMediaResource in case the channel is a nsIFile
  nsCOMPtr<nsIFileChannel> fc = do_QueryInterface(aChannel);
  if (fc) {
    RefPtr<BaseMediaResource> resource =
      new FileMediaResource(aCallback, aChannel, uri);
    return resource.forget();
  }

  RefPtr<mozilla::dom::BlobImpl> blobImpl;
  if (IsBlobURI(uri) &&
      NS_SUCCEEDED(NS_GetBlobForBlobURI(uri, getter_AddRefs(blobImpl))) &&
      blobImpl) {
    IgnoredErrorResult rv;

    nsCOMPtr<nsIInputStream> stream;
    blobImpl->CreateInputStream(getter_AddRefs(stream), rv);
    if (NS_WARN_IF(rv.Failed())) {
      return nullptr;
    }

    // It's better to read the size from the blob instead of using ::Available,
    // because, if the stream implements nsIAsyncInputStream interface,
    // ::Available will not return the size of the stream, but what can be
    // currently read.
    uint64_t size = blobImpl->GetSize(rv);
    if (NS_WARN_IF(rv.Failed())) {
      return nullptr;
    }

    // If the URL is a blob URL, with a seekable inputStream, we can still use
    // a FileMediaResource.
    nsCOMPtr<nsISeekableStream> seekableStream = do_QueryInterface(stream);
    if (seekableStream) {
      RefPtr<BaseMediaResource> resource =
        new FileMediaResource(aCallback, aChannel, uri, size);
      return resource.forget();
    }

    // Maybe this blob URL can be cloned with a range.
    nsCOMPtr<nsICloneableInputStreamWithRange> cloneableWithRange =
      do_QueryInterface(stream);
    if (cloneableWithRange) {
      RefPtr<BaseMediaResource> resource = new CloneableWithRangeMediaResource(
        aCallback, aChannel, uri, stream, size);
      return resource.forget();
    }
  }

  RefPtr<BaseMediaResource> resource =
    new ChannelMediaResource(aCallback, aChannel, uri, aIsPrivateBrowsing);
  return resource.forget();
}

void
BaseMediaResource::SetLoadInBackground(bool aLoadInBackground)
{
  if (aLoadInBackground == mLoadInBackground) {
    return;
  }
  mLoadInBackground = aLoadInBackground;
  if (!mChannel) {
    // No channel, resource is probably already loaded.
    return;
  }

  MediaDecoderOwner* owner = mCallback->GetMediaOwner();
  if (!owner) {
    NS_WARNING("Null owner in MediaResource::SetLoadInBackground()");
    return;
  }
  dom::HTMLMediaElement* element = owner->GetMediaElement();
  if (!element) {
    NS_WARNING("Null element in MediaResource::SetLoadInBackground()");
    return;
  }

  bool isPending = false;
  if (NS_SUCCEEDED(mChannel->IsPending(&isPending)) && isPending) {
    nsLoadFlags loadFlags;
    DebugOnly<nsresult> rv = mChannel->GetLoadFlags(&loadFlags);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetLoadFlags() failed!");

    if (aLoadInBackground) {
      loadFlags |= nsIRequest::LOAD_BACKGROUND;
    } else {
      loadFlags &= ~nsIRequest::LOAD_BACKGROUND;
    }
    ModifyLoadFlags(loadFlags);
  }
}

void
BaseMediaResource::ModifyLoadFlags(nsLoadFlags aFlags)
{
  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = mChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  MOZ_ASSERT(NS_SUCCEEDED(rv), "GetLoadGroup() failed!");

  nsresult status;
  mChannel->GetStatus(&status);

  bool inLoadGroup = false;
  if (loadGroup) {
    rv = loadGroup->RemoveRequest(mChannel, nullptr, status);
    if (NS_SUCCEEDED(rv)) {
      inLoadGroup = true;
    }
  }

  rv = mChannel->SetLoadFlags(aFlags);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "SetLoadFlags() failed!");

  if (inLoadGroup) {
    rv = loadGroup->AddRequest(mChannel, nullptr);
    MOZ_ASSERT(NS_SUCCEEDED(rv), "AddRequest() failed!");
  }
}

} // namespace mozilla
