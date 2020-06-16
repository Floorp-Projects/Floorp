/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TYPES = {
  CONSOLE_MESSAGE: "console-message",
  PLATFORM_MESSAGE: "platform-message",
};
exports.TYPES = TYPES;

const resources = {
  [TYPES.CONSOLE_MESSAGE]: "devtools/server/actors/resources/console-messages",
  [TYPES.PLATFORM_MESSAGE]:
    "devtools/server/actors/resources/platform-messages",
};

const LISTENERS = {};
for (const [type, path] of Object.entries(resources)) {
  loader.lazyRequireGetter(LISTENERS, type, path);
}

exports.LISTENERS = LISTENERS;
