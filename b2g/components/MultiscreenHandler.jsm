/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MultiscreenHandler"];

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

function debug(aStr) {
  // dump("MultiscreenHandler: " + aStr + "\n");
}

let window = Services.wm.getMostRecentWindow("navigator:browser");

// Multi-screen support on b2g. The following implementation will open a new
// top-level window once we receive a display connected event.
let MultiscreenHandler = {

  topLevelWindows: new Map(),

  init: function init() {
    Services.obs.addObserver(this, "display-changed", false);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  uninit: function uninit() {
    Services.obs.removeObserver(this, "display-changed");
    Services.obs.removeObserver(this, "xpcom-shutdown");
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "display-changed":
        this.handleDisplayChangeEvent(aSubject);
        break
      case "xpcom-shutdown":
        this.uninit();
        break
    }
  },

  openTopLevelWindow: function openTopLevelWindow(aDisplay) {
    if (this.topLevelWindows.get(aDisplay.id)) {
      debug("Top level window for display id: " + aDisplay.id + " has been opened.");
      return;
    }

    let flags = Services.prefs.getCharPref("toolkit.defaultChromeFeatures") +
                ",mozDisplayId=" + aDisplay.id;
    let remoteShellURL = Services.prefs.getCharPref("b2g.multiscreen.chrome_remote_url") +
                         "#" + aDisplay.id;
    let win = Services.ww.openWindow(null, remoteShellURL, "myTopWindow" + aDisplay.id, flags, null);

    this.topLevelWindows.set(aDisplay.id, win);
  },

  closeTopLevelWindow: function closeTopLevelWindow(aDisplay) {
    let win = this.topLevelWindows.get(aDisplay.id);

    if (win) {
      win.close();
      this.topLevelWindows.delete(aDisplay.id);
    }
  },

  handleDisplayChangeEvent: function handleDisplayChangeEvent(aSubject) {

    let display = aSubject.QueryInterface(Ci.nsIDisplayInfo);
    let name = "multiscreen.enabled";
    let req = window.navigator.mozSettings.createLock().get(name);

    req.addEventListener("success", () => {
      let isMultiscreenEnabled = req.result[name];
      if (display.connected) {
        if (isMultiscreenEnabled) {
          this.openTopLevelWindow(display);
        }
      } else {
        this.closeTopLevelWindow(display);
      }
    });
  },

};

MultiscreenHandler.init();
this.MultiscreenHandler = MultiscreenHandler;
