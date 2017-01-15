/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["CleanupManager"];

const cleanupHandlers = new Set();

this.CleanupManager = {
  addCleanupHandler(handler) {
    cleanupHandlers.add(handler);
  },

  removeCleanupHandler(handler) {
    cleanupHandlers.delete(handler);
  },

  cleanup() {
    for (const handler of cleanupHandlers) {
      try {
        handler();
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
  },
};
