/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, utils: Cu, interfaces: Ci} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

/**
  * The startupRecorder component observes notifications at various stages of
  * startup and records the set of JS components and modules that were already
  * loaded at each of these points.
  * The records are meant to be used by startup tests in
  * browser/base/content/test/performance
  * This component only exists in nightly and debug builds, it doesn't ship in
  * our release builds.
  */
function startupRecorder() {
  this.wrappedJSObject = this;
  this.loader = Cc["@mozilla.org/moz/jsloader;1"].getService(Ci.xpcIJSModuleLoader);
  this.data = {};
}
startupRecorder.prototype = {
  classID: Components.ID("{11c095b2-e42e-4bdf-9dd0-aed87595f6a4}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  record(name) {
    this.data[name] = {
      components: this.loader.loadedComponents(),
      modules: this.loader.loadedModules()
    };
  },

  observe(subject, topic, data) {

    if (topic == "app-startup") {
      // We can't ensure our observer will be called first or last, so the list of
      // topics we observe here should avoid the topics used to trigger things
      // during startup (eg. the topics observed by nsBrowserGlue.js).
      let topics = [
        "profile-do-change", // This catches stuff loaded during app-startup
        "toplevel-window-ready", // Catches stuff from final-ui-startup
        "widget-first-paint",
        "sessionstore-windows-restored",
      ];
      for (let t of topics)
        Services.obs.addObserver(this, t);
      return;
    }

    Services.obs.removeObserver(this, topic);

    if (topic == "sessionstore-windows-restored") {
      // We use idleDispatch here to record the set of loaded scripts after we
      // are fully done with startup and ready to react to user events.
      Services.tm.mainThread.idleDispatch(
        this.record.bind(this, "before handling user events"));
    } else {
      const topicsToNames = {
        "profile-do-change": "before profile selection",
        "toplevel-window-ready": "before opening first browser window",
        "widget-first-paint": "before first paint",
      };
      this.record(topicsToNames[topic]);
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([startupRecorder]);
