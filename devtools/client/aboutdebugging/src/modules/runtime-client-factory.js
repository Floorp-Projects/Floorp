/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  prepareTCPConnection,
} = require("devtools/client/shared/remote-debugging/adb/commands/index");
const { DevToolsClient } = require("devtools/shared/client/devtools-client");
const { DevToolsServer } = require("devtools/server/devtools-server");
const {
  ClientWrapper,
} = require("devtools/client/aboutdebugging/src/modules/client-wrapper");
const {
  remoteClientManager,
} = require("devtools/client/shared/remote-debugging/remote-client-manager");

const { RUNTIMES } = require("devtools/client/aboutdebugging/src/constants");

async function createLocalClient() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  DevToolsServer.allowChromeProcess = true;

  const client = new DevToolsClient(DevToolsServer.connectPipe());
  await client.connect();
  return new ClientWrapper(client);
}

async function createNetworkClient(host, port) {
  const transport = await DevToolsClient.socketConnect({ host, port });
  const client = new DevToolsClient(transport);
  await client.connect();
  return new ClientWrapper(client);
}

async function createUSBClient(deviceId, socketPath) {
  const port = await prepareTCPConnection(deviceId, socketPath);
  return createNetworkClient("localhost", port);
}

async function createClientForRuntime(runtime) {
  const { extra, id, type } = runtime;

  if (type === RUNTIMES.THIS_FIREFOX) {
    return createLocalClient();
  } else if (remoteClientManager.hasClient(id, type)) {
    const client = remoteClientManager.getClient(id, type);
    return new ClientWrapper(client);
  } else if (type === RUNTIMES.NETWORK) {
    const { host, port } = extra.connectionParameters;
    return createNetworkClient(host, port);
  } else if (type === RUNTIMES.USB) {
    const { deviceId, socketPath } = extra.connectionParameters;
    return createUSBClient(deviceId, socketPath);
  }

  return null;
}

exports.createClientForRuntime = createClientForRuntime;
