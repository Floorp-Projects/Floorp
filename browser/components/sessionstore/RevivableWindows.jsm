/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["RevivableWindows"];

// List of closed windows that we can revive when closing
// windows in succession until the browser quits.
let closedWindows = [];

/**
 * This module keeps track of closed windows that are revivable. On Windows
 * and Linux we can revive windows before saving to disk - i.e. moving them
 * from state._closedWindows[] to state.windows[] so that they're opened
 * automatically on next startup. This feature lets us properly support
 * closing windows in succession until the browser quits.
 *
 * The length of the list is not capped by max_undo_windows unlike
 * state._closedWindows[].
 */
this.RevivableWindows = Object.freeze({
  // Returns whether there are windows to revive.
  get isEmpty() {
    return closedWindows.length == 0;
  },

  // Add a window to the list.
  add(winState) {
#ifndef XP_MACOSX
    closedWindows.push(winState);
#endif
  },

  // Get the list of revivable windows.
  get() {
    return [...closedWindows];
  },

  // Clear the list of revivable windows.
  clear() {
    closedWindows.length = 0;
  }
});
