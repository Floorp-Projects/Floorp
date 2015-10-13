/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cc, Ci, Cu, Cr} = require("chrome");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://devtools/shared/event-emitter.js");

exports.PREF_ORIG_SOURCES = "devtools.styleeditor.source-maps-enabled";

/**
 * A PreferenceObserver observes a pref branch for pref changes.
 * It emits an event for each preference change.
 */
function PrefObserver(branchName) {
  this.branchName = branchName;
  this.branch = Services.prefs.getBranch(branchName);
  this.branch.addObserver("", this, false);

  EventEmitter.decorate(this);
}

exports.PrefObserver = PrefObserver;

PrefObserver.prototype = {
  observe: function(subject, topic, data) {
    if (topic == "nsPref:changed") {
      this.emit(this.branchName + data);
    }
  },

  destroy: function() {
    if (this.branch) {
      this.branch.removeObserver('', this);
    }
  }
};