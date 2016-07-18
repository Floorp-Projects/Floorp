/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __TCPDeviceInfo_h__
#define __TCPDeviceInfo_h__

namespace mozilla {
namespace dom {
namespace presentation {

class TCPDeviceInfo final : public nsITCPDeviceInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITCPDEVICEINFO

  explicit TCPDeviceInfo(const nsACString& aId,
                         const nsACString& aAddress,
                         const uint16_t aPort,
                         const nsACString& aCertFingerprint)
    : mId(aId)
    , mAddress(aAddress)
    , mPort(aPort)
    , mCertFingerprint(aCertFingerprint)
  {
  }

private:
  virtual ~TCPDeviceInfo() {}

  nsCString mId;
  nsCString mAddress;
  uint16_t mPort;
  nsCString mCertFingerprint;
};

NS_IMPL_ISUPPORTS(TCPDeviceInfo,
                  nsITCPDeviceInfo)

// nsITCPDeviceInfo
NS_IMETHODIMP
TCPDeviceInfo::GetId(nsACString& aId)
{
  aId = mId;
  return NS_OK;
}

NS_IMETHODIMP
TCPDeviceInfo::GetAddress(nsACString& aAddress)
{
  aAddress = mAddress;
  return NS_OK;
}

NS_IMETHODIMP
TCPDeviceInfo::GetPort(uint16_t* aPort)
{
  *aPort = mPort;
  return NS_OK;
}

NS_IMETHODIMP
TCPDeviceInfo::GetCertFingerprint(nsACString& aCertFingerprint)
{
  aCertFingerprint = mCertFingerprint;
  return NS_OK;
}

} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif /* !__TCPDeviceInfo_h__ */

