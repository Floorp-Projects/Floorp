"use strict";

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

// Load Sinon for mocking/stubbing during tests.
// Sinon assumes that setTimeout and friends are available, and looks for a
// global object named self during initialization.
Cu.import("resource://gre/modules/Timer.jsm");
const self = {}; // eslint-disable-line no-unused-vars

const loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader);
loader.loadSubScript("resource://testing-common/sinon-1.16.1.js");
