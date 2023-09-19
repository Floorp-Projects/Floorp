/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// The definitions in this file are not sorted.
// Please add new ones to the bottom.

/**
 * Base interface for all metric types to make typing more expressive.
 */
[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanMetric {};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanBoolean : GleanMetric {
  /**
   * Set to the specified boolean value.
   *
   * @param value the value to set.
   */
  undefined set(boolean value);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as a boolean.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  boolean? testGetValue(optional UTF8String aPingName = "");
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanDatetime : GleanMetric {
  /**
   * Set the datetime to the provided value, or the local now.
   * The internal value will store the local timezone.
   *
   * Note: The metric's time_unit affects the resolution of the value, not the
   *       unit of this function's parameter (which is always PRTime/nanos).
   *
   * @param aValue The (optional) time value as PRTime (nanoseconds since epoch).
   *        Defaults to local now.
   */
  undefined set(optional long long aValue);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as a Date.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric as a JS Date with timezone,
   *         or null if there is no value.
   */
  [Throws, ChromeOnly]
  any testGetValue(optional UTF8String aPingName = "");
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanCounter : GleanMetric {
  /*
   * Increases the counter by `amount`.
   *
   * @param aAmount The (optional) amount to increase by. Should be positive. Defaults to 1.
   */
  undefined add(optional long aAmount = 1);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as an integer.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  long? testGetValue(optional UTF8String aPingName = "");
};

dictionary GleanDistributionData {
  required unsigned long long sum;
  required record<UTF8String, unsigned long long> values;
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanTimingDistribution : GleanMetric {
  /**
   * Starts tracking time for the provided metric.
   *
   * @returns A unique timer id for the new timer
   */
  unsigned long long start();

  /**
   * Stops tracking time for the provided metric and timer id.
   *
   * Adds a count to the corresponding bucket in the timing distribution.
   * This will record an error if no `start` was called for this TimerId or
   * if this TimerId was used to call `cancel`.
   *
   * @param aId The TimerId associated with this timing. This allows for
   *            concurrent timing of events associated with different ids.
   */
  undefined stopAndAccumulate(unsigned long long aId);

  /**
   * Aborts a previous `start` call. No error is recorded if no `start` was
   * called. (But then where did you get that id from?)
   *
   * @param aId The TimerID whose `start` you wish to abort.
   */
  undefined cancel(unsigned long long aId);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  GleanDistributionData? testGetValue(optional UTF8String aPingName = "");

  /**
   * **Test-only API**
   *
   * Accumulates a raw numeric sample of milliseconds.
   *
   * @param aSample The sample, in milliseconds, to add.
   */
  [ChromeOnly]
  undefined testAccumulateRawMillis(unsigned long long aSample);
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanMemoryDistribution : GleanMetric {
  /**
   * Accumulates the provided signed sample in the metric.
   *
   * @param aSample The sample to be recorded by the metric. The sample is
   *                assumed to be in the confgured memory unit of the metric.
   *
   * Notes: Values bigger than 1 Terabyte (2^40 bytes) are truncated and an
   * InvalidValue error is recorded.
   */
  undefined accumulate(unsigned long long aSample);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as a DistributionData.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  GleanDistributionData? testGetValue(optional UTF8String aPingName = "");
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanCustomDistribution : GleanMetric {
  /**
   * Accumulates the provided signed samples in the metric.
   *
   * @param aSamples - The vector holding the samples to be recorded by the metric.
   *
   * Notes: Discards any negative value in `samples`
   * and report an `ErrorType::InvalidValue` for each of them.
   */
  undefined accumulateSamples(sequence<long long> aSamples);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as a DistributionData.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  GleanDistributionData? testGetValue(optional UTF8String aPingName = "");
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanString : GleanMetric {
  /**
   * Set the string to the provided value.
   *
   * @param aValue The string to set the metric to.
   */
  undefined set(UTF8String? aValue);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as a string.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  UTF8String? testGetValue(optional UTF8String aPingName = "");
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanStringList : GleanMetric {
  /**
   * Adds a new string to the list.
   *
   * Truncates the value and logs an error if it is longer than 100 bytes.
   *
   * @param value The string to add.
   */
  undefined add(UTF8String value);

  /**
   * Sets the string_list to the provided list of strings.
   *
   * Truncates the list and logs an error if longer than 100 items.
   * Truncates any item longer than 100 bytes and logs an error.
   *
   * @param aValue The list of strings to set the metric to.
   */
  undefined set(sequence<UTF8String> aValue);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  sequence<UTF8String>? testGetValue(optional UTF8String aPingName = "");
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanTimespan : GleanMetric {
  /**
   * Start tracking time for the provided metric.
   *
   * This records an error if it’s already tracking time (i.e. start was already
   * called with no corresponding [stop]): in that case the original
   * start time will be preserved.
   */
  undefined start();

  /**
   * Stop tracking time for the provided metric.
   *
   * Sets the metric to the elapsed time, but does not overwrite an already
   * existing value.
   * This will record an error if no [start] was called or there is an already
   * existing value.
   */
  undefined stop();

  /**
   * Aborts a previous start.
   *
   * Does not record an error if there was no previous call to start.
   */
  undefined cancel();

  /**
   * Explicitly sets the timespan value.
   *
   * This API should only be used if you cannot make use of
   * `start`/`stop`/`cancel`.
   *
   * @param aDuration The duration of this timespan, in units matching the
   *        `time_unit` of this metric's definition.
   */
  undefined setRaw(unsigned long aDuration);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  unsigned long long? testGetValue(optional UTF8String aPingName = "");
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanUuid : GleanMetric {
  /**
   * Set to the specified value.
   *
   * @param aValue The UUID to set the metric to.
   */
  undefined set(UTF8String aValue);

  /**
   * Generate a new random UUID and set the metric to it.
   */
  undefined generateAndSet();

  /**
   * **Test-only API**
   *
   * Gets the currently stored value.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  UTF8String? testGetValue(optional UTF8String aPingName = "");
};

dictionary GleanEventRecord {
  required unsigned long long timestamp;
  required UTF8String category;
  required UTF8String name;
  record<UTF8String, UTF8String> extra;
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanEvent : GleanMetric {

  /*
   * Record an event.
   *
   * @param aExtra An (optional) map of extra values.
   */
  undefined _record(optional record<UTF8String, UTF8String?> aExtra);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   *
   * The difference between event timestamps is in milliseconds
   * See https://mozilla.github.io/glean/book/user/metrics/event.html for further details.
   * Due to limitations of numbers in JavaScript, the timestamp will only be accurate up until 2^53.
   * (This is probably not an issue with the current clock implementation. Probably.)
   */
  [Throws, ChromeOnly]
  sequence<GleanEventRecord>? testGetValue(optional UTF8String aPingName = "");
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanQuantity : GleanMetric {
  /**
   * Set to the specified value.
   *
   * @param aValue The value to set the metric to.
   */
  undefined set(long long aValue);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  long long? testGetValue(optional UTF8String aPingName = "");
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanDenominator : GleanMetric {
  /*
   * Increases the counter by `aAmount`.
   *
   * @param aAmount The (optional) amount to increase by. Should be positive. Defaults to 1.
   */
  undefined add(optional long aAmount = 1);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as an integer.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  long? testGetValue(optional UTF8String aPingName = "");
};

dictionary GleanRateData {
  required long numerator;
  required long denominator;
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanNumerator : GleanMetric {
  /*
   * Increases the numerator by `aAmount`.
   *
   * @param aAmount The (optional) amount to increase by. Should be positive. Defaults to 1.
   */
  undefined addToNumerator(optional long aAmount = 1);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value in the form {numerator: n, denominator: d}
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  GleanRateData? testGetValue(optional UTF8String aPingName = "");
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanRate : GleanMetric {
  /*
   * Increases the numerator by `amount`.
   *
   * @param aAmount The (optional) amount to increase by. Should be positive. Defaults to 1.
   */
  undefined addToNumerator(optional long aAmount = 1);

  /*
   * Increases the denominator by `amount`.
   *
   * @param aAmount The (optional) amount to increase by. Should be positive. Defaults to 1.
   */
  undefined addToDenominator(optional long aAmount = 1);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value in the form {numerator: n, denominator: d}
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  GleanRateData? testGetValue(optional UTF8String aPingName = "");
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanUrl : GleanMetric {
  /**
   * Set to the specified value.
   *
   * @param aValue The stringified URL to set the metric to.
   */
  undefined set(UTF8String aValue);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  UTF8String? testGetValue(optional UTF8String aPingName = "");
};

[Func="nsGlobalWindowInner::IsGleanNeeded", Exposed=Window]
interface GleanText : GleanMetric {
  /**
   * Set to the provided value.
   *
   * @param aValue The text to set the metric to.
   */
  undefined set(UTF8String aValue);

  /**
   * **Test-only API**
   *
   * Gets the currently stored value as a string.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or null if there is no value.
   */
  [Throws, ChromeOnly]
  UTF8String? testGetValue(optional UTF8String aPingName = "");
};
