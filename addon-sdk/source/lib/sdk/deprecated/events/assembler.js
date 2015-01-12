/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Trait } = require("../light-traits");
const { removeListener, on } = require("../../dom/events");

/**
 * Trait may be used for building objects / composing traits that wish to handle
 * multiple dom events from multiple event targets in one place. Event targets
 * can be added / removed by calling `observe / ignore` methods. Composer should
 * provide array of event types it wishes to handle as property
 * `supportedEventsTypes` and function for handling all those events as
 * `handleEvent` property.
 */
exports.DOMEventAssembler = Trait({
  /**
   * Function that is supposed to handle all the supported events (that are
   * present in the `supportedEventsTypes`) from all the observed
   * `eventTargets`.
   * @param {Event} event
   *    Event being dispatched.
   */
  handleEvent: Trait.required,
  /**
   * Array of supported event names.
   * @type {String[]}
   */
  supportedEventsTypes: Trait.required,
  /**
   * Adds `eventTarget` to the list of observed `eventTarget`s. Listeners for
   * supported events will be registered on the given `eventTarget`.
   * @param {EventTarget} eventTarget
   */
  observe: function observe(eventTarget) {
    this.supportedEventsTypes.forEach(function(eventType) {
      on(eventTarget, eventType, this);
    }, this);
  },
  /**
   * Removes `eventTarget` from the list of observed `eventTarget`s. Listeners
   * for all supported events will be unregistered from the given `eventTarget`.
   * @param {EventTarget} eventTarget
   */
  ignore: function ignore(eventTarget) {
    this.supportedEventsTypes.forEach(function(eventType) {
      removeListener(eventTarget, eventType, this);
    }, this);
  }
});
