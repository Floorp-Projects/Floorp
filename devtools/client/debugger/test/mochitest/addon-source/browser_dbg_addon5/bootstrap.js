/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function startup(aParams, aReason) {
  ChromeUtils.import("resource://gre/modules/Services.jsm");
  let res = Services.io.getProtocolHandler("resource")
                       .QueryInterface(Ci.nsIResProtocolHandler);
  res.setSubstitution("browser_dbg_addon5", aParams.resourceURI);

  // Load a JS module
  ChromeUtils.import("resource://browser_dbg_addon5/test.jsm");
}

function shutdown(aParams, aReason) {
  // Unload the JS module
  Cu.unload("resource://browser_dbg_addon5/test.jsm");

  let res = Services.io.getProtocolHandler("resource")
                       .QueryInterface(Ci.nsIResProtocolHandler);
  res.setSubstitution("browser_dbg_addon5", null);
}
