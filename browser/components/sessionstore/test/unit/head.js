let Cu = Components.utils;
let Cc = Components.classes;
let Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");

// Call a function once initialization of SessionStartup is complete
let afterSessionStartupInitialization =
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
    let startup = Cc["@mozilla.org/browser/sessionstartup;1"].
      getService(Ci.nsIObserver);
    Services.obs.addObserver(startup, "final-ui-startup", false);
    Services.obs.addObserver(startup, "quit-application", false);
    Services.obs.notifyObservers(null, "final-ui-startup", "");
    Services.obs.addObserver(observer, "sessionstore-state-finalized", false);
};