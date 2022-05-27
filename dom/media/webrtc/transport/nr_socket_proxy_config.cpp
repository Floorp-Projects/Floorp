/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nr_socket_proxy_config.h"

#include "mozilla/net/WebrtcProxyConfig.h"

namespace mozilla {

class NrSocketProxyConfig::Private {
 public:
  net::WebrtcProxyConfig mProxyConfig;
};

NrSocketProxyConfig::NrSocketProxyConfig(
    const net::WebrtcProxyConfig& aProxyConfig)
    : mPrivate(new Private({aProxyConfig})) {}

NrSocketProxyConfig::NrSocketProxyConfig(NrSocketProxyConfig&& aOrig)
    : mPrivate(std::move(aOrig.mPrivate)) {}

NrSocketProxyConfig::~NrSocketProxyConfig() = default;

const net::WebrtcProxyConfig& NrSocketProxyConfig::GetConfig() const {
  return mPrivate->mProxyConfig;
}

bool NrSocketProxyConfig::GetForceProxy() const {
  return mPrivate->mProxyConfig.forceProxy();
}
}  // namespace mozilla
