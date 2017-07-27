/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global content, APP_SHUTDOWN */
/* exported startup, shutdown, install, uninstall */

"use strict";

const Cu = Components.utils;
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

XPCOMUtils.defineLazyGetter(this, "DevToolsPreferences", function () {
  return Cu.import("chrome://devtools/content/preferences/DevToolsPreferences.jsm", {}).DevToolsPreferences;
});

function unload(reason) {
  // This frame script is going to be executed in all processes:
  // parent and child
  Services.ppmm.loadProcessScript("data:,(" + function (scriptReason) {
    /* Flush message manager cached frame scripts as well as chrome locales */
    let obs = Components.classes["@mozilla.org/observer-service;1"]
                        .getService(Components.interfaces.nsIObserverService);
    obs.notifyObservers(null, "message-manager-flush-caches");

    /* Also purge cached modules in child processes, we do it a few lines after
       in the parent process */
    if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
      Services.obs.notifyObservers(null, "devtools-unload", scriptReason);
    }
  } + ")(\"" + reason.replace(/"/g, '\\"') + "\")", false);

  // As we can't get a reference to existing Loader.jsm instances, we send them
  // an observer service notification to unload them.
  Services.obs.notifyObservers(null, "devtools-unload", reason);

  // Then spawn a brand new Loader.jsm instance and start the main module
  Cu.unload("resource://devtools/shared/Loader.jsm");
  // Also unload all resources loaded as jsm, hopefully all of them are going
  // to be converted into regular modules
  Cu.unload("resource://devtools/client/shared/browser-loader.js");
  Cu.unload("resource://devtools/client/framework/ToolboxProcess.jsm");
  Cu.unload("resource://devtools/shared/apps/Devices.jsm");
  Cu.unload("resource://devtools/client/scratchpad/scratchpad-manager.jsm");
  Cu.unload("resource://devtools/shared/Parser.jsm");
  Cu.unload("resource://devtools/client/shared/DOMHelpers.jsm");
  Cu.unload("resource://devtools/client/shared/widgets/VariablesView.jsm");
  Cu.unload("resource://devtools/client/responsivedesign/responsivedesign.jsm");
  Cu.unload("resource://devtools/client/shared/widgets/AbstractTreeItem.jsm");
  Cu.unload("resource://devtools/shared/deprecated-sync-thenables.js");
}

function startup(data) {
  // Load DevToolsPreferences as early as possible.
  DevToolsPreferences.loadPrefs();
}

function shutdown(data, reason) {
  // On browser shutdown, do not try to cleanup anything
  if (reason == APP_SHUTDOWN) {
    return;
  }

  unload("disable");
}

function install() {}

function uninstall() {}
