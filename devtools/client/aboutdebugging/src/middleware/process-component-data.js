/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  l10n,
} = require("resource://devtools/client/aboutdebugging/src/modules/l10n.js");

const {
  DEBUG_TARGETS,
  REQUEST_PROCESSES_SUCCESS,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");

/**
 * This middleware converts tabs object that get from DevToolsClient.listProcesses() to
 * data which is used in DebugTargetItem.
 */
const processComponentDataMiddleware = store => next => action => {
  switch (action.type) {
    case REQUEST_PROCESSES_SUCCESS: {
      const mainProcessComponentData = toMainProcessComponentData(
        action.mainProcess
      );
      action.processes = [mainProcessComponentData];
      break;
    }
  }

  return next(action);
};

function toMainProcessComponentData(process) {
  const type = DEBUG_TARGETS.PROCESS;
  const icon = "chrome://devtools/skin/images/aboutdebugging-process-icon.svg";

  // For now, we assume there is only one process and this is the main process
  // So the name and title are for a remote (multiprocess) browser toolbox.
  const name = l10n.getString("about-debugging-multiprocess-toolbox-name");
  const description = l10n.getString(
    "about-debugging-multiprocess-toolbox-description"
  );

  return {
    name,
    icon,
    type,
    details: {
      description,
    },
  };
}

module.exports = processComponentDataMiddleware;
