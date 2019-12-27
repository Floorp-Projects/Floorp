/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const actionModules = [
  require("devtools/client/webconsole/actions/autocomplete"),
  require("devtools/client/webconsole/actions/filters"),
  require("devtools/client/webconsole/actions/input"),
  require("devtools/client/webconsole/actions/messages"),
  require("devtools/client/webconsole/actions/ui"),
  require("devtools/client/webconsole/actions/notifications"),
  require("devtools/client/webconsole/actions/object"),
  require("devtools/client/webconsole/actions/toolbox"),
  require("devtools/client/webconsole/actions/history"),
];

const actions = Object.assign({}, ...actionModules);

module.exports = actions;
