const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");


// Load mocking/stubbing library, sinon
// docs: http://sinonjs.org/docs/
let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader);
loader.loadSubScript("resource://testing-common/sinon-1.16.1.js");

registerCleanupFunction(function*() {
  // Cleanup window or the test runner will throw an error
  delete window.sinon;
  delete window.setImmediate;
  delete window.clearImmediate;
});
