/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LoadContext_h
#define LoadContext_h

#include "SerializedLoadContext.h"
#include "mozilla/Attributes.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/dom/Element.h"
#include "nsIInterfaceRequestor.h"

class mozIApplication;

namespace mozilla {

/**
 * Class that provides nsILoadContext info in Parent process.  Typically copied
 * from Child via SerializedLoadContext.
 *
 * Note: this is not the "normal" or "original" nsILoadContext.  That is
 * typically provided by nsDocShell.  This is only used when the original
 * docshell is in a different process and we need to copy certain values from
 * it.
 *
 * Note: we also generate a new nsILoadContext using LoadContext(uint32_t aAppId)
 * to separate the safebrowsing cookie.
 */

class LoadContext MOZ_FINAL : public nsILoadContext,
                              public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOADCONTEXT
  NS_DECL_NSIINTERFACEREQUESTOR

  // AppId/inBrowser arguments override those in SerializedLoadContext provided
  // by child process.
  LoadContext(const IPC::SerializedLoadContext& aToCopy,
              dom::Element* aTopFrameElement,
              uint32_t aAppId, bool aInBrowser)
    : mTopFrameElement(do_GetWeakReference(aTopFrameElement))
    , mAppId(aAppId)
    , mIsContent(aToCopy.mIsContent)
    , mUsePrivateBrowsing(aToCopy.mUsePrivateBrowsing)
    , mIsInBrowserElement(aInBrowser)
#ifdef DEBUG
    , mIsNotNull(aToCopy.mIsNotNull)
#endif
  {}

  // Constructor taking reserved appId for the safebrowsing cookie.
  LoadContext(uint32_t aAppId)
    : mTopFrameElement(nullptr)
    , mAppId(aAppId)
    , mIsContent(false)
    , mUsePrivateBrowsing(false)
    , mIsInBrowserElement(false)
#ifdef DEBUG
    , mIsNotNull(true)
#endif
  {}

private:
  nsWeakPtr     mTopFrameElement;
  uint32_t      mAppId;
  bool          mIsContent;
  bool          mUsePrivateBrowsing;
  bool          mIsInBrowserElement;
#ifdef DEBUG
  bool          mIsNotNull;
#endif
};

} // namespace mozilla

#endif // LoadContext_h

