/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "deprecated"
};

const ERROR_TYPE = 'error',
      UNCAUGHT_ERROR = 'An error event was dispatched for which there was'
        + ' no listener.',
      BAD_LISTENER = 'The event listener must be a function.';
/**
 * This object is used to create an `EventEmitter` that, useful for composing
 * objects that emit events. It implements an interface like `EventTarget` from
 * DOM Level 2, which is implemented by Node objects in implementations that
 * support the DOM Event Model.
 * @see http://www.w3.org/TR/DOM-Level-2-Events/events.html#Events-EventTarget
 * @see http://nodejs.org/api.html#EventEmitter
 * @see http://livedocs.adobe.com/flash/9.0/ActionScriptLangRefV3/flash/events/EventDispatcher.html
 */
const eventEmitter =  {
  /**
   * Registers an event `listener` that is called every time events of
   * specified `type` are emitted.
   * @param {String} type
   *    The type of event.
   * @param {Function} listener
   *    The listener function that processes the event.
   * @example
   *      worker.on('message', function (data) {
   *          console.log('data received: ' + data)
   *      })
   */
  on: function on(type, listener) {
    if ('function' !== typeof listener)
      throw new Error(BAD_LISTENER);
    let listeners = this._listeners(type);
    if (0 > listeners.indexOf(listener))
      listeners.push(listener);
    // Use of `_public` is required by the legacy traits code that will go away
    // once bug-637633 is fixed.
    return this._public || this;
  },

  /**
   * Registers an event `listener` that is called once the next time an event
   * of the specified `type` is emitted.
   * @param {String} type
   *    The type of the event.
   * @param {Function} listener
   *    The listener function that processes the event.
   */
  once: function once(type, listener) {
    this.on(type, function selfRemovableListener() {
      this.removeListener(type, selfRemovableListener);
      listener.apply(this, arguments);
    });
  },

  /**
   * Unregister `listener` for the specified event type.
   * @param {String} type
   *    The type of event.
   * @param {Function} listener
   *    The listener function that processes the event.
   */
  removeListener: function removeListener(type, listener) {
    if ('function' !== typeof listener)
      throw new Error(BAD_LISTENER);
    let listeners = this._listeners(type),
        index = listeners.indexOf(listener);
    if (0 <= index)
      listeners.splice(index, 1);
    // Use of `_public` is required by the legacy traits code, that will go away
    // once bug-637633 is fixed.
    return this._public || this;
  },

  /**
   * Hash of listeners on this EventEmitter.
   */
  _events: null,

  /**
   * Returns an array of listeners for the specified event `type`. This array
   * can be manipulated, e.g. to remove listeners.
   * @param {String} type
   *    The type of event.
   */
  _listeners: function listeners(type) {
    let events = this._events || (this._events = {});
    return (events.hasOwnProperty(type) && events[type]) || (events[type] = []);
  },

  /**
   * Execute each of the listeners in order with the supplied arguments.
   * Returns `true` if listener for this event was called, `false` if there are
   * no listeners for this event `type`.
   *
   * All the exceptions that are thrown by listeners during the emit
   * are caught and can be handled by listeners of 'error' event. Thrown
   * exceptions are passed as an argument to an 'error' event listener.
   * If no 'error' listener is registered exception will propagate to a
   * caller of this method.
   *
   * **It's recommended to have a default 'error' listener in all the complete
   * composition that in worst case may dump errors to the console.**
   *
   * @param {String} type
   *    The type of event.
   * @params {Object|Number|String|Boolean}
   *    Arguments that will be passed to listeners.
   * @returns {Boolean}
   */
  _emit: function _emit(type, event) {
    let args = Array.slice(arguments);
    // Use of `_public` is required by the legacy traits code that will go away
    // once bug-637633 is fixed.
    args.unshift(this._public || this);
    return this._emitOnObject.apply(this, args);
  },

  /**
   * A version of _emit that lets you specify the object on which listeners are
   * called.  This is a hack that is sometimes necessary when such an object
   * (exports, for example) cannot be an EventEmitter for some reason, but other
   * object(s) managing events for the object are EventEmitters.  Once bug
   * 577782 is fixed, this method shouldn't be necessary.
   *
   * @param {object} targetObj
   *    The object on which listeners will be called.
   * @param {string} type
   *    The event name.
   * @param {value} event
   *    The first argument to pass to listeners.
   * @param {value} ...
   *    More arguments to pass to listeners.
   * @returns {boolean}
   */
  _emitOnObject: function _emitOnObject(targetObj, type, event /* , ... */) {
    let listeners = this._listeners(type).slice(0);
    // If there is no 'error' event listener then throw.
    if (type === ERROR_TYPE && !listeners.length)
      console.exception(event);
    if (!listeners.length)
      return false;
    let params = Array.slice(arguments, 2);
    for (let listener of listeners) {
      try {
        listener.apply(targetObj, params);
      } catch(e) {
        // Bug 726967: Ignore exceptions being throws while notifying the error
        // in order to avoid infinite loops.
        if (type !== ERROR_TYPE)
          this._emit(ERROR_TYPE, e);
        else
          console.exception("Exception in error event listener " + e);
      }
    }
    return true;
  },

  /**
   * Removes all the event listeners for the specified event `type`.
   * @param {String} type
   *    The type of event.
   */
  _removeAllListeners: function _removeAllListeners(type) {
    if (typeof type == "undefined") {
      this._events = null;
      return this;
    }

    this._listeners(type).splice(0);
    return this;
  }
};
exports.EventEmitter = require("./traits").Trait.compose(eventEmitter);
exports.EventEmitterTrait = require('./light-traits').Trait(eventEmitter);
