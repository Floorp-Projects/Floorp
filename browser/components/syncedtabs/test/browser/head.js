const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");


// Load mocking/stubbing library, sinon
// docs: http://sinonjs.org/docs/
/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

registerCleanupFunction(function* () {
  // Cleanup window or the test runner will throw an error
  delete window.sinon;
});
