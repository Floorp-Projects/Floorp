/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "MockedSocketTransport.h"

using namespace mozilla::net;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(MockedSocketTransport, nsISocketTransport, nsITransport)

// static
already_AddRefed<MockedSocketTransport> MockedSocketTransport::Create() {
  RefPtr<MockedSocketTransport> transport = new MockedSocketTransport();
  return transport.forget();
}

NS_IMETHODIMP
MockedSocketTransport::SetKeepaliveEnabled(bool) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::SetKeepaliveVals(int32_t, int32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetSecurityCallbacks(nsIInterfaceRequestor**) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::SetSecurityCallbacks(nsIInterfaceRequestor*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::OpenInputStream(uint32_t, uint32_t, uint32_t,
                                       nsIInputStream**) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::OpenOutputStream(uint32_t, uint32_t, uint32_t,
                                        nsIOutputStream**) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::Close(nsresult) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
MockedSocketTransport::SetEventSink(nsITransportEventSink*, nsIEventTarget*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::Bind(NetAddr*) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
MockedSocketTransport::GetFirstRetryError(nsresult*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetEchConfigUsed(bool*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::SetEchConfig(const nsACString&) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::ResolvedByTRR(bool*) { return NS_ERROR_NOT_IMPLEMENTED; }

nsresult MockedSocketTransport::GetOriginAttributes(
    mozilla::OriginAttributes*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetKeepaliveEnabled(bool*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetSendBufferSize(uint32_t*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::SetSendBufferSize(uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetPort(int32_t*) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
MockedSocketTransport::GetPeerAddr(mozilla::net::NetAddr*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetSelfAddr(mozilla::net::NetAddr*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetScriptablePeerAddr(nsINetAddr**) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetScriptableSelfAddr(nsINetAddr**) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetSecurityInfo(nsISupports**) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::IsAlive(bool*) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
MockedSocketTransport::GetConnectionFlags(uint32_t*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::SetConnectionFlags(uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetTlsFlags(uint32_t*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::SetTlsFlags(uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetRecvBufferSize(uint32_t*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::SetRecvBufferSize(uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetResetIPFamilyPreference(bool*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult MockedSocketTransport::SetOriginAttributes(
    const mozilla::OriginAttributes&) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetScriptableOriginAttributes(
    JSContext* aCx, JS::MutableHandle<JS::Value>) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::SetScriptableOriginAttributes(JSContext* aCx,
                                                     JS::Handle<JS::Value>) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetHost(nsACString&) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
MockedSocketTransport::GetTimeout(uint32_t, uint32_t*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::SetTimeout(uint32_t, uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::SetReuseAddrPort(bool) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::SetLinger(bool, int16_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MockedSocketTransport::GetQoSBits(uint8_t*) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
MockedSocketTransport::SetQoSBits(uint8_t) { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
MockedSocketTransport::SetFastOpenCallback(TCPFastOpen*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace dom
}  // namespace mozilla
