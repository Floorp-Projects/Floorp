/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const UNCAUGHT_ERROR = 'An error event was emitted for which there was no listener.';
const BAD_LISTENER = 'The event listener must be a function.';

const { ns } = require('../core/namespace');

const event = ns();

const EVENT_TYPE_PATTERN = /^on([A-Z]\w+$)/;
exports.EVENT_TYPE_PATTERN = EVENT_TYPE_PATTERN;

// Utility function to access given event `target` object's event listeners for
// the specific event `type`. If listeners for this type does not exists they
// will be created.
const observers = function observers(target, type) {
  if (!target) throw TypeError("Event target must be an object");
  let listeners = event(target);
  return type in listeners ? listeners[type] : listeners[type] = [];
};

/**
 * Registers an event `listener` that is called every time events of
 * specified `type` is emitted on the given event `target`.
 * @param {Object} target
 *    Event target object.
 * @param {String} type
 *    The type of event.
 * @param {Function} listener
 *    The listener function that processes the event.
 */
function on(target, type, listener) {
  if (typeof(listener) !== 'function')
    throw new Error(BAD_LISTENER);

  let listeners = observers(target, type);
  if (!~listeners.indexOf(listener))
    listeners.push(listener);
}
exports.on = on;


var onceWeakMap = new WeakMap();


/**
 * Registers an event `listener` that is called only the next time an event
 * of the specified `type` is emitted on the given event `target`.
 * @param {Object} target
 *    Event target object.
 * @param {String} type
 *    The type of the event.
 * @param {Function} listener
 *    The listener function that processes the event.
 */
function once(target, type, listener) {
  let replacement = function observer(...args) {
    off(target, type, observer);
    onceWeakMap.delete(listener);
    listener.apply(target, args);
  };
  onceWeakMap.set(listener, replacement);
  on(target, type, replacement);
}
exports.once = once;

/**
 * Execute each of the listeners in order with the supplied arguments.
 * All the exceptions that are thrown by listeners during the emit
 * are caught and can be handled by listeners of 'error' event. Thrown
 * exceptions are passed as an argument to an 'error' event listener.
 * If no 'error' listener is registered exception will be logged into an
 * error console.
 * @param {Object} target
 *    Event target object.
 * @param {String} type
 *    The type of event.
 * @params {Object|Number|String|Boolean} args
 *    Arguments that will be passed to listeners.
 */
function emit (target, type, ...args) {
  emitOnObject(target, type, target, ...args);
}
exports.emit = emit;

/**
 * A variant of emit that allows setting the this property for event listeners
 */
function emitOnObject(target, type, thisArg, ...args) {
  let all = observers(target, '*').length;
  let state = observers(target, type);
  let listeners = state.slice();
  let count = listeners.length;
  let index = 0;

  // If error event and there are no handlers (explicit or catch-all)
  // then print error message to the console.
  if (count === 0 && type === 'error' && all === 0)
    console.exception(args[0]);
  while (index < count) {
    try {
      let listener = listeners[index];
      // Dispatch only if listener is still registered.
      if (~state.indexOf(listener))
        listener.apply(thisArg, args);
    }
    catch (error) {
      // If exception is not thrown by a error listener and error listener is
      // registered emit `error` event. Otherwise dump exception to the console.
      if (type !== 'error') emit(target, 'error', error);
      else console.exception(error);
    }
    index++;
  }
   // Also emit on `"*"` so that one could listen for all events.
  if (type !== '*') emit(target, '*', type, ...args);
}
exports.emitOnObject = emitOnObject;

/**
 * Removes an event `listener` for the given event `type` on the given event
 * `target`. If no `listener` is passed removes all listeners of the given
 * `type`. If `type` is not passed removes all the listeners of the given
 * event `target`.
 * @param {Object} target
 *    The event target object.
 * @param {String} type
 *    The type of event.
 * @param {Function} listener
 *    The listener function that processes the event.
 */
function off(target, type, listener) {
  let length = arguments.length;
  if (length === 3) {
    if (onceWeakMap.has(listener)) {
      listener = onceWeakMap.get(listener);
      onceWeakMap.delete(listener);
    }

    let listeners = observers(target, type);
    let index = listeners.indexOf(listener);
    if (~index)
      listeners.splice(index, 1);
  }
  else if (length === 2) {
    observers(target, type).splice(0);
  }
  else if (length === 1) {
    let listeners = event(target);
    Object.keys(listeners).forEach(type => delete listeners[type]);
  }
}
exports.off = off;

/**
 * Returns a number of event listeners registered for the given event `type`
 * on the given event `target`.
 */
function count(target, type) {
  return observers(target, type).length;
}
exports.count = count;

/**
 * Registers listeners on the given event `target` from the given `listeners`
 * dictionary. Iterates over the listeners and if property name matches name
 * pattern `onEventType` and property is a function, then registers it as
 * an `eventType` listener on `target`.
 *
 * @param {Object} target
 *    The type of event.
 * @param {Object} listeners
 *    Dictionary of listeners.
 */
function setListeners(target, listeners) {
  Object.keys(listeners || {}).forEach(key => {
    let match = EVENT_TYPE_PATTERN.exec(key);
    let type = match && match[1].toLowerCase();
    if (!type) return;

    let listener = listeners[key];
    if (typeof(listener) === 'function')
      on(target, type, listener);
  });
}
exports.setListeners = setListeners;
