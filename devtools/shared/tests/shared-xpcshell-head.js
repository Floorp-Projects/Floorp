/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

(() => {
  const {interfaces: Ci, utils: Cu} = Components;
  const {Services} = Cu.import("resource://gre/modules/Services.jsm", {});

  // Load our bootstrap extension manifest so we can access our chrome/resource URIs.
  const EXTENSION_ID = "devtools@mozilla.org";
  let extensionDir = Services.dirsvc.get("GreD", Ci.nsIFile);
  extensionDir.append("browser");
  extensionDir.append("features");
  extensionDir.append(EXTENSION_ID);
  // If the unpacked extension doesn't exist, use the packed version.
  // Typically running tests on a local build will rely on the unpacked version
  // while running tests against a packaged Firefox build (on try for instance) will
  // rely on the xpi.
  if (!extensionDir.exists()) {
    extensionDir = extensionDir.parent;
    extensionDir.append(EXTENSION_ID + ".xpi");
  }
  Components.manager.addBootstrappedManifestLocation(extensionDir);

  // Load devtools preferences that should normally be loaded by bootstrap.js
  let {DevToolsPreferences} =
    Cu.import("chrome://devtools/content/preferences/DevToolsPreferences.jsm", {});
  DevToolsPreferences.loadPrefs();
})();
