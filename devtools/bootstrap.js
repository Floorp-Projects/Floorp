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
  devtools.reload(reloadToolbox);

  // Also tells gDevTools to reload its dependencies
  const {gDevTools} = Cu.import("resource://devtools/client/framework/gDevTools.jsm", {});
  gDevTools.reload();
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
