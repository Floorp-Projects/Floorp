/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;
const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

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
};

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
  let top = getTopLevelWindow(event.view)
  let isBrowser = top.location.href.includes("/browser.xul") && top.gDevToolsBrowser;
  let reloadToolbox = false;
  if (isBrowser && top.gDevToolsBrowser.hasToolboxOpened) {
    reloadToolbox = top.gDevToolsBrowser.hasToolboxOpened(top);
  }
  dump("Reload DevTools.  (reload-toolbox:"+reloadToolbox+")\n");

  // Invalidate xul cache in order to see changes made to chrome:// files
  Services.obs.notifyObservers(null, "startupcache-invalidate", null);

  // Ask the loader to update itself and reopen the toolbox if needed
  const {devtools} = Cu.import("resource://devtools/shared/Loader.jsm", {});
  devtools.reload();

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
          let isJSONView = content.document.baseURI.startsWith("resource://devtools/");
          if (isJSONView) {
            content.location.reload();
          }
        }, false);
      }

      // Manually reload gcli if it has been used
      // Bug 1248348: Inject the developer toolbar dynamically within browser/
      // so that we can easily remove/reinject it
      const desc = Object.getOwnPropertyDescriptor(window, "DeveloperToolbar");
      if (desc && !desc.get) {
        let wasVisible = window.DeveloperToolbar.visible;
        window.DeveloperToolbar.hide()
          .then(() => {
            window.DeveloperToolbar.destroy();

            let { DeveloperToolbar } = devtools.require("devtools/client/shared/developer-toolbar");
            window.DeveloperToolbar = new DeveloperToolbar(window, window.document.getElementById("developer-toolbar"));
            if (wasVisible) {
              window.DeveloperToolbar.show();
            }
          });
      }
    } else if (windowtype === "devtools:webide") {
      window.location.reload();
    } else if (windowtype === "devtools:webconsole") {
      // Browser console document can't just be reloaded.
      // HUDService is going to close it on unload.
      // Instead we have to manually toggle it.
      let HUDService = devtools.require("devtools/client/webconsole/hudservice");
      HUDService.toggleBrowserConsole()
        .then(() => {
          HUDService.toggleBrowserConsole();
        });
    }
  }

  if (reloadToolbox) {
    // Reopen the toolbox automatically if we are reloading from toolbox shortcut
    // and are on a browser window.
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
}

let listener;
function startup() {
  dump("DevTools addon started.\n");
  listener = new MultiWindowKeyListener({
    keyCode: Ci.nsIDOMKeyEvent.DOM_VK_R, ctrlKey: true, altKey: true,
    callback: reload
  });
  listener.start();
}
function shutdown() {
  listener.stop();
  listener = null;
}
function install() {}
function uninstall() {}
