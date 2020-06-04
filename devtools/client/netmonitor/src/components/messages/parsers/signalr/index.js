/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const JsonHubProtocol = require("devtools/client/netmonitor/src/components/messages/parsers/signalr/JSONHubProtocol");
const HandshakeProtocol = require("devtools/client/netmonitor/src/components/messages/parsers/signalr/HandshakeProtocol");

module.exports = {
  JsonHubProtocol: JsonHubProtocol.JsonHubProtocol,
  HandshakeProtocol: HandshakeProtocol.HandshakeProtocol,
};
