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

  /**
   * Get the promises directly depending on a given promise.  These are:
   *
   * 1) Return values of then() calls on the promise
   * 2) Return values of Promise.all() if the given promise was passed in as one
   *    of the arguments.
   * 3) Return values of Promise.race() if the given promise was passed in as
   *    one of the arguments.
   *
   * Once a promise is settled, it will generally notify its dependent promises
   * and forget about them, so this is most useful on unsettled promises.
   *
   * Note that this function only returns the promises that directly depend on
   * p.  It does not recursively return promises that depend on promises that
   * depend on p.
   */
  static sequence<Promise<any>> getDependentPromises(Promise<any> p);

  /**
   * Get the number of milliseconds elapsed since the given promise was created.
   */
  static DOMHighResTimeStamp getPromiseLifetime(Promise<any> p);

  /*
   * Get the number of milliseconds elapsed between the promise being created
   * and being settled.  Throws NS_ERROR_UNEXPECTED if the promise has not
   * settled.
   */
  [Throws]
  static DOMHighResTimeStamp getTimeToSettle(Promise<any> p);
};
