/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function debug(str) {
  dump("CHROME PERMISSON HANDLER -- " + str + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const { Services } = Cu.import("resource://gre/modules/Services.jsm");

let browser = Services.wm.getMostRecentWindow("navigator:browser");
let shell;

function loadShell() {
  if (!browser) {
    debug("no browser");
    return false;
  }
  shell = browser.shell;
  return true;
}

function getContentWindow() {
  return shell.contentBrowser.contentWindow;
}

if (loadShell()) {
  let content = getContentWindow();
  let eventHandler = function(evt) {
    if (!evt.detail || evt.detail.type !== "permission-prompt") {
      return;
    }

    sendAsyncMessage("permission-request", evt.detail.permissions);
  };

  content.addEventListener("mozChromeEvent", eventHandler);

  // need to remove ChromeEvent listener after test finished.
  addMessageListener("teardown", function() {
    content.removeEventListener("mozChromeEvent", eventHandler);
  });
}

