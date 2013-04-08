/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function TestPilotComponent() {}
TestPilotComponent.prototype = {
  classDescription: "Test Pilot Component",
  contractID: "@mozilla.org/testpilot/service;1",
  classID: Components.ID("{e6e5e58f-7977-485a-b076-2f74bee2677b}"),
  _xpcom_categories: [{ category: "profile-after-change" }],
  _startupTimer: null,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function TPC__observe(subject, topic, data) {
    let os = Cc["@mozilla.org/observer-service;1"].
        getService(Ci.nsIObserverService);
    switch (topic) {
    case "profile-after-change":
      os.addObserver(this, "sessionstore-windows-restored", true);
      break;
    case "sessionstore-windows-restored":
      /* Stop oberver, to ensure that globalStartup doesn't get
       * called more than once. */
      os.removeObserver(this, "sessionstore-windows-restored");
      /* Call global startup on a timer so that it's off of the main
       * thread... delay a few seconds to give firefox time to finish
       * starting up.
       */
      this._startupTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this._startupTimer.initWithCallback(
        {notify: function(timer) {
           Cu.import("resource://testpilot/modules/setup.js");
           TestPilotSetup.globalStartup();
         }}, 10000, Ci.nsITimer.TYPE_ONE_SHOT);
      break;
    }
  }
};

const components = [TestPilotComponent];
var NSGetFactory, NSGetModule;
if (XPCOMUtils.generateNSGetFactory)
  NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
else
  NSGetModule = XPCOMUtils.generateNSGetModule(components);
