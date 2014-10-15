/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyGetter(this, "DebuggerServer", function() {
  Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
  return DebuggerServer;
});

XPCOMUtils.defineLazyGetter(this, "devtools", function() {
  const { devtools } =
    Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
  return devtools;
});

XPCOMUtils.defineLazyGetter(this, "discovery", function() {
  return devtools.require("devtools/toolkit/discovery/discovery");
});

XPCOMUtils.defineLazyGetter(this, "B2GTabList", function() {
  const { B2GTabList } =
    devtools.require("resource://gre/modules/DebuggerActors.js");
  return B2GTabList;
});

let RemoteDebugger = {
  _promptDone: false,
  _promptAnswer: false,
  _listening: false,

  prompt: function() {
    this._listen();

    this._promptDone = false;

    shell.sendChromeEvent({
      "type": "remote-debugger-prompt"
    });

    while(!this._promptDone) {
      Services.tm.currentThread.processNextEvent(true);
    }

    return this._promptAnswer;
  },

  _listen: function() {
    if (this._listening) {
      return;
    }

    this.handleEvent = this.handleEvent.bind(this);
    let content = shell.contentBrowser.contentWindow;
    content.addEventListener("mozContentEvent", this, false, true);
    this._listening = true;
  },

  handleEvent: function(event) {
    let detail = event.detail;
    if (detail.type !== "remote-debugger-prompt") {
      return;
    }
    this._promptAnswer = detail.value;
    this._promptDone = true;
  },

  initServer: function() {
    if (DebuggerServer.initialized) {
      return;
    }

    // Ask for remote connections.
    DebuggerServer.init(this.prompt.bind(this));

    // /!\ Be careful when adding a new actor, especially global actors.
    // Any new global actor will be exposed and returned by the root actor.

    // Add Firefox-specific actors, but prevent tab actors to be loaded in
    // the parent process, unless we enable certified apps debugging.
    let restrictPrivileges = Services.prefs.getBoolPref("devtools.debugger.forbid-certified-apps");
    DebuggerServer.addBrowserActors("navigator:browser", restrictPrivileges);

    /**
     * Construct a root actor appropriate for use in a server running in B2G.
     * The returned root actor respects the factories registered with
     * DebuggerServer.addGlobalActor only if certified apps debugging is on,
     * otherwise we used an explicit limited list of global actors
     *
     * * @param connection DebuggerServerConnection
     *        The conection to the client.
     */
    DebuggerServer.createRootActor = function createRootActor(connection)
    {
      let { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
      let parameters = {
        tabList: new B2GTabList(connection),
        // Use an explicit global actor list to prevent exposing
        // unexpected actors
        globalActorFactories: restrictPrivileges ? {
          webappsActor: DebuggerServer.globalActorFactories.webappsActor,
          deviceActor: DebuggerServer.globalActorFactories.deviceActor,
        } : DebuggerServer.globalActorFactories
      };
      let { RootActor } = devtools.require("devtools/server/actors/root");
      let root = new RootActor(connection, parameters);
      root.applicationType = "operating-system";
      return root;
    };

#ifdef MOZ_WIDGET_GONK
    DebuggerServer.on("connectionchange", function() {
      AdbController.updateState();
    });
#endif
  }
};

let USBRemoteDebugger = {

  get isDebugging() {
    if (!this._listener) {
      return false;
    }

    return DebuggerServer._connections &&
           Object.keys(DebuggerServer._connections).length > 0;
  },

  start: function() {
    if (this._listener) {
      return;
    }

    RemoteDebugger.initServer();

    let portOrPath =
      Services.prefs.getCharPref("devtools.debugger.unix-domain-socket") ||
      "/data/local/debugger-socket";

    try {
      debug("Starting USB debugger on " + portOrPath);
      this._listener = DebuggerServer.openListener(portOrPath);
      // Temporary event, until bug 942756 lands and offers a way to know
      // when the server is up and running.
      Services.obs.notifyObservers(null, "debugger-server-started", null);
    } catch (e) {
      debug("Unable to start USB debugger server: " + e);
    }
  },

  stop: function() {
    if (!this._listener) {
      return;
    }

    try {
      this._listener.close();
      this._listener = null;
    } catch (e) {
      debug("Unable to stop USB debugger server: " + e);
    }
  }

};

let WiFiRemoteDebugger = {

  start: function() {
    if (this._listener) {
      return;
    }

    RemoteDebugger.initServer();

    try {
      debug("Starting WiFi debugger");
      this._listener = DebuggerServer.openListener(-1);
      let port = this._listener.port;
      debug("Started WiFi debugger on " + port);
      discovery.addService("devtools", { port: port });
    } catch (e) {
      debug("Unable to start WiFi debugger server: " + e);
    }
  },

  stop: function() {
    if (!this._listener) {
      return;
    }

    try {
      discovery.removeService("devtools");
      this._listener.close();
      this._listener = null;
    } catch (e) {
      debug("Unable to stop WiFi debugger server: " + e);
    }
  }

};

(function() {
  // Track these separately here so we can determine the correct value for the
  // pref "devtools.debugger.remote-enabled", which is true when either mode of
  // using DevTools is enabled.
  let devtoolsUSB = false;
  let devtoolsWiFi = false;

  // Keep the old setting to not break people that won't have updated
  // gaia and gecko.
  SettingsListener.observe("devtools.debugger.remote-enabled", false,
                           function(value) {
    devtoolsUSB = value;
    Services.prefs.setBoolPref("devtools.debugger.remote-enabled",
                               devtoolsUSB || devtoolsWiFi);
    // This preference is consulted during startup
    Services.prefs.savePrefFile(null);
    try {
      value ? USBRemoteDebugger.start() : USBRemoteDebugger.stop();
    } catch(e) {
      dump("Error while initializing USB devtools: " +
           e + "\n" + e.stack + "\n");
    }
  });

  SettingsListener.observe("debugger.remote-mode", "disabled", function(value) {
    if (["disabled", "adb-only", "adb-devtools"].indexOf(value) == -1) {
      dump("Illegal value for debugger.remote-mode: " + value + "\n");
      return;
    }

    devtoolsUSB = value == "adb-devtools";
    Services.prefs.setBoolPref("devtools.debugger.remote-enabled",
                               devtoolsUSB || devtoolsWiFi);
    // This preference is consulted during startup
    Services.prefs.savePrefFile(null);

    try {
      (value == "adb-devtools") ? USBRemoteDebugger.start()
                                : USBRemoteDebugger.stop();
    } catch(e) {
      dump("Error while initializing USB devtools: " +
           e + "\n" + e.stack + "\n");
    }

#ifdef MOZ_WIDGET_GONK
    AdbController.setRemoteDebuggerState(value != "disabled");
#endif
  });

  SettingsListener.observe("devtools.remote.wifi.enabled", false,
                           function(value) {
    devtoolsWiFi = value;
    Services.prefs.setBoolPref("devtools.debugger.remote-enabled",
                               devtoolsUSB || devtoolsWiFi);
    // Allow remote debugging on non-local interfaces when WiFi debug is enabled
    // TODO: Bug 1034411: Lock down to WiFi interface, instead of all interfaces
    Services.prefs.setBoolPref("devtools.debugger.force-local", !value);
    // This preference is consulted during startup
    Services.prefs.savePrefFile(null);

    try {
      value ? WiFiRemoteDebugger.start() : WiFiRemoteDebugger.stop();
    } catch(e) {
      dump("Error while initializing WiFi devtools: " +
           e + "\n" + e.stack + "\n");
    }
  });
})();
