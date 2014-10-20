/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* This is a utility namespace for promise-debugging functionality */


dictionary PromiseDebuggingStateHolder {
  PromiseDebuggingState state = "pending";
  any value;
  any reason;
};
enum PromiseDebuggingState { "pending", "fulfilled", "rejected" };

[ChromeOnly, Exposed=(Window,System)]
interface PromiseDebugging {
  static PromiseDebuggingStateHolder getState(Promise<any> p);

  /**
   * Return the stack to the promise's allocation point.  This can
   * return null if the promise was not created from script.
   */
  static object? getAllocationStack(Promise<any> p);

  /**
   * Return the stack to the promise's rejection point, if the
   * rejection happened from script.  This can return null if the
   * promise has not been rejected or was not rejected from script.
   */
  static object? getRejectionStack(Promise<any> p);

  /**
   * Return the stack to the promise's fulfillment point, if the
   * fulfillment happened from script.  This can return null if the
   * promise has not been fulfilled or was not fulfilled from script.
   */
  static object? getFullfillmentStack(Promise<any> p);
};
