/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpctest_private.h"
#include "nsComponentManagerUtils.h"
#include "nsImportModule.h"

NS_IMPL_ISUPPORTS(nsXPCTestReturnCodeParent, nsIXPCTestReturnCodeParent)

NS_IMETHODIMP nsXPCTestReturnCodeParent::CallChild(int32_t childBehavior,
                                                   nsresult* _retval) {
  nsresult rv;
  nsCOMPtr<nsIXPCTestReturnCodeChild> child(do_ImportModule(
      "resource://test/ReturnCodeChild.jsm", "ReturnCodeChild", &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = child->DoIt(childBehavior);
  *_retval = rv;
  return NS_OK;
}
