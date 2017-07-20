/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals  APP_STARTUP, ADDON_INSTALL */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OnboardingTourType",
  "resource://onboarding/modules/OnboardingTourType.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
  "resource://gre/modules/Timer.jsm");

const BROWSER_READY_NOTIFICATION = "final-ui-startup";
const PREF_WHITELIST = [
  "browser.onboarding.enabled",
  "browser.onboarding.hidden",
  "browser.onboarding.notification.finished",
  "browser.onboarding.notification.prompt-count",
  "browser.onboarding.notification.last-time-of-changing-tour-sec",
  "browser.onboarding.notification.tour-ids-queue"
];

[
  "onboarding-tour-private-browsing",
  "onboarding-tour-addons",
  "onboarding-tour-customize",
  "onboarding-tour-default-browser",
  "onboarding-tour-library",
  "onboarding-tour-search",
  "onboarding-tour-sync",
].forEach(tourId => PREF_WHITELIST.push(`browser.onboarding.tour.${tourId}.completed`));

let waitingForBrowserReady = true;

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

/**
 * Listen and process events from content.
 */
function initContentMessageListener() {
  Services.mm.addMessageListener("Onboarding:OnContentMessage", msg => {
    switch (msg.data.action) {
      case "set-prefs":
        setPrefs(msg.data.params);
        break;
    }
  });
}

/**
 * onBrowserReady - Continues startup of the add-on after browser is ready.
 */
function onBrowserReady() {
  waitingForBrowserReady = false;

  OnboardingTourType.check();
  Services.mm.loadFrameScript("resource://onboarding/onboarding.js", true);
  initContentMessageListener();
}

/**
 * observe - nsIObserver callback to handle various browser notifications.
 */
function observe(subject, topic, data) {
  switch (topic) {
    case BROWSER_READY_NOTIFICATION:
      Services.obs.removeObserver(observe, BROWSER_READY_NOTIFICATION);
      // Avoid running synchronously during this event that's used for timing
      setTimeout(() => onBrowserReady());
      break;
  }
}

function install(aData, aReason) {}

function uninstall(aData, aReason) {}

function startup(aData, aReason) {
  // Only start Onboarding when the browser UI is ready
  if (aReason === APP_STARTUP || aReason === ADDON_INSTALL) {
    Services.obs.addObserver(observe, BROWSER_READY_NOTIFICATION);
  } else {
    onBrowserReady();
  }
}

function shutdown(aData, aReason) {
  // Stop waiting for browser to be ready
  if (waitingForBrowserReady) {
    Services.obs.removeObserver(observe, BROWSER_READY_NOTIFICATION);
  }
}
