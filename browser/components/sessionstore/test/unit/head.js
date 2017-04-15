var Cu = Components.utils;
var Cc = Components.classes;
var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");

// Call a function once initialization of SessionStartup is complete
function afterSessionStartupInitialization(cb) {
  do_print("Waiting for session startup initialization");
  let observer = function() {
    try {
      do_print("Session startup initialization observed");
      Services.obs.removeObserver(observer, "sessionstore-state-finalized");
      cb();
    } catch (ex) {
      do_throw(ex);
    }
  };

  // We need the Crash Monitor initialized for sessionstartup to run
  // successfully.
  Components.utils.import("resource://gre/modules/CrashMonitor.jsm");
  CrashMonitor.init();

  // Start sessionstartup initialization.
  let startup = Cc["@mozilla.org/browser/sessionstartup;1"].
    getService(Ci.nsIObserver);
  Services.obs.addObserver(startup, "final-ui-startup");
  Services.obs.addObserver(startup, "quit-application");
  Services.obs.notifyObservers(null, "final-ui-startup");
  Services.obs.addObserver(observer, "sessionstore-state-finalized");
}
