/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibilityService_h_
#define mozilla_a11y_xpcAccessibilityService_h_

#include "nsIAccessibilityService.h"

class xpcAccessibilityService : public nsIAccessibleRetrieval
{

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBILITYSERVICE
  NS_DECL_NSIACCESSIBLERETRIEVAL

  /**
   * Return true if xpc accessibility service is in use.
   */
  static bool IsInUse() {
    // When ref count goes down to 1 (held internally as a static reference),
    // it means that there are no more external references and thus it is not in
    // use.
    return gXPCAccessibilityService ? gXPCAccessibilityService->mRefCnt > 1 : false;
  }

protected:
  virtual ~xpcAccessibilityService() {
    if (mShutdownTimer) {
      mShutdownTimer->Cancel();
      mShutdownTimer = nullptr;
    }
    gXPCAccessibilityService = nullptr;
  }

private:
  // xpcAccessibilityService creation is controlled by friend
  // NS_GetAccessibilityService, keep constructor private.
  xpcAccessibilityService() { };

  nsCOMPtr<nsITimer> mShutdownTimer;

  /**
   * Reference for xpc accessibility service instance.
   */
  static xpcAccessibilityService* gXPCAccessibilityService;

  /**
   * Used to shutdown nsAccessibilityService if xpcom accessible service is not
   * in use any more.
   */
  static void ShutdownCallback(nsITimer* aTimer, void* aClosure);

  friend nsresult NS_GetAccessibilityService(nsIAccessibilityService** aResult);
};

// for component registration
// {3b265b69-f813-48ff-880d-d88d101af404}
#define NS_ACCESSIBILITY_SERVICE_CID \
{ 0x3b265b69, 0xf813, 0x48ff, { 0x88, 0x0d, 0xd8, 0x8d, 0x10, 0x1a, 0xf4, 0x04 } }

extern nsresult
NS_GetAccessibilityService(nsIAccessibilityService** aResult);

#endif
