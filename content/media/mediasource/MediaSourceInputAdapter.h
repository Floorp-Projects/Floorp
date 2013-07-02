/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASOURCEINPUTADAPTER_H_
#define MOZILLA_MEDIASOURCEINPUTADAPTER_H_

#include "nsIAsyncInputStream.h"
#include "nsCycleCollectionParticipant.h"
#include "MediaSource.h"

namespace mozilla {
namespace dom {

class MediaSourceInputAdapter MOZ_FINAL : public nsIAsyncInputStream
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(MediaSourceInputAdapter)
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  MediaSourceInputAdapter(MediaSource* aMediaSource);
  ~MediaSourceInputAdapter();

  void NotifyListener();

private:
  uint64_t Available();

  nsRefPtr<MediaSource> mMediaSource;
  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
  int64_t mOffset;
  uint32_t mNotifyThreshold;
  bool mClosed;
};

} // namespace dom
} // namespace mozilla
#endif /* MOZILLA_MEDIASOURCEINPUTADAPTER_H_ */
