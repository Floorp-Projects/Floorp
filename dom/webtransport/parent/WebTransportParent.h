/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_PARENT_WEBTRANSPORTPARENT_H_
#define DOM_WEBTRANSPORT_PARENT_WEBTRANSPORTPARENT_H_

#include "ErrorList.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/PWebTransportParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "nsISupports.h"
#include "nsIPrincipal.h"
#include "nsIWebTransport.h"

namespace mozilla::dom {

enum class WebTransportReliabilityMode : uint8_t;

class WebTransportParent : public PWebTransportParent,
                           public WebTransportSessionEventListener {
 public:
  WebTransportParent() = default;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_WEBTRANSPORTSESSIONEVENTLISTENER

  void Create(
      const nsAString& aURL, nsIPrincipal* aPrincipal, const bool& aDedicated,
      const bool& aRequireUnreliable, const uint32_t& aCongestionControl,
      // Sequence<WebTransportHash>* aServerCertHashes,
      Endpoint<PWebTransportParent>&& aParentEndpoint,
      std::function<void(Tuple<const nsresult&, const uint8_t&>)>&& aResolver);

  mozilla::ipc::IPCResult RecvClose(const uint32_t& aCode,
                                    const nsACString& aReason);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  bool IsClosed() const { return mClosed; }

 protected:
  virtual ~WebTransportParent();

 private:
  using ResolveType = Tuple<const nsresult&, const uint8_t&>;
  std::function<void(ResolveType)> mResolver;
  FlippedOnce<false> mClosed;
  nsCOMPtr<nsIWebTransport> mWebTransport;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
};

}  // namespace mozilla::dom

#endif  // DOM_WEBTRANSPORT_PARENT_WEBTRANSPORTPARENT_H_
