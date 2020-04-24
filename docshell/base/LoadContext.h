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
 */

class LoadContext final : public nsILoadContext, public nsIInterfaceRequestor {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOADCONTEXT
  NS_DECL_NSIINTERFACEREQUESTOR

  LoadContext(const IPC::SerializedLoadContext& aToCopy,
              dom::Element* aTopFrameElement, OriginAttributes& aAttrs)
      : mTopFrameElement(do_GetWeakReference(aTopFrameElement)),
        mNestedFrameId(0),
        mIsContent(aToCopy.mIsContent),
        mUseRemoteTabs(aToCopy.mUseRemoteTabs),
        mUseRemoteSubframes(aToCopy.mUseRemoteSubframes),
        mUseTrackingProtection(aToCopy.mUseTrackingProtection),
#ifdef DEBUG
        mIsNotNull(aToCopy.mIsNotNull),
#endif
        mOriginAttributes(aAttrs) {
  }

  LoadContext(const IPC::SerializedLoadContext& aToCopy,
              uint64_t aNestedFrameId, OriginAttributes& aAttrs)
      : mTopFrameElement(nullptr),
        mNestedFrameId(aNestedFrameId),
        mIsContent(aToCopy.mIsContent),
        mUseRemoteTabs(aToCopy.mUseRemoteTabs),
        mUseRemoteSubframes(aToCopy.mUseRemoteSubframes),
        mUseTrackingProtection(aToCopy.mUseTrackingProtection),
#ifdef DEBUG
        mIsNotNull(aToCopy.mIsNotNull),
#endif
        mOriginAttributes(aAttrs) {
  }

  LoadContext(dom::Element* aTopFrameElement, bool aIsContent,
              bool aUsePrivateBrowsing, bool aUseRemoteTabs,
              bool aUseRemoteSubframes, bool aUseTrackingProtection,
              const OriginAttributes& aAttrs)
      : mTopFrameElement(do_GetWeakReference(aTopFrameElement)),
        mNestedFrameId(0),
        mIsContent(aIsContent),
        mUseRemoteTabs(aUseRemoteTabs),
        mUseRemoteSubframes(aUseRemoteSubframes),
        mUseTrackingProtection(aUseTrackingProtection),
#ifdef DEBUG
        mIsNotNull(true),
#endif
        mOriginAttributes(aAttrs) {
    MOZ_DIAGNOSTIC_ASSERT(aUsePrivateBrowsing ==
                          (aAttrs.mPrivateBrowsingId > 0));
  }

  // Constructor taking reserved origin attributes.
  explicit LoadContext(OriginAttributes& aAttrs)
      : mTopFrameElement(nullptr),
        mNestedFrameId(0),
        mIsContent(false),
        mUseRemoteTabs(false),
        mUseRemoteSubframes(false),
        mUseTrackingProtection(false),
#ifdef DEBUG
        mIsNotNull(true),
#endif
        mOriginAttributes(aAttrs) {
  }

  // Constructor for creating a LoadContext with a given browser flag.
  explicit LoadContext(nsIPrincipal* aPrincipal,
                       nsILoadContext* aOptionalBase = nullptr);

 private:
  ~LoadContext() {}

  nsWeakPtr mTopFrameElement;
  uint64_t mNestedFrameId;
  bool mIsContent;
  bool mUseRemoteTabs;
  bool mUseRemoteSubframes;
  bool mUseTrackingProtection;
#ifdef DEBUG
  bool mIsNotNull;
#endif
  OriginAttributes mOriginAttributes;
};

already_AddRefed<nsILoadContext> CreateLoadContext();
already_AddRefed<nsILoadContext> CreatePrivateLoadContext();

}  // namespace mozilla

#endif  // LoadContext_h
