/* -*-  indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "addDebuggerToGlobal" ];

/*
 * This is the js module for Debugger. Import it like so:
 *   Components.utils.import("resource://gre/modules/jsdebugger.jsm");
 *   addDebuggerToGlobal(this);
 *
 * This will create a 'Debugger' object, which provides an interface to debug
 * JavaScript code running in other compartments in the same process, on the
 * same thread.
 *
 * For documentation on the API, see:
 *   https://developer.mozilla.org/en-US/docs/Tools/Debugger-API
 */

const init = Components.classes["@mozilla.org/jsdebugger;1"].createInstance(Components.interfaces.IJSDebugger);
this.addDebuggerToGlobal = function addDebuggerToGlobal(global) {
  init.addClass(global);
  initPromiseDebugging(global);
};

function initPromiseDebugging(global) {
  if (global.Debugger.Object.prototype.PromiseDebugging) {
    return;
  }

  // If the PromiseDebugging object doesn't have all legacy functions, we're
  // using the new accessors on Debugger.Object already.
  if (!PromiseDebugging.getDependentPromises) {
    return;
  }

  // Otherwise, polyfill them using PromiseDebugging.
  global.Debugger.Object.prototype.PromiseDebugging = PromiseDebugging;
  global.eval(polyfillSource);
}

let polyfillSource = `
  Object.defineProperty(Debugger.Object.prototype, "promiseState", {
    get() {
      const state = this.PromiseDebugging.getState(this.unsafeDereference());
      return {
        state: state.state,
        value: this.makeDebuggeeValue(state.value),
        reason: this.makeDebuggeeValue(state.reason)
      };
    }
  });
  Object.defineProperty(Debugger.Object.prototype, "promiseLifetime", {
    get() {
      return this.PromiseDebugging.getPromiseLifetime(this.unsafeDereference());
    }
  });
  Object.defineProperty(Debugger.Object.prototype, "promiseTimeToResolution", {
    get() {
      return this.PromiseDebugging.getTimeToSettle(this.unsafeDereference());
    }
  });
  Object.defineProperty(Debugger.Object.prototype, "promiseDependentPromises", {
    get() {
      let promises = this.PromiseDebugging.getDependentPromises(this.unsafeDereference());
      return promises.map(p => this.makeDebuggeeValue(p));
    }
  });
  Object.defineProperty(Debugger.Object.prototype, "promiseAllocationSite", {
    get() {
      return this.PromiseDebugging.getAllocationStack(this.unsafeDereference());
    }
  });
  Object.defineProperty(Debugger.Object.prototype, "promiseResolutionSite", {
    get() {
      let state = this.promiseState.state;
      if (state === "fulfilled") {
        return this.PromiseDebugging.getFullfillmentStack(this.unsafeDereference());
      } else {
        return this.PromiseDebugging.getRejectionStack(this.unsafeDereference());
      }
    }
  });
`;
