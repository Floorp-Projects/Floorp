/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { dumpn } = require("devtools/shared/DevToolsUtils");
const { ConnectionManager } = require("devtools/shared/client/connection-manager");
const { runCommand } = require("./run-command");

// sends adb forward localPort devicePort
const forwardPort = function(localPort, devicePort) {
  dumpn("forwardPort " + localPort + " -- " + devicePort);
  // <host-prefix>:forward:<local>;<remote>

  return runCommand("host:forward:" + localPort + ";" + devicePort)
             .then(function onSuccess(data) {
               return data;
             });
};

// Prepare TCP connection for provided socket path.
// The returned value is a port number of localhost for the connection.
const prepareTCPConnection = async function(socketPath) {
  const port = ConnectionManager.getFreeTCPPort();
  const local = `tcp:${ port }`;
  const remote = socketPath.startsWith("@")
                   ? `localabstract:${ socketPath.substring(1) }`
                   : `localfilesystem:${ socketPath }`;
  await forwardPort(local, remote);
  return port;
};
exports.prepareTCPConnection = prepareTCPConnection;
