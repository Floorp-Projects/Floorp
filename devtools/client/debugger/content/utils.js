/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { promiseInvoke } = require("devtools/shared/async-utils");
const { reportException } = require("devtools/shared/DevToolsUtils");

function rdpInvoke(client, method, ...args) {
  return promiseInvoke(client, method, ...args)
    .then((packet) => {
      let { error, message } = packet;
      if (error) {
        throw new Error(error + ": " + message);
      }

      return packet;
    });
}

function asPaused(client, func) {
  if (client.state != "paused") {
    return Task.spawn(function*() {
      yield rdpInvoke(client, client.interrupt);
      let result;

      try {
        result = yield func();
      }
      catch(e) {
        // Try to put the debugger back in a working state by resuming
        // it
        yield rdpInvoke(client, client.resume);
        throw e;
      }

      yield rdpInvoke(client, client.resume);
      return result;
    });
  } else {
    return func();
  }
}

module.exports = { rdpInvoke, asPaused };
