/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */
/* global api, CopyPasteAssistent */

"use strict";

function debug(msg) {
  // dump("BrowserElementChild - " + msg + "\n");
}

var BrowserElementIsReady;

debug(`Might load BE scripts: BEIR: ${BrowserElementIsReady}`);
if (!BrowserElementIsReady) {
  debug("Loading BE scripts");
  if (!("BrowserElementIsPreloaded" in this)) {
    Services.scriptloader.loadSubScript(
      "chrome://global/content/BrowserElementChildPreload.js",
      this
    );
  }

  function onDestroy() {
    removeMessageListener("browser-element-api:destroy", onDestroy);

    if (api) {
      api.destroy();
    }

    BrowserElementIsReady = false;
  }
  addMessageListener("browser-element-api:destroy", onDestroy);

  BrowserElementIsReady = true;
} else {
  debug("BE already loaded, abort");
}

sendAsyncMessage("browser-element-api:call", { msg_name: "hello" });
