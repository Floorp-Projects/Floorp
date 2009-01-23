const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function CookiePromptService() {
}

CookiePromptService.prototype = {
  classDescription: "Cookie Prompt Test Service",
  contractID: "@mozilla.org/embedcomp/cookieprompt-service;1",
  classID: Components.ID("{509b5540-c87c-11dd-ad8b-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICookiePromptService]),

  cookieDialog: function(parent, cookie, hostname,
                         cookiesFromHost, changingCookie,
                         rememberDecision) {
    return 0;
  }
};

function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule([CookiePromptService]);
}
