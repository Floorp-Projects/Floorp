/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const BAD_LISTENER =
  "The event listener must be a function, or an object that has " +
  "`EventEmitter.handler` Symbol.";

const eventListeners = Symbol("EventEmitter/listeners");
const onceOriginalListener = Symbol("EventEmitter/once-original-listener");
const handler = Symbol("EventEmitter/event-handler");
loader.lazyRequireGetter(this, "flags", "devtools/shared/flags");

class EventEmitter {
  constructor() {
    this[eventListeners] = new Map();
  }

  /**
   * Registers an event `listener` that is called every time events of
   * specified `type` is emitted on the given event `target`.
   *
   * @param {Object} target
   *    Event target object.
   * @param {String} type
   *    The type of event.
   * @param {Function|Object} listener
   *    The listener that processes the event.
   * @returns {Function}
   *    A function that removes the listener when called.
   */
  static on(target, type, listener) {
    if (typeof listener !== "function" && !isEventHandler(listener)) {
      throw new Error(BAD_LISTENER);
    }

    if (!(eventListeners in target)) {
      target[eventListeners] = new Map();
    }

    const events = target[eventListeners];

    if (events.has(type)) {
      events.get(type).add(listener);
    } else {
      events.set(type, new Set([listener]));
    }

    return () => EventEmitter.off(target, type, listener);
  }

  /**
   * Removes an event `listener` for the given event `type` on the given event
   * `target`. If no `listener` is passed removes all listeners of the given
   * `type`. If `type` is not passed removes all the listeners of the given
   * event `target`.
   * @param {Object} target
   *    The event target object.
   * @param {String} [type]
   *    The type of event.
   * @param {Function|Object} [listener]
   *    The listener that processes the event.
   */
  static off(target, type, listener) {
    const length = arguments.length;
    const events = target[eventListeners];

    if (!events) {
      return;
    }

    if (length >= 3) {
      // Trying to remove from the `target` the `listener` specified for the
      // event's `type` given.
      const listenersForType = events.get(type);

      // If we don't have listeners for the event's type, we bail out.
      if (!listenersForType) {
        return;
      }

      // If the listeners list contains the listener given, we just remove it.
      if (listenersForType.has(listener)) {
        listenersForType.delete(listener);
      } else {
        // If it's not present, there is still the possibility that the listener
        // have been added using `once`, since the method wraps the original listener
        // in another function.
        // So we iterate all the listeners to check if any of them is a wrapper to
        // the `listener` given.
        for (const value of listenersForType.values()) {
          if (
            onceOriginalListener in value &&
            value[onceOriginalListener] === listener
          ) {
            listenersForType.delete(value);
            break;
          }
        }
      }
    } else if (length === 2) {
      // No listener was given, it means we're removing all the listeners from
      // the given event's `type`.
      if (events.has(type)) {
        events.delete(type);
      }
    } else if (length === 1) {
      // With only the `target` given, we're removing all the listeners from the object.
      events.clear();
    }
  }

  static clearEvents(target) {
    const events = target[eventListeners];
    if (!events) {
      return;
    }
    events.clear();
  }

  /**
   * Registers an event `listener` that is called only the next time an event
   * of the specified `type` is emitted on the given event `target`.
   * It returns a promised resolved once the specified event `type` is emitted.
   *
   * @param {Object} target
   *    Event target object.
   * @param {String} type
   *    The type of the event.
   * @param {Function|Object} [listener]
   *    The listener that processes the event.
   * @return {Promise}
   *    The promise resolved once the event `type` is emitted.
   */
  static once(target, type, listener) {
    return new Promise(resolve => {
      // This is the actual listener that will be added to the target's listener, it wraps
      // the call to the original `listener` given.
      const newListener = (first, ...rest) => {
        // To prevent side effects we're removing the listener upfront.
        EventEmitter.off(target, type, newListener);

        let rv;
        if (listener) {
          if (isEventHandler(listener)) {
            // if the `listener` given is actually an object that handles the events
            // using `EventEmitter.handler`, we want to call that function, passing also
            // the event's type as first argument, and the `listener` (the object) as
            // contextual object.
            rv = listener[handler](type, first, ...rest);
          } else {
            // Otherwise we'll just call it
            rv = listener.call(target, first, ...rest);
          }
        }

        // We resolve the promise once the listener is called.
        resolve(first);

        // Listeners may return a promise, so pass it along
        return rv;
      };

      newListener[onceOriginalListener] = listener;
      EventEmitter.on(target, type, newListener);
    });
  }

  static emit(target, type, ...rest) {
    EventEmitter._emit(target, type, false, ...rest);
  }

  static emitAsync(target, type, ...rest) {
    return EventEmitter._emit(target, type, true, ...rest);
  }

  /**
   * Emit an event of a given `type` on a given `target` object.
   *
   * @param {Object} target
   *    Event target object.
   * @param {String} type
   *    The type of the event.
   * @param {Boolean} async
   *    If true, this function will wait for each listener completion.
   *    Each listener has to return a promise, which will be awaited for.
   * @param {any} ...rest
   *    The arguments to pass to each listener function.
   * @return {Promise|undefined}
   *    If `async` argument is true, returns the promise resolved once all listeners have resolved.
   *    Otherwise, this function returns undefined;
   */
  static _emit(target, type, async, ...rest) {
    logEvent(type, rest);

    if (!(eventListeners in target)) {
      return undefined;
    }

    const promises = async ? [] : null;

    if (target[eventListeners].has(type)) {
      // Creating a temporary Set with the original listeners, to avoiding side effects
      // in emit.
      const listenersForType = new Set(target[eventListeners].get(type));

      const events = target[eventListeners];
      const listeners = events.get(type);

      for (const listener of listenersForType) {
        // If the object was destroyed during event emission, stop emitting.
        if (!(eventListeners in target)) {
          break;
        }

        // If listeners were removed during emission, make sure the
        // event handler we're going to fire wasn't removed.
        if (listeners && listeners.has(listener)) {
          try {
            let promise;
            if (isEventHandler(listener)) {
              promise = listener[handler](type, ...rest);
            } else {
              promise = listener.call(target, ...rest);
            }
            if (async) {
              // Assert the name instead of `constructor != Promise` in order
              // to avoid cross compartment issues where Promise can be multiple.
              if (!promise || promise.constructor.name != "Promise") {
                console.warn(
                  `Listener for event '${type}' did not return a promise.`
                );
              } else {
                promises.push(promise);
              }
            }
          } catch (ex) {
            // Prevent a bad listener from interfering with the others.
            console.error(ex);
            const msg = ex + ": " + ex.stack;
            dump(msg + "\n");
          }
        }
      }
    }

    // Backward compatibility with the SDK event-emitter: support wildcard listeners that
    // will be called for any event. The arguments passed to the listener are the event
    // type followed by the actual arguments.
    // !!! This API will be removed by Bug 1391261.
    const hasWildcardListeners = target[eventListeners].has("*");
    if (type !== "*" && hasWildcardListeners) {
      EventEmitter.emit(target, "*", type, ...rest);
    }

    if (async) {
      return Promise.all(promises);
    }

    return undefined;
  }

  /**
   * Returns a number of event listeners registered for the given event `type`
   * on the given event `target`.
   *
   * @param {Object} target
   *    Event target object.
   * @param {String} type
   *    The type of event.
   * @return {Number}
   *    The number of event listeners.
   */
  static count(target, type) {
    if (eventListeners in target) {
      const listenersForType = target[eventListeners].get(type);

      if (listenersForType) {
        return listenersForType.size;
      }
    }

    return 0;
  }

  /**
   * Decorate an object with event emitter functionality; basically using the
   * class' prototype as mixin.
   *
   * @param Object target
   *    The object to decorate.
   * @return Object
   *    The object given, mixed.
   */
  static decorate(target) {
    const descriptors = Object.getOwnPropertyDescriptors(this.prototype);
    delete descriptors.constructor;
    return Object.defineProperties(target, descriptors);
  }

  static get handler() {
    return handler;
  }

  on(...args) {
    return EventEmitter.on(this, ...args);
  }

  off(...args) {
    EventEmitter.off(this, ...args);
  }

  clearEvents() {
    EventEmitter.clearEvents(this);
  }

  once(...args) {
    return EventEmitter.once(this, ...args);
  }

  emit(...args) {
    EventEmitter.emit(this, ...args);
  }

  emitAsync(...args) {
    return EventEmitter.emitAsync(this, ...args);
  }

  emitForTests(...args) {
    if (flags.testing) {
      EventEmitter.emit(this, ...args);
    }
  }

  count(...args) {
    return EventEmitter.count(this, ...args);
  }
}

module.exports = EventEmitter;

const isEventHandler = listener =>
  listener && handler in listener && typeof listener[handler] === "function";

const Services = require("Services");
const { getNthPathExcluding } = require("devtools/shared/platform/stack");
let loggingEnabled = false;

if (!isWorker) {
  loggingEnabled = Services.prefs.getBoolPref("devtools.dump.emit", false);
  const observer = {
    observe: () => {
      loggingEnabled = Services.prefs.getBoolPref("devtools.dump.emit");
    },
  };
  Services.prefs.addObserver("devtools.dump.emit", observer);

  // Also listen for Loader unload to unregister the pref observer and
  // prevent leaking
  const unloadObserver = function(subject) {
    if (subject.wrappedJSObject == require("@loader/unload")) {
      Services.prefs.removeObserver("devtools.dump.emit", observer);
      Services.obs.removeObserver(unloadObserver, "devtools:loader:destroy");
    }
  };
  Services.obs.addObserver(unloadObserver, "devtools:loader:destroy");
}

function serialize(target) {
  const MAXLEN = 60;

  // Undefined
  if (typeof target === "undefined") {
    return "undefined";
  }

  if (target === null) {
    return "null";
  }

  // Number / String
  if (typeof target === "string" || typeof target === "number") {
    return truncate(target, MAXLEN);
  }

  // HTML Node
  if (target.nodeName) {
    let out = target.nodeName;

    if (target.id) {
      out += "#" + target.id;
    }
    if (target.className) {
      out += "." + target.className;
    }

    return out;
  }

  // Array
  if (Array.isArray(target)) {
    return truncate(target.toSource(), MAXLEN);
  }

  // Function
  if (typeof target === "function") {
    return `function ${target.name ? target.name : "anonymous"}()`;
  }

  // Window
  if (target?.constructor?.name === "Window") {
    return `window (${target.location.origin})`;
  }

  // Object
  if (typeof target === "object") {
    let out = "{";

    const entries = Object.entries(target);
    for (let i = 0; i < Math.min(10, entries.length); i++) {
      const [name, value] = entries[i];

      if (i > 0) {
        out += ", ";
      }

      out += `${name}: ${truncate(value, MAXLEN)}`;
    }

    return out + "}";
  }

  // Other
  return truncate(target.toSource(), MAXLEN);
}

function truncate(value, maxLen) {
  // We don't use value.toString() because it can throw.
  const str = String(value);
  return str.length > maxLen ? str.substring(0, maxLen) + "..." : str;
}

function logEvent(type, args) {
  if (!loggingEnabled) {
    return;
  }

  let argsOut = "";

  // We need this try / catch to prevent any dead object errors.
  try {
    argsOut = `${args.map(serialize).join(", ")}`;
  } catch (e) {
    // Object is dead so the toolbox is most likely shutting down,
    // do nothing.
  }

  const path = getNthPathExcluding(0, "devtools/shared/event-emitter.js");

  if (args.length > 0) {
    dump(`EMITTING: emit(${type}, ${argsOut}) from ${path}\n`);
  } else {
    dump(`EMITTING: emit(${type}) from ${path}\n`);
  }
}
