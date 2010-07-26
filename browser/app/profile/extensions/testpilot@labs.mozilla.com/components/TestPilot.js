/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Weave.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
 *  Jono X <jono@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
      os.removeObserver(this, "sessionstore-windows-restored", false);
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
