/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* module registration and factory code. */

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "mozilla/ModuleUtils.h"
#include "nsCRT.h"
#include "nsIClassInfoImpl.h"
#include "xpctest_private.h"

#define NS_XPCTESTOBJECTREADONLY_CID \
{ 0x492609a7, 0x2582, 0x436b, \
   { 0xb0, 0xef, 0x92, 0xe2, 0x9b, 0xb9, 0xe1, 0x43 } }

#define NS_XPCTESTOBJECTREADWRITE_CID \
{ 0x8f37f760, 0x3686, 0x4dbb, \
   { 0xb1, 0x21, 0x96, 0x93, 0xba, 0x81, 0x3f, 0x8f } }

NS_GENERIC_FACTORY_CONSTRUCTOR(xpcTestObjectReadOnly)
NS_GENERIC_FACTORY_CONSTRUCTOR(xpcTestObjectReadWrite)
NS_DEFINE_NAMED_CID(NS_XPCTESTOBJECTREADONLY_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTOBJECTREADWRITE_CID);

static const mozilla::Module::CIDEntry kXPCTestCIDs[] = {
    { &kNS_XPCTESTOBJECTREADONLY_CID, false, NULL, xpcTestObjectReadOnlyConstructor },
    { &kNS_XPCTESTOBJECTREADWRITE_CID, false, NULL, xpcTestObjectReadWriteConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kXPCTestContracts[] = {
    { "@mozilla.org/js/xpc/test/native/ObjectReadOnly;1", &kNS_XPCTESTOBJECTREADONLY_CID },
    { "@mozilla.org/js/xpc/test/native/ObjectReadWrite;1", &kNS_XPCTESTOBJECTREADWRITE_CID },
    { NULL }
};

static const mozilla::Module kXPCTestModule = {
    mozilla::Module::kVersion,
    kXPCTestCIDs,
    kXPCTestContracts
};

NSMODULE_DEFN(xpconnect_test) = &kXPCTestModule;
