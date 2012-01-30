/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  let prefs = TiltUtils.Preferences;
  ok(prefs, "The TiltUtils.Preferences wasn't found.");

  prefs.create("test-pref-bool", "boolean", true);
  prefs.create("test-pref-int", "integer", 42);
  prefs.create("test-pref-string", "string", "hello world!");

  is(prefs.get("test-pref-bool", "boolean"), true,
    "The boolean test preference wasn't initially set correctly.");
  is(prefs.get("test-pref-int", "integer"), 42,
    "The integer test preference wasn't initially set correctly.");
  is(prefs.get("test-pref-string", "string"), "hello world!",
    "The string test preference wasn't initially set correctly.");


  prefs.set("test-pref-bool", "boolean", false);
  prefs.set("test-pref-int", "integer", 24);
  prefs.set("test-pref-string", "string", "!dlrow olleh");

  is(prefs.get("test-pref-bool", "boolean"), false,
    "The boolean test preference wasn't changed correctly.");
  is(prefs.get("test-pref-int", "integer"), 24,
    "The integer test preference wasn't changed correctly.");
  is(prefs.get("test-pref-string", "string"), "!dlrow olleh",
    "The string test preference wasn't changed correctly.");


  is(typeof prefs.get("unknown-pref", "boolean"), "undefined",
    "Inexisted boolean prefs should be handled as undefined.");
  is(typeof prefs.get("unknown-pref", "integer"), "undefined",
    "Inexisted integer prefs should be handled as undefined.");
  is(typeof prefs.get("unknown-pref", "string"), "undefined",
    "Inexisted string prefs should be handled as undefined.");


  is(prefs.get("test-pref-bool", "integer"), null,
    "The get() boolean function didn't handle incorrect types as it should.");
  is(prefs.get("test-pref-bool", "string"), null,
    "The get() boolean function didn't handle incorrect types as it should.");
  is(prefs.get("test-pref-int", "boolean"), null,
    "The get() integer function didn't handle incorrect types as it should.");
  is(prefs.get("test-pref-int", "string"), null,
    "The get() integer function didn't handle incorrect types as it should.");
  is(prefs.get("test-pref-string", "boolean"), null,
    "The get() string function didn't handle incorrect types as it should.");
  is(prefs.get("test-pref-string", "integer"), null,
    "The get() string function didn't handle incorrect types as it should.");


  is(typeof prefs.get(), "undefined",
    "The get() function should not work if not enough params are passed.");
  is(typeof prefs.set(), "undefined",
    "The set() function should not work if not enough params are passed.");
  is(typeof prefs.create(), "undefined",
    "The create() function should not work if not enough params are passed.");


  is(prefs.get("test-pref-wrong-type", "wrong-type", 1), null,
    "The get() function should expect only correct pref types.");
  is(prefs.set("test-pref-wrong-type", "wrong-type", 1), false,
    "The set() function should expect only correct pref types.");
  is(prefs.create("test-pref-wrong-type", "wrong-type", 1), false,
    "The create() function should expect only correct pref types.");
}
