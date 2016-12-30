const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function() {
  return Components.utils.import("resource://gre/modules/FxAccountsCommon.js", {});
});

Cu.import("resource://gre/modules/Timer.jsm");

do_get_profile(); // fxa needs a profile directory for storage.

// Create a window polyfill so sinon can load
let window = {
    document: {},
    location: {},
    setTimeout,
    setInterval,
    clearTimeout,
    clearinterval: clearInterval
};
let self = window;

// Load mocking/stubbing library, sinon
// docs: http://sinonjs.org/docs/
/* global sinon */
let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader);
loader.loadSubScript("resource://testing-common/sinon-1.16.1.js");
