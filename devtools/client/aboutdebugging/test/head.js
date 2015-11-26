/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {utils: Cu, classes: Cc, interfaces: Ci} = Components;

const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
DevToolsUtils.testing = true;

const CHROME_ROOT = gTestPath.substr(0, gTestPath.lastIndexOf("/") + 1);

registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
});

function openAboutDebugging() {
  info("opening about:debugging");
  return addTab("about:debugging").then(tab => {
    let browser = tab.linkedBrowser;
    return {
      tab,
      document: browser.contentDocument,
      window: browser.contentWindow
    };
  });
}

function closeAboutDebugging(tab) {
  info("Closing about:debugging");
  return removeTab(tab);
}

function addTab(aUrl, aWindow) {
  info("Adding tab: " + aUrl);

  return new Promise(done => {
    let targetWindow = aWindow || window;
    let targetBrowser = targetWindow.gBrowser;

    targetWindow.focus();
    let tab = targetBrowser.selectedTab = targetBrowser.addTab(aUrl);
    let linkedBrowser = tab.linkedBrowser;

    linkedBrowser.addEventListener("load", function onLoad() {
      linkedBrowser.removeEventListener("load", onLoad, true);
      info("Tab added and finished loading: " + aUrl);
      done(tab);
    }, true);
  });
}

function removeTab(aTab, aWindow) {
  info("Removing tab.");

  return new Promise(done => {
    let targetWindow = aWindow || window;
    let targetBrowser = targetWindow.gBrowser;
    let tabContainer = targetBrowser.tabContainer;

    tabContainer.addEventListener("TabClose", function onClose(aEvent) {
      tabContainer.removeEventListener("TabClose", onClose, false);
      info("Tab removed and finished closing.");
      done();
    }, false);

    targetBrowser.removeTab(aTab);
  });
}

function get_supports_file(path) {
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"].
  getService(Ci.nsIChromeRegistry);
  let fileurl = cr.convertChromeURL(Services.io.newURI(CHROME_ROOT + path, null, null));
  return fileurl.QueryInterface(Ci.nsIFileURL);
}
