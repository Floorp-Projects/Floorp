/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals  APP_STARTUP, ADDON_INSTALL */
"use strict";

const {utils: Cu, interfaces: Ci} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OnboardingTourType",
  "resource://onboarding/modules/OnboardingTourType.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");

const {PREF_STRING, PREF_BOOL, PREF_INT} = Ci.nsIPrefBranch;

const BROWSER_READY_NOTIFICATION = "browser-delayed-startup-finished";
const BROWSER_SESSION_STORE_NOTIFICATION = "sessionstore-windows-restored";
const PREF_WHITELIST = [
  ["browser.onboarding.enabled", PREF_BOOL],
  ["browser.onboarding.state", PREF_STRING],
  ["browser.onboarding.notification.finished", PREF_BOOL],
  ["browser.onboarding.notification.prompt-count", PREF_INT],
  ["browser.onboarding.notification.last-time-of-changing-tour-sec", PREF_INT],
  ["browser.onboarding.notification.tour-ids-queue", PREF_STRING],
];

[
  "onboarding-tour-addons",
  "onboarding-tour-customize",
  "onboarding-tour-default-browser",
  "onboarding-tour-library",
  "onboarding-tour-performance",
  "onboarding-tour-private-browsing",
  "onboarding-tour-screenshots",
  "onboarding-tour-singlesearch",
  "onboarding-tour-sync",
].forEach(tourId => PREF_WHITELIST.push([`browser.onboarding.tour.${tourId}.completed`, PREF_BOOL]));

let waitingForBrowserReady = true;

/**
 * Set pref. Why no `getPrefs` function is due to the priviledge level.
 * We cannot set prefs inside a framescript but can read.
 * For simplicity and effeciency, we still read prefs inside the framescript.
 *
 * @param {Array} prefs the array of prefs to set.
 *   The array element carrys info to set pref, should contain
 *   - {String} name the pref name, such as `browser.onboarding.state`
 *   - {*} value the value to set
 **/
function setPrefs(prefs) {
  prefs.forEach(pref => {
    let prefObj = PREF_WHITELIST.find(([name, ]) => name == pref.name);
    if (!prefObj) {
      return;
    }

    let [name, type] = prefObj;

    switch (type) {
      case PREF_BOOL:
        Services.prefs.setBoolPref(name, pref.value);
        break;

      case PREF_INT:
        Services.prefs.setIntPref(name, pref.value);
        break;

      case PREF_STRING:
        Services.prefs.setStringPref(name, pref.value);
        break;

      default:
        throw new TypeError(`Unexpected type (${type}) for preference ${name}.`);
    }
  });
}

/**
 * syncTourChecker listens to and maintains the login status inside, and can be
 * queried at any time once initialized.
 */
let syncTourChecker = {
  _registered: false,
  _loggedIn: false,

  isLoggedIn() {
    return this._loggedIn;
  },

  observe(subject, topic) {
    switch (topic) {
      case "fxaccounts:onlogin":
        this.setComplete();
        break;
      case "fxaccounts:onlogout":
        this._loggedIn = false;
        break;
    }
  },

  init() {
    // Check if we've already logged in at startup.
    fxAccounts.getSignedInUser().then(user => {
      if (user) {
        this.setComplete();
      }
      // Observe for login action if we haven't logged in yet.
      this.register();
    });
  },

  register() {
    if (this._registered) {
      return;
    }
    Services.obs.addObserver(this, "fxaccounts:onlogin");
    Services.obs.addObserver(this, "fxaccounts:onlogout");
    this._registered = true;
  },

  setComplete() {
    this._loggedIn = true;
    Services.prefs.setBoolPref("browser.onboarding.tour.onboarding-tour-sync.completed", true);
  },

  unregister() {
    if (!this._registered) {
      return;
    }
    Services.obs.removeObserver(this, "fxaccounts:onlogin");
    Services.obs.removeObserver(this, "fxaccounts:onlogout");
    this._registered = false;
  },

  uninit() {
    this.unregister();
  },
};

/**
 * Listen and process events from content.
 */
function initContentMessageListener() {
  Services.mm.addMessageListener("Onboarding:OnContentMessage", msg => {
    switch (msg.data.action) {
      case "set-prefs":
        setPrefs(msg.data.params);
        break;
      case "get-login-status":
        msg.target.messageManager.sendAsyncMessage("Onboarding:ResponseLoginStatus", {
          isLoggedIn: syncTourChecker.isLoggedIn()
        });
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
  if (Services.startup.startingUp) {
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
