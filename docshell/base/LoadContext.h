/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LoadContext_h
#define LoadContext_h

#include "SerializedLoadContext.h"
#include "mozilla/Attributes.h"
#include "mozilla/BasePrincipal.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/dom/Element.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"

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

class LoadContext final
  : public nsILoadContext
  , public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOADCONTEXT
  NS_DECL_NSIINTERFACEREQUESTOR

  // AppId/inBrowser arguments override those in SerializedLoadContext provided
  // by child process.
  LoadContext(const IPC::SerializedLoadContext& aToCopy,
              dom::Element* aTopFrameElement,
              OriginAttributes& aAttrs)
    : mTopFrameElement(do_GetWeakReference(aTopFrameElement))
    , mNestedFrameId(0)
    , mIsContent(aToCopy.mIsContent)
    , mUsePrivateBrowsing(aToCopy.mUsePrivateBrowsing)
    , mUseRemoteTabs(aToCopy.mUseRemoteTabs)
    , mOriginAttributes(aAttrs)
#ifdef DEBUG
    , mIsNotNull(aToCopy.mIsNotNull)
#endif
  {
  }

  // AppId/inBrowser arguments override those in SerializedLoadContext provided
  // by child process.
  LoadContext(const IPC::SerializedLoadContext& aToCopy,
              uint64_t aNestedFrameId,
              OriginAttributes& aAttrs)
    : mTopFrameElement(nullptr)
    , mNestedFrameId(aNestedFrameId)
    , mIsContent(aToCopy.mIsContent)
    , mUsePrivateBrowsing(aToCopy.mUsePrivateBrowsing)
    , mUseRemoteTabs(aToCopy.mUseRemoteTabs)
    , mOriginAttributes(aAttrs)
#ifdef DEBUG
    , mIsNotNull(aToCopy.mIsNotNull)
#endif
  {
  }

  LoadContext(dom::Element* aTopFrameElement,
              bool aIsContent,
              bool aUsePrivateBrowsing,
              bool aUseRemoteTabs,
              OriginAttributes& aAttrs)
    : mTopFrameElement(do_GetWeakReference(aTopFrameElement))
    , mNestedFrameId(0)
    , mIsContent(aIsContent)
    , mUsePrivateBrowsing(aUsePrivateBrowsing)
    , mUseRemoteTabs(aUseRemoteTabs)
    , mOriginAttributes(aAttrs)
#ifdef DEBUG
    , mIsNotNull(true)
#endif
  {
  }

  // Constructor taking reserved appId for the safebrowsing cookie.
  explicit LoadContext(uint32_t aAppId)
    : mTopFrameElement(nullptr)
    , mNestedFrameId(0)
    , mIsContent(false)
    , mUsePrivateBrowsing(false)
    , mUseRemoteTabs(false)
    , mOriginAttributes(aAppId, false)
#ifdef DEBUG
    , mIsNotNull(true)
#endif
  {
  }

  // Constructor for creating a LoadContext with a given principal's appId and
  // browser flag.
  explicit LoadContext(nsIPrincipal* aPrincipal,
                       nsILoadContext* aOptionalBase = nullptr);

private:
  ~LoadContext() {}

  nsWeakPtr mTopFrameElement;
  uint64_t mNestedFrameId;
  bool mIsContent;
  bool mUsePrivateBrowsing;
  bool mUseRemoteTabs;
  OriginAttributes mOriginAttributes;
#ifdef DEBUG
  bool mIsNotNull;
#endif
};

} // namespace mozilla

#endif // LoadContext_h
