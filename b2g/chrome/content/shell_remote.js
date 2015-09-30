/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

function debug(aStr) {
  // dump(" -*- ShellRemote.js: " + aStr + "\n");
}

let remoteShell = {

  get homeURL() {
    let systemAppManifestURL = Services.io.newURI(this.systemAppManifestURL, null, null);
    let shellRemoteURL = Services.prefs.getCharPref("b2g.multiscreen.system_remote_url");
    shellRemoteURL = Services.io.newURI(shellRemoteURL, null, systemAppManifestURL);
    return shellRemoteURL.spec;
  },

  get systemAppManifestURL() {
    return Services.prefs.getCharPref("b2g.system_manifest_url");
  },

  _started: false,

  hasStarted: function () {
    return this._started;
  },

  start: function () {
    this._started = true;

    let homeURL = this.homeURL;
    if (!homeURL) {
      debug("ERROR! Remote home URL undefined.");
      return;
    }
    let manifestURL = this.systemAppManifestURL;
    // <html:iframe id="remote-systemapp"
    //              mozbrowser="true" allowfullscreen="true"
    //              src="blank.html"/>
    let systemAppFrame =
      document.createElementNS("http://www.w3.org/1999/xhtml", "html:iframe");
    systemAppFrame.setAttribute("id", "remote-systemapp");
    systemAppFrame.setAttribute("mozbrowser", "true");
    systemAppFrame.setAttribute("mozapp", manifestURL);
    systemAppFrame.setAttribute("allowfullscreen", "true");
    systemAppFrame.setAttribute("src", "blank.html");

    let container = document.getElementById("container");
    this.contentBrowser = container.appendChild(systemAppFrame);
    this.contentBrowser.src = homeURL + window.location.hash;
  },

  stop: function () {
  },

};

window.onload = function() {
  if (remoteShell.hasStarted() == false) {
    remoteShell.start();
  }
};

