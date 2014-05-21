"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

SimpleTest.waitForExplicitFinish();

XPCOMUtils.defineLazyServiceGetter(this, "SettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

let tests = [
  function () {
    let callback = {
      handle: function() {
        ok(true, "handle called!");
        next();
      },

      handleAbort: function(name) {
        ok(false, "error: " + name);
        next();
      }
    }
    let lock = SettingsService.createLock(callback);
    lock.set("xasdf", true, null, null);
  }
];

function next() {
  let step = tests.shift();
  if (step) {
    try {
      step();
    } catch(e) {
      ok(false, "Test threw: " + e);
    }
  } else {
    SimpleTest.finish();
  }
}

next();
