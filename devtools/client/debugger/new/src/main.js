/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import ReactDOM from "react-dom";

import { isFirefoxPanel } from "devtools-environment";

import { onConnect } from "./client";
import { teardownWorkers } from "./utils/bootstrap";
import sourceQueue from "./utils/source-queue";

function unmountRoot() {
  const mount = document.querySelector("#mount .launchpad-root");
  ReactDOM.unmountComponentAtNode(mount);
}

if (isFirefoxPanel()) {
  module.exports = {
    bootstrap: ({
      threadClient,
      tabTarget,
      debuggerClient,
      sourceMaps,
      toolboxActions
    }: any) => {
      return onConnect(
        {
          tab: { clientType: "firefox" },
          tabConnection: {
            tabTarget,
            threadClient,
            debuggerClient
          }
        },
        {
          services: { sourceMaps },
          toolboxActions
        }
      );
    },
    destroy: () => {
      unmountRoot();
      sourceQueue.clear();
      teardownWorkers();
    }
  };
} else {
  const { bootstrap, L10N } = require("devtools-launchpad");

  window.L10N = L10N;
  // $FlowIgnore:
  window.L10N.setBundle(require("../assets/panel/debugger.properties"));

  bootstrap(React, ReactDOM).then(connection => {
    onConnect(connection, {
      services: { sourceMaps: require("devtools-source-map") },
      toolboxActions: {}
    });
  });
}
