/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_encoders_icon_win_nsIconChannel_h
#define mozilla_image_encoders_icon_win_nsIconChannel_h

#include "mozilla/Attributes.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsILoadInfo.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIURI.h"
#include "nsIInputStreamPump.h"
#include "nsIOutputStream.h"
#include "nsIStreamListener.h"
#include "nsIIconURI.h"

#include <windows.h>

class nsIFile;

class nsIconChannel final : public nsIChannel, public nsIStreamListener {
  ~nsIconChannel();

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsIconChannel();

  nsresult Init(nsIURI* uri);

 protected:
  class IconAsyncOpenTask;
  class IconSyncOpenTask;

  void OnAsyncError(nsresult aStatus);
  void FinishAsyncOpen(HICON aIcon, nsresult aStatus);
  nsresult EnsurePipeCreated(uint32_t aIconSize, bool aNonBlocking);

  nsCOMPtr<nsIURI> mUrl;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<nsILoadInfo> mLoadInfo;

  nsCOMPtr<nsIInputStreamPump> mPump;
  nsCOMPtr<nsIInputStream> mInputStream;
  nsCOMPtr<nsIOutputStream> mOutputStream;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsIEventTarget> mListenerTarget;

  nsresult ExtractIconInfoFromUrl(nsIFile** aLocalFile,
                                  uint32_t* aDesiredImageSize,
                                  nsCString& aContentType,
                                  nsCString& aFileExtension);
  nsresult GetHIcon(bool aNonBlocking, HICON* hIcon);
  nsresult GetHIconFromFile(bool aNonBlocking, HICON* hIcon);
  nsresult GetHIconFromFile(nsIFile* aLocalFile, const nsAutoString& aPath,
                            UINT aInfoFlags, HICON* hIcon);
  [[nodiscard]] nsresult MakeInputStream(nsIInputStream** _retval,
                                         bool aNonBlocking, HICON aIcon);

  // Functions specific to Vista and above
 protected:
  nsresult GetStockHIcon(nsIMozIconURI* aIconURI, HICON* hIcon);

 private:
  bool mCanceled = false;
};

#endif  // mozilla_image_encoders_icon_win_nsIconChannel_h
