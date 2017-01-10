/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/SystemAppProxy.jsm");

function debug(aStr) {
  // dump(" -*- ShellRemote.js: " + aStr + "\n");
}

var remoteShell = {

  _started: false,

  get homeURL() {
    let systemAppManifestURL = Services.io.newURI(this.systemAppManifestURL);
    let shellRemoteURL = Services.prefs.getCharPref("b2g.multiscreen.system_remote_url");
    shellRemoteURL = Services.io.newURI(shellRemoteURL, null, systemAppManifestURL);
    return shellRemoteURL.spec;
  },

  get systemAppManifestURL() {
    return Services.prefs.getCharPref("b2g.system_manifest_url");
  },

  hasStarted: function () {
    return this._started;
  },

  start: function () {
    this._started = true;
    this._isEventListenerReady = false;
    this.id = window.location.hash.substring(1);

    let homeURL = this.homeURL;
    if (!homeURL) {
      debug("ERROR! Remote home URL undefined.");
      return;
    }
    let manifestURL = this.systemAppManifestURL;
    // <html:iframe id="this.id"
    //              mozbrowser="true"
    //              allowfullscreen="true"
    //              src="blank.html"/>
    let systemAppFrame =
      document.createElementNS("http://www.w3.org/1999/xhtml", "html:iframe");
    systemAppFrame.setAttribute("id", this.id);
    systemAppFrame.setAttribute("mozbrowser", "true");
    systemAppFrame.setAttribute("allowfullscreen", "true");
    systemAppFrame.setAttribute("src", "blank.html");

    let container = document.getElementById("container");
    this.contentBrowser = container.appendChild(systemAppFrame);
    this.contentBrowser.src = homeURL + window.location.hash;

    window.addEventListener("unload", this);
    this.contentBrowser.addEventListener("mozbrowserloadstart", this);
  },

  stop: function () {
    window.removeEventListener("unload", this);
    this.contentBrowser.removeEventListener("mozbrowserloadstart", this);
    this.contentBrowser.removeEventListener("mozbrowserlocationchange", this, true);
    SystemAppProxy.unregisterFrameWithId(this.id);
  },

  notifyContentStart: function(evt) {
    this.contentBrowser.removeEventListener("mozbrowserloadstart", this);
    this.contentBrowser.removeEventListener("mozbrowserlocationchange", this, true);

    SystemAppProxy.registerFrameWithId(remoteShell.id, remoteShell.contentBrowser);
    SystemAppProxy.addEventListenerWithId(this.id, "mozContentEvent", this);

    let content = this.contentBrowser.contentWindow;
    content.addEventListener("load", this, true);
  },

  notifyContentWindowLoaded: function () {
    SystemAppProxy.setIsLoadedWithId(this.id);
  },

  notifyEventListenerReady: function () {
    if (this._isEventListenerReady) {
      Cu.reportError("shell_remote.js: SystemApp has already been declared as being ready.");
      return;
    }
    this._isEventListenerReady = true;
    SystemAppProxy.setIsReadyWithId(this.id);
  },

  handleEvent: function(evt) {
    debug("Got an event: " + evt.type);
    let content = this.contentBrowser.contentWindow;

    switch(evt.type) {
      case "mozContentEvent":
        if (evt.detail.type === "system-message-listener-ready") {
          this.notifyEventListenerReady();
        }
        break;
      case "load":
        if (content.document.location == "about:blank") {
          return;
        }
        content.removeEventListener("load", this, true);
        this.notifyContentWindowLoaded();
        break;
      case "mozbrowserloadstart":
        if (content.document.location == "about:blank") {
          this.contentBrowser.addEventListener("mozbrowserlocationchange", this, true);
          return;
        }
      case "mozbrowserlocationchange":
        if (content.document.location == "about:blank") {
          return;
        }
        this.notifyContentStart();
        break;
      case "unload":
        this.stop();
        break;
    }
  }
};

window.onload = function() {
  if (remoteShell.hasStarted() == false) {
    remoteShell.start();
  }
};

