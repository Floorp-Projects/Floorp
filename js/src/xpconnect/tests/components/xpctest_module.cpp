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

#include "xpctest_private.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "mozilla/ModuleUtils.h"
#include "nsCRT.h"
#include "nsIClassInfoImpl.h"

NS_DEFINE_NAMED_CID(NS_ECHO_CID);
NS_DEFINE_NAMED_CID(NS_CHILD_CID);
NS_DEFINE_NAMED_CID(NS_NOISY_CID);
NS_DEFINE_NAMED_CID(NS_STRING_TEST_CID);
NS_DEFINE_NAMED_CID(NS_OVERLOADED_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTOBJECTREADONLY_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTOBJECTREADWRITE_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTIN_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTOUT_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTINOUT_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTCONST_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTCALLJS_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTPARENTONE_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTPARENTTWO_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTCHILD2_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTCHILD3_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTCHILD4_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTCHILD5_CID);
NS_DEFINE_NAMED_CID(NS_ARRAY_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTDOMSTRING_CID);
NS_DEFINE_NAMED_CID(NS_XPCTESTVARIANT_CID);

static const mozilla::Module::CIDEntry kXPCTestCIDs[] = {
    { &kNS_ECHO_CID, false, NULL, xpctest::ConstructEcho },
    { &kNS_CHILD_CID, false, NULL, xpctest::ConstructChild },
    { &kNS_NOISY_CID, false, NULL, xpctest::ConstructNoisy },
    { &kNS_STRING_TEST_CID, false, NULL, xpctest::ConstructStringTest },
    { &kNS_OVERLOADED_CID, false, NULL, xpctest::ConstructOverloaded },
    { &kNS_XPCTESTOBJECTREADONLY_CID, false, NULL, xpctest::ConstructXPCTestObjectReadOnly },
    { &kNS_XPCTESTOBJECTREADWRITE_CID, false, NULL, xpctest::ConstructXPCTestObjectReadWrite },
    { &kNS_XPCTESTIN_CID, false, NULL, xpctest::ConstructXPCTestIn },
    { &kNS_XPCTESTOUT_CID, false, NULL, xpctest::ConstructXPCTestOut },
    { &kNS_XPCTESTINOUT_CID, false, NULL, xpctest::ConstructXPCTestInOut },
    { &kNS_XPCTESTCONST_CID, false, NULL, xpctest::ConstructXPCTestConst },
    { &kNS_XPCTESTCALLJS_CID, false, NULL, xpctest::ConstructXPCTestCallJS },
    { &kNS_XPCTESTPARENTONE_CID, false, NULL, xpctest::ConstructXPCTestParentOne },
    { &kNS_XPCTESTPARENTTWO_CID, false, NULL, xpctest::ConstructXPCTestParentTwo },
    { &kNS_XPCTESTCHILD2_CID, false, NULL, xpctest::ConstructXPCTestChild2 },
    { &kNS_XPCTESTCHILD3_CID, false, NULL, xpctest::ConstructXPCTestChild3 },
    { &kNS_XPCTESTCHILD4_CID, false, NULL, xpctest::ConstructXPCTestChild4 },
    { &kNS_XPCTESTCHILD5_CID, false, NULL, xpctest::ConstructXPCTestChild5 },
    { &kNS_ARRAY_CID, false, NULL, xpctest::ConstructArrayTest },
    { &kNS_XPCTESTDOMSTRING_CID, false, NULL, xpctest::ConstructXPCTestDOMString },
    { &kNS_XPCTESTVARIANT_CID, false, NULL, xpctest::ConstructXPCTestVariant },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kXPCTestContracts[] = {
    { "@mozilla.org/js/xpc/test/Echo;1", &kNS_ECHO_CID },
    { "@mozilla.org/js/xpc/test/Child;1", &kNS_CHILD_CID },
    { "@mozilla.org/js/xpc/test/Noisy;1", &kNS_NOISY_CID },
    { "@mozilla.org/js/xpc/test/StringTest;1", &kNS_STRING_TEST_CID },
    { "@mozilla.org/js/xpc/test/Overloaded;1", &kNS_OVERLOADED_CID },
    { "@mozilla.org/js/xpc/test/ObjectReadOnly;1", &kNS_XPCTESTOBJECTREADONLY_CID },
    { "@mozilla.org/js/xpc/test/ObjectReadWrite;1", &kNS_XPCTESTOBJECTREADWRITE_CID },
    { "@mozilla.org/js/xpc/test/In;1", &kNS_XPCTESTIN_CID },
    { "@mozilla.org/js/xpc/test/Out;1", &kNS_XPCTESTOUT_CID },
    { "@mozilla.org/js/xpc/test/InOut;1", &kNS_XPCTESTINOUT_CID },
    { "@mozilla.org/js/xpc/test/Const;1", &kNS_XPCTESTCONST_CID },
    { "@mozilla.org/js/xpc/test/CallJS;1", &kNS_XPCTESTCALLJS_CID },
    { "@mozilla.org/js/xpc/test/ParentOne;1", &kNS_XPCTESTPARENTONE_CID },
    { "@mozilla.org/js/xpc/test/ParentTwo;1", &kNS_XPCTESTPARENTTWO_CID },
    { "@mozilla.org/js/xpc/test/Child2;1", &kNS_XPCTESTCHILD2_CID },
    { "@mozilla.org/js/xpc/test/Child3;1", &kNS_XPCTESTCHILD3_CID },
    { "@mozilla.org/js/xpc/test/Child4;1", &kNS_XPCTESTCHILD4_CID },
    { "@mozilla.org/js/xpc/test/Child5;1", &kNS_XPCTESTCHILD5_CID },
    { "@mozilla.org/js/xpc/test/ArrayTest;1", &kNS_ARRAY_CID },
    { "@mozilla.org/js/xpc/test/DOMString;1", &kNS_XPCTESTDOMSTRING_CID },
    { "@mozilla.org/js/xpc/test/TestVariant;1", &kNS_XPCTESTVARIANT_CID },
    { NULL }
};

static const mozilla::Module kXPCTestModule = {
    mozilla::Module::kVersion,
    kXPCTestCIDs,
    kXPCTestContracts
};

NSMODULE_DEFN(xpconnect_test) = &kXPCTestModule;
