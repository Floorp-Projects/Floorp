/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu, Ci } = require("chrome");

const { TargetFactory } = require("devtools/client/framework/target");
const { DebuggerServer } = require("devtools/server/main");
const { DebuggerClient } = require("devtools/shared/client/main");
const { Task } = require("resource://gre/modules/Task.jsm");

/**
 * Construct a Target for a given URL object having various query parameters:
 *
 * type: tab, process
 *    {String} The type of target to connect to.  Currently tabs and processes are supported types.
 *
 * If type="tab":
 * id:
 *    {Number} the tab outerWindowID
 * chrome: Optional
 *    {Boolean} Force the creation of a chrome target. Gives more privileges to the tab
 *    actor. Allows chrome execution in the webconsole and see chrome files in
 *    the debugger. (handy when contributing to firefox)
 *
 * If type="process":
 * id:
 *    {Number} the process id to debug. Default to 0, which is the parent process.
 *
 * @param {URL} url
 *        The url to fetch query params from.
 *
 * @return A target object
 */
exports.targetFromURL = Task.async(function* (url) {
  let params = url.searchParams;
  let type = params.get("type");
  if (!type) {
    throw new Error("targetFromURL, missing type parameter");
  }
  let id = params.get("id");
  // Allows to spawn a chrome enabled target for any context
  // (handy to debug chrome stuff in a child process)
  let chrome = params.has("chrome");

  // Once about:debugging start supporting remote targets and use this helper,
  // client will also be defined by url params.
  let client = createClient();

  yield client.connect();

  let form, isTabActor;
  if (type === "tab") {
    // Fetch target for a remote tab
    id = parseInt(id);
    if (isNaN(id)) {
      throw new Error("targetFromURL, wrong tab id:'" + id + "', should be a number");
    }
    try {
      let response = yield client.getTab({ outerWindowID: id });
      form = response.tab;
    } catch (ex) {
      if (ex.error == "noTab") {
        throw new Error("targetFromURL, tab with outerWindowID:'" + id + "' doesn't exist");
      }
      throw ex;
    }
  } else if (type == "process") {
    // Fetch target for a remote chrome actor
    DebuggerServer.allowChromeProcess = true;
    try {
      id = parseInt(id);
      if (isNaN(id)) {
        id = 0;
      }
      let response = yield client.getProcess(id);
      form = response.form;
      chrome = true;
      if (id != 0) {
        // Child process are not exposing tab actors and only support debugger+console
        isTabActor = false;
      }
    } catch (ex) {
      if (ex.error == "noProcess") {
        throw new Error("targetFromURL, process with id:'" + id + "' doesn't exist");
      }
      throw ex;
    }
  } else {
    throw new Error("targetFromURL, unsupported type='" + type + "' parameter");
  }

  return TargetFactory.forRemoteTab({ client, form, chrome, isTabActor });
});

function createClient() {
  // Setup a server if we don't have one already running
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  return new DebuggerClient(DebuggerServer.connectPipe());
}
