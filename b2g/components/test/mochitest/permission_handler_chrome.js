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
let test_counts = 0;

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

function addChromeEventListener(type, listener) {
  let content = getContentWindow();
  content.addEventListener("mozChromeEvent", function chromeListener(evt) {
    if (!evt.detail || evt.detail.type !== type) {
      return;
    }

    let remove = listener(evt);
    if (remove) {
      content.removeEventListener("mozChromeEvent", chromeListener);
    }
  });
}

function checkPromptEvent(prompt_evt) {
  let detail = prompt_evt.detail;

  if (detail.permission == "audio-capture") {
    sendAsyncMessage("permission.granted", "audio-capture");
    test_counts--;
  } else if (detail.permission == "desktop-notification") {
    sendAsyncMessage("permission.granted", "desktop-notification");
    test_counts--;
  } else if (detail.permission == "geolocation") {
    sendAsyncMessage("permission.granted", "geolocation");
    test_counts--;
  }

  if (!test_counts) {
    debug("remove prompt event listener.");
    return true;
  }

  return false;
}

if (loadShell()) {
  addMessageListener("test.counts", function (counts) {
    test_counts = counts;
  });

  addChromeEventListener("permission-prompt", checkPromptEvent);
}
