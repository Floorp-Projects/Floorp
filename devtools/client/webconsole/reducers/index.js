/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  autocomplete,
} = require("resource://devtools/client/webconsole/reducers/autocomplete.js");
const {
  filters,
} = require("resource://devtools/client/webconsole/reducers/filters.js");
const {
  messages,
} = require("resource://devtools/client/webconsole/reducers/messages.js");
const {
  prefs,
} = require("resource://devtools/client/webconsole/reducers/prefs.js");
const { ui } = require("resource://devtools/client/webconsole/reducers/ui.js");
const {
  notifications,
} = require("resource://devtools/client/webconsole/reducers/notifications.js");
const {
  history,
} = require("resource://devtools/client/webconsole/reducers/history.js");

exports.reducers = {
  autocomplete,
  filters,
  messages,
  prefs,
  ui,
  notifications,
  history,
};
