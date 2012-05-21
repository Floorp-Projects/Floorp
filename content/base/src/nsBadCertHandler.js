/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * This component's job is to prevent "bad cert" security dialogs from
 * being shown to the user when in XHR backgroundRequest mode. This
 * causes the request to simply fail if the certificate is bad.
 */
function BadCertHandler() {
}

BadCertHandler.prototype = {

  // Suppress any certificate errors
  notifyCertProblem: function(socketInfo, status, targetSite) {
    return true;
  },

  // Suppress any ssl errors
  notifySSLError: function(socketInfo, error, targetSite) {
    return true;
  },

  // nsIInterfaceRequestor
  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBadCertListener2,
                                         Ci.nsISSLErrorListener,
                                         Ci.nsIInterfaceRequestor]),

  classID:          Components.ID("{dbded6ec-edbf-4054-a834-287b82c260f9}"),
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([BadCertHandler]);
