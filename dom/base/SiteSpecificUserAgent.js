/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const MAX_CACHE_SIZE      = 250;
const PREF_UPDATE         = "general.useragent.updates.";
const PREF_OVERRIDE       = "general.useragent.override.";
const XPCOM_SHUTDOWN      = "xpcom-shutdown";
const HTTP_PROTO_HANDLER = Cc["@mozilla.org/network/protocol;1?name=http"]
                             .getService(Ci.nsIHttpProtocolHandler);

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
  "@mozilla.org/childprocessmessagemanager;1",
  "nsISyncMessageSender");

function SiteSpecificUserAgent() {
  this.inParent = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
    .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

  if (this.inParent) {
    Cu.import("resource://gre/modules/UserAgentOverrides.jsm");
  } else {
    Cu.import("resource://gre/modules/Services.jsm");
    Services.prefs.addObserver(PREF_OVERRIDE, this, false);
    Services.prefs.addObserver(PREF_UPDATE, this, false);
    Services.obs.addObserver(this, XPCOM_SHUTDOWN, false);
    this.userAgentCache = new Map;
  }
}

SiteSpecificUserAgent.prototype = {
  getUserAgentForURIAndWindow: function ssua_getUserAgentForURIAndWindow(aURI, aWindow) {
    if (this.inParent) {
      return UserAgentOverrides.getOverrideForURI(aURI) || HTTP_PROTO_HANDLER.userAgent;
    }

    let host = aURI.asciiHost;
    let cachedResult = this.userAgentCache.get(host);
    if (cachedResult) {
      return cachedResult;
    }

    let data = { uri: aURI };
    let result = cpmm.sendSyncMessage("Useragent:GetOverride", data)[0] || HTTP_PROTO_HANDLER.userAgent;

    if (this.userAgentCache.size >= MAX_CACHE_SIZE) {
      this.userAgentCache.clear();
    }

    this.userAgentCache.set(host, result);
    return result;
  },

  invalidateCache: function() {
    this.userAgentCache.clear();
  },

  clean: function() {
    this.userAgentCache.clear();
    if (!this.inParent) {
      Services.obs.removeObserver(this, XPCOM_SHUTDOWN);
      Services.prefs.removeObserver(PREF_OVERRIDE, this);
      Services.prefs.removeObserver(PREF_UPDATE, this);
    }
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        this.invalidateCache();
        break;
      case XPCOM_SHUTDOWN:
        this.clean();
        break;
    }
  },

  classID: Components.ID("{506c680f-3d1c-4954-b351-2c80afbc37d3}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISiteSpecificUserAgent])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SiteSpecificUserAgent]);
