const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/jsdebugger.jsm");

const testingFunctions = Cu.getJSTestingFunctions();
const systemPrincipal = Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);

function addTestingFunctionsToGlobal(global) {
  for (let k in testingFunctions) {
    global[k] = testingFunctions[k];
  }
  global.print = do_print;
  global.newGlobal = newGlobal;
  addDebuggerToGlobal(global);
}

function newGlobal() {
  const global = new Cu.Sandbox(systemPrincipal, { freshZone: true });
  addTestingFunctionsToGlobal(global);
  return global;
}

addTestingFunctionsToGlobal(this);

function executeSoon(f) {
  Services.tm.dispatchToMainThread({ run: f });
}

// The onGarbageCollection tests don't play well gczeal settings and lead to
// intermittents.
if (typeof gczeal == "function") {
  gczeal(0);
}

// Make sure to GC before we start the test, so that no zones are scheduled for
// GC before we start testing onGarbageCollection hooks.
gc();
