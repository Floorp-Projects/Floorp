/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/experiments/Experiments.jsm");

function ExperimentsService() {
}

ExperimentsService.prototype = {
  _experiments: null,
  _pendingManifestUpdate: false,

  classID: Components.ID("{f7800463-3b97-47f9-9341-b7617e6d8d49}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback, Ci.nsIObserver]),

  notify: function (timer) {
    if (this._experiments) {
      this._experiments.updateManifest();
    } else {
      this._pendingManifestUpdate = true;
    }
  },

  observe: function (subject, topic, data) {
    switch(topic) {
    case "profile-after-change":
      Services.obs.addObserver(this, "xpcom-shutdown", false);
      this._experiments = Experiments.instance();
      if (this._pendingManifestUpdate) {
        this._experiments.updateManifest();
        this._pendingManifestUpdate = false;
      }
      break;
    case "xpcom-shutdown":
      Services.obs.removeObserver(this, "xpcom-shutdown");
      this._experiments.uninit();
      break;
    }
  },

  get experiments() {
    return this._experiments;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ExperimentsService]);
