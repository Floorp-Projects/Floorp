/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createEnum } = require("devtools/client/shared/enum");

createEnum([

  // Clears the flexbox state by resetting it back to the initial flexbox state.
  "CLEAR_FLEXBOX",

  // Updates the flexbox state with the newly selected flexbox.
  "UPDATE_FLEXBOX",

  // Updates the flexbox highlighted state.
  "UPDATE_FLEXBOX_HIGHLIGHTED",

], module.exports);
