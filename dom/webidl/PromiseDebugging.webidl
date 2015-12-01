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

/**
 * An observer for Promise that _may_ be leaking uncaught rejections.
 *
 * It is generally a programming error to leave a Promise rejected and
 * not consume its rejection. The information exposed by this
 * interface is designed to allow clients to track down such Promise,
 * i.e. Promise that are currently
 * - in `rejected` state;
 * - last of their chain.
 *
 * Note, however, that a promise in such a state at the end of a tick
 * may eventually be consumed in some ulterior tick. Implementers of
 * this interface are responsible for presenting the information
 * in a meaningful manner.
 */
callback interface UncaughtRejectionObserver {
  /**
   * A Promise has been left in `rejected` state and is the
   * last in its chain.
   *
   * @param p A currently uncaught Promise. If `p` is is eventually
   * caught, i.e. if its `then` callback is called, `onConsumed` will
   * be called.
   */
  void onLeftUncaught(Promise<any> p);

  /**
   * A Promise previously left uncaught is not the last in its
   * chain anymore.
   *
   * @param p A Promise that was previously left in uncaught state is
   * now caught, i.e. it is not the last in its chain anymore.
   */
  void onConsumed(Promise<any> p);
};

[ChromeOnly, Exposed=(Window,System)]
interface PromiseDebugging {
  /**
   * The various functions on this interface all expect to take promises but
   * don't want the WebIDL behavior of assimilating random passed-in objects
   * into promises.  They also want to treat Promise subclass instances as
   * promises instead of wrapping them in a vanilla Promise, which is what the
   * IDL spec says to do.  So we list all our arguments as "object" instead of
   * "Promise" and check for them being a Promise internally.
   */

  /**
   * Get the current state of the given promise.
   */
  [Throws]
  static PromiseDebuggingStateHolder getState(object p);

  /**
   * Return the stack to the promise's allocation point.  This can
   * return null if the promise was not created from script.
   */
  [Throws]
  static object? getAllocationStack(object p);

  /**
   * Return the stack to the promise's rejection point, if the
   * rejection happened from script.  This can return null if the
   * promise has not been rejected or was not rejected from script.
   */
  [Throws]
  static object? getRejectionStack(object p);

  /**
   * Return the stack to the promise's fulfillment point, if the
   * fulfillment happened from script.  This can return null if the
   * promise has not been fulfilled or was not fulfilled from script.
   */
  [Throws]
  static object? getFullfillmentStack(object p);

  /**
   * Return an identifier for a promise. This identifier is guaranteed
   * to be unique to this instance of Firefox.
   */
  [Throws]
  static DOMString getPromiseID(object p);

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
  [Throws]
  static sequence<Promise<any>> getDependentPromises(object p);

  /**
   * Get the number of milliseconds elapsed since the given promise was created.
   */
  [Throws]
  static DOMHighResTimeStamp getPromiseLifetime(object p);

  /*
   * Get the number of milliseconds elapsed between the promise being created
   * and being settled.  Throws NS_ERROR_UNEXPECTED if the promise has not
   * settled.
   */
  [Throws]
  static DOMHighResTimeStamp getTimeToSettle(object p);

  /**
   * Watching uncaught rejections on the current thread.
   *
   * Adding an observer twice will cause it to be notified twice
   * of events.
   */
  static void addUncaughtRejectionObserver(UncaughtRejectionObserver o);
  static boolean removeUncaughtRejectionObserver(UncaughtRejectionObserver o);
};
