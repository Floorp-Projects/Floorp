/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyGetter(this, "devtools", function() {
  const { devtools } =
    Cu.import("resource://devtools/shared/Loader.jsm", {});
  return devtools;
});

XPCOMUtils.defineLazyGetter(this, "DebuggerServer", function() {
  const { DebuggerServer } = devtools.require("devtools/server/main");
  return DebuggerServer;
});

XPCOMUtils.defineLazyGetter(this, "B2GTabList", function() {
  const { B2GTabList } =
    devtools.require("resource://gre/modules/DebuggerActors.js");
  return B2GTabList;
});

// Load the discovery module eagerly, so that it can set a device name at
// startup.  This does not cause discovery to start listening for packets, as
// that only happens once DevTools is enabled.
devtools.require("devtools/shared/discovery/discovery");

var RemoteDebugger = {
  _listening: false,

  /**
   * Prompt the user to accept or decline the incoming connection.
   *
   * @param session object
   *        The session object will contain at least the following fields:
   *        {
   *          authentication,
   *          client: {
   *            host,
   *            port
   *          },
   *          server: {
   *            host,
   *            port
   *          }
   *        }
   *        Specific authentication modes may include additional fields.  Check
   *        the different |allowConnection| methods in
   *        devtools/shared/security/auth.js.
   * @return An AuthenticationResult value.
   *         A promise that will be resolved to the above is also allowed.
   */
  allowConnection(session) {
    if (this._promptingForAllow) {
      // Don't stack connection prompts if one is already open
      return DebuggerServer.AuthenticationResult.DENY;
    }
    this._listen();

    this._promptingForAllow = new Promise(resolve => {
      this._handleAllowResult = detail => {
        this._handleAllowResult = null;
        this._promptingForAllow = null;
        // Newer Gaia supplies |authResult|, which is one of the
        // AuthenticationResult values.
        if (detail.authResult) {
          resolve(detail.authResult);
        } else if (detail.value) {
          resolve(DebuggerServer.AuthenticationResult.ALLOW);
        } else {
          resolve(DebuggerServer.AuthenticationResult.DENY);
        }
      };

      shell.sendChromeEvent({
        type: "remote-debugger-prompt",
        session
      });
    });

    return this._promptingForAllow;
  },

  /**
   * During OOB_CERT authentication, the user must transfer some data through some
   * out of band mechanism from the client to the server to authenticate the
   * devices.
   *
   * This implementation instructs Gaia to continually capture images which are
   * passed back here and run through a QR decoder.
   *
   * @return An object containing:
   *         * sha256: hash(ClientCert)
   *         * k     : K(random 128-bit number)
   *         A promise that will be resolved to the above is also allowed.
   */
  receiveOOB() {
    if (this._receivingOOB) {
      return this._receivingOOB;
    }
    this._listen();

    const QR = devtools.require("devtools/shared/qrcode/index");
    this._receivingOOB = new Promise((resolve, reject) => {
      this._handleAuthEvent = detail => {
        debug(detail.action);
        if (detail.action === "abort") {
          this._handleAuthEvent = null;
          this._receivingOOB = null;
          reject();
          return;
        }

        if (detail.action !== "capture") {
          return;
        }

        let url = detail.url;
        QR.decodeFromURI(url).then(data => {
          debug("Got auth data: " + data);
          let oob = JSON.parse(data);

          shell.sendChromeEvent({
            type: "devtools-auth",
            action: "stop"
          });

          this._handleAuthEvent = null;
          this._receivingOOB = null;
          resolve(oob);
        }).catch(() => {
          debug("No auth data, requesting new capture");
          shell.sendChromeEvent({
            type: "devtools-auth",
            action: "capture"
          });
        });
      };

      // Show QR scanning dialog, get an initial capture
      shell.sendChromeEvent({
        type: "devtools-auth",
        action: "start"
      });
    });

    return this._receivingOOB;
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
    if (detail.type === "remote-debugger-prompt" && this._handleAllowResult) {
      this._handleAllowResult(detail);
    }
    if (detail.type === "devtools-auth" && this._handleAuthEvent) {
      this._handleAuthEvent(detail);
    }
  },

  initServer: function() {
    if (DebuggerServer.initialized) {
      return;
    }

    // Ask for remote connections.
    DebuggerServer.init();

    // /!\ Be careful when adding a new actor, especially global actors.
    // Any new global actor will be exposed and returned by the root actor.

    // Add Firefox-specific actors, but prevent tab actors to be loaded in
    // the parent process, unless we enable certified apps debugging.
    let restrictPrivileges = Services.prefs.getBoolPref("devtools.debugger.forbid-certified-apps");
    DebuggerServer.addBrowserActors("navigator:browser", restrictPrivileges);

    // Allow debugging of chrome for any process
    if (!restrictPrivileges) {
      DebuggerServer.allowChromeProcess = true;
    }

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
      let parameters = {
        tabList: new B2GTabList(connection),
        // Use an explicit global actor list to prevent exposing
        // unexpected actors
        globalActorFactories: restrictPrivileges ? {
          webappsActor: DebuggerServer.globalActorFactories.webappsActor,
          deviceActor: DebuggerServer.globalActorFactories.deviceActor,
          settingsActor: DebuggerServer.globalActorFactories.settingsActor
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

RemoteDebugger.allowConnection =
  RemoteDebugger.allowConnection.bind(RemoteDebugger);
RemoteDebugger.receiveOOB =
  RemoteDebugger.receiveOOB.bind(RemoteDebugger);

var USBRemoteDebugger = {

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
      let AuthenticatorType = DebuggerServer.Authenticators.get("PROMPT");
      let authenticator = new AuthenticatorType.Server();
      authenticator.allowConnection = RemoteDebugger.allowConnection;
      this._listener = DebuggerServer.createListener();
      this._listener.portOrPath = portOrPath;
      this._listener.authenticator = authenticator;
      this._listener.open();
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

var WiFiRemoteDebugger = {

  start: function() {
    if (this._listener) {
      return;
    }

    RemoteDebugger.initServer();

    try {
      debug("Starting WiFi debugger");
      let AuthenticatorType = DebuggerServer.Authenticators.get("OOB_CERT");
      let authenticator = new AuthenticatorType.Server();
      authenticator.allowConnection = RemoteDebugger.allowConnection;
      authenticator.receiveOOB = RemoteDebugger.receiveOOB;
      this._listener = DebuggerServer.createListener();
      this._listener.portOrPath = -1 /* any available port */;
      this._listener.authenticator = authenticator;
      this._listener.discoverable = true;
      this._listener.encryption = true;
      this._listener.open();
      let port = this._listener.port;
      debug("Started WiFi debugger on " + port);
    } catch (e) {
      debug("Unable to start WiFi debugger server: " + e);
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
