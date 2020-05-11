/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_css_StreamLoader_h
#define mozilla_css_StreamLoader_h

#include "nsIStreamListener.h"
#include "nsString.h"
#include "mozilla/css/SheetLoadData.h"
#include "mozilla/Assertions.h"
#include "mozilla/PreloaderBase.h"

class nsIInputStream;

namespace mozilla {
namespace css {

class StreamLoader : public PreloaderBase, public nsIStreamListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  // PreloaderBase
  static void PrioritizeAsPreload(nsIChannel* aChannel);
  virtual void PrioritizeAsPreload() override;

  explicit StreamLoader(SheetLoadData&);

  void ChannelOpenFailed() {
#ifdef NIGHTLY_BUILD
    mChannelOpenFailed = true;
#endif
  }

 private:
  virtual ~StreamLoader();

  /**
   * callback method used for ReadSegments
   */
  static nsresult WriteSegmentFun(nsIInputStream*, void*, const char*, uint32_t,
                                  uint32_t, uint32_t*);

  void HandleBOM();

  RefPtr<SheetLoadData> mSheetLoadData;
  nsresult mStatus;
  Maybe<const Encoding*> mEncodingFromBOM;

  // We store the initial three bytes of the stream into mBOMBytes, and then
  // use that buffer to detect a BOM. We then shift any non-BOM bytes into
  // mBytes, and store all subsequent data in that buffer.
  nsCString mBytes;
  nsAutoCStringN<3> mBOMBytes;

#ifdef NIGHTLY_BUILD
  bool mChannelOpenFailed = false;
  bool mOnStopRequestCalled = false;
#endif
};

}  // namespace css
}  // namespace mozilla

#endif  // mozilla_css_StreamLoader_h
