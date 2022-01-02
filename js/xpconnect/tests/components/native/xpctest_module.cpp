/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* module registration and factory code. */

#include "xpctest_private.h"

#define NS_XPCTESTOBJECTREADONLY_CID                 \
  {                                                  \
    0x492609a7, 0x2582, 0x436b, {                    \
      0xb0, 0xef, 0x92, 0xe2, 0x9b, 0xb9, 0xe1, 0x43 \
    }                                                \
  }

#define NS_XPCTESTOBJECTREADWRITE_CID                \
  {                                                  \
    0x8f37f760, 0x3686, 0x4dbb, {                    \
      0xb1, 0x21, 0x96, 0x93, 0xba, 0x81, 0x3f, 0x8f \
    }                                                \
  }

#define NS_XPCTESTPARAMS_CID                         \
  {                                                  \
    0x1f11076a, 0x0fa2, 0x4f07, {                    \
      0xb4, 0x7a, 0xa1, 0x54, 0x31, 0xf2, 0xce, 0xf7 \
    }                                                \
  }

#define NS_XPCTESTRETURNCODEPARENT_CID               \
  {                                                  \
    0x3818f744, 0x5445, 0x4e9c, {                    \
      0x9b, 0xb8, 0x64, 0x62, 0xfe, 0x81, 0xb6, 0x19 \
    }                                                \
  }

#define NS_XPCTESTCENUMS_CID                         \
  {                                                  \
    0x89ba673a, 0xa987, 0xb89c, {                    \
      0x92, 0x02, 0xb9, 0xc6, 0x23, 0x38, 0x64, 0x55 \
    }                                                \
  }

NS_GENERIC_FACTORY_CONSTRUCTOR(xpcTestCEnums)
NS_GENERIC_FACTORY_CONSTRUCTOR(xpcTestObjectReadOnly)
NS_GENERIC_FACTORY_CONSTRUCTOR(xpcTestObjectReadWrite)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPCTestParams)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPCTestReturnCodeParent)
NS_DEFINE_NAMED_CID(NS_XPCTESTOBJECTREADONLY_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTOBJECTREADWRITE_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTPARAMS_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTRETURNCODEPARENT_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTCENUMS_CID);

static const mozilla::Module::CIDEntry kXPCTestCIDs[] = {
    {&kNS_XPCTESTOBJECTREADONLY_CID, false, nullptr,
     xpcTestObjectReadOnlyConstructor},
    {&kNS_XPCTESTOBJECTREADWRITE_CID, false, nullptr,
     xpcTestObjectReadWriteConstructor},
    {&kNS_XPCTESTPARAMS_CID, false, nullptr, nsXPCTestParamsConstructor},
    {&kNS_XPCTESTRETURNCODEPARENT_CID, false, nullptr,
     nsXPCTestReturnCodeParentConstructor},
    {&kNS_XPCTESTCENUMS_CID, false, nullptr, xpcTestCEnumsConstructor},
    {nullptr}};

static const mozilla::Module::ContractIDEntry kXPCTestContracts[] = {
    {"@mozilla.org/js/xpc/test/native/CEnums;1", &kNS_XPCTESTCENUMS_CID},
    {"@mozilla.org/js/xpc/test/native/ObjectReadOnly;1",
     &kNS_XPCTESTOBJECTREADONLY_CID},
    {"@mozilla.org/js/xpc/test/native/ObjectReadWrite;1",
     &kNS_XPCTESTOBJECTREADWRITE_CID},
    {"@mozilla.org/js/xpc/test/native/Params;1", &kNS_XPCTESTPARAMS_CID},
    {"@mozilla.org/js/xpc/test/native/ReturnCodeParent;1",
     &kNS_XPCTESTRETURNCODEPARENT_CID},
    {nullptr}};

const mozilla::Module kXPCTestModule = {mozilla::Module::kVersion, kXPCTestCIDs,
                                        kXPCTestContracts};
