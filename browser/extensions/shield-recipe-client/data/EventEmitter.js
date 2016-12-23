/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is meant to run inside action sandboxes

"use strict";


this.EventEmitter = function(driver) {
  if (!driver) {
    throw new Error("driver must be provided");
  }

  const listeners = {};

  return {
    emit(eventName, event) {
      // Fire events async
      Promise.resolve()
        .then(() => {
          if (!(eventName in listeners)) {
            driver.log(`EventEmitter: Event fired with no listeners: ${eventName}`);
            return;
          }
          // freeze event to prevent handlers from modifying it
          const frozenEvent = Object.freeze(event);
          // Clone callbacks array to avoid problems with mutation while iterating
          const callbacks = Array.from(listeners[eventName]);
          for (const cb of callbacks) {
            cb(frozenEvent);
          }
        });
    },

    on(eventName, callback) {
      if (!(eventName in listeners)) {
        listeners[eventName] = [];
      }
      listeners[eventName].push(callback);
    },

    off(eventName, callback) {
      if (eventName in listeners) {
        const index = listeners[eventName].indexOf(callback);
        if (index !== -1) {
          listeners[eventName].splice(index, 1);
        }
      }
    },

    once(eventName, callback) {
      const inner = event => {
        callback(event);
        this.off(eventName, inner);
      };
      this.on(eventName, inner);
    },
  };
};
