/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_encoders_icon_win_nsIconChannel_h
#define mozilla_image_encoders_icon_win_nsIconChannel_h

#include "mozilla/Attributes.h"
#include "mozilla/MozPromise.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsILoadInfo.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIURI.h"
#include "nsIInputStreamPump.h"
#include "nsIStreamListener.h"

namespace mozilla::ipc {
class ByteBuf;
}

class nsIconChannel final : public nsIChannel, public nsIStreamListener {
 public:
  using ByteBufPromise =
      mozilla::MozPromise<mozilla::ipc::ByteBuf, nsresult, true>;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsIconChannel();

  nsresult Init(nsIURI* uri);

  /// Obtains an icon in Windows ICO format as a ByteBuf instead
  /// of a channel. For use with IPC.
  static RefPtr<ByteBufPromise> GetIconAsync(nsIURI* aURI);

 private:
  ~nsIconChannel();

  nsresult StartAsyncOpen();

  nsCOMPtr<nsIURI> mUrl;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<nsILoadInfo> mLoadInfo;

  nsCOMPtr<nsIInputStreamPump> mPump;
  nsCOMPtr<nsIStreamListener> mListener;

  bool mCanceled = false;
};

#endif  // mozilla_image_encoders_icon_win_nsIconChannel_h
