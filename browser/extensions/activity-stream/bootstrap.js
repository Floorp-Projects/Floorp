/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.importGlobalProperties(["fetch"]);

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

const ACTIVITY_STREAM_ENABLED_PREF = "browser.newtabpage.activity-stream.enabled";
const BROWSER_READY_NOTIFICATION = "sessionstore-windows-restored";
const PREF_CHANGED_TOPIC = "nsPref:changed";
const REASON_SHUTDOWN_ON_PREF_CHANGE = "PREF_OFF";
const REASON_STARTUP_ON_PREF_CHANGE = "PREF_ON";
const RESOURCE_BASE = "resource://activity-stream";

const ACTIVITY_STREAM_OPTIONS = {newTabURL: "about:newtab"};

let activityStream;
let modulesToUnload = new Set();
let startupData;
let startupReason;
let waitingForBrowserReady = true;

// Lazily load ActivityStream then find related modules to unload
XPCOMUtils.defineLazyModuleGetter(this, "ActivityStream",
  "resource://activity-stream/lib/ActivityStream.jsm", null, null, () => {
    // Helper to fetch a resource directory listing and call back with each item
    const processListing = async(uri, cb) => (await (await fetch(uri)).text())
      .split("\n").slice(2).forEach(line => cb(line.split(" ").slice(1)));

    // Look for modules one level deeper than the top resource URI
    processListing(RESOURCE_BASE, ([directory, , , type]) => {
      if (type === "DIRECTORY") {
        // Look into this directory for .jsm files
        const subDir = `${RESOURCE_BASE}/${directory}`;
        processListing(subDir, ([name]) => {
          if (name && name.search(/\.jsm$/) !== -1) {
            modulesToUnload.add(`${subDir}/${name}`);
          }
        });
      }
    });
  });

/**
 * init - Initializes an instance of ActivityStream. This could be called by
 *        the startup() function exposed by bootstrap.js, or it could be called
 *        when ACTIVITY_STREAM_ENABLED_PREF is changed from false to true.
 *
 * @param  {string} reason - Reason for initialization. Could be install, upgrade, or PREF_ON
 */
function init(reason) {
  // Don't re-initialize
  if (activityStream && activityStream.initialized) {
    return;
  }
  const options = Object.assign({}, startupData || {}, ACTIVITY_STREAM_OPTIONS);
  activityStream = new ActivityStream(options);
  try {
    activityStream.init(reason);
  } catch (e) {
    Cu.reportError(e);
  }
}

/**
 * uninit - Uninitializes the activityStream instance, if it exsits.This could be
 *          called by the shutdown() function exposed by bootstrap.js, or it could
 *          be called when ACTIVITY_STREAM_ENABLED_PREF is changed from true to false.
 *
 * @param  {type} reason Reason for uninitialization. Could be uninstall, upgrade, or PREF_OFF
 */
function uninit(reason) {
  if (activityStream) {
    activityStream.uninit(reason);
  }
}

/**
 * onPrefChanged - handler for changes to ACTIVITY_STREAM_ENABLED_PREF
 *
 */
function onPrefChanged() {
  if (Services.prefs.getBoolPref(ACTIVITY_STREAM_ENABLED_PREF, false)) {
    init(REASON_STARTUP_ON_PREF_CHANGE);
  } else {
    uninit(REASON_SHUTDOWN_ON_PREF_CHANGE);
  }
}

/**
 * onBrowserReady - Continues startup of the add-on after browser is ready.
 */
function onBrowserReady() {
  waitingForBrowserReady = false;

  // Listen for changes to the pref that enables Activity Stream
  Services.prefs.addObserver(ACTIVITY_STREAM_ENABLED_PREF, observe);

  // Only initialize if the pref is true
  if (Services.prefs.getBoolPref(ACTIVITY_STREAM_ENABLED_PREF, false)) {
    init(startupReason);
  }
}

/**
 * observe - nsIObserver callback to handle various browser notifications.
 */
function observe(subject, topic, data) {
  switch (topic) {
    case BROWSER_READY_NOTIFICATION:
      Services.obs.removeObserver(observe, BROWSER_READY_NOTIFICATION);
      // Avoid running synchronously during this event that's used for timing
      Services.tm.dispatchToMainThread(() => onBrowserReady());
      break;
    case PREF_CHANGED_TOPIC:
      if (data == ACTIVITY_STREAM_ENABLED_PREF) {
        onPrefChanged();
      }
      break;
  }
}

// The functions below are required by bootstrap.js

this.install = function install(data, reason) {};

this.startup = function startup(data, reason) {
  // Cache startup data which contains stuff like the version number, etc.
  // so we can use it when we init
  startupData = data;
  startupReason = reason;

  // Only start Activity Stream up when the browser UI is ready
  if (Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup).startingUp) {
    Services.obs.addObserver(observe, BROWSER_READY_NOTIFICATION);
  } else {
    // Handle manual install or automatic install after manual uninstall
    onBrowserReady();
  }
};

this.shutdown = function shutdown(data, reason) {
  // Uninitialize Activity Stream
  startupData = null;
  startupReason = null;
  uninit(reason);

  // Stop waiting for browser to be ready
  if (waitingForBrowserReady) {
    Services.obs.removeObserver(observe, BROWSER_READY_NOTIFICATION);
  } else {
    // Stop listening to the pref that enables Activity Stream
    Services.prefs.removeObserver(ACTIVITY_STREAM_ENABLED_PREF, observe);
  }

  // Unload any add-on modules that might might have been imported
  modulesToUnload.forEach(Cu.unload);
};

this.uninstall = function uninstall(data, reason) {
  if (activityStream) {
    activityStream.uninstall(reason);
    activityStream = null;
  }
};
