/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  "stability": "stable"
};

const { on, once, off, setListeners } = require('./core');
const { method, chain } = require('../lang/functional');
const { Class } = require('../core/heritage');

/**
 * `EventTarget` is an exemplar for creating an objects that can be used to
 * add / remove event listeners on them. Events on these objects may be emitted
 * via `emit` function exported by 'event/core' module.
 */
const EventTarget = Class({
  /**
   * Method initializes `this` event source. It goes through properties of a
   * given `options` and registers listeners for the ones that look like an
   * event listeners.
   */
  /**
   * Method initializes `this` event source. It goes through properties of a
   * given `options` and registers listeners for the ones that look like an
   * event listeners.
   */
  initialize: function initialize(options) {
    setListeners(this, options);
  },
  /**
   * Registers an event `listener` that is called every time events of
   * specified `type` are emitted.
   * @param {String} type
   *    The type of event.
   * @param {Function} listener
   *    The listener function that processes the event.
   * @example
   *      worker.on('message', function (data) {
   *        console.log('data received: ' + data)
   *      })
   */
  on: chain(method(on)),
  /**
   * Registers an event `listener` that is called once the next time an event
   * of the specified `type` is emitted.
   * @param {String} type
   *    The type of the event.
   * @param {Function} listener
   *    The listener function that processes the event.
   */
  once: chain(method(once)),
  /**
   * Removes an event `listener` for the given event `type`.
   * @param {String} type
   *    The type of event.
   * @param {Function} listener
   *    The listener function that processes the event.
   */
  removeListener: function removeListener(type, listener) {
    // Note: We can't just wrap `off` in `method` as we do it for other methods
    // cause skipping a second or third argument will behave very differently
    // than intended. This way we make sure all arguments are passed and only
    // one listener is removed at most.
    off(this, type, listener);
    return this;
  },
  off: function(type, listener) {
    off(this, type, listener);
    return this;
  }
});
exports.EventTarget = EventTarget;
