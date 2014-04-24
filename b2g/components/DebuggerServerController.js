/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this,
    "Settings",
    "@mozilla.org/settingsService;1", "nsISettingsService");

XPCOMUtils.defineLazyModuleGetter(this,
     "SystemAppProxy",
     "resource://gre/modules/SystemAppProxy.jsm");

function DebuggerServerController() {
}

DebuggerServerController.prototype = {
  classID: Components.ID("{9390f6ac-7914-46c6-b9d0-ccc7db550d8c}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDebuggerServerController, Ci.nsIObserver]),

  init: function init(debuggerServer) {
    this.debugger = debuggerServer;
    Services.obs.addObserver(this, "mozsettings-changed", false);
    Services.obs.addObserver(this, "debugger-server-started", false);
    Services.obs.addObserver(this, "debugger-server-stopped", false);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  uninit: function uninit() {
    this.debugger = null;
    Services.obs.removeObserver(this, "mozsettings-changed");
    Services.obs.removeObserver(this, "debugger-server-started");
    Services.obs.removeObserver(this, "debugger-server-stopped");
    Services.obs.removeObserver(this, "xpcom-shutdown");
  },

  // nsIObserver

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "xpcom-shutdown":
        this.uninit();
        break;
      case "debugger-server-started":
        this._onDebuggerStarted(data);
        break;
      case "debugger-server-stopped":
        this._onDebuggerStopped();
        break;
      case "mozsettings-changed":
        let {key, value} = JSON.parse(data);
        switch(key) {
          case "debugger.remote-mode":
            if (["disabled", "adb-only", "adb-devtools"].indexOf(value) == -1) {
              dump("Illegal value for debugger.remote-mode: " + value + "\n");
              return;
            }

            Services.prefs.setBoolPref("devtools.debugger.remote-enabled", value == "adb-devtools");
            Services.prefs.savePrefFile(null);

            if (value != "adb-devtools") {
              // The *pref* "devtools.debugger.remote-enabled" has been set to false (setBoolPref)
              // above. In theory, it's supposed to automatically stop the debugger, but maybe the
              // debugger has been started from the command line, so the value was already false,
              // so the observer is not called because the value didn't change. We need to stop
              // the debugger manually:
              this.stop();
            }
        }
    }

  },

  // nsIDebuggerController

  start: function(portOrPath) {

    if (!this.debugger.initialized) {
      // Ask for remote connections.
      this.debugger.init(Prompt.prompt.bind(Prompt));

      // /!\ Be careful when adding a new actor, especially global actors.
      // Any new global actor will be exposed and returned by the root actor.

      // Add Firefox-specific actors, but prevent tab actors to be loaded in
      // the parent process, unless we enable certified apps debugging.
      let restrictPrivileges = Services.prefs.getBoolPref("devtools.debugger.forbid-certified-apps");
      this.debugger.addBrowserActors("navigator:browser", restrictPrivileges);

      /**
       * Construct a root actor appropriate for use in a server running in B2G.
       * The returned root actor respects the factories registered with
       * DebuggerServer.addGlobalActor only if certified apps debugging is on,
       * otherwise we used an explicit limited list of global actors
       *
       * * @param connection DebuggerServerConnection
       *        The conection to the client.
       */
      let debuggerServer = this.debugger;
      debuggerServer.createRootActor = function createRootActor(connection)
      {
        let { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
        let parameters = {
          // We do not expose browser tab actors yet,
          // but we still have to define tabList.getList(),
          // otherwise, client won't be able to fetch global actors
          // from listTabs request!
          tabList: {
            getList: function() {
              return promise.resolve([]);
            }
          },
          // Use an explicit global actor list to prevent exposing
          // unexpected actors
          globalActorFactories: restrictPrivileges ? {
            webappsActor: debuggerServer.globalActorFactories.webappsActor,
            deviceActor: debuggerServer.globalActorFactories.deviceActor,
          } : debuggerServer.globalActorFactories
        };
        let root = new debuggerServer.RootActor(connection, parameters);
        root.applicationType = "operating-system";
        return root;
      };

    }

    if (portOrPath) {
      try {
        this.debugger.openListener(portOrPath);
      } catch (e) {
        dump("Unable to start debugger server (" + portOrPath + "): " + e + "\n");
      }
    }

  },

  stop: function() {
    this.debugger.destroy();
  },

  _onDebuggerStarted: function(portOrPath) {
    dump("Devtools debugger server started: " + portOrPath + "\n");
    Settings.createLock().set("debugger.remote-mode", "adb-devtools", null);
  },


  _onDebuggerStopped: function() {
    dump("Devtools debugger server stopped\n");
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DebuggerServerController]);

// =================== Prompt ====================

let Prompt = {
  _promptDone: false,
  _promptAnswer: false,
  _listenerAttached: false,

  prompt: function () {
    if (!this._listenerAttached) {
      SystemAppProxy.addEventListener("mozContentEvent", this, false, true);
      this._listenerAttached = true;
    }

    this._promptDone = false;

    SystemAppProxy._sendCustomEvent("mozChromeEvent", {
      "type": "remote-debugger-prompt"
    });


    while(!this._promptDone) {
      Services.tm.currentThread.processNextEvent(true);
    }

    return this._promptAnswer;
  },

  // Content events listener

  handleEvent: function (event) {
    if (event.detail.type == "remote-debugger-prompt") {
      this._promptAnswer = event.detail.value;
      this._promptDone = true;
    }
  }
}
