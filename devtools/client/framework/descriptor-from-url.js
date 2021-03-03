/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DevToolsServer } = require("devtools/server/devtools-server");
const { DevToolsClient } = require("devtools/client/devtools-client");
const {
  remoteClientManager,
} = require("devtools/client/shared/remote-debugging/remote-client-manager");

/**
 * Construct a Target Descriptor for a given URL with various query parameters:
 *
 * - host, port & ws: See the documentation for clientFromURL
 *
 * - type: "tab", "extension", "worker" or "process"
 *      {String} The type of target to connect to.
 *
 * If type == "tab":
 * - id:
 *      {Number} the tab outerWindowID
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
 * @return A target descriptor
 */
exports.descriptorFromURL = async function descriptorFromURL(url) {
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

  let descriptorFront;
  try {
    descriptorFront = await _descriptorFromURL(client, id, type);
  } catch (e) {
    if (!isCachedClient) {
      // If the client was not cached, then the client was created here. If the target
      // creation failed, we should close the client.
      await client.close();
    }
    throw e;
  }

  return descriptorFront;
};

async function _descriptorFromURL(client, id, type) {
  if (!type) {
    throw new Error("descriptorFromURL, missing type parameter");
  }

  let descriptorFront;
  if (type === "tab") {
    // Fetch target for a remote tab
    id = parseInt(id, 10);
    if (isNaN(id)) {
      throw new Error(
        `descriptorFromURL, wrong tab id '${id}', should be a number`
      );
    }
    try {
      descriptorFront = await client.mainRoot.getTab({ outerWindowID: id });
    } catch (ex) {
      if (ex.message.startsWith("Protocol error (noTab)")) {
        throw new Error(
          `descriptorFromURL, tab with outerWindowID '${id}' doesn't exist`
        );
      }
      throw ex;
    }
  } else if (type === "extension") {
    descriptorFront = await client.mainRoot.getAddon({ id });

    if (!descriptorFront) {
      throw new Error(
        `descriptorFromURL, extension with id '${id}' doesn't exist`
      );
    }
  } else if (type === "worker") {
    descriptorFront = await client.mainRoot.getWorker(id);

    if (!descriptorFront) {
      throw new Error(
        `descriptorFromURL, worker with id '${id}' doesn't exist`
      );
    }
  } else if (type == "process") {
    // Fetch descriptor for a remote chrome actor
    DevToolsServer.allowChromeProcess = true;
    try {
      id = parseInt(id, 10);
      if (isNaN(id)) {
        id = 0;
      }
      descriptorFront = await client.mainRoot.getProcess(id);
    } catch (ex) {
      if (ex.error == "noProcess") {
        throw new Error(
          `descriptorFromURL, process with id '${id}' doesn't exist`
        );
      }
      throw ex;
    }
  } else {
    throw new Error(`descriptorFromURL, unsupported type '${type}' parameter`);
  }

  return descriptorFront;
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
