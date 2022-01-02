/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly, Exposed=Window]
namespace UserInteraction {
  /**
   * Starts a timer associated with a UserInteraction ID. The timer can be
   * directly associated with a UserInteraction ID, or with a pair of a
   * UserInteraction ID and an object.
   *
   * @param id - the interaction being recorded, as
   * declared in UserInteractions.yaml. This is also the annotation
   * key that will be set in the BHR hang report if a hang occurs
   * before the UserInteraction is finished.
   *
   * @param value - a value to be set on the key in the event
   * that a hang occurs when the timer is running. This value is limited
   * to 50 characters to avoid abuse, and if the value exceeds that limit
   * then no timer is started, an error is printed, and this function returns
   * false.
   *
   * @param obj - Optional parameter. If specified, the timer is
   * associated with this object, meaning that multiple timers for the
   * same annotation key may be run concurrently, as long as they are
   * associated with different objects.
   *
   * @returns True if the timer was successfully started, false otherwise.
   * If a timer already exists, it will be overwritten, and the new timer
   * will include a "(clobbered)" suffix in any BHR annotations that get
   * created.
   */
  boolean start(DOMString id,
                UTF8String value,
                optional object? obj = null);

  /**
   * Updates an already-running timer associated with a UserInteraction ID
   * (and object) with a new value string.
   *
   * @param id - the interaction being recorded, as
   * declared in UserInteractions.yaml. This is also the annotation
   * key that will be set in the BHR hang report if a hang occurs
   * before the UserInteraction is finished.
   *
   * @param value - a new value to be set on the key in the event
   * that a hang occurs when the timer is running. This value is limited
   * to 50 characters to avoid abuse.
   *
   * @param obj - Optional parameter. If specified, the timer is
   * associated with this object, meaning that multiple timers for the
   * same annotation key may be run concurrently, as long as they are
   * associated with different objects.
   *
   * @returns True if the timer was successfully updated, false
   * otherwise.
   */
  boolean update(DOMString id,
                 UTF8String value,
                 optional object? obj = null);

  /**
   * Cancels the timer associated with the given UserInteraction ID
   * (and object) and does not add the profiler marker for it.
   *
   * @param id - the interaction being recorded, as
   * declared in UserInteractions.yaml.
   *
   * @param obj - Optional parameter which associates the
   * timer with the given object.
   *
   * @returns True if the timer was successfully stopped.
   */
  boolean cancel(DOMString id, optional object? obj = null);

  /**
   * Returns whether a UserInteraction timer is currently running.
   *
   * @param id - the ID of the interaction to check, as declared in
   * UserInteractions.yaml.
   *
   * @param obj - Optional parameter which checks for a timer associated
   * with this particular object. If you're checking for a running timer
   * that was started with an object, you'll need to pass in that same
   * object here to check its running state.
   *
   * @returns True if the timer exists and is currently running.
   */
  boolean running(DOMString id, optional object? obj = null);

  /**
   * Stops the timer associated with the given UserInteraction ID
   * (and object), calculates the time delta between start and finish,
   * and adds a profiler marker with that time range.
   *
   * @param id - the interaction being recorded, as
   * declared in UserInteractions.yaml.
   *
   * @param obj - Optional parameter which associates the
   * timer with the given object.
   *
   * @param additionalText - Optional parameter with includes additional
   * text in the profiler marker. This text is not included in the hang
   * annotation.
   *
   * @returns True if the timer was successfully stopped and the data
   * was added to the profile.
   */
  boolean finish(DOMString id,
                 optional object? obj = null,
                 optional UTF8String additionalText);
};
