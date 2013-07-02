/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This module basically translates system/events to a SDK standard events
// so that `map`, `filter` and other utilities could be used with them.

module.metadata = {
  "stability": "experimental"
};

const events = require("../system/events");
const { emit } = require("../event/core");

let channel = {};

function forward({ subject, type, data })
  emit(channel, "data", { target: subject, type: type, data: data });

["sdk-panel-show", "sdk-panel-hide", "sdk-panel-shown",
 "sdk-panel-hidden", "sdk-panel-content-changed", "sdk-panel-content-loaded",
 "sdk-panel-document-loaded"
].forEach(function(type) events.on(type, forward));

exports.events = channel;
