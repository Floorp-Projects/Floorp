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
 *      the target actor. Allows chrome execution in the webconsole and see chrome
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
  const params = url.searchParams;

  // Clients retrieved from the remote-client-manager are already connected.
  const isCachedClient = params.get("remoteId");
  if (!isCachedClient) {
    // Connect any other client.
    await client.connect();
  }

  const id = params.get("id");
  const type = params.get("type");
  const chrome = params.has("chrome");

  let target;
  try {
    target = await _targetFromURL(client, id, type, chrome);
  } catch (e) {
    if (!isCachedClient) {
      // If the client was not cached, then the client was created here. If the target
      // creation failed, we should close the client.
      await client.close();
    }
    throw e;
  }

  // If this isn't a cached client, it means that we just created a new client
  // in `clientFromURL` and we have to destroy it at some point.
  // In such case, force the Target to destroy the client as soon as it gets
  // destroyed. This typically happens only for about:debugging toolboxes
  // opened for local Firefox's targets.
  target.shouldCloseClient = !isCachedClient;

  return target;
};

async function _targetFromURL(client, id, type, chrome) {
  if (!type) {
    throw new Error("targetFromURL, missing type parameter");
  }

  let front;
  if (type === "tab") {
    // Fetch target for a remote tab
    id = parseInt(id, 10);
    if (isNaN(id)) {
      throw new Error(
        `targetFromURL, wrong tab id '${id}', should be a number`
      );
    }
    try {
      const tabDescriptor = await client.mainRoot.getTab({ outerWindowID: id });
      front = await tabDescriptor.getTarget();
    } catch (ex) {
      if (ex.message.startsWith("Protocol error (noTab)")) {
        throw new Error(
          `targetFromURL, tab with outerWindowID '${id}' doesn't exist`
        );
      }
      throw ex;
    }
  } else if (type === "extension") {
    const addonDescriptor = await client.mainRoot.getAddon({ id });

    if (!addonDescriptor) {
      throw new Error(`targetFromURL, extension with id '${id}' doesn't exist`);
    }

    front = await addonDescriptor.getTarget();
  } else if (type === "worker") {
    front = await client.mainRoot.getWorker(id);

    if (!front) {
      throw new Error(
        `targetFromURL, worker with actor id '${id}' doesn't exist`
      );
    }
  } else if (type == "process") {
    // Fetch target for a remote chrome actor
    DevToolsServer.allowChromeProcess = true;
    try {
      id = parseInt(id, 10);
      if (isNaN(id)) {
        id = 0;
      }
      const frontDescriptor = await client.mainRoot.getProcess(id);
      front = await frontDescriptor.getTarget(id);
    } catch (ex) {
      if (ex.error == "noProcess") {
        throw new Error(`targetFromURL, process with id '${id}' doesn't exist`);
      }
      throw ex;
    }
  } else {
    throw new Error(`targetFromURL, unsupported type '${type}' parameter`);
  }

  // Allows to spawn a chrome enabled target for any context
  // (handy to debug chrome stuff in a content process)
  if (chrome) {
    front.forceChrome();
  }

  return front;
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

exports.clientFromURL = clientFromURL;
