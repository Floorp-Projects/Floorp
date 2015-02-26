/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginPlayPreviewInfo_h_
#define nsPluginPlayPreviewInfo_h_

#include "nsString.h"
#include "nsIPluginHost.h"

class nsPluginPlayPreviewInfo : public nsIPluginPlayPreviewInfo
{
  virtual ~nsPluginPlayPreviewInfo();

public:
   NS_DECL_ISUPPORTS
   NS_DECL_NSIPLUGINPLAYPREVIEWINFO

  nsPluginPlayPreviewInfo(const char* aMimeType,
                          bool aIgnoreCTP,
                          const char* aRedirectURL,
                          const char* aWhitelist);
  explicit nsPluginPlayPreviewInfo(const nsPluginPlayPreviewInfo* aSource);

  /** This function checks aPageURI and aObjectURI against the whitelist
   *  specified in aWhitelist. This is public static function because this
   *  whitelist checking code needs to be accessed without any instances of
   *  nsIPluginPlayPreviewInfo. In particular, the Shumway whitelist is
   *  obtained directly from prefs and compared using this code for telemetry
   *  purposes.
   */
  static nsresult CheckWhitelist(const nsACString& aPageURI,
                                 const nsACString& aObjectURI,
                                 const nsACString& aWhitelist,
                                 bool *_retval);

  nsCString mMimeType;
  bool      mIgnoreCTP;
  nsCString mRedirectURL;
  nsCString mWhitelist;
};


#endif // nsPluginPlayPreviewInfo_h_
