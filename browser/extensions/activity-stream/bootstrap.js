/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals Components, XPCOMUtils, Preferences, Services, ActivityStream */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ActivityStream",
  "resource://activity-stream/lib/ActivityStream.jsm");

const BROWSER_READY_NOTIFICATION = "browser-ui-startup-complete";
const ACTIVITY_STREAM_ENABLED_PREF = "browser.newtabpage.activity-stream.enabled";
const REASON_STARTUP_ON_PREF_CHANGE = "PREF_ON";
const REASON_SHUTDOWN_ON_PREF_CHANGE = "PREF_OFF";

const ACTIVITY_STREAM_OPTIONS = {newTabURL: "about:newtab"};

let activityStream;
let startupData;
let startupReason;

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
  activityStream.init(reason);
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
    activityStream = null;
  }
}

/**
 * onPrefChanged - handler for changes to ACTIVITY_STREAM_ENABLED_PREF
 *
 * @param  {bool} isEnabled Determines whether Activity Stream is enabled
 */
function onPrefChanged(isEnabled) {
  if (isEnabled) {
    init(REASON_STARTUP_ON_PREF_CHANGE);
  } else {
    uninit(REASON_SHUTDOWN_ON_PREF_CHANGE);
  }
}

function observe(subject, topic, data) {
  switch (topic) {
    case BROWSER_READY_NOTIFICATION:
      // Listen for changes to the pref that enables Activity Stream
      Preferences.observe(ACTIVITY_STREAM_ENABLED_PREF, onPrefChanged);
      // Only initialize if the pref is true
      if (Preferences.get(ACTIVITY_STREAM_ENABLED_PREF)) {
        init(startupReason);
        Services.obs.removeObserver(this, BROWSER_READY_NOTIFICATION);
      }
      break;
  }
}

// The functions below are required by bootstrap.js

this.install = function install(data, reason) {};

this.startup = function startup(data, reason) {
  // Only start Activity Stream up when the browser UI is ready
  Services.obs.addObserver(observe, BROWSER_READY_NOTIFICATION);

  // Cache startup data which contains stuff like the version number, etc.
  // so we can use it when we init
  startupData = data;
  startupReason = reason;
};

this.shutdown = function shutdown(data, reason) {
  // Uninitialize Activity Stream
  startupData = null;
  startupReason = null;
  uninit(reason);

  // Stop listening to the pref that enables Activity Stream
  Preferences.ignore(ACTIVITY_STREAM_ENABLED_PREF, onPrefChanged);
};

this.uninstall = function uninstall(data, reason) {};
