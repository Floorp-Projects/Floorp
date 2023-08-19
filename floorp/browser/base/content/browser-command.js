/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function OpenChromeDirectory() {
  const currProfDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  const profileDir = currProfDir.path;
  const nsLocalFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile", "initWithPath");
  new nsLocalFile(profileDir).reveal();
}

function restartbrowser() {
  Services.obs.notifyObservers(null, "startupcache-invalidate");

  const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  env.set("MOZ_DISABLE_SAFE_MODE_KEY", "1");

  Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
}

Services.obs.addObserver(restartbrowser, "floorp-restart-browser");

const sss = Components.classes["@mozilla.org/content/style-sheet-service;1"]
          .getService(Components.interfaces.nsIStyleSheetService);
const ios = Cc["@mozilla.org/network/io-service;1"]
          .getService(Components.interfaces.nsIIOService);

function loadStyleSheetWithNsStyleSheetService(styleSheetURL) {
  const uri = ios.newURI(styleSheetURL, null, null);
  sss.loadAndRegisterSheet(uri, sss.USER_SHEET);
}

function checkProvidedStyleSheetLoaded(styleSheetURL) {
  const uri = ios.newURI(styleSheetURL, null, null);
  if (!sss.sheetRegistered(uri, sss.USER_SHEET)) {
    sss.loadAndRegisterSheet(uri, sss.USER_SHEET);
  }
}

function unloadStyleSheetWithNsStyleSheetService(styleSheetURL) {
  const uri = ios.newURI(styleSheetURL, null, null);
  sss.unregisterSheet(uri, sss.USER_SHEET);
}

