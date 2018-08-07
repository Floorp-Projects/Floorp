"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.onConnect = onConnect;

var _firefox = require("./firefox");

var firefox = _interopRequireWildcard(_firefox);

var _prefs = require("../utils/prefs");

var _dbg = require("../utils/dbg");

var _bootstrap = require("../utils/bootstrap");

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function loadFromPrefs(actions) {
  const {
    pauseOnExceptions,
    pauseOnCaughtExceptions
  } = _prefs.prefs;

  if (pauseOnExceptions || pauseOnCaughtExceptions) {
    return actions.pauseOnExceptions(pauseOnExceptions, pauseOnCaughtExceptions);
  }
}

async function onConnect(connection, {
  services,
  toolboxActions
}) {
  // NOTE: the landing page does not connect to a JS process
  if (!connection) {
    return;
  }

  const commands = firefox.clientCommands;
  const {
    store,
    actions,
    selectors
  } = (0, _bootstrap.bootstrapStore)(commands, {
    services,
    toolboxActions
  });
  const workers = (0, _bootstrap.bootstrapWorkers)();
  await firefox.onConnect(connection, actions);
  await loadFromPrefs(actions);
  (0, _dbg.setupHelper)({
    store,
    actions,
    selectors,
    workers: { ...workers,
      ...services
    },
    connection,
    client: firefox.clientCommands
  });
  (0, _bootstrap.bootstrapApp)(store);
  return {
    store,
    actions,
    selectors,
    client: commands
  };
}