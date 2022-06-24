/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* module registration and factory code. */

#include "mozilla/GenericFactory.h"
#include "mozilla/ResultExtensions.h"
#include "nsComponentManager.h"
#include "xpctest_private.h"

template <typename T>
nsresult RegisterFactory(const char* aContractID) {
  auto constructor = [](REFNSIID aIID, void** aResult) {
    RefPtr inst = new T();
    return inst->QueryInterface(aIID, aResult);
  };

  nsCOMPtr<nsIFactory> factory = new mozilla::GenericFactory(constructor);

  nsID cid;
  MOZ_TRY(nsID::GenerateUUIDInPlace(cid));

  return nsComponentManagerImpl::gComponentManager->RegisterFactory(
      cid, aContractID, aContractID, factory);
}

nsresult xpcTestRegisterComponents() {
  MOZ_TRY(RegisterFactory<xpcTestObjectReadOnly>(
      "@mozilla.org/js/xpc/test/native/ObjectReadOnly;1"));
  MOZ_TRY(RegisterFactory<xpcTestObjectReadWrite>(
      "@mozilla.org/js/xpc/test/native/ObjectReadWrite;1"));
  MOZ_TRY(RegisterFactory<nsXPCTestParams>(
      "@mozilla.org/js/xpc/test/native/Params;1"));
  MOZ_TRY(RegisterFactory<nsXPCTestReturnCodeParent>(
      "@mozilla.org/js/xpc/test/native/ReturnCodeParent;1"));
  MOZ_TRY(RegisterFactory<xpcTestCEnums>(
      "@mozilla.org/js/xpc/test/native/CEnums;1"));

  return NS_OK;
}
