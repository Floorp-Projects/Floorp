/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global content */
/* exported startup, shutdown, install, uninstall */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

function actionOccurred(id) {
  let {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
  let Telemetry = require("devtools/client/shared/telemetry");
  let telemetry = new Telemetry();
  telemetry.actionOccurred(id);
}

// Helper to listen to a key on all windows
function MultiWindowKeyListener({ keyCode, ctrlKey, altKey, callback }) {
  let keyListener = function (event) {
    if (event.ctrlKey == !!ctrlKey &&
        event.altKey == !!altKey &&
        event.keyCode === keyCode) {
      callback(event);

      // Call preventDefault to avoid duplicated events when
      // doing the key stroke within a tab.
      event.preventDefault();
    }
  };

  let observer = function (window, topic, data) {
    // Listen on keyup to call keyListener only once per stroke
    if (topic === "domwindowopened") {
      window.addEventListener("keyup", keyListener);
    } else {
      window.removeEventListener("keyup", keyListener);
    }
  };

  return {
    start: function () {
      // Automatically process already opened windows
      let e = Services.ww.getWindowEnumerator();
      while (e.hasMoreElements()) {
        let window = e.getNext();
        observer(window, "domwindowopened", null);
      }
      // And listen for new ones to come
      Services.ww.registerNotification(observer);
    },

    stop: function () {
      Services.ww.unregisterNotification(observer);
      let e = Services.ww.getWindowEnumerator();
      while (e.hasMoreElements()) {
        let window = e.getNext();
        observer(window, "domwindowclosed", null);
      }
    }
  };
}

let getTopLevelWindow = function (window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsIDocShellTreeItem)
               .rootTreeItem
               .QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindow);
};

function reload(event) {
  // We automatically reload the toolbox if we are on a browser tab
  // with a toolbox already opened
  let top = getTopLevelWindow(event.view);
  let isBrowser = top.location.href.includes("/browser.xul");
  let reloadToolbox = false;
  if (isBrowser && top.gBrowser) {
    // We do not use any devtools code before the call to Loader.jsm reload as
    // any attempt to use Loader.jsm to load a module will instanciate a new
    // Loader.
    let nbox = top.gBrowser.getNotificationBox();
    reloadToolbox =
      top.document.getAnonymousElementByAttribute(nbox, "class",
        "devtools-toolbox-bottom-iframe") ||
      top.document.getAnonymousElementByAttribute(nbox, "class",
        "devtools-toolbox-side-iframe") ||
      Services.wm.getMostRecentWindow("devtools:toolbox");
  }
  let browserConsole = Services.wm.getMostRecentWindow("devtools:webconsole");
  let reopenBrowserConsole = false;
  if (browserConsole) {
    browserConsole.close();
    reopenBrowserConsole = true;
  }

  dump("Reload DevTools.  (reload-toolbox:" + reloadToolbox + ")\n");

  // Invalidate xul cache in order to see changes made to chrome:// files
  Services.obs.notifyObservers(null, "startupcache-invalidate", null);

  // This frame script is going to be executed in all processes:
  // parent and child
  Services.ppmm.loadProcessScript("data:,new " + function () {
    /* Flush message manager cached frame scripts as well as chrome locales */
    let obs = Components.classes["@mozilla.org/observer-service;1"]
                        .getService(Components.interfaces.nsIObserverService);
    obs.notifyObservers(null, "message-manager-flush-caches", null);

    /* Also purge cached modules in child processes, we do it a few lines after
       in the parent process */
    if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
      Services.obs.notifyObservers(null, "devtools-unload", "reload");
    }
  }, false);

  // As we can't get a reference to existing Loader.jsm instances, we send them
  // an observer service notification to unload them.
  Services.obs.notifyObservers(null, "devtools-unload", "reload");

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
  const {devtools} = Cu.import("resource://devtools/shared/Loader.jsm", {});
  devtools.require("devtools/client/framework/devtools-browser");

  // Go over all top level windows to reload all devtools related things
  let windowsEnum = Services.wm.getEnumerator(null);
  while (windowsEnum.hasMoreElements()) {
    let window = windowsEnum.getNext();
    let windowtype = window.document.documentElement.getAttribute("windowtype");
    if (windowtype == "navigator:browser" && window.gBrowser) {
      // Enumerate tabs on firefox windows
      for (let tab of window.gBrowser.tabs) {
        let browser = tab.linkedBrowser;
        let location = browser.documentURI.spec;
        let mm = browser.messageManager;
        // To reload JSON-View tabs and any devtools document
        if (location.startsWith("about:debugging") ||
            location.startsWith("chrome://devtools/")) {
          browser.reload();
        }
        // We have to use a frame script to query "baseURI"
        mm.loadFrameScript("data:text/javascript,new " + function () {
          let isJSONView =
            content.document.baseURI.startsWith("resource://devtools/");
          if (isJSONView) {
            content.location.reload();
          }
        }, false);
      }
    } else if (windowtype === "devtools:webide") {
      window.location.reload();
    }
  }

  if (reloadToolbox) {
    // Reopen the toolbox automatically if we are reloading from toolbox
    // shortcut and are on a browser window.
    // Wait for a second before opening the toolbox to avoid races
    // between the old and the new one.
    let {setTimeout} = Cu.import("resource://gre/modules/Timer.jsm", {});
    setTimeout(() => {
      let { TargetFactory } = devtools.require("devtools/client/framework/target");
      let { gDevTools } = devtools.require("devtools/client/framework/devtools");
      let target = TargetFactory.forTab(top.gBrowser.selectedTab);
      gDevTools.showToolbox(target);
    }, 1000);
  }

  // Browser console document can't just be reloaded.
  // HUDService is going to close it on unload.
  // Instead we have to manually toggle it.
  if (reopenBrowserConsole) {
    let HUDService = devtools.require("devtools/client/webconsole/hudservice");
    HUDService.toggleBrowserConsole();
  }

  actionOccurred("reloadAddonReload");
}

let prefs = {
  // Enable dump as some errors are only printed on the stdout
  "browser.dom.window.dump.enabled": true,
  // Enable the browser toolbox and various chrome-only features
  "devtools.chrome.enabled": true,
  "devtools.debugger.remote-enabled": true,
  // Disable the prompt to ease usage of the browser toolbox
  "devtools.debugger.prompt-connection": false,
};
let originalPrefValues = {};

let listener;
function startup() {
  dump("DevTools addon started.\n");
  listener = new MultiWindowKeyListener({
    keyCode: Ci.nsIDOMKeyEvent.DOM_VK_R, ctrlKey: true, altKey: true,
    callback: reload
  });
  listener.start();

  // Toggle development prefs and save original values
  originalPrefValues = {};
  for (let name in prefs) {
    let value = prefs[name];
    let userValue = Services.prefs.getBoolPref(name);
    // Only toggle if the pref isn't already set to the right value
    if (userValue != value) {
      Services.prefs.setBoolPref(name, value);
      originalPrefValues[name] = userValue;
    }
  }
}
function shutdown() {
  listener.stop();
  listener = null;

  // Restore preferences that used to be before the addon was installed
  for (let name in originalPrefValues) {
    let userValue = Services.prefs.getBoolPref(name);
    // Only reset the pref if it hasn't changed
    if (userValue == prefs[name]) {
      Services.prefs.setBoolPref(name, originalPrefValues[name]);
    }
  }
}
function install() {
  actionOccurred("reloadAddonInstalled");
}
function uninstall() {}
