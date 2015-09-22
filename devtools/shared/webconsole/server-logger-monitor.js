/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Ci} = require("chrome");
const Services = require("Services");

const {DebuggerServer} = require("devtools/server/main");
const {makeInfallible} = require("devtools/shared/DevToolsUtils");

loader.lazyGetter(this, "NetworkHelper", () => require("devtools/shared/webconsole/network-helper"));

// Helper tracer. Should be generic sharable by other modules (bug 1171927)
const trace = {
  log: function(...args) {
  }
}

const acceptableHeaders = ["x-chromelogger-data"];

/**
 * This object represents HTTP events observer. It's intended to be
 * used in e10s enabled browser only.
 *
 * Since child processes can't register HTTP event observer they use
 * this module to do the observing in the parent process. This monitor
 * is loaded through DebuggerServerConnection.setupInParent() that is executed
 * from within the child process. The execution is done by {@ServerLoggingListener}.
 * The monitor listens to HTTP events and forwards it into the right child process.
 *
 * Read more about the architecture:
 * https://github.com/mozilla/gecko-dev/blob/fx-team/devtools/server/docs/actor-e10s-handling.md
 */
var ServerLoggerMonitor = {
  // Initialization

  initialize: function() {
    this.onChildMessage = this.onChildMessage.bind(this);
    this.onDisconnectChild = this.onDisconnectChild.bind(this);
    this.onExamineResponse = this.onExamineResponse.bind(this);

    // Set of tracked message managers.
    this.messageManagers = new Set();

    // Set of registered child frames (loggers).
    this.targets = new Set();
  },

  // Parent Child Relationship

  attach: makeInfallible(function({mm, prefix}) {
    let size = this.messageManagers.size;

    trace.log("ServerLoggerMonitor.attach; ", size, arguments);

    if (this.messageManagers.has(mm)) {
      return;
    }

    this.messageManagers.add(mm);

    // Start listening for messages from the {@ServerLogger} actor
    // living in the child process.
    mm.addMessageListener("debug:server-logger", this.onChildMessage);

    // Listen to the disconnection message to clean-up.
    DebuggerServer.once("disconnected-from-child:" + prefix,
      this.onDisconnectChild);
  }),

  detach: function(mm) {
    let size = this.messageManagers.size;

    trace.log("ServerLoggerMonitor.detach; ", size);

    // Unregister message listeners
    mm.removeMessageListener("debug:server-logger", this.onChildMessage);
  },

  onDisconnectChild: function(event, mm) {
    let size = this.messageManagers.size;

    trace.log("ServerLoggerMonitor.onDisconnectChild; ",
      size, arguments);

    if (!this.messageManagers.has(mm)) {
      return;
    }

    this.detach(mm);

    this.messageManagers.delete(mm);
  },

  // Child Message Handling

  onChildMessage: function(msg) {
    let method = msg.data.method;

    trace.log("ServerLoggerMonitor.onChildMessage; ", method, msg);

    switch (method) {
      case "attachChild":
        return this.onAttachChild(msg);
      case "detachChild":
        return this.onDetachChild(msg);
      default:
        trace.log("Unknown method name: ", method);
    }
  },

  onAttachChild: function(event) {
    let target = event.target;
    let size = this.targets.size;

    trace.log("ServerLoggerMonitor.onAttachChild; size: ", size, target);

    // If this is the first child attached, register global HTTP observer.
    if (!size) {
      trace.log("ServerLoggerMonitor.onAttatchChild; Add HTTP Observer");
      Services.obs.addObserver(this.onExamineResponse,
        "http-on-examine-response", false);
    }

    // Collect child loggers. The frame element where the
    // window/document lives.
    this.targets.add(target);
  },

  onDetachChild: function(event) {
    let target = event.target;
    this.targets.delete(target);

    let size = this.targets.size;
    trace.log("ServerLoggerMonitor.onDetachChild; size: ", size, target);

    // If this is the last child process attached, unregister
    // the global HTTP observer.
    if (!size) {
      trace.log("ServerLoggerMonitor.onDetachChild; Remove HTTP Observer");
      Services.obs.removeObserver(this.onExamineResponse,
        "http-on-examine-response", false);
    }
  },

  // HTTP Observer

  onExamineResponse: makeInfallible(function(subject, topic) {
    let httpChannel = subject.QueryInterface(Ci.nsIHttpChannel);

    trace.log("ServerLoggerMonitor.onExamineResponse; ", httpChannel.name,
      this.targets);

    // Ignore requests from chrome or add-on code when we are monitoring
    // content.
    if (!httpChannel.loadInfo &&
        httpChannel.loadInfo.loadingDocument === null &&
        httpChannel.loadInfo.loadingPrincipal === Services.scriptSecurityManager.getSystemPrincipal()) {
      return;
    }

    let requestFrame = NetworkHelper.getTopFrameForRequest(httpChannel);
    if (!requestFrame) {
      return;
    }

    // Ignore requests from parent frames that aren't registered.
    if (!this.targets.has(requestFrame)) {
      return;
    }

    let headers = [];

    httpChannel.visitResponseHeaders((header, value) => {
      header = header.toLowerCase();
      if (acceptableHeaders.indexOf(header) !== -1) {
        headers.push({header: header, value: value});
      }
    });

    if (!headers.length) {
      return;
    }

    let { messageManager } = requestFrame;
    messageManager.sendAsyncMessage("debug:server-logger", {
      method: "examineHeaders",
      headers: headers,
    });

    trace.log("ServerLoggerMonitor.onExamineResponse; headers ",
      headers.length, ", ", headers);
  }),
};

/**
 * Executed automatically by the framework.
 */
function setupParentProcess(event) {
  ServerLoggerMonitor.attach(event);
}

// Monitor initialization.
ServerLoggerMonitor.initialize();

// Exports from this module
exports.setupParentProcess = setupParentProcess;
