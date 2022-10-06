"use strict";

const { addDebuggerToGlobal } = ChromeUtils.importESModule(
  "resource://gre/modules/jsdebugger.sys.mjs"
);

const SYSTEM_PRINCIPAL = Cc["@mozilla.org/systemprincipal;1"].createInstance(
  Ci.nsIPrincipal
);

function addTestingFunctionsToGlobal(global) {
  global.eval(
    `
      const testingFunctions = Cu.getJSTestingFunctions();
      for (let k in testingFunctions) {

        this[k] = testingFunctions[k];
      }
      `
  );
  if (!global.print) {
    global.print = info;
  }
  if (!global.newGlobal) {
    global.newGlobal = newGlobal;
  }
  if (!global.Debugger) {
    addDebuggerToGlobal(global);
  }
}

addTestingFunctionsToGlobal(this);

/* Create a new global, with all the JS shell testing functions. Similar to the
 * newGlobal function exposed to JS shells, and useful for porting JS shell
 * tests to xpcshell tests.
 */
function newGlobal(args) {
  const global = new Cu.Sandbox(SYSTEM_PRINCIPAL, {
    freshCompartment: true,
    ...args,
  });
  addTestingFunctionsToGlobal(global);
  return global;
}
