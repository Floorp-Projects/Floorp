/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://shield-recipe-client/lib/LogManager.jsm");

this.EXPORTED_SYMBOLS = ["EventEmitter"];

const log = LogManager.getLogger("event-emitter");

this.EventEmitter = function(sandboxManager) {
  const listeners = {};

  return {
    createSandboxedEmitter() {
      return sandboxManager.cloneInto({
        on: this.on.bind(this),
        off: this.off.bind(this),
        once: this.once.bind(this),
      }, {cloneFunctions: true});
    },

    emit(eventName, event) {
      // Fire events async
      Promise.resolve()
        .then(() => {
          if (!(eventName in listeners)) {
            log.debug(`EventEmitter: Event fired with no listeners: ${eventName}`);
            return;
          }
          // Clone callbacks array to avoid problems with mutation while iterating
          const callbacks = Array.from(listeners[eventName]);
          for (const cb of callbacks) {
            cb(sandboxManager.cloneInto(event));
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
