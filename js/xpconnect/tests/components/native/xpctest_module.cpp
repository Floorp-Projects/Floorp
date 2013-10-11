/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* module registration and factory code. */

#include "mozilla/ModuleUtils.h"
#include "xpctest_private.h"

#define NS_XPCTESTOBJECTREADONLY_CID                                          \
{ 0x492609a7, 0x2582, 0x436b,                                                 \
   { 0xb0, 0xef, 0x92, 0xe2, 0x9b, 0xb9, 0xe1, 0x43 } }

#define NS_XPCTESTOBJECTREADWRITE_CID                                         \
{ 0x8f37f760, 0x3686, 0x4dbb,                                                 \
   { 0xb1, 0x21, 0x96, 0x93, 0xba, 0x81, 0x3f, 0x8f } }

#define NS_XPCTESTPARAMS_CID                                                  \
{ 0x1f11076a, 0x0fa2, 0x4f07,                                                 \
    { 0xb4, 0x7a, 0xa1, 0x54, 0x31, 0xf2, 0xce, 0xf7 } }

NS_GENERIC_FACTORY_CONSTRUCTOR(xpcTestObjectReadOnly)
NS_GENERIC_FACTORY_CONSTRUCTOR(xpcTestObjectReadWrite)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPCTestParams)
NS_DEFINE_NAMED_CID(NS_XPCTESTOBJECTREADONLY_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTOBJECTREADWRITE_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTPARAMS_CID);

static const mozilla::Module::CIDEntry kXPCTestCIDs[] = {
    { &kNS_XPCTESTOBJECTREADONLY_CID, false, nullptr, xpcTestObjectReadOnlyConstructor },
    { &kNS_XPCTESTOBJECTREADWRITE_CID, false, nullptr, xpcTestObjectReadWriteConstructor },
    { &kNS_XPCTESTPARAMS_CID, false, nullptr, nsXPCTestParamsConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kXPCTestContracts[] = {
    { "@mozilla.org/js/xpc/test/native/ObjectReadOnly;1", &kNS_XPCTESTOBJECTREADONLY_CID },
    { "@mozilla.org/js/xpc/test/native/ObjectReadWrite;1", &kNS_XPCTESTOBJECTREADWRITE_CID },
    { "@mozilla.org/js/xpc/test/native/Params;1", &kNS_XPCTESTPARAMS_CID },
    { nullptr }
};

static const mozilla::Module kXPCTestModule = {
    mozilla::Module::kVersion,
    kXPCTestCIDs,
    kXPCTestContracts
};

NSMODULE_DEFN(xpconnect_test) = &kXPCTestModule;
