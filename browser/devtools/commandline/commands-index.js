/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const gcli = require("gcli/index");

const commandModules = [
  "devtools/tilt/tilt-commands",
  "gcli/commands/addon",
  "gcli/commands/appcache",
  "gcli/commands/calllog",
  "gcli/commands/cmd",
  "gcli/commands/cookie",
  "gcli/commands/jsb",
  "gcli/commands/listen",
  "gcli/commands/media",
  "gcli/commands/pagemod",
  "gcli/commands/paintflashing",
  "gcli/commands/restart",
  "gcli/commands/screenshot",
  "gcli/commands/tools",
];

gcli.addItemsByModule(commandModules, { delayedLoad: true });

const defaultTools = require("main").defaultTools;
for (let definition of defaultTools) {
  if (definition.commands) {
    gcli.addItemsByModule(definition.commands, { delayedLoad: true });
  }
}

const { mozDirLoader } = require("gcli/commands/cmd");

gcli.addItemsByModule("mozcmd", { delayedLoad: true, loader: mozDirLoader });
