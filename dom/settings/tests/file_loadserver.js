var Ci = Components.interfaces;
var Cc = Components.classes;
var Cu = Components.utils;

// Stolen from SpecialPowers, since at this point we don't know we're in a test.
var isMainProcess = function() {
  try {
    return Cc["@mozilla.org/xre/app-info;1"].
        getService(Ci.nsIXULRuntime).
        processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
  } catch (e) { }
  return true;
};

if (isMainProcess()) {
  Components.utils.import("resource://gre/modules/SettingsRequestManager.jsm");
}
