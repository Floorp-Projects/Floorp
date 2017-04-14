/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SafeMode"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

const kSafeModePref = "b2g.safe_mode";
const kSafeModePage = "safe_mode.html";

function debug(aStr) {
  //dump("-*- SafeMode: " + aStr + "\n");
}

// This module is responsible for checking whether we want to start in safe
// mode or not. The flow is as follow:
// - wait for the `b2g.safe_mode` preference to be set to something different
//   than `unset` by nsAppShell
// - If it's set to `no`, just start normally.
// - If it's set to `yes`, we load a stripped down system app from safe_mode.html"
//   - This page is responsible to dispatch a mozContentEvent to us.
//   - If the user choose SafeMode, we disable all add-ons.
//   - We go on with startup.

this.SafeMode = {
  // Returns a promise that resolves when nsAppShell has set the
  // b2g.safe_mode_state_ready preference to `true`.
  _waitForPref: function() {
    debug("waitForPref");
    try {
      let currentMode = Services.prefs.getCharPref(kSafeModePref);
      debug("current mode: " + currentMode);
      if (currentMode !== "unset") {
        return Promise.resolve();
      }
    } catch(e) { debug("No current mode available!"); }

    // Wait for the preference to toggle.
    return new Promise((aResolve, aReject) => {
      let observer = function(aSubject, aTopic, aData) {
        if (Services.prefs.getCharPref(kSafeModePref)) {
          Services.prefs.removeObserver(kSafeModePref, observer);
          aResolve();
        }
      }

      Services.prefs.addObserver(kSafeModePref, observer);
    });
  },

  // Resolves once the user has decided how to start.
  // Note that all the actions happen here, so there is no other action from
  // consumers than to go on.
  _waitForUser: function() {
    debug("waitForUser");
    let isSafeMode = Services.prefs.getCharPref(kSafeModePref) === "yes";
    if (!isSafeMode) {
      return Promise.resolve();
    }
    debug("Starting in Safe Mode!");

    // Load $system_app/safe_mode.html as a full screen iframe, and wait for
    // the user to make a choice.
    let shell = SafeMode.window.shell;
    let document = SafeMode.window.document;
    SafeMode.window.screen.mozLockOrientation("portrait");

    let url = Services.io.newURI(shell.homeURL)
                         .resolve(kSafeModePage);
    debug("Registry is ready, loading " + url);
    let frame = document.createElementNS("http://www.w3.org/1999/xhtml", "html:iframe");
    frame.setAttribute("mozbrowser", "true");
    frame.setAttribute("id", "systemapp"); // To keep screen.js happy.
    let contentBrowser = document.body.appendChild(frame);

    return new Promise((aResolve, aReject) => {
      let content = contentBrowser.contentWindow;

      // Stripped down version of the system app bootstrap.
      function handleEvent(e) {
        switch(e.type) {
          case "mozbrowserloadstart":
            if (content.document.location == "about:blank") {
              contentBrowser.addEventListener("mozbrowserlocationchange", handleEvent, true);
              contentBrowser.removeEventListener("mozbrowserloadstart", handleEvent, true);
              return;
            }

            notifyContentStart();
            break;
          case "mozbrowserlocationchange":
            if (content.document.location == "about:blank") {
              return;
            }

            contentBrowser.removeEventListener("mozbrowserlocationchange", handleEvent, true);
            notifyContentStart();
            break;
          case "mozContentEvent":
            content.removeEventListener("mozContentEvent", handleEvent, true);
            contentBrowser.remove();

            if (e.detail == "safemode-yes")  {
              // Really starting in safe mode, let's disable add-ons first.
              // TODO: disable add-ons
              aResolve();
            } else {
              aResolve();
            }
            break;
        }
      }

      function notifyContentStart() {
        let window = SafeMode.window;
        window.shell.sendEvent(window, "SafeModeStart");
        contentBrowser.setVisible(true);

        // browser-ui-startup-complete is used by the AppShell to stop the
        // boot animation and start gecko rendering.
        Services.obs.notifyObservers(null, "browser-ui-startup-complete", "");
        content.addEventListener("mozContentEvent", handleEvent, true);
      }

      contentBrowser.addEventListener("mozbrowserloadstart", handleEvent, true);
      contentBrowser.src = url;
    });
  },

  // Returns a Promise that resolves once we have decided to run in safe mode
  // or not. All the safe mode switching actions happen before resolving the
  // promise.
  check: function(aWindow) {
    debug("check");
    this.window = aWindow;
    if (AppConstants.platform !== "gonk") {
      // For now we only have gonk support.
      return Promise.resolve();
    }

    return this._waitForPref().then(this._waitForUser);
  }
}
