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
  /* Callback tests */
  function() {
    let callbackCount = 10;

    let callback = {
      handle: function(name, result) {
        switch (callbackCount) {
        case 10:
        case 9:
          is(result, true, "result is true");
          break;
        case 8:
        case 7:
          is(result, false, "result is false");
          break;
        case 6:
        case 5:
          is(result, 9, "result is 9");
          break;
        case 4:
        case 3:
          is(result, 9.4, "result is 9.4");
          break;
        case 2:
          is(result, false, "result is false");
          break;
        case 1:
          is(result, null, "result is null");
          break;
        default:
          ok(false, "Unexpected call: " + callbackCount);
        }

        --callbackCount;
        if (callbackCount === 0) {
          next();
        }
      },

      handleError: function(name) {
        ok(false, "error: " + name);
      }
    };

    let lock = SettingsService.createLock();
    let lock1 = SettingsService.createLock();

    lock.set("asdf", true, callback, null);
    lock1.get("asdf", callback);
    lock.get("asdf", callback);
    lock.set("asdf", false, callback, null);
    lock.get("asdf", callback);
    lock.set("int", 9, callback, null);
    lock.get("int", callback);
    lock.set("doub", 9.4, callback, null);
    lock.get("doub", callback);
    lock1.get("asdfxxx", callback);
  },

  /* Observer tests */
  function() {
    const XPCOM_SHUTDOWN        = "xpcom-shutdown";
    const MOZSETTINGS_CHANGED   = "mozsettings-changed";

    const TEST_OBSERVER_KEY     = "test.observer.key";
    const TEST_OBSERVER_VALUE   = true;
    const TEST_OBSERVER_MESSAGE = "test.observer.message";

    let observerCount = 2;

    function observer(subject, topic, data) {
      if (topic === XPCOM_SHUTDOWN) {
        Services.obs.removeObserver(this, XPCOM_SHUTDOWN);
        Services.obs.removeObserver(this, MOZSETTINGS_CHANGED);
        return;
      }

      if (topic !== MOZSETTINGS_CHANGED) {
        ok(false, "Event is not mozsettings-changed.");
        return;
      }

      data = JSON.parse(data);

      function checkProp(name, type, value) {
        ok(name in data, "data." + name + " is present");
        is(typeof data[name], type, "data." + name + " is " + type);
        is(data[name], value, "data." + name + " is " + value);
      }

      checkProp("key", "string", TEST_OBSERVER_KEY);
      checkProp("value", "boolean", TEST_OBSERVER_VALUE);
      if (observerCount === 2) {
        checkProp("message", "object", null);
      } else {
        checkProp("message", "string", TEST_OBSERVER_MESSAGE);
      }
      --observerCount;

      if (observerCount === 0) {
        next();
      }
    }

    Services.obs.addObserver(observer, XPCOM_SHUTDOWN, false);
    Services.obs.addObserver(observer, MOZSETTINGS_CHANGED, false);

    let lock = SettingsService.createLock();
    lock.set(TEST_OBSERVER_KEY, TEST_OBSERVER_VALUE, null, null);
    lock.set(TEST_OBSERVER_KEY, TEST_OBSERVER_VALUE, null, TEST_OBSERVER_MESSAGE);
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
