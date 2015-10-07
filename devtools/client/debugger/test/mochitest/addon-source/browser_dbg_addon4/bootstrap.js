/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { interfaces: Ci, classes: Cc, utils: Cu } = Components;

function notify() {
  // Log objects so makeDebuggeeValue can get the global to use
  console.log({ msg: "Hello again" });
}

function startup(aParams, aReason) {
  Cu.import("resource://gre/modules/Services.jsm");
  let res = Services.io.getProtocolHandler("resource")
                       .QueryInterface(Ci.nsIResProtocolHandler);
  res.setSubstitution("browser_dbg_addon4", aParams.resourceURI);

  // Load a JS module
  Cu.import("resource://browser_dbg_addon4/test.jsm");
  // Log objects so makeDebuggeeValue can get the global to use
  console.log({ msg: "Hello from the test add-on" });

  Services.obs.addObserver(notify, "addon-test-ping", false);
}

function shutdown(aParams, aReason) {
  Services.obs.removeObserver(notify, "addon-test-ping");

  // Unload the JS module
  Cu.unload("resource://browser_dbg_addon4/test.jsm");

  let res = Services.io.getProtocolHandler("resource")
                       .QueryInterface(Ci.nsIResProtocolHandler);
  res.setSubstitution("browser_dbg_addon4", null);
}
