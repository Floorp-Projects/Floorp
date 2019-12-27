/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { prepareTCPConnection } = require("devtools/shared/adb/commands/index");
const { DebuggerClient } = require("devtools/shared/client/debugger-client");
const { DebuggerServer } = require("devtools/server/debugger-server");
const { ClientWrapper } = require("./client-wrapper");
const {
  remoteClientManager,
} = require("devtools/client/shared/remote-debugging/remote-client-manager");

const { RUNTIMES } = require("../constants");

async function createLocalClient() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  DebuggerServer.allowChromeProcess = true;

  const client = new DebuggerClient(DebuggerServer.connectPipe());
  await client.connect();
  return new ClientWrapper(client);
}

async function createNetworkClient(host, port) {
  const transport = await DebuggerClient.socketConnect({ host, port });
  const client = new DebuggerClient(transport);
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
