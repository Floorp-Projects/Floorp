/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

/**
 * The connect module creates a connection to a debugger server based on the current
 * context (e.g. URL parameters).
 */

const { clientFromURL } = require("devtools/client/framework/target-from-url");
const { DebuggerServer } = require("devtools/server/main");

// Supported connection types
const TYPE = {
  // Default, connected to the current instance of Firefox
  LOCAL: "LOCAL",
  // Connected to a remote instance of Firefox via host&port settings.
  REMOTE: "REMOTE",
};

/**
 * Create a plain object containing the connection information relevant to aboutdebugging
 * components.
 *
 * @returns {Object}
 *          - type: {String} from TYPE ("LOCAL", "REMOTE")
 *          - params: {Object} additional metadata depending on the type.
 *            - if type === "LOCAL", empty object
 *            - if type === "REMOTE", {host: {String}, port: {String}}
 */
function createDescriptorFromURL(url) {
  const params = url.searchParams;

  const host = params.get("host");
  const port = params.get("port");

  let descriptor;
  if (host && port) {
    descriptor = {
      type: TYPE.REMOTE,
      params: {host, port}
    };
  } else {
    descriptor = {
      type: TYPE.LOCAL,
      params: {}
    };
  }

  return descriptor;
}

/**
 * Returns a promise that resolves after creating a debugger client corresponding to the
 * provided options.
 *
 * @returns Promise that resolves an object with the following properties:
 *          - client: a DebuggerClient instance
 *          - connect: a connection descriptor, see doc for createDescriptorFromURL(url).
 */
exports.createClient = async function() {
  const href = window.location.href;
  const url = new window.URL(href.replace("about:", "http://"));

  const connect = createDescriptorFromURL(url);
  const client = await clientFromURL(url);

  DebuggerServer.allowChromeProcess = true;

  return {client, connect};
};
