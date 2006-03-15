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
#include "nsIGenericFactory.h"
#include "nsCRT.h"
#include "nsIClassInfoImpl.h"

NS_DECL_CLASSINFO(xpcTestCallJS)
NS_DECL_CLASSINFO(xpcTestChild2)

static const nsModuleComponentInfo components[] = {
  {nsnull, NS_ECHO_CID,                   "@mozilla.org/js/xpc/test/Echo;1",                 xpctest::ConstructEcho                  },
  {nsnull, NS_CHILD_CID,                  "@mozilla.org/js/xpc/test/Child;1",                xpctest::ConstructChild                 },
  {nsnull, NS_NOISY_CID,                  "@mozilla.org/js/xpc/test/Noisy;1",                xpctest::ConstructNoisy                 },
  {nsnull, NS_STRING_TEST_CID,            "@mozilla.org/js/xpc/test/StringTest;1",           xpctest::ConstructStringTest            },
  {nsnull, NS_OVERLOADED_CID,             "@mozilla.org/js/xpc/test/Overloaded;1",           xpctest::ConstructOverloaded            },
  {nsnull, NS_XPCTESTOBJECTREADONLY_CID,  "@mozilla.org/js/xpc/test/ObjectReadOnly;1",  xpctest::ConstructXPCTestObjectReadOnly },
  {nsnull, NS_XPCTESTOBJECTREADWRITE_CID, "@mozilla.org/js/xpc/test/ObjectReadWrite;1", xpctest::ConstructXPCTestObjectReadWrite},
  {nsnull, NS_XPCTESTIN_CID,              "@mozilla.org/js/xpc/test/In;1",              xpctest::ConstructXPCTestIn             },
  {nsnull, NS_XPCTESTOUT_CID,             "@mozilla.org/js/xpc/test/Out;1",             xpctest::ConstructXPCTestOut            },
  {nsnull, NS_XPCTESTINOUT_CID,           "@mozilla.org/js/xpc/test/InOut;1",           xpctest::ConstructXPCTestInOut          },
  {nsnull, NS_XPCTESTCONST_CID,           "@mozilla.org/js/xpc/test/Const;1",           xpctest::ConstructXPCTestConst          },
  {nsnull, NS_XPCTESTCALLJS_CID,          "@mozilla.org/js/xpc/test/CallJS;1",          xpctest::ConstructXPCTestCallJS, NULL, NULL, NULL, NS_CI_INTERFACE_GETTER_NAME(xpcTestCallJS), NULL, &NS_CLASSINFO_NAME(xpcTestCallJS) },
  {nsnull, NS_XPCTESTPARENTONE_CID,       "@mozilla.org/js/xpc/test/ParentOne;1",       xpctest::ConstructXPCTestParentOne      },
  {nsnull, NS_XPCTESTPARENTTWO_CID,       "@mozilla.org/js/xpc/test/ParentTwo;1",       xpctest::ConstructXPCTestParentTwo      },
  {nsnull, NS_XPCTESTCHILD2_CID,          "@mozilla.org/js/xpc/test/Child2;1",          xpctest::ConstructXPCTestChild2, NULL, NULL, NULL, NS_CI_INTERFACE_GETTER_NAME(xpcTestChild2), NULL, &NS_CLASSINFO_NAME(xpcTestChild2) },
  {nsnull, NS_XPCTESTCHILD3_CID,          "@mozilla.org/js/xpc/test/Child3;1",          xpctest::ConstructXPCTestChild3         },
  {nsnull, NS_XPCTESTCHILD4_CID,          "@mozilla.org/js/xpc/test/Child4;1",          xpctest::ConstructXPCTestChild4         },
  {nsnull, NS_XPCTESTCHILD5_CID,          "@mozilla.org/js/xpc/test/Child5;1",          xpctest::ConstructXPCTestChild5         },
  {nsnull, NS_ARRAY_CID,                  "@mozilla.org/js/xpc/test/ArrayTest;1",       xpctest::ConstructArrayTest             },
  {nsnull, NS_XPCTESTDOMSTRING_CID,       "@mozilla.org/js/xpc/test/DOMString;1",       xpctest::ConstructXPCTestDOMString      },
  {nsnull, NS_XPCTESTVARIANT_CID,         "@mozilla.org/js/xpc/test/TestVariant;1",     xpctest::ConstructXPCTestVariant        }
};
                                                               
NS_IMPL_NSGETMODULE(xpconnect_test, components)
