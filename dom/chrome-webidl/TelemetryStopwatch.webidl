/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

typedef DOMString HistogramID;
typedef DOMString HistogramKey;

[ChromeOnly, Exposed=Window]
namespace TelemetryStopwatch {
  /**
   * Starts a timer associated with a telemetry histogram. The timer can be
   * directly associated with a histogram, or with a pair of a histogram and
   * an object.
   *
   * @param histogram - a string which must be a valid histogram name.
   *
   * @param obj - Optional parameter. If specified, the timer is
   *              associated with this object, meaning that multiple
   *              timers for the same histogram may be run
   *              concurrently, as long as they are associated with
   *              different objects.
   * @param [options.inSeconds=false] - record elapsed time for this
   *        histogram in seconds instead of milliseconds. Defaults to
   *        false.
   *
   * @returns True if the timer was successfully started, false
   *          otherwise. If a timer already exists, it can't be
   *          started again, and the existing one will be cleared in
   *          order to avoid measurements errors.
   */
  boolean start(HistogramID histogram, optional object? obj = null,
                optional TelemetryStopwatchOptions options = {});

  /**
   * Returns whether a timer associated with a telemetry histogram is currently
   * running. The timer can be directly associated with a histogram, or with a
   * pair of a histogram and an object.
   *
   * @param histogram - a string which must be a valid histogram name.
   *
   * @param obj - Optional parameter. If specified, the timer is
   *              associated with this object, meaning that multiple
   *              timers for the same histogram may be run
   *              concurrently, as long as they are associated with
   *              different objects.
   *
   * @returns True if the timer exists and is currently running.
   */
  boolean running(HistogramID histogram, optional object? obj = null);

  /**
   * Deletes the timer associated with a telemetry histogram. The timer can be
   * directly associated with a histogram, or with a pair of a histogram and
   * an object. Important: Only use this method when a legitimate cancellation
   * should be done.
   *
   * @param histogram - a string which must be a valid histogram name.
   *
   * @param obj - Optional parameter. If specified, the timer is
   *              associated with this object, meaning that multiple
   *              timers or a same histogram may be run concurrently,
   *              as long as they are associated with different
   *              objects.
   *
   * @returns True if the timer exist and it was cleared, False
   *          otherwise.
   */
  boolean cancel(HistogramID histogram, optional object? obj = null);

  /**
   * Returns the elapsed time for a particular stopwatch. Primarily for
   * debugging purposes. Must be called prior to finish.
   *
   * @param histogram - a string which must be a valid histogram name.
   *                    if an invalid name is given, the function will
   *                    throw.
   *
   * @param obj - Optional parameter which associates the histogram
   *              timer with the given object.
   *
   * @param canceledOkay - Optional parameter which will suppress any
   *                       warnings that normally fire when a stopwatch
   *                       is finished after being cancelled. Defaults
   *                       to false.
   *
   * @returns Time in milliseconds or -1 if the stopwatch was not
   *          found.
   */
  long timeElapsed(HistogramID histogram,
                   optional object? obj = null,
                   optional boolean canceledOkay = false);

  /**
   * Stops the timer associated with the given histogram (and object),
   * calculates the time delta between start and finish, and adds the value
   * to the histogram.
   *
   * @param histogram - a string which must be a valid histogram name.
   *
   * @param obj - Optional parameter which associates the histogram
   *              timer with the given object.
   *
   * @param canceledOkay - Optional parameter which will suppress any
   *                       warnings that normally fire when a stopwatch
   *                       is finished after being cancelled. Defaults
   *                       to false.
   *
   * @returns True if the timer was succesfully stopped and the data
   *          was added to the histogram, false otherwise.
   */
  boolean finish(HistogramID histogram,
                 optional object? obj = null,
                 optional boolean canceledOkay = false);

  /**
   * Starts a timer associated with a keyed telemetry histogram. The timer can
   * be directly associated with a histogram and its key. Similarly to
   * @see{TelemetryStopwatch.start} the histogram and its key can be associated
   * with an object. Each key may have multiple associated objects and each
   * object can be associated with multiple keys.
   *
   * @param histogram - a string which must be a valid histogram name.
   *
   * @param key - a string which must be a valid histgram key.
   *
   * @param obj - Optional parameter. If specified, the timer is
   *              associated with this object, meaning that multiple
   *              timers for the same histogram may be run
   *              concurrently,as long as they are associated with
   *              different objects.
   * @param [options.inSeconds=false] - record elapsed time for this
   *         histogram in seconds instead of milliseconds. Defaults to
   *         false.
   *
   * @returns True if the timer was successfully started, false
   *          otherwise. If a timer already exists, it can't be
   *          started again, and the existing one will be cleared in
   *          order to avoid measurements errors.
   */
  boolean startKeyed(HistogramID histogram, HistogramKey key,
                     optional object? obj = null,
                     optional TelemetryStopwatchOptions options = {});

  /**
   * Returns whether a timer associated with a telemetry histogram is currently
   * running. Similarly to @see{TelemetryStopwatch.running} the timer and its
   * key can be associated with an object. Each key may have multiple associated
   * objects and each object can be associated with multiple keys.
   *
   * @param histogram - a string which must be a valid histogram name.
   *
   * @param key - a string which must be a valid histgram key.
   *
   * @param obj - Optional parameter. If specified, the timer is
   *              associated with this object, meaning that multiple
   *              timers for the same histogram may be run
   *              concurrently, as long as they are associated with
   *              different objects.
   *
   * @returns True if the timer exists and is currently running.
   */
  boolean runningKeyed(HistogramID histogram, HistogramKey key,
                       optional object? obj = null);

  /**
   * Deletes the timer associated with a keyed histogram. Important: Only use
   * this method when a legitimate cancellation should be done.
   *
   * @param histogram - a string which must be a valid histogram name.
   *
   * @param key - a string which must be a valid histgram key.
   *
   * @param obj - Optional parameter. If specified, the timer
   *              associated with this object is deleted.
   *
   * @returns True if the timer exist and it was cleared, False
   *          otherwise.
   */
  boolean cancelKeyed(HistogramID histogram, HistogramKey key,
                      optional object? obj = null);

  /**
   * Returns the elapsed time for a particular stopwatch. Primarily for
   * debugging purposes. Cannot be called after finish().
   *
   * @param histogram - a string which must be a valid histogram name.
   *
   * @param key - a string which must be a valid histgram key.
   *
   * @param obj - Optional parameter. If specified, the timer
   *              associated with this object is used to calculate
   *              the elapsed time.
   *
   * @returns Time in milliseconds or -1 if the stopwatch was not
   *          found.
   */
  long timeElapsedKeyed(HistogramID histogram, HistogramKey key,
                        optional object? obj = null,
                        optional boolean canceledOkay = false);

  /**
   * Stops the timer associated with the given keyed histogram (and object),
   * calculates the time delta between start and finish, and adds the value
   * to the keyed histogram.
   *
   * @param histogram - a string which must be a valid histogram name.
   *
   * @param key - a string which must be a valid histgram key.
   *
   * @param obj - optional parameter which associates the histogram
   *              timer with the given object.
   *
   * @param canceledOkay - Optional parameter which will suppress any
   *                       warnings that normally fire when a stopwatch
   *                       is finished after being cancelled. Defaults
   *                       to false.
   *
   * @returns True if the timer was succesfully stopped and the data
   *          was added to the histogram, false otherwise.
   */
  boolean finishKeyed(HistogramID histogram, HistogramKey key,
                      optional object? obj = null,
                      optional boolean canceledOkay = false);

  /**
   * Set the testing mode. Used by tests.
   */
  undefined setTestModeEnabled(optional boolean testing = true);
};

dictionary TelemetryStopwatchOptions {
  /**
   * If true, record elapsed time for this histogram in seconds instead of
   * milliseconds.
   */
  boolean inSeconds = false;
};
