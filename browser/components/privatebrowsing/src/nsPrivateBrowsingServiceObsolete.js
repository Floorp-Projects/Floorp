/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Ci = Components.interfaces;

// This dummy object just emulates the old nsIPrivateBrowsingService, and is
// non-functional in every aspect.  It is only used to make Jetpack work
// again.

function PrivateBrowsingService() {
}

PrivateBrowsingService.prototype = {
  classID: Components.ID("{ba0e4d3d-7be2-41a0-b723-a7c16b22ebe9}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrivateBrowsingService])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PrivateBrowsingService]);
