/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { events } = require("../window/events");
const { filter } = require("../event/utils");
const { isBrowser } = require("../window/utils");

// TODO: `isBrowser` detects weather window is a browser by checking
// `windowtype` attribute, which means that all 'open' events will be
// filtered out since document is not loaded yet. Maybe we can find a better
// implementation for `isBrowser`. Either way it's not really needed yet
// neither window tracker provides this event.

exports.events = filter(function({target}) isBrowser(target), events);
