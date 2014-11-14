/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const nsIAudioManager = Ci.nsIAudioManager;
const nsITelephonyAudioService = Ci.nsITelephonyAudioService;

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";
const kPrefRilDebuggingEnabled = "ril.debugging.enabled";

const AUDIO_STATE_NAME = [
  "PHONE_STATE_NORMAL",
  "PHONE_STATE_RINGTONE",
  "PHONE_STATE_IN_CALL"
];

let DEBUG;
function debug(s) {
  dump("TelephonyAudioService: " + s + "\n");
}

XPCOMUtils.defineLazyGetter(this, "RIL", function () {
  let obj = {};
  Cu.import("resource://gre/modules/ril_consts.js", obj);
  return obj;
});

XPCOMUtils.defineLazyGetter(this, "gAudioManager", function getAudioManager() {
  try {
    return Cc["@mozilla.org/telephony/audiomanager;1"]
             .getService(nsIAudioManager);
  } catch (ex) {
    //TODO on the phone this should not fall back as silently.

    // Fake nsIAudioManager implementation so that we can run the telephony
    // code in a non-Gonk build.
    if (DEBUG) debug("Using fake audio manager.");
    return {
      microphoneMuted: false,
      masterVolume: 1.0,
      masterMuted: false,
      phoneState: nsIAudioManager.PHONE_STATE_CURRENT,
      _forceForUse: {},

      setForceForUse: function(usage, force) {
        this._forceForUse[usage] = force;
      },

      getForceForUse: function(usage) {
        return this._forceForUse[usage] || nsIAudioManager.FORCE_NONE;
      }
    };
  }
});

function TelephonyAudioService() {
  this._updateDebugFlag();
  Services.prefs.addObserver(kPrefRilDebuggingEnabled, this, false);
}
TelephonyAudioService.prototype = {
  classDescription: "TelephonyAudioService",
  classID: Components.ID("{c8605499-cfec-4cb5-a082-5f1f56d42adf}"),
  contractID: "@mozilla.org/telephony/audio-service;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITelephonyAudioService,
                                         Ci.nsIObserver]),

  _updateDebugFlag: function() {
    try {
      DEBUG = RIL.DEBUG_RIL ||
              Services.prefs.getBoolPref(kPrefRilDebuggingEnabled);
    } catch (e) {}
  },

  /**
   * nsITelephonyAudioService interface.
   */

  get microphoneMuted() {
    return gAudioManager.microphoneMuted;
  },

  set microphoneMuted(aMuted) {
    if (aMuted == this.microphoneMuted) {
      return;
    }
    gAudioManager.microphoneMuted = aMuted;
  },

  get speakerEnabled() {
    let force = gAudioManager.getForceForUse(nsIAudioManager.USE_COMMUNICATION);
    return (force == nsIAudioManager.FORCE_SPEAKER);
  },

  set speakerEnabled(aEnabled) {
    if (aEnabled == this.speakerEnabled) {
      return;
    }
    let force = aEnabled ? nsIAudioManager.FORCE_SPEAKER :
                           nsIAudioManager.FORCE_NONE;
    gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION, force);
  },

  setPhoneState: function(aState) {
    switch (aState) {
      case nsITelephonyAudioService.PHONE_STATE_NORMAL:
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_NORMAL;
        break;
      case nsITelephonyAudioService.PHONE_STATE_RINGTONE:
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_RINGTONE;
        break;
      case nsITelephonyAudioService.PHONE_STATE_IN_CALL:
        gAudioManager.phoneState = nsIAudioManager.PHONE_STATE_IN_CALL;
        if (this.speakerEnabled) {
          gAudioManager.setForceForUse(nsIAudioManager.USE_COMMUNICATION,
                                       nsIAudioManager.FORCE_SPEAKER);
        }
        break;
      default:
        throw new Error("Unknown audioPhoneState: " + aState);
    }

    if (DEBUG) {
      debug("Put audio system into " + AUDIO_STATE_NAME[aState] + ": " +
            aState + ", result is: " + gAudioManager.phoneState);
    }
  },

  /**
   * nsIObserver interface.
   */

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        if (aData === kPrefRilDebuggingEnabled) {
          this._updateDebugFlag();
        }
        break;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TelephonyAudioService]);
