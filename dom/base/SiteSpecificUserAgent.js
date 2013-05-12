/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/UserAgentOverrides.jsm");

const HTTP_PROTO_HANDLER = Cc["@mozilla.org/network/protocol;1?name=http"]
                             .getService(Ci.nsIHttpProtocolHandler);

function SiteSpecificUserAgent() {}

SiteSpecificUserAgent.prototype = {
  getUserAgentForURIAndWindow: function ssua_getUserAgentForURIAndWindow(aURI, aWindow) {
    return UserAgentOverrides.getOverrideForURI(aURI) || HTTP_PROTO_HANDLER.userAgent;
  },

  classID: Components.ID("{506c680f-3d1c-4954-b351-2c80afbc37d3}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISiteSpecificUserAgent])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SiteSpecificUserAgent]);
