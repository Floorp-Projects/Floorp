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
                          const char* aRedirectURL);
  explicit nsPluginPlayPreviewInfo(const nsPluginPlayPreviewInfo* aSource);

  nsCString mMimeType;
  bool      mIgnoreCTP;
  nsCString mRedirectURL;
};


#endif // nsPluginPlayPreviewInfo_h_
