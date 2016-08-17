/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 /* globals addMessageListener, sendAsyncMessage */
"use strict";

const { utils: Cu } = Components;
const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { SimulatorCore } = require("devtools/shared/touch/simulator-core");

/**
 * Launches SimulatorCore in the content window to simulate touch events
 * This frame script is managed by `simulator.js`.
 */

var simulator = {
  messages: [
    "TouchEventSimulator:Start",
    "TouchEventSimulator:Stop",
  ],

  init() {
    this.simulatorCore = new SimulatorCore(docShell.chromeEventHandler);
    this.messages.forEach(msgName => {
      addMessageListener(msgName, this);
    });
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "TouchEventSimulator:Start":
        this.simulatorCore.start();
        sendAsyncMessage("TouchEventSimulator:Started");
        break;
      case "TouchEventSimulator:Stop":
        this.simulatorCore.stop();
        sendAsyncMessage("TouchEventSimulator:Stopped");
        break;
    }
  },
};

simulator.init();
