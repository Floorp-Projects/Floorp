"use strict";

const Services = require("Services");

/*
 * Create a writable property by tracking it with a private variable.
 * We cannot make a normal property writeable on `exports` because
 * the module system freezes it.
 */
function makeWritableFlag(exports, name, pref) {
  let flag;
  // We don't have access to pref in worker, so disable all logs by default
  if (isWorker) {
    flag = false;
  } else {
    flag = Services.prefs.getBoolPref(pref, false);
    let prefObserver = () => {
      flag = Services.prefs.getBoolPref(pref, false);
    };
    Services.prefs.addObserver(pref, prefObserver);

    // Also listen for Loader unload to unregister the pref observer and prevent leaking
    let unloadObserver = function() {
      Services.prefs.removeObserver(pref, prefObserver);
      Services.obs.removeObserver(unloadObserver, "devtools:loader:destroy");
    };
    Services.obs.addObserver(unloadObserver, "devtools:loader:destroy");
  }
  Object.defineProperty(exports, name, {
    get: function() {
      return flag;
    }
  });
}

makeWritableFlag(exports, "wantLogging", "devtools.debugger.log");
makeWritableFlag(exports, "wantVerbose", "devtools.debugger.log.verbose");

// When the testing flag is set, various behaviors may be altered from
// production mode, typically to enable easier testing or enhanced
// debugging.
makeWritableFlag(exports, "testing", "devtools.testing");
