/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ShieldStudySavant"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

class ShieldStudySavantClass {
  constructor() {
    this.SHIELD_STUDY_SAVANT_PREF = "shield.savant.enabled";
  }

  init() {
    // check the pref in case Normandy flipped it on before we could add the pref listener
    this.shouldCollect = Services.prefs.getBoolPref(this.SHIELD_STUDY_SAVANT_PREF);
    if (this.shouldCollect) {
      this.enableCollection();
    }
    Services.prefs.addObserver(this.SHIELD_STUDY_SAVANT_PREF, this);
  }

  observe(subject, topic, data) {
    if (topic === "nsPref:changed" && data === this.SHIELD_STUDY_SAVANT_PREF) {
      // toggle state of the pref
      this.shouldCollect = !this.shouldCollect;
      if (this.shouldCollect) {
        this.enableCollection();
      } else {
        // Normandy has flipped off the pref
        this.endStudy("expired");
      }
    }
  }

  enableCollection() {
    // TODO: enable data collection
  }

  endStudy(reason) {
    this.disableCollection();
    // TODO: send endStudy ping with reason code
    this.uninit();
  }

  disableCollection() {
    // TODO: disable data collection
  }

  uninit() {
    Services.prefs.removeObserver(this.SHIELD_STUDY_SAVANT_PREF, this);
    Services.prefs.clearUserPref(this.SHIELD_STUDY_SAVANT_PREF);
  }
};

const ShieldStudySavant = new ShieldStudySavantClass();
