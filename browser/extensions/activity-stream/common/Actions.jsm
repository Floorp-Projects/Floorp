/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.actionTypes = [
  "INIT",
  "UNINIT",
// The line below creates an object like this:
// {
//   INIT: "INIT",
//   UNINIT: "UNINIT"
// }
// It prevents accidentally adding a different key/value name.
].reduce((obj, type) => { obj[type] = type; return obj; }, {});

this.EXPORTED_SYMBOLS = [
  "actionTypes"
];
