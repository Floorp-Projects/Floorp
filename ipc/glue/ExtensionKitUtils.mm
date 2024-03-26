/* -*- Mode: c++; c-basic-offset: 2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionKitUtils.h"
#include "LaunchError.h"

#import <BrowserEngineKit/BrowserEngineKit.h>

namespace mozilla::ipc {

void BEProcessCapabilityGrantDeleter::operator()(void* aGrant) const {
  auto grant = static_cast<id<BEProcessCapabilityGrant>>(aGrant);
  [grant invalidate];
  [grant release];
}

void ExtensionKitProcess::StartProcess(
    Kind aKind,
    const std::function<void(Result<ExtensionKitProcess, LaunchError>&&)>&
        aCompletion) {
  auto callCompletion = [=](auto* aProcess, NSError* aError) {
    if (aProcess) {
      [aProcess retain];
      aCompletion(ExtensionKitProcess(aKind, aProcess));
    } else {
      NSLog(@"Error launching process, description '%@', reason '%@'",
            [aError localizedDescription], [aError localizedFailureReason]);
      aCompletion(Err(LaunchError("ExtensionKitProcess::StartProcess")));
    }
  };

  switch (aKind) {
    case Kind::WebContent: {
      [BEWebContentProcess
          webContentProcessWithInterruptionHandler:^{
          }
          completion:^(BEWebContentProcess* process, NSError* error) {
            callCompletion(process, error);
          }];
      return;
    }
    case Kind::Networking: {
      [BENetworkingProcess
          networkProcessWithInterruptionHandler:^{
          }
          completion:^(BENetworkingProcess* process, NSError* error) {
            callCompletion(process, error);
          }];
      return;
    }
    case Kind::Rendering: {
      [BERenderingProcess
          renderingProcessWithInterruptionHandler:^{
          }
          completion:^(BERenderingProcess* process, NSError* error) {
            callCompletion(process, error);
          }];
      return;
    }
  }
}

template <typename F>
static void SwitchObject(ExtensionKitProcess::Kind aKind, void* aProcessObject,
                         F&& aMatcher) {
  switch (aKind) {
    case ExtensionKitProcess::Kind::WebContent:
      aMatcher(static_cast<BEWebContentProcess*>(aProcessObject));
      break;
    case ExtensionKitProcess::Kind::Networking:
      aMatcher(static_cast<BENetworkingProcess*>(aProcessObject));
      break;
    case ExtensionKitProcess::Kind::Rendering:
      aMatcher(static_cast<BERenderingProcess*>(aProcessObject));
      break;
  }
}

DarwinObjectPtr<xpc_connection_t> ExtensionKitProcess::MakeLibXPCConnection() {
  NSError* error = nullptr;
  DarwinObjectPtr<xpc_connection_t> xpcConnection;
  SwitchObject(mKind, mProcessObject, [&](auto* aProcessObject) {
    xpcConnection = [aProcessObject makeLibXPCConnectionError:&error];
  });
  return xpcConnection;
}

void ExtensionKitProcess::Invalidate() {
  SwitchObject(mKind, mProcessObject,
               [&](auto* aProcessObject) { [aProcessObject invalidate]; });
}

UniqueBEProcessCapabilityGrant
ExtensionKitProcess::GrantForegroundCapability() {
  NSError* error = nullptr;
  BEProcessCapability* cap = [BEProcessCapability foreground];
  id<BEProcessCapabilityGrant> grant = nil;
  SwitchObject(mKind, mProcessObject, [&](auto* aProcessObject) {
    grant = [aProcessObject grantCapability:cap error:&error];
  });
  return UniqueBEProcessCapabilityGrant(grant ? [grant retain] : nil);
}

ExtensionKitProcess::ExtensionKitProcess(const ExtensionKitProcess& aOther)
    : mKind(aOther.mKind), mProcessObject(aOther.mProcessObject) {
  SwitchObject(mKind, mProcessObject,
               [&](auto* aProcessObject) { [aProcessObject retain]; });
}

ExtensionKitProcess& ExtensionKitProcess::operator=(
    const ExtensionKitProcess& aOther) {
  Kind oldKind = std::exchange(mKind, aOther.mKind);
  void* oldProcessObject = std::exchange(mProcessObject, aOther.mProcessObject);
  SwitchObject(mKind, mProcessObject,
               [&](auto* aProcessObject) { [aProcessObject retain]; });
  SwitchObject(oldKind, oldProcessObject,
               [&](auto* aProcessObject) { [aProcessObject release]; });
  return *this;
}

ExtensionKitProcess::~ExtensionKitProcess() {
  SwitchObject(mKind, mProcessObject,
               [&](auto* aProcessObject) { [aProcessObject release]; });
}

}  // namespace mozilla::ipc
