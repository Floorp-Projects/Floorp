/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

const PREF_WHITELIST = [
  "browser.onboarding.enabled",
  "browser.onboarding.hidden",
  "browser.onboarding.notification.finished",
  "browser.onboarding.notification.lastPrompted"
];

/**
 * Set pref. Why no `getPrefs` function is due to the priviledge level.
 * We cannot set prefs inside a framescript but can read.
 * For simplicity and effeciency, we still read prefs inside the framescript.
 *
 * @param {Array} prefs the array of prefs to set.
 *   The array element carrys info to set pref, should contain
 *   - {String} name the pref name, such as `browser.onboarding.hidden`
 *   - {*} value the value to set
 **/
function setPrefs(prefs) {
  prefs.forEach(pref => {
    if (PREF_WHITELIST.includes(pref.name)) {
      Preferences.set(pref.name, pref.value);
    }
  });
}

function initContentMessageListener() {
  Services.mm.addMessageListener("Onboarding:OnContentMessage", msg => {
    switch (msg.data.action) {
      case "set-prefs":
        setPrefs(msg.data.params);
        break;
    }
  });
}

function install(aData, aReason) {}

function uninstall(aData, aReason) {}

function startup(aData, reason) {
  Services.mm.loadFrameScript("resource://onboarding/onboarding.js", true);
  initContentMessageListener();
}

function shutdown(aData, reason) {}
