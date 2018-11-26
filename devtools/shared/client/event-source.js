/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");

/**
 * TODO: Get rid of this API in favor of EventTarget (bug 1042642)
 *
 * Add simple event notification to a prototype object. Any object that has
 * some use for event notifications or the observer pattern in general can be
 * augmented with the necessary facilities by passing its prototype to this
 * function.
 *
 * @param proto object
 *        The prototype object that will be modified.
 */
function eventSource(proto) {
  /**
   * Add a listener to the event source for a given event.
   *
   * @param name string
   *        The event to listen for.
   * @param listener function
   *        Called when the event is fired. If the same listener
   *        is added more than once, it will be called once per
   *        addListener call.
   */
  proto.addListener = function(name, listener) {
    if (typeof listener != "function") {
      throw TypeError("Listeners must be functions.");
    }

    if (!this._listeners) {
      this._listeners = {};
    }

    this._getListeners(name).push(listener);
  };

  /**
   * Add a listener to the event source for a given event. The
   * listener will be removed after it is called for the first time.
   *
   * @param name string
   *        The event to listen for.
   * @param listener function
   *        Called when the event is fired.
   * @returns Promise
   *          Resolved with an array of the arguments of the event.
   */
  proto.addOneTimeListener = function(name, listener) {
    return new Promise(resolve => {
      const l = (eventName, ...rest) => {
        this.removeListener(name, l);
        if (listener) {
          listener(eventName, ...rest);
        }
        resolve(rest[0]);
      };
      this.addListener(name, l);
    });
  };

  /**
   * Remove a listener from the event source previously added with
   * addListener().
   *
   * @param name string
   *        The event name used during addListener to add the listener.
   * @param listener function
   *        The callback to remove. If addListener was called multiple
   *        times, all instances will be removed.
   */
  proto.removeListener = function(name, listener) {
    if (!this._listeners || (listener && !this._listeners[name])) {
      return;
    }

    if (!listener) {
      this._listeners[name] = [];
    } else {
      this._listeners[name] =
        this._listeners[name].filter(l => l != listener);
    }
  };

  /**
   * Returns the listeners for the specified event name. If none are defined it
   * initializes an empty list and returns that.
   *
   * @param name string
   *        The event name.
   */
  proto._getListeners = function(name) {
    if (name in this._listeners) {
      return this._listeners[name];
    }
    this._listeners[name] = [];
    return this._listeners[name];
  };

  /**
   * Notify listeners of an event.
   *
   * @param name string
   *        The event to fire.
   * @param arguments
   *        All arguments will be passed along to the listeners,
   *        including the name argument.
   */
  proto.emit = function() {
    if (!this._listeners) {
      return;
    }

    const name = arguments[0];
    const listeners = this._getListeners(name).slice(0);

    for (const listener of listeners) {
      try {
        listener.apply(null, arguments);
      } catch (e) {
        // Prevent a bad listener from interfering with the others.
        DevToolsUtils.reportException("notify event '" + name + "'", e);
      }
    }
  };
}

module.exports = eventSource;
