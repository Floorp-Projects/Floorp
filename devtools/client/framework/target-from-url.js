/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { TargetFactory } = require("devtools/client/framework/target");
const { DebuggerServer } = require("devtools/server/main");
const { DebuggerClient } = require("devtools/shared/client/debugger-client");

/**
 * Construct a Target for a given URL object having various query parameters:
 *
 * - host, port & ws: See the documentation for clientFromURL
 *
 * - type: tab, process, window
 *      {String} The type of target to connect to.
 *
 * If type == "tab":
 * - id:
 *      {Number} the tab outerWindowID
 * - chrome: Optional
 *      {Boolean} Force the creation of a chrome target. Gives more privileges to
 *      the tab actor. Allows chrome execution in the webconsole and see chrome
 *      files in the debugger. (handy when contributing to firefox)
 *
 * If type == "process":
 * - id:
 *      {Number} the process id to debug. Default to 0, which is the parent process.
 *
 * If type == "window":
 * - id:
 *      {Number} the window outerWindowID
 *
 * @param {URL} url
 *        The url to fetch query params from.
 *
 * @return A target object
 */
exports.targetFromURL = async function targetFromURL(url) {
  const client = await clientFromURL(url);
  await client.connect();

  const params = url.searchParams;
  const type = params.get("type");
  if (!type) {
    throw new Error("targetFromURL, missing type parameter");
  }
  let id = params.get("id");
  // Allows to spawn a chrome enabled target for any context
  // (handy to debug chrome stuff in a child process)
  let chrome = params.has("chrome");

  let form, isTabActor;
  if (type === "tab") {
    // Fetch target for a remote tab
    id = parseInt(id, 10);
    if (isNaN(id)) {
      throw new Error(`targetFromURL, wrong tab id '${id}', should be a number`);
    }
    try {
      const response = await client.getTab({ outerWindowID: id });
      form = response.tab;
    } catch (ex) {
      if (ex.error == "noTab") {
        throw new Error(`targetFromURL, tab with outerWindowID '${id}' doesn't exist`);
      }
      throw ex;
    }
  } else if (type == "process") {
    // Fetch target for a remote chrome actor
    DebuggerServer.allowChromeProcess = true;
    try {
      id = parseInt(id, 10);
      if (isNaN(id)) {
        id = 0;
      }
      const response = await client.getProcess(id);
      form = response.form;
      chrome = true;
      if (id != 0) {
        // Child process are not exposing tab actors and only support debugger+console
        isTabActor = false;
      }
    } catch (ex) {
      if (ex.error == "noProcess") {
        throw new Error(`targetFromURL, process with id '${id}' doesn't exist`);
      }
      throw ex;
    }
  } else if (type == "window") {
    // Fetch target for a remote window actor
    DebuggerServer.allowChromeProcess = true;
    try {
      id = parseInt(id, 10);
      if (isNaN(id)) {
        throw new Error("targetFromURL, window requires id parameter");
      }
      const response = await client.mainRoot.getWindow({
        outerWindowID: id,
      });
      form = response.window;
      chrome = true;
    } catch (ex) {
      if (ex.error == "notFound") {
        throw new Error(`targetFromURL, window with id '${id}' doesn't exist`);
      }
      throw ex;
    }
  } else {
    throw new Error(`targetFromURL, unsupported type '${type}' parameter`);
  }

  return TargetFactory.forRemoteTab({ client, form, chrome, isTabActor });
};

/**
 * Create a DebuggerClient for a given URL object having various query parameters:
 *
 * host:
 *    {String} The hostname or IP address to connect to.
 * port:
 *    {Number} The TCP port to connect to, to use with `host` argument.
 * ws:
 *    {Boolean} If true, connect via websocket instead of regular TCP connection.
 *
 * @param {URL} url
 *        The url to fetch query params from.
 * @return a promise that resolves a DebuggerClient object
 */
async function clientFromURL(url) {
  const params = url.searchParams;
  const host = params.get("host");
  const port = params.get("port");
  const webSocket = !!params.get("ws");

  let transport;
  if (port) {
    transport = await DebuggerClient.socketConnect({ host, port, webSocket });
  } else {
    // Setup a server if we don't have one already running
    DebuggerServer.init();
    DebuggerServer.registerAllActors();
    transport = DebuggerServer.connectPipe();
  }
  return new DebuggerClient(transport);
}

exports.clientFromURL = clientFromURL;
