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
XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");

const BROWSER_READY_NOTIFICATION = "browser-delayed-startup-finished";
const BROWSER_SESSION_STORE_NOTIFICATION = "sessionstore-windows-restored";
const PREF_WHITELIST = [
  "browser.onboarding.enabled",
  "browser.onboarding.hidden",
  "browser.onboarding.notification.finished",
  "browser.onboarding.notification.prompt-count",
  "browser.onboarding.notification.last-time-of-changing-tour-sec",
  "browser.onboarding.notification.tour-ids-queue"
];

[
  "onboarding-tour-addons",
  "onboarding-tour-customize",
  "onboarding-tour-default-browser",
  "onboarding-tour-library",
  "onboarding-tour-performance",
  "onboarding-tour-private-browsing",
  "onboarding-tour-search",
  "onboarding-tour-singlesearch",
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

let syncTourChecker = {
  registered: false,

  observe() {
    this.setComplete();
  },

  init() {
    if (Services.prefs.getBoolPref("browser.onboarding.tour.onboarding-tour-sync.completed", false)) {
      return;
    }
    // Check if we've already logged in at startup.
    fxAccounts.getSignedInUser().then(user => {
      if (user) {
        this.setComplete();
        return;
      }
      // Observe for login action if we haven't logged in yet.
      this.register();
    });
  },

  register() {
    if (this.registered) {
      return;
    }
    Services.obs.addObserver(this, "fxaccounts:onverified");
    this.registered = true;
  },

  setComplete() {
    Services.prefs.setBoolPref("browser.onboarding.tour.onboarding-tour-sync.completed", true);
    this.unregister();
  },

  unregister() {
    if (!this.registered) {
      return;
    }
    Services.obs.removeObserver(this, "fxaccounts:onverified");
    this.registered = false;
  },

  uninit() {
    this.unregister();
  },
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
      onBrowserReady();
      break;
    case BROWSER_SESSION_STORE_NOTIFICATION:
      Services.obs.removeObserver(observe, BROWSER_SESSION_STORE_NOTIFICATION);
      // Postpone Firefox account checking until "before handling user events"
      // phase to meet performance criteria. The reason we don't postpone the
      // whole onBrowserReady here is because in that way we will miss onload
      // events for onboarding.js.
      Services.tm.idleDispatchToMainThread(() => syncTourChecker.init());
      break;
  }
}

function install(aData, aReason) {}

function uninstall(aData, aReason) {}

function startup(aData, aReason) {
  // Only start Onboarding when the browser UI is ready
  if (aReason === APP_STARTUP || aReason === ADDON_INSTALL) {
    Services.obs.addObserver(observe, BROWSER_READY_NOTIFICATION);
    Services.obs.addObserver(observe, BROWSER_SESSION_STORE_NOTIFICATION);
  } else {
    onBrowserReady();
    syncTourChecker.init();
  }
}

function shutdown(aData, aReason) {
  // Stop waiting for browser to be ready
  if (waitingForBrowserReady) {
    Services.obs.removeObserver(observe, BROWSER_READY_NOTIFICATION);
  }
  syncTourChecker.uninit();
}
