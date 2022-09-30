/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const actionModules = [
  require("resource://devtools/client/webconsole/actions/autocomplete.js"),
  require("resource://devtools/client/webconsole/actions/filters.js"),
  require("resource://devtools/client/webconsole/actions/input.js"),
  require("resource://devtools/client/webconsole/actions/messages.js"),
  require("resource://devtools/client/webconsole/actions/ui.js"),
  require("resource://devtools/client/webconsole/actions/notifications.js"),
  require("resource://devtools/client/webconsole/actions/object.js"),
  require("resource://devtools/client/webconsole/actions/toolbox.js"),
  require("resource://devtools/client/webconsole/actions/history.js"),
];

const actions = Object.assign({}, ...actionModules);

module.exports = actions;
