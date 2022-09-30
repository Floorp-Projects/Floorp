/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that preference helpers work properly with custom types of Float and Json.

const { PrefsHelper } = require("resource://devtools/client/shared/prefs.js");

function test() {
  const Prefs = new PrefsHelper("prefs.helper.test", {
    float: ["Float", "float"],
    json: ["Json", "json"],
    jsonWithSpecialChar: ["Json", "jsonWithSpecialChar"],
  });

  Prefs.registerObserver();

  // JSON
  Services.prefs.setCharPref("prefs.helper.test.json", '{"a":1}');
  is(Prefs.json.a, 1, "The JSON pref value is correctly casted on get.");

  Prefs.json = { b: 2 };
  is(
    Prefs.json.a,
    undefined,
    "The JSON pref value is correctly casted on set (1)."
  );
  is(Prefs.json.b, 2, "The JSON pref value is correctly casted on set (2).");

  // JSON with special character
  Services.prefs.setStringPref(
    "prefs.helper.test.jsonWithSpecialChar",
    `{"hello":"おはよう皆！"}`
  );
  is(
    Prefs.jsonWithSpecialChar.hello,
    "おはよう皆！",
    "The JSON pref value with a special character is correctly stored."
  );

  Prefs.jsonWithSpecialChar = { bye: "さよなら！" };
  is(
    Prefs.jsonWithSpecialChar.hello,
    undefined,
    "The JSON with the special characters pref value is correctly set. (1)"
  );
  is(
    Prefs.jsonWithSpecialChar.bye,
    "さよなら！",
    "The JSON with the special characters pref value is correctly set. (2)"
  );

  // Float
  Services.prefs.setCharPref("prefs.helper.test.float", "3.14");
  is(Prefs.float, 3.14, "The float pref value is correctly casted on get.");

  Prefs.float = 6.28;
  is(Prefs.float, 6.28, "The float pref value is correctly casted on set.");

  Prefs.unregisterObserver();

  Services.prefs.clearUserPref("prefs.helper.test.float");
  Services.prefs.clearUserPref("prefs.helper.test.json");
  Services.prefs.clearUserPref("prefs.helper.test.jsonWithSpecialChar");
  finish();
}
