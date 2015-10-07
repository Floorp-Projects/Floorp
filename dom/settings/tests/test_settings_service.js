"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

if (SpecialPowers.isMainProcess()) {
  SpecialPowers.Cu.import("resource://gre/modules/SettingsRequestManager.jsm");
}

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

SimpleTest.waitForExplicitFinish();

XPCOMUtils.defineLazyServiceGetter(this, "SettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

var tests = [
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
        const MOZSETTINGS_CHANGED   = "mozsettings-changed";
        const TEST_OBSERVER_KEY     = "test.observer.key";
        const TEST_OBSERVER_VALUE   = true;
        const TEST_OBSERVER_MESSAGE = "test.observer.message";

        var obs = {
          observe: function (subject, topic, data) {

            if (topic !== MOZSETTINGS_CHANGED) {
              ok(false, "Event is not mozsettings-changed.");
              return;
            }
            // Data is now stored in subject
            if ("wrappedJSObject" in subject) {
              ok(true, "JS object wrapped into subject");
              subject = subject.wrappedJSObject;
            }
            if (subject["key"] != TEST_OBSERVER_KEY) {
              return;
            }

            function checkProp(name, type, value) {
              ok(name in subject, "subject." + name + " is present");
              is(typeof subject[name], type, "subject." + name + " is " + type);
              is(subject[name], value, "subject." + name + " is " + value);
            }

            checkProp("key", "string", TEST_OBSERVER_KEY);
            checkProp("value", "boolean", TEST_OBSERVER_VALUE);
            checkProp("isInternalChange", "boolean", true);

            Services.obs.removeObserver(this, MOZSETTINGS_CHANGED);
            next();
          }
        };

        Services.obs.addObserver(obs, MOZSETTINGS_CHANGED, false);

        let lock = SettingsService.createLock();
        lock.set(TEST_OBSERVER_KEY, TEST_OBSERVER_VALUE, null);
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
