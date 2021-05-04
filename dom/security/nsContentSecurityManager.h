/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentSecurityManager_h___
#define nsContentSecurityManager_h___

#include "nsIContentSecurityManager.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"

class nsILoadInfo;
class nsIStreamListener;

#define NS_CONTENTSECURITYMANAGER_CONTRACTID \
  "@mozilla.org/contentsecuritymanager;1"
// cdcc1ab8-3cea-4e6c-a294-a651fa35227f
#define NS_CONTENTSECURITYMANAGER_CID                \
  {                                                  \
    0xcdcc1ab8, 0x3cea, 0x4e6c, {                    \
      0xa2, 0x94, 0xa6, 0x51, 0xfa, 0x35, 0x22, 0x7f \
    }                                                \
  }

class nsContentSecurityManager : public nsIContentSecurityManager,
                                 public nsIChannelEventSink {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTSECURITYMANAGER
  NS_DECL_NSICHANNELEVENTSINK

  nsContentSecurityManager() = default;

  static nsresult doContentSecurityCheck(
      nsIChannel* aChannel, nsCOMPtr<nsIStreamListener>& aInAndOutListener);

  static bool AllowTopLevelNavigationToDataURI(nsIChannel* aChannel);
  static bool AllowInsecureRedirectToDataURI(nsIChannel* aNewChannel);
  static void MeasureUnexpectedPrivilegedLoads(nsILoadInfo* aLoadInfo,
                                               nsIURI* aFinalURI,
                                               const nsACString& aRemoteType);

 private:
  static nsresult CheckChannel(nsIChannel* aChannel);
  static nsresult CheckFTPSubresourceLoad(nsIChannel* aChannel);
  static nsresult CheckAllowLoadInSystemPrivilegedContext(nsIChannel* aChannel);
  static nsresult CheckChannelHasProtocolSecurityFlag(nsIChannel* aChannel);

  virtual ~nsContentSecurityManager() = default;
};

#endif /* nsContentSecurityManager_h___ */
