/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LoadContext_h
#define LoadContext_h

#include "SerializedLoadContext.h"
#include "mozilla/Attributes.h"
#include "nsWeakReference.h"

namespace mozilla {

/**
 * Class that provides nsILoadContext info in Parent process.  Typically copied
 * from Child via SerializedLoadContext.
 *
 * Note: this is not the "normal" or "original" nsILoadContext.  That is
 * typically provided by nsDocShell.  This is only used when the original
 * docshell is in a different process and we need to copy certain values from
 * it.
 */

class LoadContext MOZ_FINAL : public nsILoadContext
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOADCONTEXT

  LoadContext(const IPC::SerializedLoadContext& aToCopy)
    : mIsNotNull(aToCopy.mIsNotNull)
    , mIsContent(aToCopy.mIsContent)
    , mUsePrivateBrowsing(aToCopy.mUsePrivateBrowsing)
    , mIsInBrowserElement(aToCopy.mIsInBrowserElement)
    , mAppId(aToCopy.mAppId)
  {}

  LoadContext(const IPC::SerializedLoadContext& aToCopy,
              nsIDOMElement* aTopFrameElemenet);

private:
  bool          mIsNotNull;
  bool          mIsContent;
  bool          mUsePrivateBrowsing;
  bool          mIsInBrowserElement;
  uint32_t      mAppId;
  nsWeakPtr     mTopFrameElement;
};

} // namespace mozilla

#endif // LoadContext_h

