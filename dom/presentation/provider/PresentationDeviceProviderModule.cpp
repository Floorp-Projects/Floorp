/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayDeviceProvider.h"
#include "MulticastDNSDeviceProvider.h"
#include "mozilla/ModuleUtils.h"

#define MULTICAST_DNS_PROVIDER_CID \
  {0x814f947a, 0x52f7, 0x41c9, \
    { 0x94, 0xa1, 0x36, 0x84, 0x79, 0x72, 0x84, 0xac }}
#define DISPLAY_DEVICE_PROVIDER_CID \
  { 0x515d9879, 0xfe0b, 0x4d9f, \
    { 0x89, 0x49, 0x7f, 0xa7, 0x65, 0x6c, 0x01, 0x0e } }

#define DISPLAY_DEVICE_PROVIDER_CONTRACT_ID "@mozilla.org/presentation-device/displaydevice-provider;1"
#define MULTICAST_DNS_PROVIDER_CONTRACT_ID "@mozilla.org/presentation-device/multicastdns-provider;1"

using mozilla::dom::presentation::MulticastDNSDeviceProvider;
using mozilla::dom::presentation::DisplayDeviceProvider;

NS_GENERIC_FACTORY_CONSTRUCTOR(MulticastDNSDeviceProvider)
NS_DEFINE_NAMED_CID(MULTICAST_DNS_PROVIDER_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(DisplayDeviceProvider)
NS_DEFINE_NAMED_CID(DISPLAY_DEVICE_PROVIDER_CID);

static const mozilla::Module::CIDEntry kPresentationDeviceProviderCIDs[] = {
  { &kMULTICAST_DNS_PROVIDER_CID, false, nullptr, MulticastDNSDeviceProviderConstructor },
  { &kDISPLAY_DEVICE_PROVIDER_CID, false, nullptr, DisplayDeviceProviderConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kPresentationDeviceProviderContracts[] = {
  { MULTICAST_DNS_PROVIDER_CONTRACT_ID, &kMULTICAST_DNS_PROVIDER_CID },
  { DISPLAY_DEVICE_PROVIDER_CONTRACT_ID, &kDISPLAY_DEVICE_PROVIDER_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kPresentationDeviceProviderCategories[] = {
#if defined(MOZ_WIDGET_COCOA) || defined(MOZ_WIDGET_ANDROID) || (defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 16)
  { PRESENTATION_DEVICE_PROVIDER_CATEGORY, "MulticastDNSDeviceProvider", MULTICAST_DNS_PROVIDER_CONTRACT_ID },
#endif
#if defined(MOZ_WIDGET_GONK)
  { PRESENTATION_DEVICE_PROVIDER_CATEGORY, "DisplayDeviceProvider", DISPLAY_DEVICE_PROVIDER_CONTRACT_ID },
#endif
  { nullptr }
};

static const mozilla::Module kPresentationDeviceProviderModule = {
  mozilla::Module::kVersion,
  kPresentationDeviceProviderCIDs,
  kPresentationDeviceProviderContracts,
  kPresentationDeviceProviderCategories
};

NSMODULE_DEFN(PresentationDeviceProviderModule) = &kPresentationDeviceProviderModule;
