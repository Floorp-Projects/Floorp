/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* module registration and factory code. */

#include "xpctest_private.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsCRT.h"

// XXX progids need to be standardized!
static nsModuleComponentInfo components[] = {
  {nsnull, NS_ECHO_CID,                   "nsEcho",                 xpctest::ConstructEcho                  },
  {nsnull, NS_CHILD_CID,                  "nsChild",                xpctest::ConstructChild                 },
  {nsnull, NS_NOISY_CID,                  "nsNoisy",                xpctest::ConstructNoisy                 },
  {nsnull, NS_STRING_TEST_CID,            "nsStringTest",           xpctest::ConstructStringTest            },
  {nsnull, NS_OVERLOADED_CID,             "nsOverloaded",           xpctest::ConstructOverloaded            },
  {nsnull, NS_XPCTESTOBJECTREADONLY_CID,  "xpcTestObjectReadOnly",  xpctest::ConstructXPCTestObjectReadOnly },
  {nsnull, NS_XPCTESTOBJECTREADWRITE_CID, "xpcTestObjectReadWrite", xpctest::ConstructXPCTestObjectReadWrite},
  {nsnull, NS_XPCTESTIN_CID,              "xpcTestIn",              xpctest::ConstructXPCTestIn             },
  {nsnull, NS_XPCTESTOUT_CID,             "xpcTestOut",             xpctest::ConstructXPCTestOut            },
  {nsnull, NS_XPCTESTINOUT_CID,           "xpcTestInOut",           xpctest::ConstructXPCTestInOut          },
  {nsnull, NS_XPCTESTCONST_CID,           "xpcTestConst",           xpctest::ConstructXPCTestConst          },
  {nsnull, NS_XPCTESTCALLJS_CID,          "xpcTestCallJS",          xpctest::ConstructXPCTestCallJS         },
  {nsnull, NS_XPCTESTPARENTONE_CID,       "xpcTestParentOne",       xpctest::ConstructXPCTestParentOne      },
  {nsnull, NS_XPCTESTPARENTTWO_CID,       "xpcTestParentTwo",       xpctest::ConstructXPCTestParentTwo      },
  {nsnull, NS_XPCTESTCHILD2_CID,          "xpcTestChild2",          xpctest::ConstructXPCTestChild2         },
  {nsnull, NS_XPCTESTCHILD3_CID,          "xpcTestChild3",          xpctest::ConstructXPCTestChild3         },
  {nsnull, NS_XPCTESTCHILD4_CID,          "xpcTestChild4",          xpctest::ConstructXPCTestChild4         },
  {nsnull, NS_XPCTESTCHILD5_CID,          "xpcTestChild5",          xpctest::ConstructXPCTestChild5         },
  {nsnull, NS_ARRAY_CID,                  "nsArrayTest",            xpctest::ConstructArrayTest             }
};
                                                               
NS_IMPL_NSGETMODULE("xpconnect test", components)

