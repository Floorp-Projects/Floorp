/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TYPES = {
  CONSOLE_MESSAGE: "console-message",
};
exports.TYPES = TYPES;

const LISTENERS = {};
loader.lazyRequireGetter(
  LISTENERS,
  TYPES.CONSOLE_MESSAGE,
  "devtools/server/actors/resources/console-messages"
);
exports.LISTENERS = LISTENERS;
