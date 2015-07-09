/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The dispatcher for actions. This dispatches actions to stores registered
 * for those actions.
 *
 * If stores need to perform async operations for actions, they should return
 * straight away, and set up a new action for the changes if necessary.
 *
 * It is an error if a returned promise rejects - they should always pass.
 */
var loop = loop || {};
loop.Dispatcher = (function() {
  "use strict";

  function Dispatcher() {
    this._eventData = {};
    this._actionQueue = [];
    this._debug = loop.shared.utils.getBoolPreference("debug.dispatcher");
  }

  Dispatcher.prototype = {
    /**
     * Register a store to receive notifications of specific actions.
     *
     * @param {Object} store The store object to register
     * @param {Array} eventTypes An array of action names
     */
    register: function(store, eventTypes) {
      eventTypes.forEach(function(type) {
        if (this._eventData.hasOwnProperty(type)) {
          this._eventData[type].push(store);
        } else {
          this._eventData[type] = [store];
        }
      }.bind(this));
    },

    /**
     * Dispatches an action to all registered stores.
     */
    dispatch: function(action) {
      // Always put it on the queue, to make it simpler.
      this._actionQueue.push(action);
      this._dispatchNextAction();
    },

    /**
     * Dispatches the next action in the queue if one is not already active.
     */
    _dispatchNextAction: function() {
      if (!this._actionQueue.length || this._active) {
        return;
      }

      var action = this._actionQueue.shift();
      var type = action.name;

      var registeredStores = this._eventData[type];
      if (!registeredStores) {
        console.warn("No stores registered for event type ", type);
        return;
      }

      this._active = true;

      if (this._debug) {
        console.log("[Dispatcher] Dispatching action", action);
      }

      registeredStores.forEach(function(store) {
        try {
          store[type](action);
        } catch (x) {
          console.error("[Dispatcher] Dispatching action caused an exception: ", x);
        }
      });

      this._active = false;
      this._dispatchNextAction();
    }
  };

  return Dispatcher;
})();
