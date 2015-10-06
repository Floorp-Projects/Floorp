/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var Ci = Components.interfaces;

function CookiePromptService() {
}

CookiePromptService.prototype = {
  classID: Components.ID("{509b5540-c87c-11dd-ad8b-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICookiePromptService]),

  cookieDialog: function(parent, cookie, hostname,
                         cookiesFromHost, changingCookie,
                         rememberDecision) {
    return 0;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([CookiePromptService]);
