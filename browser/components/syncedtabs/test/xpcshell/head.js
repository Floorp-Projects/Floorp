const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function() {
  return Components.utils.import("resource://gre/modules/FxAccountsCommon.js", {});
});

do_get_profile(); // fxa needs a profile directory for storage.

// ================================================
// Load mocking/stubbing library, sinon
// docs: http://sinonjs.org/releases/v2.3.2/
Cu.import("resource://gre/modules/Timer.jsm");
const {Loader} = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});
const loader = new Loader.Loader({
  paths: {
    "": "resource://testing-common/",
  },
  globals: {
    setTimeout,
    setInterval,
    clearTimeout,
    clearInterval,
  },
});
const require = Loader.Require(loader, {id: ""});
const sinon = require("sinon-2.3.2");
// ================================================
