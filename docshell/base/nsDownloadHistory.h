/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsDownloadHistory_h__
#define __nsDownloadHistory_h__

#include "nsIDownloadHistory.h"
#include "mozilla/Attributes.h"

#define NS_DOWNLOADHISTORY_CID \
  {0x2ee83680, 0x2af0, 0x4bcb, {0xbf, 0xa0, 0xc9, 0x70, 0x5f, 0x65, 0x54, 0xf1}}

class nsDownloadHistory MOZ_FINAL : public nsIDownloadHistory
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOWNLOADHISTORY

  NS_DEFINE_STATIC_CID_ACCESSOR(NS_DOWNLOADHISTORY_CID)

private:
  ~nsDownloadHistory() {}
};

#endif // __nsDownloadHistory_h__
