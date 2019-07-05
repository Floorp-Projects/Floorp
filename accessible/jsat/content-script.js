/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

ChromeUtils.defineModuleGetter(
  this,
  "Logger",
  "resource://gre/modules/accessibility/Utils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Utils",
  "resource://gre/modules/accessibility/Utils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "EventManager",
  "resource://gre/modules/accessibility/EventManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ContentControl",
  "resource://gre/modules/accessibility/ContentControl.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "States",
  "resource://gre/modules/accessibility/Constants.jsm"
);

Logger.info("content-script.js", content.document.location);

function onStop(m) {
  Logger.debug("AccessFu:Stop");

  removeMessageListener("AccessFu:Stop", onStop);

  this._jsat_eventManager.stop();
  this._jsat_contentControl.stop();
}

addMessageListener("AccessFu:Stop", onStop);

if (!this._jsat_contentControl) {
  this._jsat_contentControl = new ContentControl(this);
}
this._jsat_contentControl.start();

if (!this._jsat_eventManager) {
  this._jsat_eventManager = new EventManager(this);
}
this._jsat_eventManager.start();

function contentStarted() {
  let accDoc = Utils.AccService.getAccessibleFor(content.document);
  if (accDoc && !Utils.getState(accDoc).contains(States.BUSY)) {
    sendAsyncMessage("AccessFu:ContentStarted");
  } else {
    content.setTimeout(contentStarted, 0);
  }
}

if (Utils.inTest) {
  // During a test we want to wait for the document to finish loading for
  // consistency.
  contentStarted();
}
