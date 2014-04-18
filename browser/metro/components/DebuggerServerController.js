/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this,
    "Alerts",
    "@mozilla.org/alerts-service;1", "nsIAlertsService");

XPCOMUtils.defineLazyGetter(this,
    "l10n",
    () => Services.strings.createBundle("chrome://global/locale/devtools/debugger.properties"));

function DebuggerServerController() {
}

DebuggerServerController.prototype = {
  classID: Components.ID("{0849238d-6fb7-4bc4-87b7-4019bb53e01b}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDebuggerServerController, Ci.nsIObserver]),

  init: function(debuggerServer) {
    this.debugger = debuggerServer;
    Services.obs.addObserver(this, "debugger-server-started", false);
    Services.obs.addObserver(this, "debugger-server-stopped", false);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  uninit: function() {
    this.debugger = null;
    Services.obs.removeObserver(this, "debugger-server-started");
    Services.obs.removeObserver(this, "debugger-server-stopped");
    Services.obs.removeObserver(this, "xpcom-shutdown");
  },

  start: function(pathOrPort) {
    if (!this.debugger.initialized) {
      this.debugger.init();
      this.debugger.addBrowserActors();
      this.debugger.addActors("chrome://browser/content/dbg-metro-actors.js");
    }

    if (!pathOrPort) {
      // If the "devtools.debugger.unix-domain-socket" pref is set, we use a unix socket.
      // If not, we use a regular TCP socket.
      try {
        pathOrPort = Services.prefs.getCharPref("devtools.debugger.unix-domain-socket");
      } catch (e) {
        pathOrPort = Services.prefs.getIntPref("devtools.debugger.remote-port");
      }
    }

    try {
      this.debugger.openListener(pathOrPort);
    } catch (e if e.result != Cr.NS_ERROR_NOT_AVAILABLE) {
      dump("Unable to start debugger server (" + pathOrPort + "): " + e + "\n");
    }
  },

  stop: function() {
    this.debugger.closeListener(true);
  },

  // nsIObserver

  observe: function (subject, topic, data) {
    if (topic == "xpcom-shutdown")
      this.uninit();
    if (topic == "debugger-server-started")
      this._onDebuggerStarted(data);
    if (topic == "debugger-server-stopped")
      this._onDebuggerStopped();
  },

  _onDebuggerStarted: function(portOrPath) {
    let title = l10n.GetStringFromName("debuggerStartedAlert.title");
    let port = Number(portOrPath);
    let detail;
    if (port) {
      detail = l10n.formatStringFromName("debuggerStartedAlert.detailPort", [portOrPath], 1);
    } else {
      detail = l10n.formatStringFromName("debuggerStartedAlert.detailPath", [portOrPath], 1);
    }
    Alerts.showAlertNotification(null, title, detail, false, "", function(){});
  },

  _onDebuggerStopped: function() {
    let title = l10n.GetStringFromName("debuggerStopped.title");
    Alerts.showAlertNotification(null, title, null, false, "", function(){});
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DebuggerServerController]);
