"use strict";

// List these manually due to bug 1366719.
/* global Cc, Ci, Cu */
const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

// Load our bootstrap extension manifest so we can access our chrome/resource URIs.
// Cargo culted from formautofill system add-on
const EXTENSION_ID = "shield-recipe-client@mozilla.org";
let extensionDir = Services.dirsvc.get("GreD", Ci.nsIFile);
extensionDir.append("browser");
extensionDir.append("features");
extensionDir.append(EXTENSION_ID);
// If the unpacked extension doesn't exist, use the packed version.
if (!extensionDir.exists()) {
  extensionDir = extensionDir.parent;
  extensionDir.append(EXTENSION_ID + ".xpi");
}
Components.manager.addBootstrappedManifestLocation(extensionDir);

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
