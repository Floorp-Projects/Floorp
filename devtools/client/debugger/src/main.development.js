/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import ReactDOM from "react-dom";

import { onConnect } from "./client";

const { bootstrap, L10N } = require("devtools-launchpad");

window.L10N = L10N;
// $FlowIgnore:
window.L10N.setBundle(require("../../locales/en-US/debugger.properties"));

bootstrap(React, ReactDOM).then(connection => {
  onConnect(connection, require("devtools-source-map"), {
    emit: eventName => console.log(`emitted: ${eventName}`),
    openLink: url => {
      const win = window.open(url, "_blank");
      win.focus();
    },
    openInspector: () => console.log("opening inspector"),
    openElementInInspector: grip =>
      alert(`Opening node in Inspector: ${grip.class}`),
    openConsoleAndEvaluate: input => alert(`console.log: ${input}`),
    highlightDomElement: (grip: Object) =>
      console.log("highlighting dom element"),
    unHighlightDomElement: (grip: Object) =>
      console.log("unhighlighting dom element"),
    getToolboxStore: () => {
      throw new Error("Cannot connect to Toolbox store when running Launchpad");
    },
    panelWin: window,
  });
});
