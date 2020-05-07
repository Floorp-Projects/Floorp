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
 * typically provided by BrowsingContext.  This is only used when the original
 * docshell is in a different process and we need to copy certain values from
 * it.
 */

class LoadContext final : public nsILoadContext, public nsIInterfaceRequestor {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOADCONTEXT
  NS_DECL_NSIINTERFACEREQUESTOR

  LoadContext(const IPC::SerializedLoadContext& aToCopy,
              dom::Element* aTopFrameElement, OriginAttributes& aAttrs);

  // Constructor taking reserved origin attributes.
  explicit LoadContext(OriginAttributes& aAttrs);

  // Constructor for creating a LoadContext with a given browser flag.
  explicit LoadContext(nsIPrincipal* aPrincipal,
                       nsILoadContext* aOptionalBase = nullptr);

 private:
  ~LoadContext();

  nsWeakPtr mTopFrameElement;
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
