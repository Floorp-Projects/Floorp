/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");
const {
  DevToolsClient,
} = require("resource://devtools/client/devtools-client.js");
const {
  remoteClientManager,
} = require("resource://devtools/client/shared/remote-debugging/remote-client-manager.js");
const {
  CommandsFactory,
} = require("resource://devtools/shared/commands/commands-factory.js");

/**
 * Construct a commands object for a given URL with various query parameters:
 *
 * - host, port & ws: See the documentation for clientFromURL
 *
 * - type: "tab", "extension", "worker" or "process"
 *      {String} The type of target to connect to.
 *
 * If type == "tab":
 * - id:
 *      {Number} the tab browserId
 *
 * If type == "extension":
 * - id:
 *      {String} the addonID of the webextension to debug.
 *
 * If type == "worker":
 * - id:
 *      {String} the unique Worker id of the Worker to debug.
 *
 * If type == "process":
 * - id:
 *      {Number} the process id to debug. Default to 0, which is the parent process.
 *
 *
 * @param {URL} url
 *        The url to fetch query params from.
 *
 * @return A commands object
 */
exports.commandsFromURL = async function commandsFromURL(url) {
  const client = await clientFromURL(url);
  const params = url.searchParams;

  // Clients retrieved from the remote-client-manager are already connected.
  const isCachedClient = params.get("remoteId");
  if (!isCachedClient) {
    // Connect any other client.
    await client.connect();
  }

  const id = params.get("id");
  const type = params.get("type");

  let commands;
  try {
    commands = await _commandsFromURL(client, id, type);
  } catch (e) {
    if (!isCachedClient) {
      // If the client was not cached, then the client was created here. If the target
      // creation failed, we should close the client.
      await client.close();
    }
    throw e;
  }

  // When opening about:debugging's toolboxes for remote runtimes,
  // we create a new commands using a shared and cached client.
  // Prevent closing the DevToolsClient on toolbox close and Commands destruction
  // as this can be used by about:debugging and other toolboxes.
  if (isCachedClient) {
    commands.shouldCloseClient = false;
  }

  return commands;
};

async function _commandsFromURL(client, id, type) {
  if (!type) {
    throw new Error("commandsFromURL, missing type parameter");
  }

  let commands;
  if (type === "tab") {
    // Fetch target for a remote tab
    id = parseInt(id, 10);
    if (isNaN(id)) {
      throw new Error(
        `commandsFromURL, wrong tab id '${id}', should be a number`
      );
    }
    try {
      commands = await CommandsFactory.forRemoteTab(id, { client });
    } catch (ex) {
      if (ex.message.startsWith("Protocol error (noTab)")) {
        throw new Error(
          `commandsFromURL, tab with browserId '${id}' doesn't exist`
        );
      }
      throw ex;
    }
  } else if (type === "extension") {
    commands = await CommandsFactory.forAddon(id, { client });

    if (!commands) {
      throw new Error(
        `commandsFromURL, extension with id '${id}' doesn't exist`
      );
    }
  } else if (type === "worker") {
    commands = await CommandsFactory.forWorker(id, { client });

    if (!commands) {
      throw new Error(`commandsFromURL, worker with id '${id}' doesn't exist`);
    }
  } else if (type == "process") {
    // When debugging firefox itself, force the server to accept debugging processes.
    DevToolsServer.allowChromeProcess = true;
    commands = await CommandsFactory.forMainProcess({ client });
  } else {
    throw new Error(`commandsFromURL, unsupported type '${type}' parameter`);
  }

  return commands;
}

/**
 * Create a DevToolsClient for a given URL object having various query parameters:
 *
 * host:
 *    {String} The hostname or IP address to connect to.
 * port:
 *    {Number} The TCP port to connect to, to use with `host` argument.
 * remoteId:
 *    {String} Remote client id, for runtimes from the remote-client-manager
 * ws:
 *    {Boolean} If true, connect via websocket instead of regular TCP connection.
 *
 * @param {URL} url
 *        The url to fetch query params from.
 * @return a promise that resolves a DevToolsClient object
 */
async function clientFromURL(url) {
  const params = url.searchParams;

  // If a remote id was provided we should already have a connected client available.
  const remoteId = params.get("remoteId");
  if (remoteId) {
    const client = remoteClientManager.getClientByRemoteId(remoteId);
    if (!client) {
      throw new Error(`Could not find client with remote id: ${remoteId}`);
    }
    return client;
  }

  const host = params.get("host");
  const port = params.get("port");
  const webSocket = !!params.get("ws");

  let transport;
  if (port) {
    transport = await DevToolsClient.socketConnect({ host, port, webSocket });
  } else {
    // Setup a server if we don't have one already running
    DevToolsServer.init();
    DevToolsServer.registerAllActors();
    transport = DevToolsServer.connectPipe();
  }
  return new DevToolsClient(transport);
}
